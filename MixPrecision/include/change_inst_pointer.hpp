#pragma once

#ifndef CHANGE_INST_POINTER
#define CHANGE_INST_POINTER


#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instructions.h>
// #include "Debug.h"

#include "utils.hpp"

namespace llvm {
class Value;
class ConstantInt;
}

using namespace std;
using namespace llvm;

class ChangeInstPointer : public InstVisitor<ChangeInstPointer, bool> {
    public:
        ChangeInstPointer(LLVMContext &context, Value::use_iterator it, Value *newTarget, Value *oldTarget,
                    PtrDep newType, PtrDep oldType, unsigned alignment)
                    : context(context), it(it), newTarget(newTarget), oldTarget(oldTarget),
                    newTypeP(newType), oldTypeP(oldType), alignment(alignment) {}

  // 调用入口：自动分发到具体指令的 visitXXXInst
        bool change(Instruction &inst) {
            return visit(inst);
        }

  // 覆盖具体指令处理逻辑
        bool visitLoadInst(LoadInst &inst);
        bool visitStoreInst(StoreInst &inst);
        bool visitBitCastInst(BitCastInst &inst);
        bool visitGetElementPtrInst(GetElementPtrInst &inst);
        bool visitCallInst(CallInst &inst);
        bool visitFPTruncInst(FPTruncInst &inst);
        bool visitBinaryOperator(BinaryOperator &inst);
        bool visitFPExtInst(FPExtInst &inst);
        bool visitFCmpInst(FCmpInst &inst);
        bool visitInstruction(Instruction &I) {
    return false;  // 所有没特殊处理的指令，默认返回 false
}


  // 如果需要，补充其他指令

  // 工具方法
        ConstantInt* getInt32(llvm::LLVMContext& context,int n) {
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n);
        }


        ConstantInt* getInt64(llvm::LLVMContext& context,int n) {
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), n);
        }
inline Function* findTargetFunctionP(Value *v) {
  if (auto *func = dyn_cast<Function>(v->stripPointerCasts())) {
    return func;
  }
  return nullptr;
}
// 创建 float 类型的 PtrDep（dep = 0）
inline PtrDep getFloatPtrDep(llvm::LLVMContext &ctx) {
    return PtrDep(llvm::Type::getFloatTy(ctx), 0);
}

// 创建 double 类型的 PtrDep（dep = 0）
inline PtrDep getDoublePtrDep(llvm::LLVMContext &ctx) {
    return PtrDep(llvm::Type::getDoubleTy(ctx), 0);
}

// 创建 half 类型的 PtrDep（dep = 0）
inline PtrDep getHalfPtrDep(llvm::LLVMContext &ctx) {
    return PtrDep(llvm::Type::getHalfTy(ctx), 0);
}


inline static Value* transTyP(llvm::Type* old_ty,llvm::Instruction* _new,llvm::Instruction* insert_before){
  if(old_ty->isPointerTy()||old_ty->isArrayTy()){
    return nullptr;
  }

  auto new_ty=_new->getType();
  if(old_ty==new_ty){
    return _new;
  }
  assert(new_ty->getTypeID()<=Type::TypeID::DoubleTyID&&new_ty->getTypeID()>=Type::TypeID::HalfTyID);
  assert(old_ty->getTypeID()<=Type::TypeID::DoubleTyID&&old_ty->getTypeID()>=Type::TypeID::HalfTyID);
  if(new_ty->getTypeID()<old_ty->getTypeID()){
    return new FPExtInst(_new,old_ty, "",insert_before);
  }else{
    return new FPTruncInst(_new,old_ty, "",insert_before);
  }
}

                template <typename Container>
        struct Obfuscator {
            using Iter = decltype(std::begin(std::declval<Container&>()));
            using ElemPtr = typename std::iterator_traits<Iter>::value_type;
            using Elem = typename std::remove_pointer<ElemPtr>::type;

            static_assert(std::is_same<ElemPtr, Instruction*>::value, "ElemPtr must be Instruction*");
            static_assert(std::is_class<Elem>::value, "Elem must be a class type");

            static void erase(Container &c) {
                auto fn = [](ElemPtr const& p) noexcept -> decltype(auto) {
                    // 通过std::invoke调用成员函数eraseFromParent
                    return std::invoke(&Elem::eraseFromParent, *p), void();
                };

                std::for_each(std::begin(c), std::end(c), fn);
            }
        };

        inline void removeDeadInsts(){
            Obfuscator<decltype(deadInsts)>::erase(deadInsts);
        }

    private:
        LLVMContext &context;
        Value::use_iterator it;
        Value *newTarget;
        Value *oldTarget;
        PtrDep newTypeP;
        PtrDep oldTypeP;
        unsigned alignment;
        vector<Instruction*> deadInsts;
};




#endif