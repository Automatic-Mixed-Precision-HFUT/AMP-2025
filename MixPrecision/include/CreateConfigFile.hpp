#include <nlohmann/json_fwd.hpp>
#ifndef CREATE_CONFIG_FILE_GUARD
#define CREATE_CONFIG_FILE_GUARD 1

#include <llvm/Pass.h>
#include <nlohmann/json.hpp>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/CommandLine.h>

#include <set>

namespace llvm {
class GlobalVariable;
class raw_fd_ostream;
class Type;
class Value;
}

using namespace std;
using namespace llvm;



class CreateConfigFilePass : public llvm::PassInfoMixin<CreateConfigFilePass> {
public:
  CreateConfigFilePass()=default;
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);

private:
  void initLoadFilters();
  bool runOnFunction(Function &function, raw_fd_ostream &outfile, bool &first) ;

  void collectGlobals(llvm::Module &M, nlohmann::json &arr);
  void collectLocals(llvm::Function &F, nlohmann::json &arr);
  void collectCalls(llvm::Function &F, nlohmann::json &arr);
  void findOperators(Function &function, raw_fd_ostream &outfile, bool &first);

  static char ID; // Pass identification, replacement for typeid

private:
  set<string> excludedFunctions;

  set<string> includedFunctions;

  set<string> includedGlobalVars;

  set<string> excludedLocalVars;

  set<string> functionCalls;
};

#endif // CREATE_CONFIG_FILE_GUARD
