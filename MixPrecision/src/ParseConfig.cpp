#include "../include/ParseConfig.hpp"
#include "../include/utils.hpp"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstrTypes.h>
#include <memory>
#include <nlohmann/json.hpp>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <vector>

std::map<std::string, std::unique_ptr<StrChange>> parse_config(const std::string& configPath);
llvm::cl::opt<std::string> ConfigFilePath(
    "json-config",
    llvm::cl::desc("Specify the JSON config path for ParseConfigPass"),
    llvm::cl::init("config.json"));

ParseConfigPass::ParseConfigPass() : configPath(ConfigFilePath) {}



llvm::AnalysisKey ParseConfigPass::Key;
using namespace llvm;
using Json = nlohmann::json;

string dump(ChangeType ty){
    switch (ty) {
    case ChangeType::GLOBALVAR:
      return "ChangeType::GLOBALVAR";
    case ChangeType::LOCALVAR:
      return "ChangeType::LOCALVAR";
    case ChangeType::OP:
      return "ChangeType::OP";
    case ChangeType::CALL:
      return "ChangeType::CALL";
    default:
      return"ERROR::ERROR";
      }
}

static Type* constructStruct(Value *value, unsigned int fieldToChange, Type *fieldType) {

  Type *type = value->getType();
  StructType *newStructType = NULL;

  if (PointerType *pointerType = dyn_cast<PointerType>(type)) {
    auto type1= resolvePointerElementType(value);
    StructType *oldStructType =
        dyn_cast<StructType>(type1.ty);
    vector<Type *> fields;

    for (unsigned int i = 0; i < oldStructType->getNumElements(); i++) {
      if (i != fieldToChange) {
        fields.push_back(oldStructType->getElementType(i));
      } else {
        // replace old field type with new type
        fields.push_back(fieldType);
      }
    }

    newStructType =
        StructType::create(fields, oldStructType->getName());

  }
  return newStructType;
}


static PtrDep parsePtrDep(const std::string &typeStr, llvm::LLVMContext &context) {
    using namespace llvm;

    std::string base = typeStr;
    unsigned int dep = 0;

    // 1. 统计 '*'
    while (!base.empty() && base.back() == '*') {
        dep++;
        base.pop_back();
    }

    // 去掉多余空格
    while (!base.empty() && std::isspace(base.back())) {
        base.pop_back();
    }

    // 2. 基础类型映射
    Type *ty = nullptr;

    if (base == "half") {
        ty = Type::getHalfTy(context);
    } else if (base == "float") {
        ty = Type::getFloatTy(context);
    } else if (base == "double") {
        ty = Type::getDoubleTy(context);
    } else if (base == "longdouble") {
        ty = Type::getX86_FP80Ty(context);
    } else if (base == "i1") {
        ty = Type::getInt1Ty(context);
    } else if (base == "i8") {
        ty = Type::getInt8Ty(context);
    } else if (base == "i16") {
        ty = Type::getInt16Ty(context);
    } else if (base == "i32") {
        ty = Type::getInt32Ty(context);
    } else if (base == "i64") {
        ty = Type::getInt64Ty(context);
    } else if (base.find('[')) {
        // 3. 解析 LLVM IR 风格数组
        std::string dims = base;
        std::vector<int> arraySizes;
        Type *elemTy = nullptr;

        size_t pos = 0;
        while ((pos = dims.find('[')) != std::string::npos) {
            size_t end = dims.find(']', pos);
            if (end == std::string::npos) break;
            int dim = std::stoi(dims.substr(pos + 1, end - pos - 1));
            arraySizes.push_back(dim);
            dims = dims.substr(end + 1);
        }

        // 查找数组元素类型
        dims.erase(std::remove_if(dims.begin(), dims.end(), ::isspace), dims.end());
        if (dims == "i8") {
            elemTy = Type::getInt8Ty(context);
        } else if (dims == "half") {
            elemTy = Type::getHalfTy(context);
        } else if (dims == "float") {
            elemTy = Type::getFloatTy(context);
        } else if (dims == "double") {
            elemTy = Type::getDoubleTy(context);
        } else {
            elemTy = nullptr;
        }

        if (!elemTy) return PtrDep(nullptr, dep);

        // 从内向外包裹数组
        for (auto it = arraySizes.rbegin(); it != arraySizes.rend(); ++it) {
            elemTy = ArrayType::get(elemTy, *it);
        }
        ty = elemTy;
    } else {
        assert(0&&"unknown type");
        return PtrDep(nullptr, dep);
    }

    return PtrDep(ty, dep);
}


