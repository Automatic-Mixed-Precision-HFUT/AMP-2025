#include "utils.hpp"

bool ChangeInstPointer::visitBinaryOperator(BinaryOperator &inst)
{

  auto byop = &inst;

auto toFloat = [&]() {
    // 使用外部作用域的变量 byop, oldTarget, newTarget, newTypeP, oldTypeP, context, deadInsts, alignment
   // 先处理 oldTarget 替换
for (unsigned i = 0; i < byop->getNumOperands(); i++) {
    if (byop->getOperand(i) == oldTarget && oldTarget != newTarget) {
        auto new_inst = transTyP(oldTarget->getType(),
                                 dyn_cast<Instruction>(newTarget),
                                 dyn_cast<Instruction>(oldTarget));
        oldTarget->replaceAllUsesWith(new_inst);
    }
}

// 然后生成 arg
std::vector<llvm::Value *> arg;
arg.reserve(byop->getNumOperands());

for (llvm::Value* operand : byop->operands()) {
    if (operand->getType()->isDoubleTy()) {
        arg.push_back(new FPTruncInst(operand, Type::getFloatTy(context), "", byop));
    } else {
        arg.push_back(operand);
    }
}


// 定义操作码到函数的映射
static const std::unordered_map<unsigned, std::function<BinaryOperator*(llvm::Value*, llvm::Value*, llvm::Instruction*)>> binOpFactory = {
    {Instruction::FDiv, [](llvm::Value* a, llvm::Value* b, llvm::Instruction* insertAt) { return BinaryOperator::CreateFDiv(a, b, "", insertAt); }},
    {Instruction::FMul, [](llvm::Value* a, llvm::Value* b, llvm::Instruction* insertAt) { return BinaryOperator::CreateFMul(a, b, "", insertAt); }},
    {Instruction::FAdd, [](llvm::Value* a, llvm::Value* b, llvm::Instruction* insertAt) { return BinaryOperator::CreateFAdd(a, b, "", insertAt); }},
    {Instruction::FSub, [](llvm::Value* a, llvm::Value* b, llvm::Instruction* insertAt) { return BinaryOperator::CreateFSub(a, b, "", insertAt); }}
};

// 创建新的 BinaryOperator
auto itOp = binOpFactory.find(byop->getOpcode());
BinaryOperator* newbinaryop = (itOp != binOpFactory.end()) ? itOp->second(arg[0], arg[1], byop)
                                                           : BinaryOperator::CreateFSub(arg[0], arg[1], "", byop);

// 遍历使用者
for (auto it1 = byop->use_begin(); it1 != byop->use_end(); ++it1)
{
    Instruction* userInst = dyn_cast<Instruction>(it1->getUser());
    if (!userInst) continue;

    if (userInst->getOperand(0)->getType()->getTypeID() != newTypeP.ty->getTypeID())
    {
        if (auto fp = dyn_cast<FPTruncInst>(userInst))
        {
            deadInsts.push_back(fp);
            fp->replaceAllUsesWith(newbinaryop);
        }
        else if (auto fpex = dyn_cast<FPExtInst>(userInst))
        {
            deadInsts.push_back(fpex);
            fpex->replaceAllUsesWith(newbinaryop);
        }
        else
        {
            PtrDep newTypetemp_PtrDep(newTypeP.ty, 0);
            PtrDep oldTypetemp_PtrDep(oldTypeP.ty, 0);
            bool is_erease = ChangeVisitor::changePrecision(context, it1, newbinaryop, byop,
                                                             newTypetemp_PtrDep, oldTypetemp_PtrDep,
                                                             alignment);
            if (!is_erease)
                deadInsts.push_back(userInst);
        }
    }
}

removeDeadInsts();

};


auto toHalf = [&]() {
   std::vector<llvm::Value*> arg;
arg.reserve(byop->getNumOperands());

std::transform(byop->op_begin(), byop->op_end(), std::back_inserter(arg),
    [&](llvm::Value* operand) -> llvm::Value* {
        // 替换 oldTarget
        if (operand == oldTarget && oldTarget != newTarget) {
            oldTarget->replaceAllUsesWith(newTarget);
        }

        // 浮点降精
        if (operand->getType()->isFloatTy()) {
            return new FPTruncInst(operand, Type::getHalfTy(context), "", byop);
        }

        return operand;
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

// 遍历所有使用者
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
                auto new_inst = transTyP(inst->getType(), newbinaryop, inst);
                inst->replaceAllUsesWith(new_inst);
            }
            else if constexpr (std::is_same_v<T, FPExtInst*>) {
                deadInsts.push_back(inst);
                inst->replaceAllUsesWith(newbinaryop);

                // 处理 FPExt 使用者
                for (auto item = newbinaryop->use_begin(); item != newbinaryop->use_end(); ++item) {
                    if (auto *st = dyn_cast<StoreInst>(item->getUser())) {
                        Type *storeDestType = getElementTypeP(st->getOperand(1)->getType(), st->getOperand(1));
                        if (st->getValueOperand()->getType()->isHalfTy() && storeDestType->isDoubleTy()) {
                            Align Align8(8);
                            auto *fpex1 = new FPExtInst(newbinaryop, oldTypeP.ty, "", st);
                            auto *fpex2 = new FPExtInst(fpex1, storeDestType, "", st);
                            auto *newstore = new StoreInst(fpex2, st->getOperand(1), false, Align8, st);
                            (void)newstore;
                            deadInsts.push_back(st);
                        }
                    }
                    else if (auto *fptrunc = dyn_cast<FPTruncInst>(item->getUser())) {
                        if (fptrunc->getDestTy()->getTypeID() > fptrunc->getSrcTy()->getTypeID()) {
                            auto *fpex1 = new FPExtInst(fptrunc->getOperand(0), oldTypeP.ty, "", fptrunc);
                            fptrunc->replaceAllUsesWith(fpex1);
                            deadInsts.push_back(fptrunc);
                        }
                    }
                }
            }
            else { // 普通 Instruction
                if (userInst->getOperand(0)->getType()->getTypeID() != newTypeP.ty->getTypeID()) {
                    bool is_erase = ChangeVisitor::changePrecision(
                        context, useIt, newbinaryop, byop, 
                        PtrDep(newTypeP.ty, 0), PtrDep(oldTypeP.ty, 0), alignment
                    );
                    if (!is_erase) deadInsts.push_back(userInst);
                }
            }
        }, varUser);
    }
}

