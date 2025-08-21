#pragma once

#ifndef CHANGE_PRECISION
#define CHANGE_PRECISION

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <map>

#include "ParseConfig.hpp"
#include "change_visitor.hpp"

using namespace std;
using namespace llvm;

class ChangePrecisionPass: public PassInfoMixin<ChangePrecisionPass>{
    public:
        ChangePrecisionPass():changes(nullptr) {}
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

    private:
        static unsigned getAlignment(Type* type){return type->isFloatTy() ? 4 
                                                                          : type->isDoubleTy() ? 8 
                                                                                               : type->isHalfTy() ? 2 : 0;}

        static ConstantInt* getInt32(LLVMContext& context, int n){return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n);}
        static ConstantInt* getInt64(LLVMContext& context, int n){return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), n);}

        void safeDeleteInstruction(Instruction* inst);
        static MDNode* getTypeMetadata(Module& module, DIVariable &oldDIVar, Type* newType);
        static void updateMetadata(Module& module, Value* oldTarget, Value* newTarget, Type* newType);
    
    private:
        map<ChangeType, Changes>const *const changes;
};

#endif