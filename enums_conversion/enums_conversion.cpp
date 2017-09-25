#include "../plugins_common.h"

#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/ASTContext.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;
namespace {

    /* TODO:
     * 1. Make this work on implicit casts not just in call locations but also in
     *    assignemnts and comparisons
     * 2. Improve handling of binary ops. Instead of TWO_LEVEL_BIN_OP we can write a recursive
     *    function that checka tree of binary, conditional and assignment operations to make sure
     *    all of them are enum values / variables of the same type.
     */

    static void emit_conversion_warning(DiagnosticsEngine &diagEngine, SourceLocation loc, std::string enumDeclName)
    {
        unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error,
                                                     "enum conversion to or from enum %0");
        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        DB << enumDeclName;
    }

    static void emit_case_warning(DiagnosticsEngine &diagEngine, SourceLocation loc, std::string switchDeclName, std::string caseDeclName, std::string desc)
    {
        unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error,
                                                     "enum conversion in switch case: expected %0, but got %1 (%2)");
        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        DB << switchDeclName;
        DB << caseDeclName;
        DB << desc;
    }

    class EnumConversion : public MatchFinder::MatchCallback
    {
    public:
        EnumConversion() {}

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *context = Result.Context;
            DiagnosticsEngine &diagEngine = context->getDiagnostics();
            const Expr *expr = Result.Nodes.getNodeAs<Expr>("problem");
            const EnumDecl *enum_decl = Result.Nodes.getNodeAs<EnumDecl>("enumDecl");
            emit_conversion_warning(diagEngine, expr->getLocStart(), enum_decl ? enum_decl->getNameAsString() : "<unknown>");
        }

    };

#define BIN_OP(op, x, y) \
    ignoringParenImpCasts(binaryOperator(hasOperatorName(op), hasLHS(x), hasRHS(y)))

