#include "../include/utils.hpp"
#include "precision_lowering.hpp"
#include "../include/ParseConfig.hpp"
#include "../include/CreateConfigFile.hpp"
#include "llvm/IR/Argument.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Passes/PassPlugin.h>
#include <vector>
//如果是一级指针，dep=0，
//如：double* 为{double,1},double** 为{double,2}
PtrDep resolvePointerElementType(Value *val){
  if(dyn_cast<Argument>(val)){
    return resolveTypeByUser(val,0);
  }
  return resolveTypeByUser(val,0)-1;
}
std::vector<PtrDep> resolveByUser(Value *val, int dep) ;
PtrDep resolveTypeByUser(Value *val,int dep) {
  Type* ty = nullptr;

  // 尝试直接获取底层类型的情况
  if (auto* alloca = dyn_cast<AllocaInst>(val)) {
    ty = alloca->getAllocatedType();
    if(ty->isPointerTy()==false)
      return PtrDep(ty, dep+1);
  }
  else if (auto* global = dyn_cast<GlobalVariable>(val)) {
    ty = global->getValueType();
    if(ty->isPointerTy()==false)
      return PtrDep(ty, dep+1);
  }else if(auto* arg = dyn_cast<Argument>(val)) {
    ty = arg->getType();
  }
  else if (auto* gep = dyn_cast<GetElementPtrInst>(val)) {
    ty = gep->getSourceElementType();
    if(ty->isPointerTy()==false)
      return PtrDep(ty, dep+1);
  }
  else if (auto* load = dyn_cast<LoadInst>(val)) {
      ty = load->getType();
  }
  // else if (auto* store = dyn_cast<StoreInst>(val)) {
  //     ty = store->getValueOperand()->getType();
  // }
  else if (auto* call = dyn_cast<CallInst>(val)) {
    ty = call->getFunctionType()->getReturnType();
  }
  else {
    // 其他情况直接用当前值类型
    ty = val->getType();
  }

  // 如果当前类型不是指针，递归终止，直接返回
  if (!ty->isPointerTy()) {
    return PtrDep(ty, dep);
  }

  // 当前类型是指针，尝试递归其所有用户
  // for (auto* user : val->users()) {
  //   if (auto* userInst = dyn_cast<Instruction>(user)) {
  //     auto pd= resolveByUser(userInst, dep);
  //     if(pd.ty->isPointerTy()||pd.ty->isVoidTy()){
  //       continue;
  //     }
  //     return pd;
  //   }
  // }
  auto pds= resolveByUser(val, dep);
  if(pds.empty()) return PtrDep(ty, dep);
  auto i=-1;
  for(auto [t,d]:pds){
    if(t->isPointerTy())continue;
    if(i==-1) i=d;
    if(d!=i){
      assert(0);
    }
  }
  for(auto pd:pds){
    if(pd.ty->isVoidTy()||pd.ty->isPointerTy()||pd.dep<0){
      continue;
    }else{
      return pd;
    }
  }

  // 如果没有用户或者用户都无法进一步解析，返回当前类型及当前指针层数cd
  return PtrDep(ty, dep);
}

// 递归 user 持续下降直到不是 pointer
std::vector<PtrDep> resolveByUser(Value *val, int dep) {
  std::vector<PtrDep> ptrdeps{};
  for (User* user : val->users()) {
    if (auto* ld = dyn_cast<LoadInst>(user)) {
      ptrdeps.push_back(resolveTypeByUser(ld, dep + 1));
    }
    else if (auto* st = dyn_cast<StoreInst>(user);st&&val==st->getValueOperand()) {
      // if(val==st->getPointerOperand())
        // ptrdeps.push_back(resolveTypeByUser(st->getValueOperand(), dep+1 ));
       ptrdeps.push_back(resolveTypeByUser(st->getPointerOperand(), dep-1));
      }
    else if (auto* gep = dyn_cast<GetElementPtrInst>(user)) {
      ptrdeps.push_back(resolveTypeByUser(gep, dep));
    }
    else if (auto* call = dyn_cast<CallInst>(user)) {
      Function *callee = call->getCalledFunction();
      if (!callee || callee->isDeclaration()) continue; //
      // 找到 val 是哪个参数
      for (unsigned i = 0; i < call->arg_size(); ++i) {
          if (call->getArgOperand(i) == val) {
              if (i >= callee->arg_size())
                  break; // 防御，实参与形参数量不符

              Argument *arg = callee->getArg(i);
              // 进入函数参数递归
              ptrdeps.push_back(resolveTypeByUser(arg, dep));
          }
      }
    }else{
      errs()<<*val;
    }
    if(ptrdeps.empty()==false){
      auto b=ptrdeps.back();
      if(b.ty->isVoidTy()==false&&b.ty->isPointerTy()==false){
        return ptrdeps;
      }
    }
  }
  // errs()<<*val;

  return ptrdeps;
}
class ParseConfigTest : public llvm::PassInfoMixin<ParseConfigTest> {
public:
    ParseConfigTest()=default;
    llvm::PreservedAnalyses run(Module &M, llvm::ModuleAnalysisManager &AM) {
    auto &config = AM.getResult<ParseConfigPass>(M);
    // errs() << "[config] Loaded " << config.getChanges().size() << " changes\n";
    return PreservedAnalyses::all(); // 不修改 IR
}
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "MyCombinedPasses", "v0.1",
    [](llvm::PassBuilder &PB) {

      // 1. 注册分析 Pass（必须）
      PB.registerAnalysisRegistrationCallback(
        [](llvm::ModuleAnalysisManager &MAM) {
          MAM.registerPass([]() { return ParseConfigPass(); });
        });


      // 2. 注册转换 Pass（可通过 -passes=xxx 调用）
      PB.registerPipelineParsingCallback(
        [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
           llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {

          if (Name == "parse-config") {
            MPM.addPass(ParseConfigTest());  // 这里 ParseConfigTest 是转换 Pass
            return true;
          }

          // 你其它的 Pass
          if (Name == "create-config") {
            MPM.addPass(CreateConfigFilePass());
            return true;
          }
 
          if (Name == "pl") {
            MPM.addPass(PrecisionLoweringPass());
            return true;
          }

          return false;
        });
    }
  };
}
