#ifndef PARSE_CONFIG_H
#define PARSE_CONFIG_H

#include "Change.hpp"
#include <llvm/IR/PassManager.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <map>
#include <memory>

enum ChangeType { GLOBALVAR, LOCALVAR, OP, CALL };
string dump(ChangeType ty);

using Changes = std::vector<std::unique_ptr<Change>>;

struct ParseConfigResult {
  std::map<ChangeType, Changes> const *const changes;

  // 你可以加访问接口
  const std::map<ChangeType, Changes> *const getChanges() const {
    return changes;
  }
};

class ParseConfigPass : public llvm::AnalysisInfoMixin<ParseConfigPass> {
public:
    
    using Result = ParseConfigResult;

    ParseConfigResult run(Module &M, ModuleAnalysisManager &);

    static AnalysisKey Key; // 必须有这个静态 key

    ParseConfigPass();
    std::map<ChangeType, Changes> &getChanges(){return changes_;}
private:
    const std::string configPath;

    std::map<std::string, std::unique_ptr<StrChange>> types_;
    std::map<ChangeType, Changes> changes_;

    bool doInitialization(llvm::Module &M);
    void updateChanges(const std::string &id, llvm::Value *value, llvm::LLVMContext &context);
    // llvm::Type* parseTypeString(const std::string &typeStr, llvm::LLVMContext &context);
    static llvm::Value* findAlloca(llvm::Value *value, llvm::Function *function);

    void runOnFunction(llvm::Function &F);
};

#endif
