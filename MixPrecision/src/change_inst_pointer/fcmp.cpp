#include "utils.hpp"

bool ChangeInstPointer::visitFCmpInst(FCmpInst &inst)
{

  auto fCmp = &inst;
  std::vector<llvm::Value *> collectedVals;
  auto targetTypeID = newTypeP.ty->getTypeID();

  for (unsigned idx = 0, N = fCmp->getNumOperands(); idx < N; ++idx)
  {
    auto *currentOp = fCmp->getOperand(idx);
    bool isOldTarget = (currentOp == oldTarget);

    bool needsTrunc = false;
    switch (targetTypeID)
    {
    case llvm::Type::FloatTyID:
      needsTrunc = currentOp->getType()->getTypeID() == llvm::Type::DoubleTyID;
      break;
    case llvm::Type::HalfTyID:
      needsTrunc = currentOp->getType()->getTypeID() == llvm::Type::FloatTyID;
      break;
    default:
      break;
    }

    if (isOldTarget)
    {
      collectedVals.emplace_back(newTarget);
    }
    else if (needsTrunc)
    {
      collectedVals.emplace_back(
          new llvm::FPTruncInst(currentOp, newTypeP.ty, "", fCmp));
    }
  }

  fCmp->replaceAllUsesWith(new FCmpInst(fCmp, fCmp->getPredicate(), collectedVals[0], collectedVals[1], ""));

  return false;
}