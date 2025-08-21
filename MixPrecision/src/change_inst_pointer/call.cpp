#include "utils.hpp"

bool ChangeInstPointer::visitCallInst(CallInst &inst)
{

  CallInst *oldCall = &inst;

  if (oldCall->getCalledFunction()->isIntrinsic() && oldCall->getIntrinsicID() == Intrinsic::fmuladd)
  {
    if (oldCall->getType()->isDoubleTy())
    {

IRBuilder<> builder(oldCall); // 在 oldCall 位置插入新指令

auto operands = oldCall->operands(); // llvm::Use 列表
std::vector<llvm::Value*> indices;
indices.reserve(oldCall->getNumOperands() - 1); // 提前分配空间

std::transform(operands.begin(), operands.end() - 1, std::back_inserter(indices),
    [&](llvm::Use &U) -> llvm::Value* {
        llvm::Value *op = U.get();

        // 替换目标操作数
        if (op == oldTarget) {
            oldCall->setArgOperand(&U - &*operands.begin(), newTarget);
            op = newTarget;
        }

        // double 类型转 float
        return op->getType()->isDoubleTy() 
            ? builder.CreateFPTrunc(op, Type::getFloatTy(context)) 
            : op;
    }
);

std::pair<Function *, Module *> funcModPair{oldCall->getFunction(), nullptr};
funcModPair.second = funcModPair.first->getParent();

auto &&fmaDecl = Intrinsic::getDeclaration(funcModPair.second, Intrinsic::fmuladd, {Type::getFloatTy(context)});
auto *newCall = CallInst::Create(fmaDecl, indices, "", oldCall);

// 统一处理 lambda
auto handleUser = [&](Instruction *userInst, Value::use_iterator &itUse) {
    if (!userInst) return;

    using T = std::decay_t<decltype(*userInst)>;

    if constexpr (std::is_same_v<T, FPTruncInst>) {
        deadInsts.emplace_back(userInst);
        userInst->replaceAllUsesWith(newCall);
    }
    else if constexpr (std::is_same_v<T, BinaryOperator>) {
        auto *binOp = cast<BinaryOperator>(userInst);
        if (binOp->getOpcode() != Instruction::FDiv) {
            // 普通 BinaryOperator 当作 default 处理
            if (!ChangeVisitor::changePrecision(context, itUse, newCall, oldCall,
                                                getFloatPtrDep(context), getDoublePtrDep(context), alignment))
            {
                deadInsts.emplace_back(binOp);
            }
            return;
        }

        deadInsts.emplace_back(binOp);

        std::vector<Value *> operands;
        operands.reserve(binOp->getNumOperands());

        std::for_each_n(binOp->op_begin(), binOp->getNumOperands(), [&](const Use &U) {
            auto *operandVal = U.get();
            if (operandVal == oldCall)
                oldCall->replaceAllUsesWith(newCall);

            operands.push_back(operandVal->getType()->isDoubleTy()
                                   ? static_cast<Value *>(new FPTruncInst(operandVal, Type::getFloatTy(context), "", binOp))
                                   : operandVal);
        });

        auto *newFdiv = BinaryOperator::Create(BinaryOperator::FDiv, operands.at(0), operands.at(1), "", binOp);

        std::vector<Instruction *> pendingErase;
        for (auto useIt = binOp->use_begin(); useIt != binOp->use_end(); ++useIt) {
            if (!ChangeVisitor::changePrecision(context, useIt, newFdiv, binOp,
                                                getFloatPtrDep(context), getDoublePtrDep(context), alignment))
            {
                if (auto *toErase = dyn_cast<Instruction>(useIt->getUser()))
                    pendingErase.push_back(toErase);
            }
        }
        std::for_each(pendingErase.begin(), pendingErase.end(),
                      [](Instruction *I) { I->eraseFromParent(); });
    }
    else {
        // 默认处理
        if (!ChangeVisitor::changePrecision(context, itUse, newCall, oldCall,
                                            getFloatPtrDep(context), getDoublePtrDep(context), alignment))
        {
            if (auto *toKill = dyn_cast<Instruction>(userInst))
                deadInsts.emplace_back(toKill);
        }
    }
};

// 主循环
for (auto it = oldCall->use_begin(); it != oldCall->use_end(); ++it) {
    handleUser(dyn_cast<Instruction>(it->getUser()), it);
}

removeDeadInsts();

    }
    else if ((newTarget == oldTarget) && oldCall->getType()->isFloatTy())
    {
auto processUses = [&]() {
    for (auto it = oldCall->use_begin(); it != oldCall->use_end(); ++it) {
        auto *userInst = dyn_cast<Instruction>(it->getUser());
        if (!userInst) continue;

        // FPTrunc 类型萃取
        if (auto *fpTrunc = dyn_cast<FPTruncInst>(userInst)) {
            deadInsts.push_back(fpTrunc);
            fpTrunc->replaceAllUsesWith(oldCall); // 或 newCall
            continue;
        }

        // FDiv 类型萃取
        if (auto *binOp = dyn_cast<BinaryOperator>(userInst); binOp && binOp->getOpcode() == Instruction::FDiv) {
            deadInsts.push_back(binOp);

            std::vector<Value*> operands;
            operands.reserve(binOp->getNumOperands());
            for (auto &op : binOp->operands()) {
                operands.push_back(op->getType()->isDoubleTy()
                                   ? static_cast<Value*>(new FPTruncInst(op.get(), Type::getFloatTy(context), "", binOp))
                                   : op.get());
            }

            auto *newFdiv = BinaryOperator::Create(BinaryOperator::FDiv, operands[0], operands[1], "", binOp);
            binOp->replaceAllUsesWith(newFdiv);
            continue;
        }

        // 默认处理
        bool erased = ChangeVisitor::changePrecision(context, it, oldCall, oldCall,
                                                     getFloatPtrDep(context), getDoublePtrDep(context), alignment);
        if (!erased)
            deadInsts.push_back(userInst);
    }
};

processUses();
removeDeadInsts();


    }
    else if (newTypeP.ty->isHalfTy())
    {
std::vector<llvm::Value*> indices;
indices.reserve(oldCall->getNumOperands() - 1);  // 提前分配空间

std::transform(
    oldCall->op_begin(),
    oldCall->op_begin() + (oldCall->getNumOperands() - 1),
    std::back_inserter(indices),
    [&](llvm::Use &U) -> llvm::Value* {
        llvm::Value *val = U.get();

        // 替换目标操作数
        if (val == oldTarget) {
            oldCall->setArgOperand(&U - oldCall->op_begin(), newTarget);
            val = newTarget;
        }

        // 对 float 类型降精
        if (val->getType()->isFloatTy()) {
            // 用 IRBuilder 插入 FPTruncInst 更安全
            IRBuilder<> builder(oldCall);
            return builder.CreateFPTrunc(val, newTypeP.ty);
        }

        return val;
    }
);


      Function *F = oldCall->getFunction();
      auto M = F->getParent();
      Function *FmulAddF16 = Intrinsic::getDeclaration(M, Intrinsic::fmuladd, newTypeP.ty);
      CallInst *NewCall = CallInst::Create(FmulAddF16, indices, "", oldCall);

      Value::use_iterator it1 = oldCall->use_begin();
      for (; it1 != oldCall->use_end(); it1++)
      {
        if (FPTruncInst *fp = dyn_cast<FPTruncInst>(it1->getUser()))
        {
          deadInsts.push_back(fp);
          fp->replaceAllUsesWith(NewCall);
        }
        else if (BinaryOperator *byop = dyn_cast<BinaryOperator>(it1->getUser()))
        {
          if (byop->getOpcode() == Instruction::FDiv)
          {

            deadInsts.push_back(byop);
            std::vector<llvm::Value *> arg;
            for (unsigned i = 0; i < byop->getNumOperands(); i++)
            {
              if (byop->getOperand(i) == oldCall)
              {
                oldCall->replaceAllUsesWith(NewCall);
              }

              if (byop->getOperand(i)->getType()->isFloatTy())
              {
                FPTruncInst *truncInst1 = new FPTruncInst(byop->getOperand(i), newTypeP.ty, "", byop);

                arg.push_back(truncInst1);
              }
              else
              {
                arg.push_back(byop->getOperand(i));
              }
            }

            BinaryOperator *newFdiv = BinaryOperator::Create(BinaryOperator::FDiv, arg[0], arg[1], "", byop);

            vector<Instruction *> erased2;
            Value::use_iterator itfdiv = byop->use_begin();
            for (; itfdiv != byop->use_end(); itfdiv++)
            {
              bool is_erase = ChangeVisitor::changePrecision(
                  context, itfdiv, newFdiv, byop, newTypeP,
                  oldTypeP, alignment);
              if (!is_erase)
                erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
            }
            for (unsigned int i = 0; i < erased2.size(); i++)
            {
              erased2[i]->eraseFromParent();
            }
          }
        }
        else
        {
          bool is_erased = ChangeVisitor::changePrecision(
              context, it1, NewCall, oldCall, newTypeP,
              oldTypeP, alignment);
          if (!is_erased)
            deadInsts.push_back(dyn_cast<Instruction>(it1->getUser()));
        }
      }
      removeDeadInsts();
    }
    return false;
  }
  else if (oldCall->getCalledFunction()->getName() == "sqrt" || oldCall->getCalledFunction()->getName() == "llvm.sqrt.f32")
  {
    if (newTypeP != oldTypeP)
    {
Function *F = oldCall->getFunction();
auto M = F->getParent();

Function *sqrtFunction = Intrinsic::getDeclaration(M, Intrinsic::sqrt, newTypeP.ty);
CallInst *newSqrt = CallInst::Create(sqrtFunction, newTarget, "", oldCall);

// 处理类型拓宽或缩减
if (oldTypeP.ty > newTypeP.ty) {
    FPExtInst *ext = new FPExtInst(newSqrt, oldTypeP.ty, "", oldCall);
    oldCall->replaceAllUsesWith(ext);
} else if (oldTypeP.ty < newTypeP.ty) {
    FPTruncInst *fp = new FPTruncInst(newSqrt, oldTypeP.ty, "", oldCall);
    oldCall->replaceAllUsesWith(fp);
}

// Lambda 统一处理每个 use
auto handleUse = [&](Value::use_iterator &it) {
    Instruction *userInst = dyn_cast<Instruction>(it->getUser());
    if (!userInst) return;

    // 类型萃取
    std::variant<FPTruncInst*, FPExtInst*, Instruction*> varUser;
    if (auto *fptrunc = dyn_cast<FPTruncInst>(userInst)) varUser = fptrunc;
    else if (auto *fpext = dyn_cast<FPExtInst>(userInst)) varUser = fpext;
    else varUser = userInst;

    // 使用 std::visit 对不同类型统一操作
    std::visit([&](auto *inst) {
        if (inst) {
            inst->replaceAllUsesWith(newSqrt);
            deadInsts.push_back(inst);
        }
    }, varUser);

    // 调用 changePrecision 处理其他情况
    PtrDep newDep(newTypeP.ty, 0);
    PtrDep oldDep(oldTypeP.ty, 0);
    bool erased = ChangeVisitor::changePrecision(context, it, newSqrt, oldTarget, newDep, oldDep, alignment);
    if (!erased && userInst) deadInsts.push_back(userInst);
};

// 遍历所有 use
for (auto it = newSqrt->use_begin(); it != newSqrt->use_end(); ++it) {
    handleUse(it);
}

removeDeadInsts();
return false;


    }
  }
  else if (oldCall->getCalledFunction()->getName() == "llvm.fabs.f64" || oldCall->getCalledFunction()->getName() == "llvm.fabs.f32")
  {
    if (newTypeP != oldTypeP)
    {
Function *F = oldCall->getFunction();
auto M = F->getParent();

Function *fabsFunction = Intrinsic::getDeclaration(M, Intrinsic::fabs, newTypeP.ty);
CallInst *newfabs = CallInst::Create(fabsFunction, newTarget, "", oldCall);

// 替换原调用
oldCall->replaceAllUsesWith(newfabs);

// lambda 统一处理每个 use
auto handleUse = [&](Value::use_iterator &it) {
    Instruction *userInst = dyn_cast<Instruction>(it->getUser());
    if (!userInst) return;

    // 类型萃取
    std::variant<FPTruncInst*, FPExtInst*, Instruction*> varUser;
    if (auto *fptrunc = dyn_cast<FPTruncInst>(userInst)) varUser = fptrunc;
    else if (auto *fpext = dyn_cast<FPExtInst>(userInst)) varUser = fpext;
    else varUser = userInst;

    // 统一操作
    std::visit([&](auto *inst) {
        if (inst) {
            inst->replaceAllUsesWith(newfabs);
            deadInsts.push_back(inst);
        }
    }, varUser);

    // 调用 changePrecision 处理其他情况
    PtrDep newDep(newTypeP.ty, 0);
    PtrDep oldDep(oldTypeP.ty, 0);
    bool erased = ChangeVisitor::changePrecision(context, it, newfabs, oldCall, newDep, oldDep, alignment);
    if (!erased && userInst) deadInsts.push_back(userInst);
};

// 遍历所有 use
for (auto it = newfabs->use_begin(); it != newfabs->use_end(); ++it) {
    handleUse(it);
}

removeDeadInsts();
return false;

    }
  }

  return true;
}