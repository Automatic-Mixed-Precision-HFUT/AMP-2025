#pragma once

#ifndef CHANGE_VISITOR
#define CHANGE_VISITOR

#include <llvm/IR/Instructions.h>

#include "change_inst.hpp"
#include "change_inst_pointer.hpp"

namespace llvm {
class Value;
class ConstantInt;
}

using namespace std;
using namespace llvm;


template<typename T>
struct TransformerImpl;

template<>
struct TransformerImpl<Type*> {
    static bool apply(LLVMContext &context, Value::use_iterator it,
                      Value* newTarget, Value* oldTarget,
                      Type* newType, Type* oldType, unsigned alignment) {
        Instruction *inst = dyn_cast<Instruction>(it->getUser());
        if (!inst) {
            // 不是指令，无法处理
            errs().changeColor(raw_ostream::RED, /*bold=*/true);
            errs()<<"NO INST!!!\n";
            errs().resetColor();
            return false;
        }
        ChangeInst changer(context, it, newTarget, oldTarget, newType, oldType, alignment);
        return changer.change(*inst);
    }
};

template<>
struct TransformerImpl<PtrDep> {
    static bool apply(LLVMContext &context, Value::use_iterator it,
                      Value* newTarget, Value* oldTarget,
                      PtrDep newType, PtrDep oldType, unsigned alignment) {
        Instruction *inst = dyn_cast<Instruction>(it->getUser());
        if (!inst) {
            // 不是指令，无法处理
            errs().changeColor(raw_ostream::RED, /*bold=*/true);
            errs()<<"NO INST!!!\n";
            errs().resetColor();
            return false;
        }
        ChangeInstPointer changer(context, it, newTarget, oldTarget, newType, oldType, alignment);
        return changer.change(*inst);
    }
};

class ChangeVisitor {
public:
    template<typename T>
    static bool changePrecision(
        LLVMContext &context,
        Value::use_iterator it,
        Value* newTarget,
        Value* oldTarget,
        T newType,
        T oldType,
        unsigned alignment) {
        return TransformerImpl<T>::apply(context, it, newTarget, oldTarget, newType, oldType, alignment);
    }
};



#endif