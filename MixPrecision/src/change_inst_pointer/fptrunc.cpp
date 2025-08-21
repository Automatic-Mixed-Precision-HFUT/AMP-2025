#include "utils.hpp"


bool ChangeInstPointer::visitFPTruncInst(llvm::FPTruncInst &inst) {

    using V  = llvm::Value;
    using I  = llvm::Instruction;
    using CI = llvm::CallInst;
    using SI = llvm::StoreInst;
    using FT = llvm::FPTruncInst;

    using UVar = std::variant<CI*, SI*, FT*, I*>;

    auto H = [&](){ return llvm::Type::getHalfTy(context); };
    auto F = [&](){ return llvm::Type::getFloatTy(context); };
    auto kill = [&](I* p){ if(p) deadInsts.push_back(p); };

    auto asVar = [&](llvm::User* u) -> UVar {
        if (auto *c = llvm::dyn_cast<CI>(u)) return c;
        if (auto *s = llvm::dyn_cast<SI>(u)) return s;
        if (auto *t = llvm::dyn_cast<FT>(u)) return t;
        return llvm::dyn_cast<I>(u);
    };

    auto sweepUses = [&](V* oldV, V* repl, I* anchor, llvm::Type* newTy, llvm::Type* oldTy){
        std::vector<I*> trash;
        for (auto it = oldV->use_begin(); it != oldV->use_end(); ++it) {
            auto *usr = llvm::dyn_cast<I>(it->getUser());
            if (!usr) continue;

            if (auto *fptr = llvm::dyn_cast<FT>(usr)) {
                trash.push_back(fptr);
                fptr->replaceAllUsesWith(repl);
            } else {
                bool erased = ChangeVisitor::changePrecision(context, it, repl, anchor, newTy, oldTy, alignment);
                if (!erased) trash.push_back(usr);
            }
        }
        for (auto *p : trash) p->eraseFromParent();
    };

    if (newTypeP.ty->isHalfTy()) {

        for (auto it = inst.use_begin(); it != inst.use_end(); ++it) {
            llvm::User* U = it->getUser();

            std::visit([&](auto* P){
                using T = std::decay_t<decltype(P)>;
                
                if constexpr (std::is_same_v<T, CI*>) {
                    if (!P || !P->getCalledFunction()) return;
                    if (P->getCalledFunction()->getName() != "llvm.fmuladd.f32") return;

                    kill(P);

                    std::vector<V*> args;
                    for (unsigned i = 0, n = P->getNumOperands()-1; i < n; ++i) {
                        auto *op = P->getOperand(i);
                        args.push_back(op->getType()->isFloatTy() ? static_cast<V*>(new FT(op, H(), "", P)) : op);
                    }

                    Function* FmuladdF16 = llvm::Intrinsic::getDeclaration(P->getFunction()->getParent(),
                                                                          llvm::Intrinsic::fmuladd, {H()});
                    CI* NewCall = CI::Create(FmuladdF16, args, "", P);
                    sweepUses(P, NewCall, P, H(), F());
                }
                else if constexpr (std::is_same_v<T, SI*>) {
                    auto *vT = P->getValueOperand()->getType();
                    auto *pT = P->getOperand(1)->getType();
                    if (vT == newTypeP.ty || pT == newTypeP.ty) return;

                    FT* newFPtr = new FT(newTarget, H(), "", &inst);
                    if (!ChangeVisitor::changePrecision(context, it, newFPtr, &inst, getHalfPtrDep(context),
                                                        getFloatPtrDep(context), alignment))
                        kill(P);
                }
                else if constexpr (std::is_same_v<T, FT*>) {
                    kill(P);
                    P->replaceAllUsesWith(new FT(newTarget, H(), "", &inst));
                }
            }, asVar(U));
        }

        removeDeadInsts();
    }
    else if (newTypeP.ty->isFloatTy()) {

        inst.replaceAllUsesWith(newTarget);

        for (auto it = newTarget->use_begin(); it != newTarget->use_end(); ++it) {
            if (!ChangeVisitor::changePrecision(context, it, newTarget, &inst, newTypeP.ty, oldTypeP.ty, alignment))
                kill(llvm::dyn_cast<I>(it->getUser()));
        }

        removeDeadInsts();
    }

    return false;
}
