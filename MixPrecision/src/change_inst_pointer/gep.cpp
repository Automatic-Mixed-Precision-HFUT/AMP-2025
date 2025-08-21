#include "utils.hpp"

bool ChangeInstPointer::visitGetElementPtrInst(GetElementPtrInst &inst)
{
  GetElementPtrInst *oldGetElementPtr = &inst;

  auto newGetElementPtr = GetElementPtrInst::CreateInBounds(
      newTypeP.ty,
      newTarget,
      ([&]
       {
        std::vector<Value*> idx;
        std::for_each(
            std::next(oldGetElementPtr->op_begin()),
            oldGetElementPtr->op_end(),
            [&](auto &u) { idx.push_back(u.get()); }
        );
        return idx; })(),
      "",
      oldGetElementPtr);

  std::function<void(Value::use_iterator, Value::use_iterator)> recur = [&](auto curIt, auto endIt)
  {
    if (curIt == endIt)
      return;

    newTarget = newGetElementPtr;

    std::function<bool()> func = [&]() -> bool
    {
      return (newTypeP.dep != 0)
                 ? ([&]() -> bool
                    {
                PtrDep temp_newTypeP(newTypeP.ty, 0);
                PtrDep temp_oldTypeP(oldTypeP.ty, 0);
                return ChangeVisitor::changePrecision(context, curIt, newTarget, oldTarget, temp_newTypeP, temp_oldTypeP, alignment); })()
                 : ([&]() -> bool
                    {
                if (auto *callInst = dyn_cast<CallInst>(curIt->getUser())) {
                    auto *bitCast = new BitCastInst(newTarget, oldGetElementPtr->getType(), "", callInst);

                    std::vector<unsigned> idx(callInst->getNumOperands());
                    std::iota(idx.begin(), idx.end(), 0);

                    std::function<void(size_t)> recurSet = [&](size_t pos) {
                        if (pos == idx.size()) return;
                        if (callInst->getOperand(idx[pos]) == oldGetElementPtr)
                            callInst->setArgOperand(idx[pos], bitCast);
                        recurSet(pos + 1);
                    };

                    recurSet(0);

                    return true;
                }
                else {
                    return (newTypeP.ty->getTypeID() != oldTypeP.ty->getTypeID())
                        ? ChangeVisitor::changePrecision(context, curIt, newTarget, oldTarget, newTypeP, oldTypeP, alignment)
                        : true;
                } })();
    };

    if (!func())
      deadInsts.push_back(dyn_cast<Instruction>(curIt->getUser()));

    recur(std::next(curIt), endIt);
  };

  recur(oldGetElementPtr->use_begin(), oldGetElementPtr->use_end());

  removeDeadInsts();

  auto &srcPtr = *oldGetElementPtr;
  auto &dstPtr = *newGetElementPtr;

  Value *replacement = nullptr;

  if (int depthValue = newTypeP.dep; depthValue)
  {
    auto *casted = new BitCastInst(&dstPtr, srcPtr.getType(), "", &srcPtr);
    replacement = casted;
  }
  else
  {
    replacement = &dstPtr;
  }

  srcPtr.replaceAllUsesWith(replacement);

  return false;
}
