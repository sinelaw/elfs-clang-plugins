#include "private_api.h"
#include "../plugins_common.h"

#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/ASTContext.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <set>
#include <iostream>
#include <string>

#define STR(s) _STR(s)
#define _STR(s) #s

// TODO accept as parameter
#define MAX_ALLOWED_ASSIGN_SIZE (64)
#define ASSERT(x, msg) do {                                             \
        if (!x) {                                                       \
            std::cout << "ASSERTION FAILED: " << msg << " " << #x << std::endl; \
            exit(1);                                                    \
        }                                                               \
    } while (0)

// #define ON_DEBUG(x)
#define ON_DEBUG(x) x

using namespace clang;
using namespace clang::ast_matchers;
namespace {

    static void emit_warn(DiagnosticsEngine &diagEngine, SourceLocation loc, std::string private_type, SourceLocation privateLocation)
                          // std::string expected_prefix, std::string actual_prefix)
    {
        unsigned diagID = diagEngine.getCustomDiagID(
            diagEngine.getWarningsAsErrors()
            ? DiagnosticsEngine::Error
            : DiagnosticsEngine::Warning,
            "access to private member of %0");
        unsigned noteDiagID = diagEngine.getCustomDiagID(
            DiagnosticsEngine::Note,
            "declaration of %0");

        {
            DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
            DB << private_type;
        }
        {
            DiagnosticBuilder DB2 = diagEngine.Report(privateLocation, noteDiagID);
            DB2 << private_type;
        }

    }

    static std::string trim_suffix(std::string name, std::string suffix)
    {
        if (name.length() >= suffix.length()
            && (name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0))
        {
            return name.substr(0, name.length() - suffix.length());
        }
        return name;
    }

    static std::string trim_prefix(std::string name, std::string trimmed_prefix)
    {
        if (name.compare(0, trimmed_prefix.length(), trimmed_prefix) == 0) {
            return name.substr(trimmed_prefix.length(), name.length());
        }
        return name;
    }

