#include "../plugins_common.h"

#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/ASTContext.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <string>

// #define TAGGED_UNION_DEBUG

#define PANIC(msg, args...) do { fprintf (stderr, "FATAL: at\n    %s:%d - " msg "\n", __FILE__, __LINE__, ## args); my_exit(-1);} while(0)
#define ASSERT(x) if(!(x)) { fprintf (stderr, "ASSERTION FAILED: at\n    %s:%d - %s\n", __FILE__, __LINE__, STR(x)); my_exit(-1);}
#define ASSERT_DYN_CAST(type, x) ({                                     \
            const type *_v_ = dyn_cast<type>(x);                        \
            ASSERT(_v_);                                                \
            _v_;                                                        \
        })
#define ASSERT_DYN_CAST_EXPR(context, type, expr) ({                    \
          const type *_v_ = dyn_cast<type>(expr->IgnoreParenImpCasts()); \
          if (!(_v_)) {                                                 \
              DUMP_AST(context, expr);                                  \
              ASSERT(_v_);                                              \
          }                                                             \
          _v_;                                                          \
      })
#define DUMP_AST(ctx, stmt_or_expr) do {                                \
      fprintf(stderr, "Dumping AST at: %s\n", stmt_or_expr->getLocStart().printToString(ctx.getSourceManager()).c_str()); \
      stmt_or_expr->dumpColor();                                        \
      stmt_or_expr->dumpPretty(ctx);                                    \
      fprintf(stderr, "\n");                                            \
  } while (0)

#ifdef TAGGED_UNION_DEBUG
#define DEBUG(fmt, args...) fprintf(stderr, "DEBUG " fmt "\n", ## args)
#define DEBUG_AST(ctx, x) DUMP_AST(ctx, x)
#else
#define DEBUG(fmt, args...)
#define DEBUG_AST(ctx, stmt_or_expr)
#endif

using namespace clang;
using namespace clang::ast_matchers;
namespace {

    static bool iequals(const std::string& a, const std::string& b)
    {
        unsigned int sz = a.size();
        if (b.size() != sz)
            return false;
        for (unsigned int i = 0; i < sz; ++i)
            if (tolower(a[i]) != tolower(b[i]))
                return false;
        return true;
    }

    static void my_exit(int a) __attribute__((noreturn));
    static void my_exit(int a)
    {
        llvm::outs().flush();
        llvm::errs().flush();
        exit(a);
    }

