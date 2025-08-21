#include "utils.hpp"

bool ChangeInst::visitCallInst(CallInst &inst){
  CallInst* oldCall = &inst;
auto handleMemcpy = [&]() -> void {
    if (Constant *constant = dyn_cast<Constant>(oldCall->getOperand(1)->stripPointerCasts())) {
        if (GlobalValue *global = dyn_cast<GlobalValue>(constant)) {
            if (PointerType *pointer = dyn_cast<PointerType>(global->getType())) {
                if (ArrayType *array = dyn_cast<ArrayType>(global->getValueType())) {
                    if (Type *oldElem = array->getElementType()) {
                        if (Type *newElem = dyn_cast<ArrayType>(newType)->getElementType()) {
                            if (oldElem->getTypeID() != newElem->getTypeID()) {

                                ConstantArray *constantArray =
                                    dyn_cast<ConstantArray>(constant->getOperand(0));
                                unsigned numElements = constantArray->getType()->getNumElements();
                                ArrayType *newArrayType = ArrayType::get(newElem, numElements);

                                std::vector<llvm::Constant *> arrayElements;
                                arrayElements.reserve(numElements);

                                for (unsigned i = 0; i < numElements; i++) {
                                    if (ConstantFP *oldConstant =
                                            dyn_cast<ConstantFP>(constantArray->getOperand(i))) {

                                        ConstantFP *newConstant = nullptr;
                                        llvm::Constant *newConstant_ = nullptr;

                                        if (oldElem->getTypeID() == Type::DoubleTyID &&
                                            newElem->getTypeID() == Type::FloatTyID) {
                                            float fconstant =
                                                (float)(oldConstant->getValueAPF().convertToDouble());
                                            newConstant = ConstantFP::get(context, APFloat(fconstant));
                                            arrayElements.push_back(newConstant);
                                        } else if (oldElem->getTypeID() == Type::FloatTyID &&
                                                   newElem->getTypeID() == Type::DoubleTyID) {
                                            double fconstant =
                                                (double)(oldConstant->getValueAPF().convertToFloat());
                                            newConstant = ConstantFP::get(context, APFloat(fconstant));
                                            arrayElements.push_back(newConstant);
                                        } else if (oldElem->getTypeID() == Type::FloatTyID &&
                                                   newElem->getTypeID() == Type::HalfTyID) {
                                            APFloat f = oldConstant->getValueAPF();
                                            bool losesInfo = false;
                                            f.convert(APFloat::IEEEhalf(),
                                                      APFloat::rmNearestTiesToEven,
                                                      &losesInfo);
                                            newConstant_ = ConstantFP::get(Type::getHalfTy(context), f);
                                            arrayElements.push_back(newConstant_);
                                        } else {
                                            errs() << "WARNING: Unhandled type when creating constant array\n";
                                            exit(1);
                                        }
                                    }
                                }

                                Constant *newConstantArray =
                                    ConstantArray::get(newArrayType, arrayElements);

                                Module *module = oldCall->getCalledFunction()->getParent();
                                GlobalVariable *newGlobal = new GlobalVariable(
                                    *module, newConstantArray->getType(), true,
                                    GlobalValue::PrivateLinkage, newConstantArray, "");

                                newGlobal->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
                                newGlobal->setAlignment(Align(alignment));
                                newGlobal->takeName(constant);

                                constant->replaceAllUsesWith(newGlobal);

                                int size = 0;
                                switch (newElem->getTypeID()) {
                                    case Type::HalfTyID:   size = 2; break;
                                    case Type::FloatTyID:  size = 4; break;
                                    case Type::DoubleTyID: size = 8; break;
                                    default:               size = 16; break;
                                }

                                oldCall->setArgOperand(2, getInt64(context, numElements * size));
                                oldCall->setArgOperand(3, getInt32(context, alignment));

                                global->eraseFromParent();
                            }
                        }
                    }
                }
            }
        }
    }
};

 
auto handleFMA = [&]() -> void {
    if (oldCall->getType()->isDoubleTy()) {

        std::vector<llvm::Value*> indices;
        for (unsigned i = 0; i < oldCall->getNumOperands() - 1; i++) {
            if (oldCall->getOperand(i) == oldTarget) {
                oldCall->setArgOperand(i, newTarget);
            }
            if (oldCall->getOperand(i)->getType()->isDoubleTy()) {
                FPTruncInst *truncInst1 =
                    new FPTruncInst(oldCall->getOperand(i), Type::getFloatTy(context), "", oldCall);
                indices.push_back(truncInst1);
            } else {
                indices.push_back(oldCall->getOperand(i));
            }
        }
        ArrayRef<llvm::Value*> arrayRef(indices);
        Function *F = oldCall->getFunction();
        auto M = F->getParent();
        Function *FmulAddF32 =
            Intrinsic::getDeclaration(M, Intrinsic::fmuladd, {Type::getFloatTy(context)});
        CallInst *NewCall = CallInst::Create(FmulAddF32, arrayRef, "", oldCall);

        vector<Instruction*> erased1;
        for (auto it1 = oldCall->use_begin(); it1 != oldCall->use_end(); it1++) {
            if (FPTruncInst *fp = dyn_cast<FPTruncInst>(it1->getUser())) {
                erased1.push_back(fp);
                fp->replaceAllUsesWith(NewCall);

            } else if (BinaryOperator *byop = dyn_cast<BinaryOperator>(it1->getUser())) {
                if (byop->getOpcode() == Instruction::FDiv) {

                    erased1.push_back(byop);
                    std::vector<llvm::Value*> arg;
                    for (unsigned i = 0; i < byop->getNumOperands(); i++) {
                        if (byop->getOperand(i) == oldCall) {
                            oldCall->replaceAllUsesWith(NewCall);
                        }
                        if (byop->getOperand(i)->getType()->isDoubleTy()) {
                            FPTruncInst *truncInst1 =
                                new FPTruncInst(byop->getOperand(i), Type::getFloatTy(context), "", byop);
                            arg.push_back(truncInst1);
                        } else {
                            arg.push_back(byop->getOperand(i));
                        }
                    }

                    BinaryOperator *newFdiv =
                        BinaryOperator::Create(BinaryOperator::FDiv, arg[0], arg[1], "", byop);

                    vector<Instruction*> erased2;
                    for (auto itfdiv = byop->use_begin(); itfdiv != byop->use_end(); itfdiv++) {
                        bool is_erase = ChangeVisitor::changePrecision(
                            context, itfdiv, newFdiv, byop,
                            Type::getFloatTy(context), Type::getDoubleTy(context), alignment);
                        if (!is_erase)
                            erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
                    }
                    for (auto *inst : erased2) {
                        inst->eraseFromParent();
                    }
                }
            } else {
                bool is_erased = ChangeVisitor::changePrecision(
                    context, it1, NewCall, oldCall,
                    Type::getFloatTy(context), Type::getDoubleTy(context), alignment);
                if (!is_erased)
                    erased1.push_back(dyn_cast<Instruction>(it1->getUser()));
            }
        }
        for (auto *inst : erased1) {
            inst->eraseFromParent();
        }

    } else if ((newTarget == oldTarget) && oldCall->getType()->isFloatTy()) {

        vector<Instruction*> erased2;
        for (auto itfdiv = oldCall->use_begin(); itfdiv != oldCall->use_end(); itfdiv++) {
            bool is_erase = ChangeVisitor::changePrecision(
                context, itfdiv, oldCall, oldCall,
                Type::getFloatTy(context), Type::getDoubleTy(context), alignment);
            if (!is_erase)
                erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
        }
        for (auto *inst : erased2) {
            inst->eraseFromParent();
        }

    } else if (newType->isHalfTy()) {

        std::vector<llvm::Value*> indices;
        for (unsigned i = 0; i < oldCall->getNumOperands() - 1; i++) {
            if (oldCall->getOperand(i) == oldTarget) {
                oldCall->setArgOperand(i, newTarget);
            }
            if (oldCall->getOperand(i)->getType()->isFloatTy()) {
                FPTruncInst *truncInst1 =
                    new FPTruncInst(oldCall->getOperand(i), newType, "", oldCall);
                indices.push_back(truncInst1);
            } else {
                indices.push_back(oldCall->getOperand(i));
            }
        }
        ArrayRef<llvm::Value*> arrayRef(indices);
        Function *F = oldCall->getFunction();
        auto M = F->getParent();
        Function *FmulAddF16 = Intrinsic::getDeclaration(M, Intrinsic::fmuladd, newType);
        CallInst *NewCall = CallInst::Create(FmulAddF16, arrayRef, "", oldCall);

        vector<Instruction*> erased1;
        for (auto it1 = oldCall->use_begin(); it1 != oldCall->use_end(); it1++) {
            if (FPTruncInst *fp = dyn_cast<FPTruncInst>(it1->getUser())) {
                erased1.push_back(fp);
                fp->replaceAllUsesWith(NewCall);

            } else if (BinaryOperator *byop = dyn_cast<BinaryOperator>(it1->getUser())) {
                if (byop->getOpcode() == Instruction::FDiv) {

                    erased1.push_back(byop);
                    std::vector<llvm::Value*> arg;
                    for (unsigned i = 0; i < byop->getNumOperands(); i++) {
                        if (byop->getOperand(i) == oldCall) {
                            oldCall->replaceAllUsesWith(NewCall);
                        }
                        if (byop->getOperand(i)->getType()->isFloatTy()) {
                            FPTruncInst *truncInst1 =
                                new FPTruncInst(byop->getOperand(i), newType, "", byop);
                            arg.push_back(truncInst1);
                        } else {
                            arg.push_back(byop->getOperand(i));
                        }
                    }

                    BinaryOperator *newFdiv =
                        BinaryOperator::Create(BinaryOperator::FDiv, arg[0], arg[1], "", byop);

                    vector<Instruction*> erased2;
                    for (auto itfdiv = byop->use_begin(); itfdiv != byop->use_end(); itfdiv++) {
                        bool is_erase = ChangeVisitor::changePrecision(
                            context, itfdiv, newFdiv, byop, newType, oldType, alignment);
                        if (!is_erase)
                            erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
                    }
                    for (auto *inst : erased2) {
                        inst->eraseFromParent();
                    }
                }
            } else {
                bool is_erased = ChangeVisitor::changePrecision(
                    context, it1, NewCall, oldCall, newType, oldType, alignment);
                if (!is_erased)
                    erased1.push_back(dyn_cast<Instruction>(it1->getUser()));
            }
        }
        for (auto *inst : erased1) {
            inst->eraseFromParent();
        }
    }
};

  
  if (oldCall->getCalledFunction()->getName() == "llvm.memcpy.p0i8.p0i8.i64") {
  
    handleMemcpy();
  }else if(oldCall->getCalledFunction()->isIntrinsic() && oldCall->getIntrinsicID() == Intrinsic::fmuladd){
      handleFMA();
  } 
  else if(oldCall->getCalledFunction()->getName()=="sqrt"||oldCall->getCalledFunction()->getName()=="llvm.sqrt.f32"){
if (newType != oldType) {
    auto *parentFunc = oldCall->getFunction();
    auto *mod = parentFunc->getParent();

    auto *sqrtInst = CallInst::Create(
        Intrinsic::getDeclaration(mod, Intrinsic::sqrt, newType),
        newTarget, "", oldCall
    );

    Instruction *replacement = nullptr;
    if (oldType->getPrimitiveSizeInBits() > newType->getPrimitiveSizeInBits()) {
        replacement = new FPExtInst(sqrtInst, oldType, "", oldCall);
    } else if (oldType->getPrimitiveSizeInBits() < newType->getPrimitiveSizeInBits()) {
        replacement = new FPTruncInst(sqrtInst, oldType, "", oldCall);
    }

    if (replacement)
        oldCall->replaceAllUsesWith(replacement);

    // 遍历 sqrtInst 使用者，按引用接收 llvm::Use
    for (auto it = sqrtInst->use_begin(); it != sqrtInst->use_end(); ++it) {
        Value *userVal = it->getUser();

        if (auto *fpTrunc = dyn_cast<FPTruncInst>(userVal)) {
            fpTrunc->replaceAllUsesWith(sqrtInst);
            deadInsts.push_back(fpTrunc);
        } else {
            bool erased = ChangeVisitor::changePrecision(
                context, it, sqrtInst, oldTarget, newType, oldType, alignment
            );
            if (!erased)
                if (auto *inst = dyn_cast<Instruction>(userVal))
                    deadInsts.push_back(inst);
        }
    }

    removeDeadInsts();
    return false;
}

  }
  else if(oldCall->getCalledFunction()->getName()=="llvm.fabs.f64"||oldCall->getCalledFunction()->getName()=="llvm.fabs.f32"){
if (newType != oldType) {
    auto *parentFunc = oldCall->getFunction();
    auto *mod = parentFunc->getParent();

    // 创建 fabs intrinsic
    auto *fabsInst = CallInst::Create(
        Intrinsic::getDeclaration(mod, Intrinsic::fabs, newType),
        newTarget, "", oldCall
    );

    // 替换旧指令
    oldCall->replaceAllUsesWith(fabsInst);

    // 使用模板 lambda 处理每个 use
    auto processUse = [&](auto it) {
        bool erased = ChangeVisitor::changePrecision(
            context, it, fabsInst, oldCall, newType, oldType, alignment
        );
        if (!erased) {
            if (auto *inst = dyn_cast<Instruction>(it->getUser()))
                deadInsts.push_back(inst);
        }
    };

    // 遍历使用者
    for (auto it = fabsInst->use_begin(); it != fabsInst->use_end(); ++it)
        processUse(it);

    removeDeadInsts();
    return false;
}

  }

 
  return true;
}