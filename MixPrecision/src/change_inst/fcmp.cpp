#include "utils.hpp"

bool ChangeInst::visitFCmpInst(FCmpInst &inst)
{

  auto fCmp = &inst;
  std::vector<llvm::Value *> paramList;
  auto tyID = newType->getTypeID(); // 使用枚举值代替直观的 isXxxTy()

  for (unsigned idx = 0, totalOps = fCmp->getNumOperands(); idx < totalOps; ++idx)
  {
    auto *opVal = fCmp->getOperand(idx);

    bool isMatchedTarget = (opVal == oldTarget);
    bool requiresTrunc = false;

    switch (tyID)
    {
    case llvm::Type::FloatTyID:
      requiresTrunc = opVal->getType()->isDoubleTy();
      break;
    case llvm::Type::HalfTyID:
      requiresTrunc = opVal->getType()->isFloatTy();
      break;
    default:
      break;
    }

    if (isMatchedTarget)
    {
      paramList.emplace_back(newTarget);
    }
    else if (requiresTrunc)
    {
      paramList.emplace_back(new llvm::FPTruncInst(opVal, newType, "", fCmp));
    }
  }

  fCmp->replaceAllUsesWith(new FCmpInst(fCmp, fCmp->getPredicate(), paramList[0], paramList[1], ""));

  return false;
}