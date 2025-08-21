#include "utils.hpp"

bool ChangeInstPointer::visitFPExtInst(FPExtInst &inst)
{
  FPExtInst *fpext = &inst;
  auto *tempExtInst = new llvm::FPExtInst(newTarget, oldTypeP.ty, "", fpext);

  llvm::Type *tempType = tempExtInst->getType();
  llvm::Type *origType = fpext->getType();

  llvm::Value *replaceVal = (tempType->getTypeID() < origType->getTypeID())
                                ? new llvm::FPExtInst(tempExtInst, origType, "", fpext)
                                : tempExtInst;

  fpext->replaceAllUsesWith(replaceVal);

#include <optional>
#include <functional>
#include <type_traits>

  auto getUserInst = [](auto &useIt) -> std::optional<llvm::Instruction *>
  {
    if (auto *inst = llvm::dyn_cast<llvm::Instruction>(useIt->getUser()))
      return inst;
    return std::nullopt;
  };

  auto handleFPtrunc = [&](llvm::FPTruncInst *fi, auto &it) -> bool
  {
    return ChangeVisitor::changePrecision(
        context, it, fpext, fpext, newTypeP, oldTypeP, alignment);
  };

  auto handleStore = [&](llvm::StoreInst *si)
  {
    auto *vTy = si->getValueOperand()->getType();
    if (newTypeP.ty->isHalfTy() && vTy->isDoubleTy())
    {
      auto *ext = new llvm::FPExtInst(fpext, vTy, "", fpext);
      fpext->replaceAllUsesWith(ext);
    }
  };

  for (auto it = fpext->use_begin(), end = fpext->use_end(); it != end; ++it)
  {
    if (auto maybeInst = getUserInst(it))
    {
      llvm::Instruction *userInst = *maybeInst;

      std::function<void()> process = [&]
      {
        if (auto *fi = llvm::dyn_cast<llvm::FPTruncInst>(userInst))
        {

          if (!handleFPtrunc(fi, it))
            deadInsts.emplace_back(fi);
        }
        else if (auto *si = llvm::dyn_cast<llvm::StoreInst>(userInst))
        {
          handleStore(si);
        }
      };

      process();
    }
  }

  removeDeadInsts();
  return false;
}