#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/ValueSymbolTable.h>
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Verifier.h>

#include <memory>

#include "change_precision.hpp"
#include "ParseConfig.hpp"

#include "utils.hpp"


void ChangePrecisionPass::safeDeleteInstruction(Instruction* inst) {
    if (!inst) return;
    while (!inst->use_empty()) {
        for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE;) {
            Instruction* userInst = dyn_cast<Instruction>(UI->getUser());
            ++UI;
            if (userInst) {
                safeDeleteInstruction(userInst);
            } else {
                UI->getUser()->replaceUsesOfWith(inst, UndefValue::get(inst->getType()));
            }
        }
    }
    inst->eraseFromParent();
}


PreservedAnalyses ChangePrecisionPass::run(Module &M, ModuleAnalysisManager &AM){
    errs() << "Change precision: \n";

    auto changes = AM.getResult<ParseConfigPass>(M).changes;

    for(auto &change:changes->at(LOCALVAR)) {
        AllocaInst* newTarget = nullptr;
        vector<Instruction *> eraseInsts;
        llvm::LLVMContext &context=M.getContext();
        //AllocaInst* newTarget = changeLocal(M,v.get());
        auto value = change.get()->getValue();
        auto newTypePD = change.get()->getType()[0];
        if(newTypePD.dep==0){
            auto newType = newTypePD.ty;
            if(AllocaInst *oldTarget = dyn_cast<AllocaInst>(value)){
                Type* oldType = oldTarget->getAllocatedType();
                errs().changeColor(raw_ostream::GREEN, /*bold=*/true);
                errs()<< "\tVariable\t\"" << oldTarget->getName() << "\"\t" << *oldType<< "\t-->\t" << *newType << "\n";
                errs().resetColor();

                if(oldType->getTypeID()!=newType->getTypeID()){
                    unsigned alignment = getAlignment(newType);
                    Align Alignment(alignment);
                    auto &DL=M.getDataLayout();    
                    newTarget = new AllocaInst(newType, DL.getAllocaAddrSpace(), nullptr, Alignment,"new", oldTarget);
                    newTarget->takeName(oldTarget);
                    for (auto &use : oldTarget->uses()) {
                        // 反查 use_iterator（注意是 find，不能直接用 &use）
                        auto it = std::find_if(oldTarget->use_begin(), oldTarget->use_end(),
                                [&](const llvm::Use &u) { return &u == &use; });
                   

                        bool is_erased = ChangeVisitor::changePrecision(context, it, newTarget, oldTarget, newType, oldType, alignment);

                        if (!is_erased || isa<StoreInst>(use.getUser())) {
                            eraseInsts.push_back(dyn_cast<Instruction>(use.getUser()));
                        }
                    }

                    if (newType->isHalfTy()) {
                        for (auto &use : oldTarget->uses()) {
                            if (auto *bitcast = dyn_cast<BitCastInst>(use.getUser())) {                              
                                bitcast->replaceAllUsesWith(newTarget);
                                eraseInsts.push_back(bitcast);
                            }
                        }
                    }

                    for (unsigned int i = 0; i < eraseInsts.size(); i++) {
                        Instruction *inst = eraseInsts[i];
                        if (!inst) {
                          
                            continue;
                        }
                        if (!inst->getParent()) {
            
                            continue;
                        }
        
                        // 先安全递归删除所有使用者，确保inst无use
                        while (!inst->use_empty()) {
                            for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ) {
                                Instruction *userInst = dyn_cast<Instruction>(UI->getUser());
                                ++UI;  // 先++，因为下面递归可能删除指令导致迭代器失效
                                if (userInst) {
                                    // 递归删除使用者
                                    // 你也可以这里添加对userInst的特殊处理逻辑，比如替换而非删除
                                    safeDeleteInstruction(userInst);
                                } else {
                                    // 非指令使用者，替换为undef
                                    UI->getUser()->replaceUsesOfWith(inst, UndefValue::get(inst->getType()));
                                }
                            }
                        }
        
                        inst->eraseFromParent();
                    }

                    oldTarget->eraseFromParent();

                }
                else{
                    errs().changeColor(raw_ostream::RED, /*bold=*/true);
                    errs()<< "\tNo precision conversion is needed for the variable\t"<< oldTarget->getName() <<"\n";
                    errs().resetColor();
                }

            }
        }
        else{
            auto newType = newTypePD;
            AllocaInst *oldTarget = dyn_cast<AllocaInst>(value);
            PointerType* oldPointerType = dyn_cast<PointerType>(oldTarget->getType());
            auto oldpd=resolvePointerElementType(oldTarget);
            errs().changeColor(raw_ostream::GREEN, /*bold=*/true);
            errs() << "\tPointer\t\"" << oldTarget->getName() << "\"\t" <<oldpd<< "\t-->\t" << newType << "\n";
            errs().resetColor();
            auto &DL = M.getDataLayout();
            if(oldpd!=newType){
                newTarget = new AllocaInst(newType.getPoint(), DL.getAllocaAddrSpace(), getInt32(context, 1), "", oldTarget);
                unsigned alignment = getAlignment(newType.ty);
                Align Alignment(alignment);
                newTarget->setAlignment(Alignment);
                newTarget->takeName(oldTarget);
                for (auto &use : oldTarget->uses()) {
                    // 找出这个 use 对应的 iterator
                    auto it = std::find_if(oldTarget->use_begin(), oldTarget->use_end(),
                        [&](const Use &u) { return &u == &use; });
                    if (it != oldTarget->use_end()) {
                    bool is_erased = ChangeVisitor::changePrecision(context, it, newTarget, oldTarget, newType, oldpd, alignment);
                        if (!is_erased) {
                            if (auto *inst = dyn_cast<Instruction>(use.getUser()))
                                eraseInsts.push_back(inst);
                        }
                    }
                }

                for (Instruction *inst : eraseInsts) {
                    inst->eraseFromParent();
                }

                oldTarget->eraseFromParent();
            }
            else{
                errs().changeColor(raw_ostream::RED, /*bold=*/true);
                errs()<< "\tNo precision conversion is needed for the variable\t"<< oldTarget->getName() <<"\n";
                errs().resetColor();
            }
        }
        
        if(newTarget){
            updateMetadata(M, value, newTarget, newTypePD.ty);
        }
    }
    

    errs() << "Change precision completed!!! \n";
    return PreservedAnalyses::none();
}

