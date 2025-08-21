#include "../include/CreateConfigFile.hpp"
#include "../include/utils.hpp"

#include <cassert>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/Passes/PassPlugin.h>

#include <nlohmann/json.hpp>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <fstream>
#include <string>
using nlohmann::json;
using namespace llvm;
using std::string,std::vector;
cl::opt<string> FileName("filename", cl::value_desc("filename"), cl::desc("The file name"), cl::init("file.json"));
cl::opt<string> ExcludedFunctionsFileName("exclude", cl::value_desc("filename"), cl::desc("List of functions to exclude (if in dependencies)"), cl::init("exclude.txt"));
cl::opt<string> IncludedFunctionsFileName("include", cl::value_desc("filename"), cl::desc("List of functions to include (dependencies)"), cl::init("include.txt"));
cl::opt<string> IncludedGlobalVarsFileName("include_global_vars", cl::value_desc("filename"), cl::desc("List of global variables to include"), cl::init("include_global.txt"));
cl::opt<string> ExcludedLocalVarsFileName("exclude_local_vars", cl::value_desc("filename"), cl::desc("List of local variables to exclude"), cl::init("exclude_local.txt"));
cl::opt<bool> PythonFormat("pformat", cl::value_desc("flag"), cl::desc("Use python format"), cl::init(false));
cl::opt<bool> ListOperators("ops", cl::value_desc("flag"), cl::desc("Print operators"), cl::init(false));
cl::opt<bool> ListFunctions("funs", cl::value_desc("flag"), cl::desc("Print functions"), cl::init(false));
cl::opt<bool> OnlyScalars("only-scalars", cl::value_desc("flag"), cl::desc("Print only scalars"), cl::init(false));
cl::opt<bool> OnlyArrays("only-arrays", cl::value_desc("flag"), cl::desc("Print only arrays"), cl::init(false));

static void printDimensions(vector<unsigned> &dimensions, raw_fd_ostream &outfile) {
  for(unsigned i = 0; i < dimensions.size(); i++) {
    outfile << "[" << dimensions[i] << "]";
  }
  return;
}


std::string type2Str(Type *type, Value *value = nullptr) {
  std::string result;
  std::vector<unsigned> dimensions;

  // 展开数组维度
  while (auto *arrayType = dyn_cast<ArrayType>(type)) {
    dimensions.push_back(arrayType->getNumElements());
    type = arrayType->getElementType();
  }
  // 指针特殊处理（opaque pointer 无 element type）
  if (type->isPointerTy()) {
    Type *element = value ? resolvePointerElementType(value).ty : nullptr;
    if (element) {
      result = type2Str(element) + "*";
    } else {
      result = "ptr"; // fallback
    }
  }
  else if (type->isFloatTy()) {
    result = "float";
  } else if (type->isDoubleTy()) {
    result = "double";
  }else if (type->isHalfTy()) {
    result = "half";
  } else if (type->isIntegerTy()) {
    result = "int";
  } else if (isa<StructType>(type)) {
    result = "struct";
  } else {
    // fallback
    std::string tmp;
    llvm::raw_string_ostream rso(tmp);
    type->print(rso);
    result = rso.str();
  }
  // 添加数组维度
  for (unsigned dim : dimensions) {
    result += "[" + std::to_string(dim) + "]";
  }
  return result;
}
//修改过
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
    errs() << "WARNING: Did not find metadata\n";
  }
  return id;
}

static bool isFPArray(Type *type,Value *value) {
  if (ArrayType *array = dyn_cast<ArrayType>(type)) {
    type = array->getElementType();
    if (type->isFloatingPointTy()) {
      return true;
    }
    else {
      return isFPArray(type ,value);
    }
  }else if (PointerType *pointer = dyn_cast<PointerType>(type)) {
      Type *elementType = resolvePointerElementType(value).ty;
      if (elementType && elementType->isFloatingPointTy()) {
        return true;
      } else if (elementType && elementType->isPointerTy()) {
        return  false;
      }

  }
  return false;
}


void CreateConfigFilePass::collectGlobals(Module &M, nlohmann::json &outJson) {
    for (GlobalVariable &GV : M.globals()) {
        std::string name = GV.getName().str();
        Type *type = GV.getValueType();

        if (includedGlobalVars.find(name) == includedGlobalVars.end()) continue;
        if (OnlyScalars && !type->isFloatingPointTy()) continue;
        if (OnlyArrays && !isFPArray(type, &GV)) continue;

        nlohmann::json entry;
        entry["name"] = name;
        entry["type"] = type2Str(type, &GV);
        outJson["globalVar"].push_back(entry);  
    }
}

void CreateConfigFilePass::collectCalls(Function &F, nlohmann::json &outJson) {
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (auto *call = dyn_cast<CallInst>(&I)) {
                Function *callee = call->getCalledFunction();
                if (!callee) continue;

                std::string name = callee->getName().str();
                if (functionCalls.count(name) == 0) continue;

                nlohmann::json entry;
                entry["id"] = getID(I);
                entry["function"] = F.getName().str();
                entry["name"] = name;
                entry["switch"] = name;

                // TODO: 实际可以推导参数类型，简化处理为固定
                entry["type"] = {"double", "double"};

                outJson["call"].push_back(entry);  // 👈 添加至 "call" 数组
            }
        }
    }
}

