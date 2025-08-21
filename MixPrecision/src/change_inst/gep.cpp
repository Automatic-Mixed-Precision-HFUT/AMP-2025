#include "utils.hpp"

bool ChangeInst::visitGetElementPtrInst(GetElementPtrInst &inst)
{
  GetElementPtrInst *oldGetElementPtr = &inst;

  auto newGetElementPtr = GetElementPtrInst::CreateInBounds(
      newType,
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

  auto it = oldGetElementPtr->use_begin();
  auto endIt = oldGetElementPtr->use_end();

  std::function<void(Value::use_iterator)> loopFunc = [&](Value::use_iterator curIt)
  {
    if (curIt == endIt)
      return;

    newTarget = newGetElementPtr;

    std::function<bool()> func = [&]() -> bool
    {
      return std::invoke([&]() -> bool
                         { return ([&]() -> bool
                                   {
                if (auto *ci = dyn_cast<CallInst>(curIt->getUser())) {
                    auto *bc = new BitCastInst(newTarget, oldGetElementPtr->getType(), "", ci);

                    std::vector<unsigned> idx(ci->getNumOperands());
                    std::iota(idx.begin(), idx.end(), 0);

                    std::for_each(idx.begin(), idx.end(), [&](unsigned idx) {
                        if (ci->getOperand(idx) == oldGetElementPtr)
                            ci->setArgOperand(idx, bc);
                    });

                    return true;
                } else {
                    return std::invoke([&]() -> bool {
                        bool cond = newType->getTypeID() != oldType->getTypeID();

                        return cond
                            ? ([](auto&&... args) {
                                  return ChangeVisitor::changePrecision(std::forward<decltype(args)>(args)...);
                              })(context, curIt, newTarget, oldTarget, newType, oldType, alignment)
                            : true;
                    });
                } })(); });
    };

    if (!func())
    {
      deadInsts.push_back(dyn_cast<Instruction>(curIt->getUser()));
    }

    loopFunc(std::next(curIt));
  };

  loopFunc(it);

  removeDeadInsts();

  oldGetElementPtr->replaceAllUsesWith(newGetElementPtr);

  return false;
}