// horrid, I know.
#define TWO_LEVEL_BIN_OP(op, x)                                         \
    anyOf(                                                              \
        BIN_OP(op, x, x),                                               \
        BIN_OP(op, x, BIN_OP(op, x, x)),                                \
        BIN_OP(op, BIN_OP(op, x, x), x),                                \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x),                 \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x), x),  \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x), x), x), \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x), x), x), x), \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x), x), x), x), x), \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x), x), x), x), x), x), \
        BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, BIN_OP(op, x, x), x), x), x), x), x), x), x), x) \
        )

    StatementMatcher baseEnumTypedExpr =
        expr(
            ignoringParenImpCasts(
                anyOf(
                    expr(hasType(qualType(hasCanonicalType(qualType(hasDeclaration(enumDecl().bind("enumDecl"))))))),
                    declRefExpr(hasDeclaration(enumConstantDecl().bind("enumDecl")))
                    )
                )
            );

    StatementMatcher secondEnumTypedExpr =
        expr(ignoringParenImpCasts(anyOf(
            baseEnumTypedExpr,
            TWO_LEVEL_BIN_OP("|", baseEnumTypedExpr),
            BIN_OP("&", baseEnumTypedExpr, baseEnumTypedExpr),
            BIN_OP("&", baseEnumTypedExpr, expr(hasType(qualType(isInteger())))))));

    StatementMatcher enumTypedExpr =
        expr(ignoringParenImpCasts(anyOf(
            secondEnumTypedExpr,
            conditionalOperator(
                hasFalseExpression(
                    ignoringParenImpCasts(anyOf(secondEnumTypedExpr,
                                                conditionalOperator(
                                                    hasFalseExpression(secondEnumTypedExpr),
                                                    hasTrueExpression(secondEnumTypedExpr))))),
                hasTrueExpression(
                    ignoringParenImpCasts(anyOf(secondEnumTypedExpr,
                                                conditionalOperator(
                                                    hasFalseExpression(secondEnumTypedExpr),
                                                    hasTrueExpression(secondEnumTypedExpr)))))
                // This didn't work as expected:
                // hasFalseExpression(enumTypedExpr),
                // hasTrueExpression(enumTypedExpr))
                ))));

    StatementMatcher enumNotValidForBool =
        expr(enumTypedExpr,
             unless(BIN_OP("&", baseEnumTypedExpr, baseEnumTypedExpr)),
             unless(BIN_OP("&", baseEnumTypedExpr, expr(hasType(qualType(isInteger()))))));

    StatementMatcher fromEnumImplicit =
        implicitCastExpr(
            hasSourceExpression(ignoringParenImpCasts(enumNotValidForBool)),
            unless(hasImplicitDestinationType(qualType(hasCanonicalType(hasDeclaration(namedDecl(enumDecl().bind("enumDecl"))))).bind("qt"))),
            hasImplicitDestinationType(isInteger()));

    StatementMatcher enumConversionMatcher =
        expr(anyOf(
                 // non-enum -> enum
                 implicitCastExpr(
                     anyOf(hasParent(callExpr()),
                           hasParent(returnStmt()),
                           hasParent(binaryOperator(hasOperatorName("="))),
                           hasParent(varDecl())),
                     hasSourceExpression(expr(hasType(qualType(isInteger()).bind("t")),
                                              unless(enumTypedExpr)
                                             )),
                     hasImplicitDestinationType(qualType(hasCanonicalType(hasDeclaration(namedDecl(enumDecl().bind("enumDecl"))))).bind("qt"))),
                 // enum -> non-enum
                 expr(
                     fromEnumImplicit,
                     anyOf(hasParent(callExpr(unless(callee(functionDecl(hasAttr(attr::Format)))))),
                           hasParent(returnStmt()),
                           hasParent(binaryOperator(hasOperatorName("="))),
                           hasParent(varDecl()))),
                 // ==/!=
                 binaryOperator(
                     anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                     hasRHS(anyOf(expr(ignoringParenImpCasts(declRefExpr(hasDeclaration(enumConstantDecl(hasDeclContext(decl(enumDecl().bind("enumDecl")).bind("decl_enum"))))))),
                                  expr(ignoringParenImpCasts( hasType(qualType(hasCanonicalType(qualType(hasDeclaration(decl(enumDecl().bind("enumDecl")).bind("decl_enum")))))))))),
                     unless(hasLHS(anyOf(
                                       expr(ignoringParenImpCasts(declRefExpr(hasDeclaration(enumConstantDecl(hasDeclContext(enumDecl(equalsBoundNode("decl_enum")))))))),
                                       expr(ignoringParenImpCasts(hasType(qualType(hasCanonicalType(qualType(hasDeclaration(decl(enumDecl(equalsBoundNode("decl_enum")))))))))))))
                     ),
                 binaryOperator(
                     anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                     hasLHS(anyOf(expr(ignoringParenImpCasts(declRefExpr(hasDeclaration(enumConstantDecl(hasDeclContext(decl(enumDecl().bind("enumDecl")).bind("decl_enum"))))))),
                                  expr(ignoringParenImpCasts( hasType(qualType(hasCanonicalType(qualType(hasDeclaration(decl(enumDecl().bind("enumDecl")).bind("decl_enum")))))))))),
                     unless(hasRHS(anyOf(
                                       expr(ignoringParenImpCasts(declRefExpr(hasDeclaration(enumConstantDecl(hasDeclContext(enumDecl(equalsBoundNode("decl_enum")))))))),
                                       expr(ignoringParenImpCasts(hasType(qualType(hasCanonicalType(qualType(hasDeclaration(decl(enumDecl(equalsBoundNode("decl_enum")))))))))))))
                     )
                 ),
             unless(isExpansionInSystemHeader()))
        .bind("problem");

    StatementMatcher enumUsedAsBoolMatcher =
        anyOf(
            conditionalOperator(hasCondition(ignoringParenImpCasts(expr(enumNotValidForBool).bind("problem")))),
            ifStmt(hasCondition(ignoringParenImpCasts(expr(enumNotValidForBool).bind("problem")))),
            forStmt(hasCondition(ignoringParenImpCasts(expr(enumNotValidForBool).bind("problem")))),
            whileStmt(hasCondition(ignoringParenImpCasts(expr(enumNotValidForBool).bind("problem")))),
            doStmt(hasCondition(ignoringParenImpCasts(expr(enumNotValidForBool).bind("problem")))),
            unaryOperator(hasOperatorName("!"), hasUnaryOperand(ignoringParenImpCasts(expr(enumNotValidForBool).bind("problem"))))
            );

    StatementMatcher problemMatcher = anyOf(enumConversionMatcher, enumUsedAsBoolMatcher);
    StatementMatcher switchCaseStmtMatcher = switchStmt().bind("switch");