    static std::vector<std::string> split(const std::string &text, char sep) {
        std::vector<std::string> tokens;
        std::size_t start = 0;
        while (true) {
            const std::size_t end = text.find(sep, start);
            if (end == std::string::npos) break;
            tokens.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        tokens.push_back(text.substr(start));
        return tokens;
    }

    static std::string trim_filename(std::string name)
    {
        std::vector<std::string> parts = split(name, '/');
        std::string res = "", prev = "";
        uint32_t idx = 0;
        for (auto it = parts.begin(); it != parts.end(); it++) {
            idx++;
            if (*it == ".") continue;
            if (*it == "..") {
                res = prev;
                continue;
            }
            prev = res;
            res += *it + (idx < parts.size() ? "/" : "");
        }
        return res;
        // return trim_prefix(name, "./");
    }

    static std::string get_filename(SourceManager *source_mgr, SourceLocation loc)
    {
        if (!loc.isValid()) return "";
        std::string filename = source_mgr->getFilename(loc);
        return trim_filename(filename);
    }

    static std::string getFilenameFromDecl(SourceManager *source_mgr, const Decl *decl)
    {
        std::string filename = get_filename(source_mgr, decl->getLocation());
        if (filename.length() == 0) {
            /* For some weird reason if the declaration is
             * behind a macro expansion sometimes it returns an
             * empty filename above */
            filename = get_filename(source_mgr, decl->getLocStart());
        }
        if (filename.length() == 0) {
            filename = get_filename(source_mgr, decl->getLocEnd());
        }
        return filename;
    }

    /* REVIEW: isExpansionInMainFile might hide access in one h file to another h file. */
    StatementMatcher memberExprMatcher = memberExpr(isExpansionInMainFile()).bind("memberExpr");
    DeclarationMatcher friendVarDeclMatcher = varDecl(matchesName(STR(PRIVATE_VAR_PREFIX))).bind("friendVarDecl");

    class PrivateMatchCallback : public MatchFinder::MatchCallback
    {
        SourceManager &m_src_mgr;
        std::string m_filename;
        std::vector<std::pair<SourceLocation, const TagDecl *> > *m_problems;
        std::vector<const TagDecl *> *m_friends;
        bool m_done;
        DiagnosticsEngine *m_diag;
    public:
        PrivateMatchCallback(SourceManager &src_mgr, std::string filename,
                             std::vector<std::pair<SourceLocation, const TagDecl *> > *problems,
                             std::vector<const TagDecl *> *friends)
            : m_src_mgr(src_mgr), m_filename(filename), m_problems(problems), m_friends(friends)
        {
            m_done = false;
            m_diag = nullptr;
        }

        void handle_memberExpr(ASTContext *context, const MemberExpr *expr)
        {
            if (expr->getExprLoc().isMacroID()) return;
            const FieldDecl *member_decl = dyn_cast<FieldDecl>(expr->getMemberDecl());
            std::string member_decl_filename = getFilenameFromDecl(&context->getSourceManager(), member_decl);
            /* REVIEW: you can use auto more */
            auto parts = split(member_decl_filename, '_');
            if (parts.back() == "private.h") {
                std::string prefix = trim_suffix(m_filename, ".c");
                std::string actual_prefix = trim_suffix(member_decl_filename, "_private.h");
                if (prefix != actual_prefix) {
                    const RecordDecl *record_decl = member_decl->getParent();
                    m_problems->push_back(std::pair<SourceLocation, const TagDecl *>(expr->getMemberLoc(), record_decl));
                }
            }
        }

        void handle_friendVarDecl(const VarDecl *friendVarDecl)
        {
            const PointerType *friendTypePtr = dyn_cast<const PointerType>(friendVarDecl->getType().getTypePtr());
            const Type *friendType = friendTypePtr->getPointeeType().getTypePtr();
            const TagDecl *friendTagDecl = friendType->getAsTagDecl();
            m_friends->push_back(friendTagDecl);
            ON_DEBUG(std::cout << "In '" << m_filename << "', declared friend: " << friendTagDecl->getNameAsString() << std::endl);
        }

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *context = Result.Context;
            DiagnosticsEngine &diagEngine = context->getDiagnostics();
            m_diag = &diagEngine;
            const MemberExpr *expr = Result.Nodes.getNodeAs<MemberExpr>("memberExpr");
            if (expr) {
                handle_memberExpr(context, expr);
                return;
            }
            const VarDecl *friendVarDecl = Result.Nodes.getNodeAs<VarDecl>("friendVarDecl");
            if (friendVarDecl) {
                handle_friendVarDecl(friendVarDecl);
                return;
            }
            ASSERT(false, "Didn't match anything");
        }

        virtual void onEndOfTranslationUnit() {
            /* this funciton may be run several times */
            if (m_done) return;
            m_done = true;

            for (auto problem : *m_problems) {
                bool is_friend = false;
                for (auto _friend : *m_friends) {
                    if (problem.second == _friend) {
                        is_friend = true;
                        break;
                    }
                }
                if (!is_friend) {
                    emit_warn(*m_diag, problem.first, problem.second->getNameAsString(), problem.second->getLocation());
                }
            }
        }
    };

    class PrivateAction : public PluginASTAction {
    public:
        PrivateAction() {
        }

    protected:
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                       llvm::StringRef filename) override
        {
            MatchFinder *find = new MatchFinder();

            auto *problems = new std::vector<std::pair<SourceLocation, const TagDecl *> >();
            auto *friends = new std::vector<const TagDecl *>();

            PrivateMatchCallback *cb = new PrivateMatchCallback(CI.getSourceManager(), filename, problems, friends);
            find->addMatcher(memberExprMatcher, cb);
            find->addMatcher(friendVarDeclMatcher, cb);
            return find->newASTConsumer();
        }

        bool ParseArgs(const CompilerInstance &UNUSED(CI),
                       const std::vector<std::string> &UNUSED(args)) override
        {
            return true;
        }

        void PrintHelp(llvm::raw_ostream& ros) {
            ros << "Help for switch_stmt plugin goes here\n";
        }

    };

}


static FrontendPluginRegistry::Add<PrivateAction> X("private", "");
