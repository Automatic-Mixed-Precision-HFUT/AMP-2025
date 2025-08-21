#include "utils.hpp"

bool ChangeInstPointer::visitStoreInst(StoreInst &inst){

  llvm::Align Alignment(alignment);
  StoreInst* storeInst = &inst;
  Value *valueOp = storeInst->getValueOperand();
  StoreInst* newStore UNUSED = NULL;
  Value *destination = storeInst->getOperand(1);


  if (newTypeP.dep!=0) {

auto castAndStore = [&](Value *val, Value *dest, Type *targetType) -> StoreInst* {
    Value *toStore = nullptr;

    if (val == oldTarget) {
        // 如果值是 oldTarget，直接 BitCast
        toStore = new BitCastInst(newTarget, oldTypeP.getPoint(), "", storeInst);
    } else if (dyn_cast<PointerType>(val->getType())) {
        // 如果值是指针，BitCast 到新类型
        toStore = new BitCastInst(val, newTypeP.getPoint(), "", storeInst);
    } else if (CallInst *ci = dyn_cast<CallInst>(newTarget)) {
        // 特殊处理 llvm.fmuladd.f16
        if (ci->getCalledFunction()->getName() == "llvm.fmuladd.f16" &&
            dyn_cast<PointerType>(destination->getType()) &&
            val->getType()->isFloatTy()) {

            PointerType *HalfPtrType = Type::getHalfPtrTy(context);
            Value *destCast = new BitCastInst(destination, HalfPtrType, "", storeInst);
            return new StoreInst(newTarget, destCast, false, Alignment, storeInst);
        }
        toStore = val;
    } else if (val->getType()->isFloatingPointTy()) {
        // 浮点类型转换
        if (targetType->getTypeID() > val->getType()->getTypeID())
            toStore = new FPExtInst(val, targetType, "", storeInst);
        else if (targetType->getTypeID() < val->getType()->getTypeID())
            toStore = new FPTruncInst(val, targetType, "", storeInst);
        else
            toStore = val;
    } else {
        // 其他情况直接用原值
        toStore = val;
    }

    return new StoreInst(toStore, dest, false, Alignment, storeInst);
};

// 使用函数生成 newStore
newStore = castAndStore(valueOp, (valueOp == oldTarget ? destination : newTarget), newTypeP.ty);

  } 
  else {
    auto doStore = [&](llvm::Value *val, llvm::Value *ptr) -> llvm::StoreInst* {
    llvm::Align alignObj(Alignment); // LLVM17要求Align类型
    return new llvm::StoreInst(val, ptr, false, alignObj, storeInst);
};

// 处理 ConstantFP
if (auto *constant = llvm::dyn_cast<llvm::ConstantFP>(valueOp)) {
    llvm::CastInst *fpOperand = llvm::CastInst::CreateFPCast(constant, newTypeP.ty, "", storeInst);
    newStore = doStore(fpOperand, newTarget);
} 
else if (auto *inst1 = llvm::dyn_cast<llvm::CallInst>(newTarget)) {
    auto name = inst1->getCalledFunction()->getName();
    if (name == "llvm.fmuladd.f32" || name == "llvm.fmuladd.f16" || name == "llvm.sqrt.f32" || name == "llvm.sqrt.f16") {
        if (llvm::PointerType *ptrType = llvm::dyn_cast<llvm::PointerType>(destination->getType())) {
            llvm::Value *targetPtr = destination;
            if ((name == "llvm.fmuladd.f32" || name == "llvm.sqrt.f32") && valueOp->getType()->isDoubleTy())
                targetPtr = new llvm::BitCastInst(destination, llvm::Type::getFloatPtrTy(context), "", storeInst);
            else if (name == "llvm.fmuladd.f16" && valueOp->getType()->isFloatTy())
                targetPtr = new llvm::BitCastInst(destination, llvm::Type::getHalfPtrTy(context), "", storeInst);
            else if (name == "llvm.sqrt.f16" && valueOp->getType()->isFloatTy())
                targetPtr = oldTarget;

            newStore = doStore(newTarget, targetPtr);
        }
    }
}
else {
    // 根据类型大小进行 FP 扩展或截断
    if (newTypeP.ty->getTypeID() > oldTypeP.ty->getTypeID()) {
        llvm::FPExtInst *ext = new llvm::FPExtInst(valueOp, newTypeP.ty, "", storeInst);
        newStore = doStore(ext, newTarget);
    } 
    else if (valueOp->getType()->isFloatTy() && !(newTypeP.ty->getTypeID() == llvm::Type::TypeID::HalfTyID)) {
        llvm::Value *valPtr = ((valueOp == newTarget) && (newTarget == oldTarget)) ? newTarget : valueOp;
        newStore = doStore(valPtr, (valueOp == newTarget && newTarget == oldTarget) ? destination : newTarget);
    }
    else if (valueOp->getType()->isHalfTy()) {
        llvm::Value *valPtr = ((valueOp == newTarget) && (newTarget == oldTarget)) ? newTarget : valueOp;
        newStore = doStore(valPtr, (valueOp == newTarget && newTarget == oldTarget) ? destination : newTarget);
    }
    else if (newTarget->getType()->isFloatTy() && valueOp->getType()->isDoubleTy()) {
        llvm::FPExtInst *ext = new llvm::FPExtInst(newTarget, oldTypeP.ty, "", storeInst);
        newStore = doStore(ext, destination);
    }
    else if (llvm::dyn_cast<llvm::CallInst>(valueOp) && (llvm::dyn_cast<llvm::GetElementPtrInst>(destination) || llvm::dyn_cast<llvm::BitCastInst>(destination))) {
        llvm::FPTruncInst *fptrunc = new llvm::FPTruncInst(valueOp, newTypeP.ty, "", storeInst);
        newStore = doStore(fptrunc, newTarget);

        if (auto *callinst = llvm::dyn_cast<llvm::CallInst>(valueOp))
            if (callinst->getCalledFunction()->getName() == "dlange")
                return false;
        return true;
    }
    else if (valueOp == oldTarget && newTypeP.ty->isHalfTy()) {
        llvm::FPExtInst *ext = nullptr;
        if (!newTarget->getType()->isDoubleTy())
            ext = new llvm::FPExtInst(newTarget, oldTypeP.ty, "", storeInst);

        if (valueOp->getType()->isDoubleTy()) {
            if (newTarget->getType()->isDoubleTy()) {
                llvm::Align align1(8);
                newStore = new llvm::StoreInst(newTarget, destination, false, align1, storeInst);
            } else {
                llvm::FPExtInst *ext2 = new llvm::FPExtInst(ext, valueOp->getType(), "", storeInst);
                newStore = doStore(ext2, destination);
            }
        } else {
            newStore = doStore(ext, destination);
        }
    }
    else {
        llvm::FPTruncInst *fptrunc = new llvm::FPTruncInst(valueOp, newTypeP.ty, "", storeInst);
        newStore = doStore(fptrunc, newTarget);

        if (newTypeP.ty->isHalfTy() || newTypeP.ty->isFloatTy()) {
            bool is_store = false, is_load = false;
            for (auto it = destination->use_begin(); it != destination->use_end(); ++it) {
                if (llvm::dyn_cast<llvm::StoreInst>(it->getUser())) is_store = true;
                else if (llvm::dyn_cast<llvm::LoadInst>(it->getUser())) is_load = true;
            }
            if (is_store && is_load) return true;
        }
    }
}

  }



 
  return false;
}