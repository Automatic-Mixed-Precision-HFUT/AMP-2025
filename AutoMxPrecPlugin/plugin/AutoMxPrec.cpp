/*

使用命令，自己改路径 -Xclang output.json指定输出
clang --target=aarch64-linux-gnu -march=armv8.2-a+fp16 -static \
    -Xclang -load -Xclang ~/opt/AMP/AutoMxPrecPlugin/build/plugin/AutoMxPrec.so \
    -Xclang -plugin -Xclang auto-mxprec-plugin -Xclang -plugin-arg-auto-mxprec-plugin -Xclang output.json /mnt/e/code/hpl-ai-finalist/hpl-ai.c -c -o hpl-ai.o

json格式：
[
  {
    "name": "fclose",
    "type": "call"
  },
  {
    "func": "MxHPLTest",
    "name": "tol",
    "type": "var"
  },
  {
    "name": "gmres",
    "type": "call"
  }
]
*/
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "nlohmann/json.hpp"
#include <vector>
#include <string>
#include <fstream>

using namespace clang;
using json = nlohmann::json;

namespace {

static std::vector<SourceLocation> GlobalPragmaLocs;

class AutoMxPrecPragmaHandler : public PragmaHandler {
public:
    AutoMxPrecPragmaHandler() : PragmaHandler("AutoMxPrec") {}
    void HandlePragma(Preprocessor &PP, PragmaIntroducer, Token &Tok) override {
        GlobalPragmaLocs.push_back(Tok.getLocation());
    }
};

class AutoMxPrecVisitor : public RecursiveASTVisitor<AutoMxPrecVisitor> {
public:
    AutoMxPrecVisitor(ASTContext &Ctx)
        : Context(Ctx), SM(Ctx.getSourceManager()), LangOpts(Ctx.getLangOpts()) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (FD->hasBody()) {
            CurrentFuncName = FD->getNameAsString();
        }
        return true;
    }

    bool VisitCompoundStmt(CompoundStmt *CS) {
        SourceLocation StartLoc = CS->getBeginLoc();
        if (InsideTargetBlock) return true;

        for (auto PragmaLoc : GlobalPragmaLocs) {
            if (!SM.isWrittenInSameFile(PragmaLoc, StartLoc)) continue;
            if (SM.isBeforeInTranslationUnit(PragmaLoc, StartLoc) &&
                areLocationsAdjacent(PragmaLoc, StartLoc)) {

                InsideTargetBlock = true;
                TraverseStmt(CS);
                InsideTargetBlock = false;
                break;
            }
        }
        return true;
    }

    bool VisitCallExpr(CallExpr *CE) {
        if (!InsideTargetBlock) return true;
        if (auto *FD = CE->getDirectCallee()) {
            json callObj;
            callObj["type"] = "call";
            callObj["name"] = FD->getNameAsString();
            Data.push_back(callObj);
        }
        return true;
    }

    bool VisitDeclStmt(DeclStmt *DS) {
        if (!InsideTargetBlock) return true;
        for (auto *D : DS->decls()) {
            if (auto *VD = dyn_cast<VarDecl>(D)) {
                if (VD->isLocalVarDecl() && !VD->isImplicit()) {
                    json varObj;
                    varObj["type"] = "var";
                    varObj["name"] = VD->getNameAsString();
                    varObj["func"] = CurrentFuncName;
                    //只有automix中的tol变量，这个变量需要过滤掉
                    // Data.push_back(varObj);
                }
            }
        }
        return true;
    }

    const json &getData() const { return Data; }

private:
    ASTContext &Context;
    SourceManager &SM;
    const LangOptions &LangOpts;
    bool InsideTargetBlock = false;
    std::string CurrentFuncName;
    json Data = json::array();

    bool areLocationsAdjacent(SourceLocation PragmaLoc, SourceLocation BraceLoc) {
        SourceLocation PragmaEnd = Lexer::getLocForEndOfToken(PragmaLoc, 0, SM, LangOpts);
        bool Invalid = false;
        CharSourceRange Range = CharSourceRange::getCharRange(PragmaEnd, BraceLoc);
        StringRef Text = Lexer::getSourceText(Range, SM, LangOpts, &Invalid);
        if (Invalid) return false;
        return Text.trim().empty();
    }
};

class AutoMxPrecASTConsumer : public ASTConsumer {
public:
    AutoMxPrecASTConsumer(ASTContext &Context, const std::string &outputPath)
        : Visitor(Context), OutputPath(outputPath) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
        if (!OutputPath.empty()) {
            std::ofstream outFile(OutputPath);
            outFile << Visitor.getData().dump(2) << "\n";
        } else {
            llvm::outs() << Visitor.getData().dump(2) << "\n";
        }
    }

private:
    AutoMxPrecVisitor Visitor;
    std::string OutputPath;
};

class AutoMxPrecPluginAction : public PluginASTAction {
public:
    std::string OutputPath;

protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef) override {
        CI.getPreprocessor().AddPragmaHandler(new AutoMxPrecPragmaHandler());
        return std::make_unique<AutoMxPrecASTConsumer>(CI.getASTContext(), OutputPath);
    }

    bool ParseArgs(const CompilerInstance &, const std::vector<std::string> &args) override {
        if (!args.empty()) {
            OutputPath = args[0]; // 第一个参数作为输出路径
        }
        return true;
    }
};

}

static FrontendPluginRegistry::Add<AutoMxPrecPluginAction>
X("auto-mxprec-plugin", "Extract calls and variables from #pragma AutoMxPrec blocks");
