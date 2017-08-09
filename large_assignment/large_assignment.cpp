#include "../plugins_common.h"

#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/ASTContext.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <string>

#define DEFAULT_MAX_ALLOWED_ASSIGN_SIZE (1023)

using namespace clang;
using namespace clang::ast_matchers;
namespace {

    static void emit_warn(DiagnosticsEngine &diagEngine, SourceLocation loc, uint64_t allowed, uint64_t actual)
    {
        unsigned diagID = diagEngine.getCustomDiagID(
            diagEngine.getWarningsAsErrors()
            ? DiagnosticsEngine::Error
            : DiagnosticsEngine::Warning,
            "large assignment of %0 bytes is more than allowed %1 bytes");

        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        DB << std::to_string(actual) << std::to_string(allowed);
    }


    StatementMatcher literalAssignExpr = binaryOperator(hasOperatorName("=")).bind("assignment");

    class LargeAssignmentMatcher : public MatchFinder::MatchCallback
    {
    private:
        uint64_t m_max_allowed_size;
    public:
        LargeAssignmentMatcher(uint64_t max_allowed_size) : m_max_allowed_size(max_allowed_size) {}

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *context = Result.Context;
            DiagnosticsEngine &diagEngine = context->getDiagnostics();
            const BinaryOperator *binOp = Result.Nodes.getNodeAs<BinaryOperator>("assignment");
            const Expr *expr = binOp->getRHS();
            uint64_t actual = context->getTypeInfo(expr->getType()).Width / 8;
            if (m_max_allowed_size < actual) {
                emit_warn(diagEngine, expr->getLocStart(), m_max_allowed_size, actual);
            }
        }

    };

    class LargeAssignmentAction : public PluginASTAction {
        std::set<std::string> ParsedTemplates;
    protected:
        uint64_t m_max_allowed_size = DEFAULT_MAX_ALLOWED_ASSIGN_SIZE;
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &UNUSED(CI),
                                                       llvm::StringRef) override {

            MatchFinder *find = new MatchFinder();
            find->addMatcher(literalAssignExpr, new LargeAssignmentMatcher(m_max_allowed_size));

            return find->newASTConsumer();
        }

        bool ParseArgs(const CompilerInstance &UNUSED(CI),
                       const std::vector<std::string> &args) override {
            if (args.size() > 0) {
                m_max_allowed_size = std::stoul(args[0]);
            }
            return true;
        }
        void PrintHelp(llvm::raw_ostream& ros) {
            ros << "Help for switch_stmt plugin goes here\n";
        }

    };

}

static FrontendPluginRegistry::Add<LargeAssignmentAction>
X("large_assignment", "Disallows large struct assignments");