static llvm::Type* parseType(const std::string &typeStr, llvm::LLVMContext &context) {
    std::string base;
    std::vector<int> arraySizes;
    bool isPointer = false;

    // 1. 判断是否是指针类型
    size_t starPos = typeStr.find('*');
    if (starPos != std::string::npos) {
        base = typeStr.substr(0, starPos);
        isPointer = true;
    } else {
        base = typeStr;
    }

    // 2. 提取数组维度
    size_t bracketPos = base.find('[');
    if (bracketPos != std::string::npos) {
        std::string typeName = base.substr(0, bracketPos);
        std::string dims = base.substr(bracketPos);
        base = typeName;

        size_t pos = 0;
        while ((pos = dims.find('[')) != std::string::npos) {
            size_t end = dims.find(']', pos);
            if (end == std::string::npos) break;
            int dim = std::stoi(dims.substr(pos + 1, end - pos - 1));
            arraySizes.push_back(dim);
            dims = dims.substr(end + 1);
        }
    }

    // 3. 基础类型映射
    llvm::Type *ty = nullptr;
    if (base == "half") {
        ty = Type::getHalfTy(context);
    } else if (base == "float") {
        ty = llvm::Type::getFloatTy(context);
    } else if (base == "double") {
        ty = llvm::Type::getDoubleTy(context);
    } else if (base == "longdouble") {
        ty = llvm::Type::getX86_FP80Ty(context);
    } else {
        return nullptr;
    }

    // 4. 数组嵌套构造：从里到外
    for (auto it = arraySizes.rbegin(); it != arraySizes.rend(); ++it) {
        ty = llvm::ArrayType::get(ty, *it);
    }

    // 5. 最外层指针
    if (isPointer) {
        ty = llvm::PointerType::getUnqual(ty);
    }

    return ty;
}
void ParseConfigPass::updateChanges(
    const std::string &id,
    llvm::Value *value,
    llvm::LLVMContext &context) {
    auto it = types_.find(id);
    if (it == types_.end()) return;

    const StrChange *meta = it->second.get();
    std::istringstream ss(meta->getTypes());
    std::vector<PtrDep> parsedTypes;
    std::string word;
    while (ss >> word) {
        if (auto pd = parsePtrDep(word, context);pd.ty)
            parsedTypes.push_back(pd);
    }

    if (parsedTypes.empty()) return;

    int field = meta->getField();
    std::string kind = meta->getClassification();

    // 结构体字段替换支持
    if (field >= 0) {
        parsedTypes[0].ty = constructStruct(value, field, parsedTypes[0].ty);
    }

    if (kind == "globalVar") {
        changes_[GLOBALVAR].emplace_back(std::make_unique<Change>(parsedTypes, value, field));
    } else if (kind == "localVar") {
        changes_[LOCALVAR].emplace_back(std::make_unique<Change>(parsedTypes, value, field));
    } else if (kind == "op") {
        changes_[OP].emplace_back(std::make_unique<Change>(parsedTypes, value));
    } else if (kind == "call") {
        /*直接强制类型转换，可能有异常*/
        auto funcMeta = (const FuncStrChange*) (meta);
        // auto *funcMeta = dynamic_cast<const FuncStrChange*>(meta);
        std::string swit = funcMeta ? funcMeta->getSwitch() : "";
        changes_[CALL].emplace_back(std::make_unique<FunctionChange>(parsedTypes, value, swit));
    }
}


// 初始化 JSON 类型映射 → StrChange / FuncStrChange 对象
bool ParseConfigPass::doInitialization(Module &M) {
    // std::ifstream file(configPath);
    this->types_=parse_config(configPath);

    // 初始化每个 ChangeType 类型的容器
    changes_[GLOBALVAR] = std::move(std::vector<std::unique_ptr<Change>>{});
    changes_[LOCALVAR] = std::move(std::vector<std::unique_ptr<Change>>{});
    changes_[OP]       = std::move(std::vector<std::unique_ptr<Change>>{});
    changes_[CALL]     = std::move(std::vector<std::unique_ptr<Change>>{});

    return true;
}

