#pragma once

#ifndef UTILS
#define UTILS

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>

#include <stdio.h>
#include <numeric> 
#include <variant>
#include <type_traits>
#include <string_view>

#include "change_visitor.hpp"
#define UNUSED __attribute__((__unused__))


inline static Type* GetPointerElementType(Value *ptr) {
  for (auto *U : ptr->users()) {
    if (auto *LI = dyn_cast<LoadInst>(U)) {
      return LI->getType();
    } else if (auto *SI = dyn_cast<StoreInst>(U)) {
      return SI->getValueOperand()->getType();
    } else if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
      return GEP->getSourceElementType();
    } else if (auto *CI = dyn_cast<CallInst>(U)) {
      return CI->getFunctionType()->getReturnType();
    } else if (auto *AI = dyn_cast<AllocaInst>(U)) {
      return AI->getAllocatedType();
    } else if (auto *GV = dyn_cast<GlobalVariable>(U)) {
      return GV->getValueType();
    }
  }
  return nullptr; // 如果无法推断类型，返回 nullptr
}



inline Type* getElementType(Type* type,Value *value) {
  Type *elementType = NULL;
  if (ArrayType *arrayType = dyn_cast<ArrayType>(type)) {
    elementType = arrayType->getElementType();
  } else if (PointerType *pointerType = dyn_cast<PointerType>(type)) {
    //修改
//    if (pointerType->isOpaque()){
      elementType= GetPointerElementType(value);
//    }else {
//      elementType = pointerType->getPointerElementType();
//    }
  }
  return elementType;
}

#endif