MDNode* ChangePrecisionPass::getTypeMetadata(Module &module, DIVariable &oldDIVar, Type *newType) {
  
  const DIType *oldDIType = oldDIVar.getType();
  vector<Value*> operands;
  LLVMContext &context=module.getContext();
  switch(newType->getTypeID()) {
  case Type::FloatTyID:
    operands.push_back(getInt32(context,524324));
    operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(1).get())->getValue()); // preserve old context
    operands.push_back(MetadataAsValue::get(module.getContext(),MDString::get(module.getContext(), "float")));
    operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(3).get())->getValue()); // preserve old file descriptor
    operands.push_back(getInt32(context,0));
    operands.push_back(getInt64(context,32));
    operands.push_back(getInt64(context,32));
    operands.push_back(getInt64(context,0));
    operands.push_back(getInt32(context,0));
    operands.push_back(getInt32(context,4));
    break;
  case Type::DoubleTyID:
    operands.push_back(getInt32(context,524324));
    operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(1).get())->getValue()); // preserve old context
    operands.push_back(MetadataAsValue::get(module.getContext(),MDString::get(module.getContext(), "double")));
    operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(3).get())->getValue()); // preserve old file descriptor
    operands.push_back(getInt32(context,0));
    operands.push_back(getInt64(context,64));
    operands.push_back(getInt64(context,64));
    operands.push_back(getInt64(context,0));
    operands.push_back(getInt32(context,0));
    operands.push_back(getInt32(context,4));
    break;
  case Type::PointerTyID: { // added on 07/26
    // if (newType->isOpaquePointerTy()){
      break;
    // }else{
    // Type *elementType = (dyn_cast<PointerType>(newType))->getPointerElementType();
    // operands.push_back(getInt32(context,786447));
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(1).get())->getValue()); // preserve old file descriptor
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(2).get())->getValue()); // preserve old context
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(3).get())->getValue()); // preserve old name
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(4).get())->getValue()); // preserve old line number
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(5).get())->getValue()); // preserve old size in bits
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(6).get())->getValue()); // preserve old align in bits
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(7).get())->getValue()); // preserve old offset in bits
    // operands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIType)->getOperand(8).get())->getValue()); // preserve old flags
    // operands.push_back(MetadataAsValue::get(module.getContext(),getTypeMetadata(module, oldDIVar, elementType))); // element basic type
    // }
    break;
  }
  default:

    exit(1);
  }
  std::vector<llvm::Metadata*> metadataOperands;
  for (llvm::Value* value : operands) {
    metadataOperands.push_back(llvm::ValueAsMetadata::get(value));
  }
  llvm::ArrayRef<llvm::Metadata*> arrayRefOperands(metadataOperands);
  return MDNode::get(module.getContext(), arrayRefOperands);
}

