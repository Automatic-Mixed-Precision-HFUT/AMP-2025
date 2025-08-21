#include <llvm/IR/Module.h>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <fstream>
#include <bitset>

#include "precision_lowering.hpp"
#include "change_precision.hpp"

constexpr unsigned MAX_OPCODE = llvm::Instruction::OtherOpsEnd;

std::bitset<MAX_OPCODE> initOpsBitset(){
    std::bitset<MAX_OPCODE> bs;
    bs.set(llvm::Instruction::FAdd);
    bs.set(llvm::Instruction::FSub);
    bs.set(llvm::Instruction::FMul);
    bs.set(llvm::Instruction::FDiv);
    return bs;
}

const std::bitset<MAX_OPCODE> OpsBitset = initOpsBitset();


llvm::PreservedAnalyses PrecisionLoweringPass::run(llvm::Module &module, llvm::ModuleAnalysisManager &AM){
    ModulePassManager MPM;
    MPM.addPass(ChangePrecisionPass());
    MPM.run(module, AM);

    errs() << "Start precision lowering:\n";
    debugInfo.processModule(module);
    
    for (llvm::Function &F : module) {
        if (F.hasFnAttribute(llvm::Attribute::NoInline))
            F.removeFnAttr(llvm::Attribute::NoInline);

        if (F.hasFnAttribute(llvm::Attribute::OptimizeNone))
            F.removeFnAttr(llvm::Attribute::OptimizeNone);

        runOnFunction(F);
    }


    errs() << "Precision lowering completed!\n";
    return PreservedAnalyses::none();
}


bool PrecisionLoweringPass::hasSIToFPInst(BinaryOperator *binOp) {
    Value *op0 = binOp->getOperand(0);
    Value *op1 = binOp->getOperand(1);

    return (isa<CastInst>(op0) && isa<SIToFPInst>(op1)) ||
           (isa<CastInst>(op1) && isa<SIToFPInst>(op0));
}



bool PrecisionLoweringPass::runOnFunction(Function &func){
    string funcName = func.getName().str();
    if(!func.isDeclaration()){
        errs()<<"\tProcessing function:"<<funcName<<"\n";
        for(auto &bb: func){
            for(auto &inst: bb){
                if(auto binOpInst = dyn_cast<BinaryOperator>(&inst)){
                    if(OpsBitset.test(binOpInst->getOpcode())){
                        Value *val1 = binOpInst->getOperand(0);
                        Value *val2 = binOpInst->getOperand(1);
                        if(hasSIToFPInst(binOpInst)){
                            SIToFPInst *spInst = dyn_cast<SIToFPInst>(val1) ?: dyn_cast<SIToFPInst>(val2);
                            CastInst  *castInst = dyn_cast<CastInst>(spInst == dyn_cast<SIToFPInst>(val1) ? val2 : val1);

                            assert(spInst && castInst && "Operands must be SIToFPInst and CastInst");

                            spInst = new SIToFPInst(spInst->getOperand(0), castInst->getOperand(0)->getType(), "", binOpInst);
                            BinaryOperator *newTarget = BinaryOperator::Create(binOpInst->getOpcode(), castInst->getOperand(0), spInst, "", binOpInst);

                            if (newTarget->getType()->getTypeID() < binOpInst->getType()->getTypeID()) {
                                FPExtInst *ext = new FPExtInst(newTarget, binOpInst->getType(), "", binOpInst);
                                binOpInst->replaceAllUsesWith(ext);
                            }
                            else {
                                binOpInst->replaceAllUsesWith(newTarget);
                            }
                        }
                        else{
                            auto tryRemoveCast = [](Value *&val, CastInst *&castInst) {
                                castInst = dyn_cast<FPExtInst>(val);
                                if (castInst) {
                                    val = castInst->getOperand(0);
                                    return;
                                }
                                castInst = dyn_cast<FPTruncInst>(val);
                                if (castInst) {
                                    val = castInst->getOperand(0);
                                }
                            };

                            CastInst *castInst1 = nullptr;
                            CastInst *castInst2 = nullptr;
                            
                            tryRemoveCast(val1, castInst1);
                            tryRemoveCast(val2, castInst2);

                            if ((castInst1 == NULL && castInst2 == NULL) ||
                                (val1->getType()->getTypeID() == binOpInst->getType()->getTypeID()) ||
                                (val2->getType()->getTypeID() == binOpInst->getType()->getTypeID())) {
                                    continue;
                            }

    
                            Value *&smallerVal = (val1->getType()->getTypeID() < val2->getType()->getTypeID()) ? val1 : val2;
                            Type *largerType = (val1->getType()->getTypeID() > val2->getType()->getTypeID()) ? val1->getType() : val2->getType();

                            if (val1->getType()->getTypeID() != val2->getType()->getTypeID()) {
                                smallerVal = CastInst::CreateFPCast(smallerVal, largerType, "", binOpInst);
                            }
                            
                            BinaryOperator *newTarget = BinaryOperator::Create(binOpInst->getOpcode(), val1, val2, "", binOpInst);

                            if (newTarget->getType()->getTypeID() < binOpInst->getType()->getTypeID()) {
                                FPExtInst *ext = new FPExtInst(newTarget, binOpInst->getType(), "", binOpInst);
                                binOpInst->replaceAllUsesWith(ext);
                            }
                            else if(newTarget->getType()->getTypeID() > binOpInst->getType()->getTypeID()){
                                FPTruncInst *tru = new FPTruncInst(newTarget, binOpInst->getType(), "", binOpInst);
                                binOpInst->replaceAllUsesWith(tru);
                            }
                            else{
                                binOpInst->replaceAllUsesWith(newTarget);
                            }
                            
                        }
                    }
                }
            }
        }
    }
    
    for(auto inst: eraseInsts) 
        inst->eraseFromParent();
    eraseInsts.clear();
    return true;
}