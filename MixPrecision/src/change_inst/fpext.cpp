#include "utils.hpp"

bool ChangeInst::visitFPExtInst(FPExtInst &inst)
{

  FPExtInst *fpext = &inst;
  auto *tempExt = new llvm::FPExtInst(newTarget, oldType, "", fpext);

  auto typeRankNew = tempExt->getType()->getTypeID();
  auto typeRankOld = fpext->getType()->getTypeID();

  llvm::Value *finalVal = tempExt;

  if (typeRankNew < typeRankOld)
  {
    finalVal = new llvm::FPExtInst(tempExt, fpext->getType(), "", fpext);
  }

  fpext->replaceAllUsesWith(finalVal);

  using UserVariant = std::variant<llvm::FPTruncInst *, llvm::StoreInst *, llvm::Instruction *>;

  for (auto it = fpext->use_begin(), itEnd = fpext->use_end(); it != itEnd; ++it)
  {
    llvm::Instruction *rawUser = llvm::dyn_cast<llvm::Instruction>(it->getUser());
    if (!rawUser)
      continue;

    UserVariant userObj;
    if (auto *t = llvm::dyn_cast<llvm::FPTruncInst>(rawUser))
    {
      userObj = t;
    }
    else if (auto *s = llvm::dyn_cast<llvm::StoreInst>(rawUser))
    {
      userObj = s;
    }
    else
    {
      userObj = rawUser;
    }

    std::visit([&](auto *instPtr)
               {
        using T = std::decay_t<decltype(instPtr)>;
        if constexpr (std::is_same_v<T, llvm::FPTruncInst*>) {
            bool handled = ChangeVisitor::changePrecision(
                context, it, finalVal, fpext, newType, oldType, alignment
            );
            if (!handled) {
                deadInsts.emplace_back(instPtr);
            }
        } 
        else if constexpr (std::is_same_v<T, llvm::StoreInst*>) {
            auto* valTy = instPtr->getValueOperand()->getType();
            if (newType->isHalfTy() && valTy->isDoubleTy()) {
                auto* extTmp = new llvm::FPExtInst(finalVal, valTy, "", fpext);
                fpext->replaceAllUsesWith(extTmp);
            }
        } 
        else {
            
        } }, userObj);
  }

  removeDeadInsts();
  return false;
}