//             unless(isExpansionInSystemHeader()));

    class BadSwitchCaseFinder : public MatchFinder::MatchCallback
    {
    public:
        BadSwitchCaseFinder() {}

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *const context = Result.Context;
            const SwitchStmt *const stmt = Result.Nodes.getNodeAs<SwitchStmt>("switch");
            const QualType qualType = stmt->getCond()->IgnoreParenImpCasts()->getType().getCanonicalType();
            const Type *const condType = qualType.getTypePtr();
            const EnumType *const enumType = dyn_cast<EnumType>(condType);
            if (!enumType) {
                return;
            }
            check(context, enumType, stmt);
        }

        void check(ASTContext *context, const EnumType *enumType, const SwitchStmt *stmt)
        {
            for (const SwitchCase *switchCase = stmt->getSwitchCaseList();
                 switchCase != nullptr;
                 switchCase = switchCase->getNextSwitchCase())
            {
                const CaseStmt *caseStmt = dyn_cast<CaseStmt>(switchCase);
                if (!caseStmt) {
                    continue;
                    // if (dyn_cast<DefaultStmt>(switchCase) != nullptr) {
                    //     continue;
                    // } else {
                    //     return false;
                    // }
                }
                DiagnosticsEngine &diagEngine = context->getDiagnostics();
                const Expr *lhsExpr = caseStmt->getLHS()->IgnoreParenImpCasts();
                const DeclRefExpr *caseExpr = dyn_cast<DeclRefExpr>(lhsExpr);
                if (!caseExpr) {
                    emit_case_warning(diagEngine, caseStmt->getLocStart(),
                                      enumType->getDecl()->getNameAsString(),
                                      lhsExpr->getType().getAsString(),
                                      "not a declared constant");
                } else {
                    const EnumConstantDecl *enumConstDecl = dyn_cast<EnumConstantDecl>(caseExpr->getFoundDecl());
                    if (!enumConstDecl) {
                        emit_case_warning(diagEngine, caseStmt->getLocStart(),
                                          enumType->getDecl()->getNameAsString(),
                                          caseExpr->getFoundDecl()->getNameAsString(),
                                          "not an enum constant");
                        continue;
                    }
                    const EnumDecl *enumCaseDecl = dyn_cast<EnumDecl>(enumConstDecl->getDeclContext());
                    if (!enumCaseDecl) {
                        emit_case_warning(diagEngine, caseStmt->getLocStart(),
                                          enumType->getDecl()->getNameAsString(),
                                          caseExpr->getFoundDecl()->getNameAsString(),
                                          "wrong type - not an enum?");
                    } else if (!enumCaseDecl->Equals(enumType->getDecl())) {
                        emit_case_warning(diagEngine, caseStmt->getLocStart(),
                                          enumType->getDecl()->getNameAsString(),
                                          enumCaseDecl->getNameAsString(),
                                          "wrong enum type");
                    }
                }
            }
        }
    };

    class CheckEnumConversionAction : public PluginASTAction {
        std::set<std::string> ParsedTemplates;
    protected:
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &UNUSED(CI),
                                                       llvm::StringRef) override {

            MatchFinder *find = new MatchFinder();
            find->addMatcher(problemMatcher, new EnumConversion());
            find->addMatcher(switchCaseStmtMatcher, new BadSwitchCaseFinder());

            return find->newASTConsumer();
        }

        bool ParseArgs(const CompilerInstance &UNUSED(CI),
                       const std::vector<std::string> &UNUSED(args)) override {
            // ASSERT(0 == args.size());
            return true;
        }
        void PrintHelp(llvm::raw_ostream& ros) {
            ros << "Help for switch_stmt plugin goes here\n";
        }

    };

}

static FrontendPluginRegistry::Add<CheckEnumConversionAction>
X("enums_conversion", "asdfasdfa");
