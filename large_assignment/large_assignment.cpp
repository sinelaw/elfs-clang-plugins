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

    static void emit_warn(DiagnosticsEngine &diagEngine, SourceLocation loc, uint64_t allowed, uint64_t actual, const Expr *expr)
    {
        unsigned diagID = diagEngine.getCustomDiagID(
            diagEngine.getWarningsAsErrors()
            ? DiagnosticsEngine::Error
            : DiagnosticsEngine::Warning,
            "large assignment of %0 bytes is more than allowed %1 bytes (assigned type: %2)");

        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        DB << std::to_string(actual) << std::to_string(allowed) << expr->getType().getAsString();
    }


    StatementMatcher literalAssignExpr = binaryOperator(hasOperatorName("=")).bind("assignment");
    StatementMatcher callMatcher = callExpr().bind("call");

    class LargeAssignmentMatcher : public MatchFinder::MatchCallback
    {
    private:
        uint64_t m_max_allowed_size;
    public:
        LargeAssignmentMatcher(uint64_t max_allowed_size) : m_max_allowed_size(max_allowed_size) {}

        void check(ASTContext *context, const Expr *expr)
        {
            DiagnosticsEngine &diagEngine = context->getDiagnostics();
            const uint64_t actual = context->getTypeInfo(expr->getType()).Width / 8;
            if (m_max_allowed_size < actual) {
                emit_warn(diagEngine, expr->getLocStart(), m_max_allowed_size, actual, expr);
            }
        }

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *context = Result.Context;
            const BinaryOperator *binOp = Result.Nodes.getNodeAs<BinaryOperator>("assignment");
            if (binOp) {
                check(context, binOp->getRHS());
            }
            const CallExpr *callExp = Result.Nodes.getNodeAs<CallExpr>("call");
            if (callExp) {
                for (auto arg : callExp->arguments()) {
                    if (arg->getType().getTypePtr()->isArrayType()) continue;
                    check(context, arg);
                }
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
            auto m = new LargeAssignmentMatcher(m_max_allowed_size);
            find->addMatcher(literalAssignExpr, m);
            find->addMatcher(callMatcher, m);

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