ParseConfigResult ParseConfigPass::run(Module &M, ModuleAnalysisManager &) {
    if (!doInitialization(M)) {
        return ParseConfigResult{nullptr};  // 配置加载失败也不破坏分析结果
    }

    LLVMContext &context = M.getContext();

    // 遍历全局变量
    for (auto &global : M.globals()) {
        std::string varId = global.getName().str();
        updateChanges(varId, &global, context);
    }

    // 遍历函数内局部变量、指令等
    for (auto &F : M) {
        if (!F.isDeclaration()) {
            runOnFunction(F);
        }
    }

    // 调试输出收集到的 Change 信息
    for (const auto &[change_type, change_vec] : changes_) {
        errs() << "[ParseConfigPass] " << dump(change_type) << ":\n";
        for (const auto &change_ptr : change_vec) {
            Value *value = change_ptr->getValue();
            errs() << "  name: ";
            if (auto *op = dyn_cast<BinaryOperator>(value)) {
                errs() << op->getOpcodeName();
            } else if (auto *fcmp = dyn_cast<FCmpInst>(value)) {
                errs() << fcmp->getOpcodeName();
            } else {
                errs() << value->getName();
            }
            errs() << ", type: ";
            if (!change_ptr->getType().empty()) {
                errs()<<change_ptr->getType()[0];
            } else {
                errs() << "<empty>";
            }
            errs() << "\n";
        }
    }

    return ParseConfigResult{&this->changes_};
}

Value* ParseConfigPass::findAlloca(Value *value, Function *function) {

  for(Function::iterator b = function->begin(), be = function->end(); b != be; b++) {
    for(BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; i++) {

      if (StoreInst *store = dyn_cast<StoreInst>(i)) {
        Value *op1 = store->getOperand(0);
        Value *op2 = store->getOperand(1);
        if (op1 == value) {
          op2->takeName(value);
          return op2;
        }
      }
    }
  }
  return value;
}

static string getID(Instruction &inst) {
  string id = "";
  if (MDNode *node = inst.getMetadata("corvette.inst.id")) {
    if (node->getNumOperands() > 0) {
      // Access the operand as Metadata, then attempt to cast to MDString
      const Metadata *md = node->getOperand(0).get();
      if (const MDString *mdstring = dyn_cast<MDString>(md)) {
        id = mdstring->getString().str();
      } else {
        errs() << "WARNING: Metadata is not a string\n";
      }
    }
  }
  else {
    //errs() << "WARNING: Did not find metadata\n";
  }
  return id;
}

void ParseConfigPass::runOnFunction(Function &f) {
  // local variables
  string functionName = f.getName().str();
  LLVMContext& context = f.getContext();
  const ValueSymbolTable *  symbol_table = f.getValueSymbolTable();
  auto iter=symbol_table->begin();

  for(; iter != symbol_table->end(); ++iter) {
    Value *value = iter->second;
    if (dyn_cast<Argument>(value)) {
      value = findAlloca(value, &f);
    }

    string varIdStr = "";
    //varIdStr += value->getName();
    varIdStr += iter->second->getName();
    varIdStr += "@";
    varIdStr += functionName;
    updateChanges(varIdStr, value, context);
  }

  // operations and calls
  for (Function::iterator b = f.begin(), be = f.end(); b != be; b++) {
    for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; i++) {
      Instruction *inst = &*i;
      string id = getID(*i);
      updateChanges(id, inst, context);
    }
  }

}

// static Type* constructStruct(Value *value, unsigned int fieldToChange, Type *fieldType) {
// //   Type *type = value->getType();

// //   if (PointerType *pointerType = dyn_cast<PointerType>(type)) {
// //     if (StructType *oldStructType = dyn_cast<StructType>(pointerType->getElementType())) {
// //       std::vector<Type*> fields;
// //       for (unsigned int i = 0; i < oldStructType->getNumElements(); ++i) {
// //         if (i == fieldToChange)
// //           fields.push_back(fieldType); // 替换字段
// //         else
// //           fields.push_back(oldStructType->getElementType(i));
// //       }

// //       // 保留旧结构体名字 + 后缀
// //       std::string newName = oldStructType->hasName()
// //                               ? oldStructType->getName().str() + ".modified"
// //                               : "";

// //       StructType *newStructType = StructType::create(value->getContext(), fields, newName, /*isPacked=*/false);
// //       return PointerType::getUnqual(newStructType);
// //     }
// //   }

// //   // 如果不是结构体或不是指针，直接返回原类型
// //   return value->getType();
// }


// ParseConfigPass::ParseConfigPass(std::string path) : configPath(std::move(path)) {}
// void ParseConfigPass::updateChanges(const std::string& id, llvm::Value* value, llvm::LLVMContext& context) {
//   auto typeIt = types.find(id);
//   if (typeIt == types.end()) return;

//   const std::string& changeType = typeIt->second->getClassification();
//   const std::string& typeStrs = typeIt->second->getTypes();
//   int field = typeIt->second->getField();
//   Types type;

