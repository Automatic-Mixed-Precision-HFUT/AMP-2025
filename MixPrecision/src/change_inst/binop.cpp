#include "utils.hpp"


bool ChangeInst::visitBinaryOperator(BinaryOperator &inst)
{

  auto byop = &inst;
  auto toFloat = [&]() {
std::vector<llvm::Value*> arg;
arg.reserve(byop->getNumOperands());

// 先生成可能的替换值
auto getOperandValue = [&](llvm::Value* v) -> llvm::Value* {
    if (v == oldTarget && oldTarget != newTarget) {
        auto new_inst = transTy(
            oldTarget->getType(),
            dyn_cast<Instruction>(newTarget),
            dyn_cast<Instruction>(oldTarget)
        );
        oldTarget->replaceAllUsesWith(new_inst);
        v = new_inst;
    }

    return v->getType()->isDoubleTy()
        ? static_cast<llvm::Value*>(new FPTruncInst(v, Type::getFloatTy(context), "", byop))
        : v;
};

// 遍历操作数并转换
for (llvm::Use &u : byop->operands()) {
    arg.push_back(getOperandValue(u.get()));
}


// 构建 opcode -> 创建 BinaryOperator 的映射
static const std::unordered_map<unsigned,
    std::function<BinaryOperator*(llvm::Value*, llvm::Value*, const Twine&, Instruction*)>> opMap = {
    {Instruction::FDiv, [](auto a, auto b, const Twine &n, auto insert){ return BinaryOperator::CreateFDiv(a,b,n,insert); }},
    {Instruction::FMul, [](auto a, auto b, const Twine &n, auto insert){ return BinaryOperator::CreateFMul(a,b,n,insert); }},
    {Instruction::FAdd, [](auto a, auto b, const Twine &n, auto insert){ return BinaryOperator::CreateFAdd(a,b,n,insert); }},
    {Instruction::FSub, [](auto a, auto b, const Twine &n, auto insert){ return BinaryOperator::CreateFSub(a,b,n,insert); }}
};

// 创建新的 BinaryOperator
BinaryOperator *newbinaryop = opMap.at(byop->getOpcode())(arg[0], arg[1], "", byop);

// 遍历所有使用者
for (auto it = byop->use_begin(); it != byop->use_end(); ++it) {
    Instruction *userInst = dyn_cast<Instruction>(it->getUser());
    if (!userInst) continue;

    if (userInst->getOperand(0)->getType()->getTypeID() != newType->getTypeID()) {
        // 判断类型并处理
        if (isa<FPTruncInst>(userInst) || isa<FPExtInst>(userInst)) {
            deadInsts.push_back(userInst);
            userInst->replaceAllUsesWith(newbinaryop);
        } else {
            // 调用 changePrecision，保持迭代器参数
            bool is_erase = ChangeVisitor::changePrecision(context, it, newbinaryop, byop, newType, oldType, alignment);
            if (!is_erase) {
                deadInsts.push_back(userInst);
            }
        }
    }
}


        removeDeadInsts();
    };

    auto toHalf = [&]() {
std::vector<llvm::Value*> arg;
arg.reserve(byop->getNumOperands()); // 提前分配空间

std::transform(byop->op_begin(), byop->op_end(), std::back_inserter(arg),
    [&](llvm::Use &u) -> llvm::Value* {
        llvm::Value *v = u.get();

        // 如果是 oldTarget 且需要替换
        if (v == oldTarget && oldTarget != newTarget) {
            oldTarget->replaceAllUsesWith(newTarget);
        }

        // 如果是 float 类型，降精度为 half
        return v->getType()->isFloatTy() 
            ? static_cast<llvm::Value*>(new FPTruncInst(v, llvm::Type::getHalfTy(context), "", byop))
            : v;
    });


// 创建新的二元运算
BinaryOperator *newbinaryop = nullptr;
auto opcodeMap = std::unordered_map<unsigned, std::function<BinaryOperator*(llvm::Value*, llvm::Value*)>>{
    {Instruction::FDiv, [&](llvm::Value *a, llvm::Value *b){ return BinaryOperator::CreateFDiv(a, b, "", byop); }},
    {Instruction::FMul, [&](llvm::Value *a, llvm::Value *b){ return BinaryOperator::CreateFMul(a, b, "", byop); }},
    {Instruction::FAdd, [&](llvm::Value *a, llvm::Value *b){ return BinaryOperator::CreateFAdd(a, b, "", byop); }},
    {Instruction::FSub, [&](llvm::Value *a, llvm::Value *b){ return BinaryOperator::CreateFSub(a, b, "", byop); }},
};

auto it = opcodeMap.find(byop->getOpcode());
newbinaryop = (it != opcodeMap.end()) ? it->second(arg[0], arg[1]) 
                                     : BinaryOperator::CreateFAdd(arg[0], arg[1], "", byop);

// 遍历使用者
for (auto useIt = byop->use_begin(); useIt != byop->use_end(); ++useIt) {
    if (auto *userInst = dyn_cast<Instruction>(useIt->getUser())) {
        std::variant<FPTruncInst*, FPExtInst*, Instruction*> varUser;
        if (auto *fp = dyn_cast<FPTruncInst>(userInst)) varUser = fp;
        else if (auto *fpex = dyn_cast<FPExtInst>(userInst)) varUser = fpex;
        else varUser = userInst;

        std::visit([&](auto *inst){
            using T = std::decay_t<decltype(inst)>;
            if constexpr (std::is_same_v<T, FPTruncInst*>) {
                deadInsts.push_back(inst);
                auto new_inst = transTy(inst->getType(), newbinaryop, inst);
                inst->replaceAllUsesWith(new_inst);
            }
            else if constexpr (std::is_same_v<T, FPExtInst*>) {
                deadInsts.push_back(inst);
                inst->replaceAllUsesWith(newbinaryop);

                for (auto item = newbinaryop->use_begin(); item != newbinaryop->use_end(); ++item) {
                    if (auto *st = dyn_cast<StoreInst>(item->getUser())) {
                        Type *storeDestType = getElementType(st->getOperand(1)->getType(), st->getOperand(1));
                        if (st->getValueOperand()->getType()->isHalfTy() &&
                            storeDestType->isDoubleTy()) {

                            Align Align8(8);
                            auto *fpex1 = new FPExtInst(newbinaryop, oldType, "", st);
                            auto *fpex2 = new FPExtInst(fpex1, storeDestType, "", st);

                            auto *newstore = new StoreInst(fpex2, st->getOperand(1), false, Align8, st);
                            (void)newstore;
                            deadInsts.push_back(st);
                        }
                    }
                    else if (auto *fptrunc = dyn_cast<FPTruncInst>(item->getUser())) {
                        if (fptrunc->getDestTy()->getTypeID() > fptrunc->getSrcTy()->getTypeID()) {
                            auto *fpex1 = new FPExtInst(fptrunc->getOperand(0), oldType, "", fptrunc);
                            fptrunc->replaceAllUsesWith(fpex1);
                            deadInsts.push_back(fptrunc);
                        }
                    }
                }
            }
            else { // 普通 Instruction
                if (userInst->getOperand(0)->getType()->getTypeID() != newType->getTypeID()) {
                    bool is_erase = ChangeVisitor::changePrecision(context, useIt, newbinaryop, byop, newType, oldType, alignment);
                    if (!is_erase) deadInsts.push_back(userInst);
                }
            }
        }, varUser);
    }
}


    removeDeadInsts();
};


    // 使用集合保存需要处理的 opcode
    static const std::array<Instruction::BinaryOps, 4> allOps = {
        Instruction::FDiv, Instruction::FMul, Instruction::FAdd, Instruction::FSub
    };
    static const std::array<Instruction::BinaryOps, 3> halfOps = {
        Instruction::FDiv, Instruction::FMul, Instruction::FAdd
    };

    auto isIn = [](auto op, auto &&container) {
        return std::find(container.begin(), container.end(), op) != container.end();
    };

    auto dispatch = [&]() {
        if (isIn(byop->getOpcode(), allOps) && !newType->isHalfTy()) {
            toFloat();          // 全精度处理
        } 
        else if (newType->isHalfTy() && byop->use_begin() != byop->use_end() && isIn(byop->getOpcode(), halfOps)) {
            toHalf();           // half 精度处理
        } 
        else if (byop->getOpcode() == Instruction::FMul && byop->getType()->isFloatTy()) {
            return true;        // 特例
        }
        return false;
    };

    return dispatch();
}