void CreateConfigFilePass::collectLocals(Function &F, nlohmann::json &outJson) {
    auto *symbolTable = F.getValueSymbolTable();

    for (auto iter = symbolTable->begin(); iter != symbolTable->end(); ++iter) {
        Value *val = iter->second;
        std::string name = val->getName().str();

        if (excludedLocalVars.count(name)) continue;
        if (name.find('.') != std::string::npos) continue;


        if(dyn_cast<AllocaInst>(val)==nullptr&&dyn_cast<Argument>(val)==nullptr)
          continue;
        auto dep=resolvePointerElementType(val);
        // Type *type = nullptr;
        // if (auto *alloca = dyn_cast<AllocaInst>(val)) {
        //     type = alloca->getAllocatedType();
        // } else if (auto *arg = dyn_cast<Argument>(val)) {
        //     type = arg->getType();
        // }

        // if (!type) continue;
        // if ((OnlyScalars && !type->isFloatingPointTy()) ||
        //     (OnlyArrays && !isFPArray(type, val))) {
        //     continue;
        // }

        nlohmann::json entry;
        entry["name"] = name;
        entry["function"] = F.getName().str();
        entry["type"] = dep.to_string();
        // entry["type"] = type2Str(type, val);

        // 可选：添加调试信息（如文件名）
        if (auto *term = F.getEntryBlock().getTerminator()) {
            if (DILocation *loc = term->getDebugLoc()) {
                entry["file"] = loc->getFilename().str();
            }
        }

        outJson["localVar"].push_back(entry);  // 👈 添加至 "localVar" 数组
    }
}
void CreateConfigFilePass::initLoadFilters() {
  ifstream inFile(ExcludedFunctionsFileName.c_str());
  string name;

  // reading functions to ignore
  if (!inFile) {
    errs() << "Unable to open " << ExcludedFunctionsFileName << '\n';
    exit(1);
  }

  while(inFile >> name) {
    excludedFunctions.insert(name);
  }
  inFile.close();

  // reading functions to include
  inFile.open (IncludedFunctionsFileName.c_str(), ifstream::in);
  if (!inFile) {
    errs() << "Unable to open " << IncludedFunctionsFileName << '\n';
    exit(1);
  }

  while(inFile >> name) {
    includedFunctions.insert(name);
  }
  inFile.close();

  // reading global variables to include
  inFile.open (IncludedGlobalVarsFileName.c_str(), ifstream::in);
  if (!inFile) {
    errs() << "Unable to open " << IncludedGlobalVarsFileName << '\n';
    exit(1);
  }

  while(inFile >> name) {
    includedGlobalVars.insert(name);
  }
  inFile.close();

  // reading local variables to exclude
  // assuming unique names given by LLVM, so no need to include function name
  inFile.open (ExcludedLocalVarsFileName.c_str(), ifstream::in);
  while(inFile >> name) {
    excludedLocalVars.insert(name);
  }
  inFile.close();


  if (PythonFormat) {
    errs() << "Using python format\n";
  }
  else {
    errs() << "NOT using python format\n";
  }

  // includedFunctions.insert("main");

  // populating function calls
  functionCalls.insert("log");
  //functionCalls.insert("sqrt");
  functionCalls.insert("cos"); //FT
  functionCalls.insert("sin"); //FT
  functionCalls.insert("acos"); //funarc
  return ;
}


PreservedAnalyses CreateConfigFilePass::run(Module &M, ModuleAnalysisManager &) {
    initLoadFilters();

    nlohmann::json output = nlohmann::json::object();  

    collectGlobals(M, output);
    for (auto &F : M) {
        if (!F.isDeclaration() && includedFunctions.count(F.getName().str()) &&
            !excludedFunctions.count(F.getName().str())) {
            collectLocals(F, output);
            if (ListFunctions) collectCalls(F, output);
        }
    }

    std::ofstream fileOut(FileName);
    if (PythonFormat) {
        nlohmann::json wrapper = {{"config", output}};
        fileOut << wrapper.dump(4) << std::endl;
    } else {
        fileOut << output.dump(4) << std::endl;
    }

  return PreservedAnalyses::all();
}



static Type* findLocalType(Value *value) {
  Type *type = NULL;
  if (AllocaInst* alloca = dyn_cast<AllocaInst>(value)) {
    type = alloca->getAllocatedType();
  }
  else if (Argument* arg = dyn_cast<Argument>(value)) {
    type = arg->getType();
  }
  return type;
}

//Function 类中查找局部变量，并将相关信息格式化输出到一个文件中。

// bool CreateConfigFilePass::runOnFunction(Function &function, raw_fd_ostream &outfile, bool &first) {
//   findLocalVariables(function, outfile, first);

// //  if (ListOperators) {
// //    findOperators(function, outfile, first);
// //  }

//   if (ListFunctions) {
//     findFunctionCalls(function, outfile, first);
//   }
//   return false;
// }




// char CreateConfigFile::ID = 0;
// static const RegisterPass<CreateConfigFile> registration("config-file", "Creating a config file with original types");