//   std::istringstream iss(typeStrs);
//   std::string typeStr;
//   while (iss >> typeStr) {
//     if (typeStr == "float") {
//       type.push_back(Type::getFloatTy(context));
//     } else if (typeStr == "double" || typeStr == "longdouble") {
//       // ARM平台longdouble退化为double处理
//       type.push_back(Type::getDoubleTy(context));
//     } else if (typeStr.compare(0, 6, "float[") == 0) {
//       bool first = true;
//       size_t openBracketPos, closeBracketPos; 
//       int size = 0;
//       Type* aType = nullptr;
//       std::string tmpTypeStr = typeStr;
//       while ((openBracketPos = tmpTypeStr.find_last_of('[')) != std::string::npos) {
//         closeBracketPos = tmpTypeStr.find_last_of(']');
//         size = std::stoi(tmpTypeStr.substr(openBracketPos+1, closeBracketPos - openBracketPos - 1));
//         if (first) {
//           aType = ArrayType::get(Type::getFloatTy(context), size);
//           first = false;
//         } else {
//           aType = ArrayType::get(aType, size);
//         }
//         tmpTypeStr = tmpTypeStr.substr(0, openBracketPos);
//       }
//       type.push_back(aType);
//     } else if (typeStr.compare(0, 7, "double[") == 0 || typeStr.compare(0, 11, "longdouble[") == 0) {
//       bool first = true;
//       size_t openBracketPos, closeBracketPos;
//       int size = 0;
//       Type* aType = nullptr;
//       std::string tmpTypeStr = typeStr;
//       while ((openBracketPos = tmpTypeStr.find_last_of('[')) != std::string::npos) {
//         closeBracketPos = tmpTypeStr.find_last_of(']');
//         size = std::stoi(tmpTypeStr.substr(openBracketPos+1, closeBracketPos - openBracketPos - 1));
//         if (first) {
//           aType = ArrayType::get(Type::getDoubleTy(context), size);
//           first = false;
//         } else {
//           aType = ArrayType::get(aType, size);
//         }
//         tmpTypeStr = tmpTypeStr.substr(0, openBracketPos);
//       }
//       type.push_back(aType);
//     } else if (typeStr.compare(0, 6, "float*") == 0) {
//       type.push_back(PointerType::getUnqual(Type::getFloatTy(context)));
//     } else if (typeStr.compare(0, 7, "double*") == 0 || typeStr.compare(0, 11, "longdouble*") == 0) {
//       type.push_back(PointerType::getUnqual(Type::getDoubleTy(context)));
//     }
//   }


//     if (field >= 0 && !type.empty()) {
//         type[0] = constructStruct(value, field, type[0]);
//     }

//     if (!type.empty()) {
//         if (changeType == "globalVar") {
//             changes[GLOBALVAR].push_back(make_unique<Change>(type, value, field));
//         }
//         else if (changeType == "localVar") {
//             changes[LOCALVAR].push_back(make_unique<Change>(type, value, field));
//         }
//         else if (changeType == "op") {
//             changes[OP].push_back(make_unique<Change>(type, value));
//         }
//         else if (changeType == "call") {
//             auto* change = static_cast<FuncStrChange*>(typeIt->second.get());
//             std::string swit = change->getSwitch();
//             changes[CALL].push_back(make_unique<FunctionChange>(type, value, swit));
//         }
//     }
// }

// PreservedAnalyses ParseConfigPass::run(Module &M, ModuleAnalysisManager &) {
//     // 1. Parse config.json
//     std::ifstream in(configPath);
//     json root = json::parse(in);

//     // 2. Convert json → types (std::map<string, StrChange*>)
//     for (const auto& el : root["globalVar"]) {
//         std::string name = el["name"];
//         std::string type = el["type"];
//         types[name] = std::make_unique<StrChange>("globalVar", type, -1);
//     }
//     // 同理处理 localVar, op, call...

//     // 3. 遍历 Module 全局变量
//     for (auto &g : M.globals()) {
//         updateChanges(g.getName().str(), &g, M.getContext());
//     }

//     // 4. 遍历函数与指令
//     for (auto &f : M) {
//         if (f.isDeclaration()) continue;
//         for (auto &bb : f) {
//             for (auto &i : bb) {
//                 std::string id;
//                 if (MDNode *node = i.getMetadata("corvette.inst.id")) {
//                     if (auto *s = dyn_cast<MDString>(node->getOperand(0)))
//                         id = s->getString().str();
//                 }
//                 updateChanges(id, &i, M.getContext());
//             }
//         }
//     }

//     return PreservedAnalyses::all();
// }
