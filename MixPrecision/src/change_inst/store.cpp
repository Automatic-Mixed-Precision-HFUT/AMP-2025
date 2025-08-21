#include "utils.hpp"

bool ChangeInst::visitStoreInst(StoreInst &inst) {
    StoreInst* storeInst = &inst;
    Value* valueOp = storeInst->getValueOperand();
    Value* destination = storeInst->getOperand(1);
    StoreInst* newStore = nullptr;
    llvm::Align Alignment(alignment);

    // lambda: 创建 StoreInst（必须显式传入 align）
    auto createStore = [&](Value* val, Value* dest, llvm::Align align) -> StoreInst* {
        return new StoreInst(val, dest, false, align, storeInst);
    };

    // lambda: 处理 CallInst 内建函数逻辑
    auto handleCall = [&](CallInst* call) {
        StringRef fname = call->getCalledFunction()->getName();
        if (!isa<PointerType>(destination->getType())) return;

        if (fname == "llvm.fmuladd.f32" || fname == "llvm.sqrt.f32") {
            if (valueOp->getType()->isDoubleTy()) {
                BitCastInst* bc = new BitCastInst(destination, Type::getFloatPtrTy(context), "", storeInst);
                newStore = createStore(newTarget, bc, Alignment);
            } else {
                newStore = createStore(newTarget, destination, Alignment);
            }
        } else if (fname == "llvm.fmuladd.f16") {
            if (valueOp->getType()->isFloatTy()) {
                BitCastInst* bc = new BitCastInst(destination, Type::getHalfPtrTy(context), "", storeInst);
                newStore = createStore(newTarget, bc, Alignment);
            } else {
                newStore = createStore(newTarget, destination, Alignment);
            }
        } else if (fname == "llvm.sqrt.f16") {
            if (valueOp->getType()->isFloatTy()) {
                newStore = createStore(newTarget, oldTarget, Alignment);
            } else {
                newStore = createStore(newTarget, destination, Alignment);
            }
        }
    };

    // lambda: 处理普通类型逻辑
    auto handleNormal = [&]() -> bool {
        if (newType->getTypeID() > oldType->getTypeID()) {
            FPExtInst* ext = new FPExtInst(valueOp, newType, "", storeInst);
            newStore = createStore(ext, newTarget, Alignment);
        } else if (valueOp->getType()->isFloatTy() && !newType->isHalfTy()) {
            newStore = createStore(valueOp, newTarget, Alignment);
        } else if (valueOp->getType()->isHalfTy()) {
            newStore = createStore(valueOp, newTarget, Alignment);
        } else if (newTarget->getType()->isFloatTy() && valueOp->getType()->isDoubleTy()) {
            FPExtInst* ext = new FPExtInst(newTarget, oldType, "", storeInst);
            newStore = createStore(ext, destination, Alignment);
        } else if (dyn_cast<CallInst>(valueOp) && (dyn_cast<GetElementPtrInst>(destination) || dyn_cast<BitCastInst>(destination))) {
            FPTruncInst* fptrunc = new FPTruncInst(valueOp, newType, "", storeInst);
            newStore = createStore(fptrunc, newTarget, Alignment);
            if (CallInst* callinst = dyn_cast<CallInst>(valueOp)) {
                if (callinst->getCalledFunction()->getName() == "dlange") return false;
            }
            return true;
        } else if (valueOp == oldTarget && newType->isHalfTy()) {
            FPExtInst* ext = !newTarget->getType()->isDoubleTy() ? new FPExtInst(newTarget, oldType, "", storeInst) : nullptr;
            if (valueOp->getType()->isDoubleTy()) {
                if (newTarget->getType()->isDoubleTy()) {
                    newStore = createStore(newTarget, destination, Align(8));
                } else {
                    FPExtInst* ext2 = new FPExtInst(ext, valueOp->getType(), "", storeInst);
                    newStore = createStore(ext2, destination, Alignment);
                }
            } else {
                newStore = createStore(ext, destination, Alignment);
            }
        } else {
            FPTruncInst* fptrunc = new FPTruncInst(valueOp, newType, "", storeInst);
            newStore = createStore(fptrunc, newTarget, Alignment);

            if (newType->isHalfTy() || newType->isFloatTy()) {
                bool is_store = false, is_load = false;
                for (auto it = destination->use_begin(); it != destination->use_end(); ++it) {
                    if (isa<StoreInst>(it->getUser())) is_store = true;
                    else if (isa<LoadInst>(it->getUser())) is_load = true;
                }
                if (is_store && is_load) return true;
            }
        }
        return false;
    };

    // 主逻辑
    if (ConstantFP* constant = dyn_cast<ConstantFP>(valueOp)) {
        CastInst* fpOperand = CastInst::CreateFPCast(constant, newType, "", storeInst);
        newStore = createStore(fpOperand, newTarget, Alignment);
    } else if (CallInst* call = dyn_cast<CallInst>(newTarget)) {
        handleCall(call);
    } else {
        return handleNormal();
    }

    return false;
}
