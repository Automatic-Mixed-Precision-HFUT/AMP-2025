#pragma once

#ifndef PRECISION_LOWERING
#define PRECISION_LOWERING

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Pass.h>
#include <llvm//IR/Module.h>
#include <llvm/Support/CommandLine.h>

#include <vector>
#include <set>

namespace llvm {
    class Value;
    class BinaryOperator;
    class FCmpInst;
    class Type;
}


using namespace std;
using namespace llvm;

class PrecisionLoweringPass :  public llvm::PassInfoMixin<PrecisionLoweringPass>{
    public:
        llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);
        
    private:
        bool runOnFunction(Function &function);
        bool hasSIToFPInst(BinaryOperator *binOp);

    private:
        DebugInfoFinder debugInfo;
        vector<Instruction*> eraseInsts;

};

#endif