// 删除死指令
removeDeadInsts();

};


 // 定义动作类型枚举
enum class ActionType { ToFloat, ToHalf, ReturnTrue, NoOp };

// 根据操作码和目标类型选择动作
auto getAction = [&](llvm::BinaryOperator* op, bool isHalf) -> ActionType {
    unsigned opcode = op->getOpcode();

    if (!isHalf) {
        if (opcode == Instruction::FDiv || opcode == Instruction::FMul ||
            opcode == Instruction::FAdd || opcode == Instruction::FSub)
            return ActionType::ToFloat;
    } else {
        if (opcode == Instruction::FDiv || opcode == Instruction::FMul || opcode == Instruction::FAdd)
            return ActionType::ToHalf;
    }

    if (opcode == Instruction::FMul && op->getType()->isFloatTy())
        return ActionType::ReturnTrue;

    return ActionType::NoOp;
};

// 获取动作
ActionType action = getAction(byop, newTypeP.ty->isHalfTy());

// 执行动作
switch (action) {
    case ActionType::ToFloat:
        toFloat();
        return false;

    case ActionType::ToHalf:
        if (byop->use_begin() == byop->use_end())
            return false;
        toHalf();
        return false;

    case ActionType::ReturnTrue:
        return true;

    case ActionType::NoOp:
    default:
        return false;
}

}