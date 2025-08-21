#include "utils.hpp"

bool ChangeInstPointer::visitBitCastInst(BitCastInst &inst)
{
  BitCastInst *oldBitCast = &inst;

  std::optional<bool> result = ([&]() -> std::optional<bool>
                                {
        if (oldBitCast->getType() != newTypeP.ty) {
            BitCastInst *newBitCast = new BitCastInst(newTarget, oldBitCast->getType(), "", oldBitCast);
            oldBitCast->replaceAllUsesWith(newBitCast);

            auto it = newBitCast->use_begin();
            auto end = newBitCast->use_end();

            std::function<void(Value::use_iterator)> recur = [&](Value::use_iterator curIt) {
                if (curIt == end)
                    return;

                std::function<bool()> func = [this, &curIt]() -> bool {
                    return ChangeVisitor::changePrecision(context, curIt, newTarget, oldTarget, newTypeP, oldTypeP, alignment);
                };

                if (!func()) {
                    if (auto *inst = dyn_cast<Instruction>(curIt->getUser()))
                        deadInsts.push_back(inst);
                }

                recur(std::next(curIt));
            };

            recur(it);

            removeDeadInsts();

            return true;
        }
       
        return std::nullopt; })();

  return result.has_value()
         ? (void)result.value(),
         false
         : ([&]() -> bool
            {
            oldBitCast->replaceAllUsesWith(newTarget);
            return false; })();
}
