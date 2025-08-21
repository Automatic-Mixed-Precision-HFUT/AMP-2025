#include "utils.hpp"



bool ChangeInst::visitFPTruncInst(llvm::FPTruncInst &Xx) {


    using V  = llvm::Value;
    using I  = llvm::Instruction;
    using CI = llvm::CallInst;
    using SI = llvm::StoreInst;
    using FT = llvm::FPTruncInst;

    using UVar = std::variant<CI*, SI*, FT*, I*>;

    auto H = [&](){ return llvm::Type::getHalfTy(context); };
    auto F = [&](){ return llvm::Type::getFloatTy(context); };
    auto kill = [&](I* p){ if (p) deadInsts.emplace_back(p); };

    auto asVar = [&](llvm::User* u)->UVar {
        if (auto *c = llvm::dyn_cast<CI>(u)) return c;
        if (auto *s = llvm::dyn_cast<SI>(u)) return s;
        if (auto *t = llvm::dyn_cast<FT>(u)) return t;
        return llvm::dyn_cast<I>(u);
    };

    auto sweepUses = [&](V* oldV, V* repl, I* anchor, llvm::Type* newTy, llvm::Type* oldTy) {
        std::vector<I*> trash;
        for (auto it = oldV->use_begin(); it != oldV->use_end();) {
            auto cur = it++;
            if (auto *usr = llvm::dyn_cast<I>(cur->getUser())) {
                if (auto *t = llvm::dyn_cast<FT>(usr)) {
                    trash.push_back(t);
                    t->replaceAllUsesWith(repl);
                } else if (!ChangeVisitor::changePrecision(context, cur, repl, anchor, newTy, oldTy, alignment)) {
                    trash.push_back(usr);
                }
            }
        }
        for (auto *q : trash) q->eraseFromParent();
    };

    if (newType->isHalfTy()) {
        for (auto it = Xx.use_begin(); it != Xx.use_end(); ++it) {
            llvm::User* U = it->getUser();

            std::visit([&](auto *P){
                using T = std::decay_t<decltype(P)>;

                if constexpr (std::is_same_v<T, CI*>) {
                    std::string_view nm = (P && P->getCalledFunction())
                                          ? P->getCalledFunction()->getName().data()
                                          : "";
                    if (nm != "llvm.fmuladd.f32") return;

                    kill(P);

                    auto args = [&](){
                        std::vector<V*> r;
                        r.reserve(P->getNumOperands());
                        for (unsigned i = 0, n = P->getNumOperands() - 1; i < n; ++i) {
                            auto *op = P->getOperand(i);
                            r.emplace_back(op->getType()->isFloatTy()
                                ? static_cast<V*>(new FT(op, H(), "", P))
                                : op);
                        }
                        return r;
                    }();

                    auto *decl = llvm::Intrinsic::getDeclaration(
                        P->getFunction()->getParent(), llvm::Intrinsic::fmuladd, {H()});
                    auto *NC = CI::Create(decl, args, "", P);

                    sweepUses(P, NC, P, H(), F());
                }
                else if constexpr (std::is_same_v<T, SI*>) {
                    auto *vT = P->getValueOperand()->getType();
                    auto *pT = P->getOperand(1)->getType();
                    if (vT == newType || pT == newType) return;

                    auto *Nt = new FT(newTarget, H(), "", &Xx);
                    if (!ChangeVisitor::changePrecision(context, it, Nt, &Xx, H(), F(), alignment))
                        kill(P);
                }
                else if constexpr (std::is_same_v<T, FT*>) {
                    kill(P);
                    P->replaceAllUsesWith(new FT(newTarget, H(), "", &Xx));
                }
                else {
                    
                }
            }, asVar(U));
        }
        removeDeadInsts();
    }
    else if (newType->isFloatTy()) {
        Xx.replaceAllUsesWith(newTarget);

        for (auto it = newTarget->use_begin(); it != newTarget->use_end();) {
            auto cur = it++;
            if (!ChangeVisitor::changePrecision(context, cur, newTarget, &Xx, newType, oldType, alignment))
                kill(llvm::dyn_cast<I>(cur->getUser()));
        }
        removeDeadInsts();
    }

    return false;
}