void ChangePrecisionPass::updateMetadata(Module& module, Value* oldTarget, Value* newTarget, Type* newType) {

  vector<Instruction*> to_remove;
  LLVMContext& context = module.getContext();
  if (newTarget) {
  
    bool changed = false;

    for(Module::iterator f = module.begin(), fe = module.end(); f != fe; f++) {
      for(Function::iterator b = f->begin(), be = f->end(); b != be; b++) {
        for(BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; i++) {

          if (DbgDeclareInst *oldDeclare = dyn_cast<DbgDeclareInst>(i)) {
            if (Value *address = oldDeclare->getAddress()) {
              if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(address)) {
                if (allocaInst == oldTarget) { // the alloca we are looking for

                  // DIVariable oldDIVar(oldDeclare->getVariable());
                  auto* oldDIVar = oldDeclare->getVariable();
                  // MDNode* newDIType = getTypeMetadata(module, oldDIVar, newType);
                  MDNode* newDIType = getTypeMetadata(module, *dyn_cast<DIVariable>(oldDIVar), newType);
                  // construct new DIVariable with new type descriptor
                  vector<Value*> doperands;
                  for(unsigned i = 0; i < oldDIVar->getNumOperands(); i++) {
                    if (i == 5) { // the argument that corresponds to the type descriptor
                      Value* metadataValue = MetadataAsValue::get(context, newDIType);
                      doperands.push_back(metadataValue);

                    }
                    else {
                      doperands.push_back(dyn_cast<ValueAsMetadata>(dyn_cast<MDNode>(oldDIVar)->getOperand(i).get())->getValue()); // preserve other descriptors
                    }
                  }
                  std::vector<llvm::Metadata*> metadataOperands;
                  for (llvm::Value* value : doperands) {
                    metadataOperands.push_back(llvm::ValueAsMetadata::get(value));
                  }
                  ArrayRef<llvm::Metadata*> *arrayRefDOperands = new ArrayRef<llvm::Metadata*>(metadataOperands);
                  MDNode* newMDNode = MDNode::get(module.getContext(), *arrayRefDOperands);

                  auto  *newDIVar = dyn_cast<DILocalVariable>(newMDNode);

                  // insert new declare instruction
                  DIBuilder builder(module);
                  const DILocation* dl = oldDeclare->getDebugLoc();
                  DIExpression* expr = builder.createExpression();
                  //BasicBlock* insertAtEnd = oldDeclare->getParent();
                  Instruction *newDeclare = builder.insertDeclare(newTarget, newDIVar,expr,dl,oldDeclare);

                  // make sure the declare instruction preserves its own metadata
                  unsigned id = 0;
                  if (oldDeclare->getMetadata(id)) {
                    newDeclare->setMetadata(id, oldDeclare->getMetadata(id));
                  }
                  to_remove.push_back(oldDeclare); // can't erase while iterating through instructions
                  changed = true;
                }
              }
            }
          }
        }
      }
    }
    for(unsigned i = 0; i < to_remove.size(); i++) {
      to_remove[i]->eraseFromParent();
    }

  }
  return;
}