    static void emit_warn(DiagnosticsEngine &diagEngine, SourceLocation loc)
    {
        unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error,
                                                     "couldn't verify union tag in union member access.");
        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        // DB << expr.getNameAsString() << expr.getType().getAsString();
    }

    template <typename T> static const T *findPreviousSiblingStmt(const CompoundStmt *parent, const Stmt *node)
    {
        const T *res = nullptr;
        const Stmt *cur = node;
        bool found_node = false;
        for (auto cur = parent->body_rbegin(); cur != parent->body_rend(); cur++) {
            if (!found_node)
            {
                if (*cur == node) {
                    found_node = true;
                }
                continue;
            }
            res = dyn_cast<T>(*cur);
            if (res) break;
        }
        return res;
    }

    template <typename T> static const T *findAncestorStmt(
        ASTContext &context, const Stmt *node, bool traverse_compounds, const Stmt **out_last_child = nullptr)
    {
        const T *cs = nullptr;
        const Stmt *stmt = node;
        while (stmt) {
            const Stmt *prev_stmt = stmt;
            if (out_last_child) { *out_last_child = prev_stmt; }
            auto nodes = context.getParents(*prev_stmt);
            stmt = nullptr;
            // Multiple parents can only happen in C++ templates, AFAIK
            ASSERT(nodes.size() <= 1);
            if (nodes.size() == 1)
            {
                stmt = nodes[0].get<Stmt>();
                if (!stmt) {
                    // TODO this is horrible.
                    const Decl *decl = nodes[0].get<Decl>();
                    if (decl) {
                        auto nodes2 = context.getParents(*decl);
                        ASSERT(nodes2.size() <= 1);
                        if (nodes2.size() == 1) {
                            stmt = nodes2[0].get<Stmt>();
                        }
                    }
                }
            }
            if (!stmt) {
                DEBUG("No more ancestors!");
                return nullptr; /* No more ancestors! Reached function top-level */
            }

            DEBUG("Parent:");
            DEBUG_AST(context, stmt);
            DEBUG("-------");

            cs = dyn_cast<T>(stmt);
            if (cs) {
                return cs;
            }

            if (traverse_compounds) {
                const CompoundStmt *compoundStmt = dyn_cast<CompoundStmt>(stmt);
                if (compoundStmt) {
                    const T *prev = findPreviousSiblingStmt<T>(compoundStmt, prev_stmt);
                    if (prev) {
                        return prev;
                    }
                }
            }
        }
        PANIC("shouldn't reach here");
    }

    const RecordType *getSingleUnionField(const RecordDecl *rd)
    {
        const RecordType *unionType = nullptr;
        for (RecordDecl::field_iterator field = rd->field_begin();
             field != rd->field_end(); field++)
        {
            // TODO verify there's only a union and an enum tag here
            const QualType fieldQType = field->getType().getCanonicalType();
            // std::cout << "field type: " << fieldType.getAsString() << std::endl;
            const Type *fieldType = fieldQType.getTypePtr();
            ASSERT(fieldType);
            unionType = fieldType->getAsUnionType();
            if (unionType) break;
        }
        return unionType;
    }

    static const Expr *getTaggedUnionBaseOfSwitchCond(const std::string tag_name,
                                                      const Expr *expr,
                                                      const EnumDecl **tagEnum)
    {
        const MemberExpr *condMembExp = dyn_cast<MemberExpr>(expr->IgnoreParenImpCasts());
        if (!condMembExp) {
            DEBUG("The switch case condition is not a member access expression.");
            return nullptr;
        }
        const FieldDecl *fieldDecl = ASSERT_DYN_CAST(FieldDecl, condMembExp->getMemberDecl());

        if (0 != tag_name.compare(fieldDecl->getNameAsString())) {
            // wrong tag field
            // std::cout << "Wrong tag field: " << fieldDecl->getNameAsString() << ", expected: " << tag_name << std::endl;
            return nullptr;
        }

        const RecordDecl* rd = fieldDecl->getParent()->getDefinition();
        if (!rd->isStruct()) {
            return nullptr;
        }

        const RecordType *unionType = getSingleUnionField(rd);
        if (unionType == nullptr) return nullptr;


        const QualType enumQualType = fieldDecl->getType().getCanonicalType();
        DEBUG("enum type: %s", enumQualType.getAsString().c_str());
        const EnumType *enumType = ASSERT_DYN_CAST(EnumType, enumQualType);
        *tagEnum = enumType->getDecl();
        /* REVIEW: is this just a way to cast ? */
        return condMembExp->getBase();
    }

    // TODO cache
    static unsigned int getEnumDeclCommonPrefix(const EnumDecl *tagEnumDecl)
    {
        // Start by finding how long a common prefix needs to be removed
        /* REVIEW: this is expensive! */
        unsigned int index = 0;
        while (true) {
            char c = '\0';
            bool first = true;
            for (EnumDecl::enumerator_iterator enumerator = tagEnumDecl->enumerator_begin();
                 enumerator != tagEnumDecl->enumerator_end(); enumerator++)
            {
                const std::string enum_name = enumerator->getNameAsString();
                // TODO: Convert to clang error, the source code is invalid, has
                // one enum name that matches exactly the common prefix of all
                // others. (not a problem in the plugin itself)
                ASSERT(index < enum_name.length());
                if (first) {
                    c = enum_name[index];
                    first = false;
                    continue;
                }
                if (c != enum_name[index]) {
                    return index;
                }
            }
            index++;
        }
        ASSERT(false);
    }

    static bool findEnclosingTagSwitch(ASTContext &context,
                                       const std::string tag_name,
                                       const MemberExpr *unionMemberExpr,
                                       const FieldDecl *unionFieldAccessed,
                                       const Stmt *parentStart,
                                       const Stmt **nextParentStart)
    {
        const CaseStmt *cs = findAncestorStmt<CaseStmt>(context, parentStart, true);
        *nextParentStart = nullptr;
        if (cs == nullptr) {
            DEBUG("Failed to find case enclosing member expr.");
            return false;
        }
        const SwitchStmt *switch_stmt = findAncestorStmt<SwitchStmt>(context, cs, true);
        *nextParentStart = switch_stmt;
        if (switch_stmt == nullptr) {
            DEBUG("Failed to find switch enclosing case");
            return false;
        }

        const EnumDecl *tagEnumDecl;
        const Expr *taggedUnionBase = getTaggedUnionBaseOfSwitchCond(tag_name, switch_stmt->getCond(), &tagEnumDecl);
        if (taggedUnionBase == nullptr) {
            DEBUG("Failed to find taggedUnionBase");
            return false;
        }
        const DeclRefExpr *taggedUnionBaseDeclRef;
        const ArraySubscriptExpr *arrSubExpr = dyn_cast<ArraySubscriptExpr>(taggedUnionBase);
        if (arrSubExpr) {
            DUMP_AST(context, cs);
            PANIC("NOT IMPLEMENTED: Tagged union access of array subscript expression!");
        } else {
            taggedUnionBaseDeclRef = ASSERT_DYN_CAST_EXPR(context, DeclRefExpr, taggedUnionBase);
        }

        /**********************************************************************/

        const Expr *caseLHS = cs->getLHS();
        const DeclRefExpr *caseDeclRef = ASSERT_DYN_CAST_EXPR(context, DeclRefExpr, caseLHS);

        const ValueDecl *vd = caseDeclRef->getDecl();
        const QualType declType = vd->getType().getCanonicalType();
        const EnumConstantDecl *ecd = ASSERT_DYN_CAST(EnumConstantDecl, vd);

        // }
        // Check that the tag member expr in the switch condition is on the same
        // struct as the union field being accessed here.

        // i.e. in:
        //   switch (bla.tag) { case A: bla.foo; }
        // check that both 'bla' are the same expression

        // TODO do this outside this function
        const MemberExpr *enclosingUnion = ASSERT_DYN_CAST_EXPR(context, MemberExpr, unionMemberExpr->getBase());
        const DeclRefExpr *enclosingTaggedUnion = ASSERT_DYN_CAST_EXPR(context, DeclRefExpr, enclosingUnion->getBase());

        if (enclosingTaggedUnion->getDecl() != taggedUnionBaseDeclRef->getDecl()) {
            DEBUG("Failed to compare: enclosingTaggedUnion->getDecl() != taggedUnionBaseDeclRef->getDecl()");
            DEBUG_AST(context, enclosingTaggedUnion);
            DEBUG_AST(context, taggedUnionBaseDeclRef);
            // std::cout << "don't match" << std::endl;
            // unionMemberExpr->dumpColor();
            // taggedUnionBaseDeclRef->dumpColor();
            bool found_through_if = false;
            const Stmt *ifBranch;
            const IfStmt *if_stmt = findAncestorStmt<IfStmt>(context, unionMemberExpr, false, &ifBranch);
            if (if_stmt) {
                const bool isElseBranch = ifBranch == if_stmt->getElse();
                DEBUG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Found if");
                DEBUG_AST(context, if_stmt);
                const BinaryOperator *binOpExpr = dyn_cast<BinaryOperator>(if_stmt->getCond());
                const enum BinaryOperatorKind op = isElseBranch ? BO_NE : BO_EQ;
                DEBUG("branch type: %s", isElseBranch ? "else" : "then");
                if (binOpExpr && binOpExpr->getOpcode() == op) {
                    DEBUG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Found binop");
                    DEBUG_AST(context, binOpExpr);
                    MemberExpr *lhsExpr = dyn_cast<MemberExpr>(binOpExpr->getLHS()->IgnoreParenImpCasts());
                    MemberExpr *rhsExpr = dyn_cast<MemberExpr>(binOpExpr->getRHS()->IgnoreParenImpCasts());
                    if (lhsExpr && rhsExpr) {
                        const DeclRefExpr *lhsEnclosingTaggedUnion = dyn_cast<DeclRefExpr>(lhsExpr->getBase());
                        const DeclRefExpr *rhsEnclosingTaggedUnion = dyn_cast<DeclRefExpr>(rhsExpr->getBase());
                        if (lhsEnclosingTaggedUnion && rhsEnclosingTaggedUnion) {
                            if (((lhsEnclosingTaggedUnion->getDecl() == taggedUnionBaseDeclRef->getDecl())
                                 && (rhsEnclosingTaggedUnion->getDecl() == enclosingTaggedUnion->getDecl()))
                                || ((rhsEnclosingTaggedUnion->getDecl() == taggedUnionBaseDeclRef->getDecl())
                                    && (lhsEnclosingTaggedUnion->getDecl() == enclosingTaggedUnion->getDecl()))) {
                                DEBUG("Found through if!");
                                found_through_if = true;
                            }
                        }
                    }
                }
            }
            if (!found_through_if) {
                return false;
            }
        }


        // Check that FieldDecl is accessing the union field matching the enum
        // appearing in the "case:"
        const std::string lcFieldName = unionFieldAccessed->getNameAsString();
        const unsigned int common_prefix_length = getEnumDeclCommonPrefix(tagEnumDecl);
        const std::string ecdStr = ecd->getNameAsString();
        // Allow parts of the common prefix to be in the name (e.g. enum {
        // FOO_BAR, FOO_BAT } the field is allowed to be called 'bar' even
        // though 'b' is common)
        if (ecdStr.length() >= lcFieldName.length()) {
            const std::string lcECD = ecdStr.substr(std::min(common_prefix_length, (unsigned int)(ecdStr.length() - lcFieldName.length())));
            if (iequals(lcECD, lcFieldName)) {
                return true;
            }
        }
        *nextParentStart = nullptr;
        DEBUG("Failed to compare: enum name: %s to union member name: %s", ecdStr.c_str(), lcFieldName.c_str());
        return false;
    }

    static const std::string tagged_union_prefix = std::string("tagged_union__");

    static bool isMarkedAsTaggedUnion(const RecordDecl *tu, std::string &tag_name)
    {
        for (RecordDecl::field_iterator field = tu->field_begin();
             field != tu->field_end(); field++)
        {
            const std::string name = field->getNameAsString();
            if (0 != name.compare(0, tagged_union_prefix.length(), tagged_union_prefix)) {
                continue;
            }
            // std::cout << "Found tagged union: " << name << std::endl;

            const QualType fieldQType = field->getType().getCanonicalType();
            const Type *fieldType = fieldQType.getTypePtr();
            ASSERT(fieldType);
            const RecordType *structType = fieldType->getAsStructureType();

            // TODO throw clang error, the source code is using our tagged union prefix on something that isn't a tagged union!
            ASSERT(structType);

            const RecordDecl *recordDecl = structType->getDecl()->getDefinition();
            ASSERT(recordDecl);

            // TODO: throw clang error if not empty, source code using tagged union prefix in a weird way.
            ASSERT(recordDecl->field_empty());
            tag_name = name.substr(tagged_union_prefix.length());
            return true;
        }
        return false;
    }

    class TaggedUnionAccess : public MatchFinder::MatchCallback
    {
    public:
        TaggedUnionAccess() {}

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *context = Result.Context;
            DiagnosticsEngine &diagEngine = context->getDiagnostics();
            const MemberExpr *membExp = Result.Nodes.getNodeAs<MemberExpr>("member");
            // Some platforms have by default a completely broken implementation
            // of STL that manifests as getNodeAs above always returning
            // nullptr. Example: STL that ships with g++ 4.8.5
            ASSERT(membExp);
            if (!check(*context, membExp)) {
                emit_warn(diagEngine, membExp->getLocStart());
            }
        }

        bool check(ASTContext &context, const MemberExpr *membExp)
        {
            const FieldDecl *fieldDecl = ASSERT_DYN_CAST(FieldDecl, membExp->getMemberDecl());
            const RecordDecl* fieldUnion = fieldDecl->getParent()->getDefinition();
            if (!fieldUnion->isUnion()) return true;

            std::string tag_name;
            if (!isMarkedAsTaggedUnion(fieldUnion, tag_name)) {
                return true;
            }

            const Stmt *cur = membExp;
            const Stmt *next_parent;
            while (cur) {
                if (findEnclosingTagSwitch(context, tag_name, membExp, fieldDecl, cur, &next_parent)) {
                    return true;
                }
                cur = next_parent;
            }
            DEBUG("Failed to find enclosing tag switch");
            DEBUG_AST(context, membExp);
            return false;
        }
    };

    // TODO: match only union member accesses
    StatementMatcher memberExprMatcher = memberExpr().bind("member");

    class PrintFunctionNamesAction : public PluginASTAction {
        std::set<std::string> ParsedTemplates;
    protected:
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &UNUSED(CI),
                                                       llvm::StringRef) override {

            MatchFinder *find = new MatchFinder();
            find->addMatcher(memberExprMatcher, new TaggedUnionAccess());

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

static FrontendPluginRegistry::Add<PrintFunctionNamesAction>
X("tagged_union", "asdfasdfa");
