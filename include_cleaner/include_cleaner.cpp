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
#include <cinttypes>

// TODO accept as parameter
#define MAX_ALLOWED_ASSIGN_SIZE (64)

#define ON_DEBUG(x)
//#define ON_DEBUG(x) x

using namespace clang;
using namespace clang::ast_matchers;
namespace {

    static void emit_redundant_allowed_warn(DiagnosticsEngine &diagEngine, SourceLocation loc, std::string filename)
    {
        unsigned diagID = diagEngine.getCustomDiagID(
            diagEngine.getWarningsAsErrors()
            ? DiagnosticsEngine::Error
            : DiagnosticsEngine::Warning,
            "include cleaner: #include marked as allowed, but is used directly: '%0'");
        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        DB << filename;
    }

    static void emit_unused_include_warn(DiagnosticsEngine &diagEngine, SourceLocation loc, std::string filename)
    {
        unsigned diagID = diagEngine.getCustomDiagID(
            diagEngine.getWarningsAsErrors()
            ? DiagnosticsEngine::Error
            : DiagnosticsEngine::Warning,
            "include cleaner: unused #include of '%0'");
        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
        DB << filename;
    }

    static llvm::Optional<std::string> trim_suffix(std::string name, std::string suffix)
    {
        if (name.length() >= suffix.length()
            && (name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0))
        {
            return { name.substr(0, name.length() - suffix.length()) };
        }
        return llvm::Optional<std::string>();
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
        for (auto part : parts) {
            idx++;
            if (part == ".") continue;
            if (part == "..") {
                res = prev;
                continue;
            }
            prev = res;
            res += part + (idx < parts.size() ? "/" : "");
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

    StatementMatcher memberExprMatcher = memberExpr(isExpansionInMainFile()).bind("memberExpr");
    StatementMatcher declInOtherFile = declRefExpr(isExpansionInMainFile(), unless(hasDeclaration(isExpansionInMainFile()))).bind("declRef");
    TypeLocMatcher typesInMainFile = typeLoc(isExpansionInMainFile()).bind("typeLoc");
    DeclarationMatcher definitionsInMainFile = valueDecl(isExpansionInMainFile(), hasDeclContext(unless(isExpansionInMainFile()))).bind("decl");
    DeclarationMatcher declarationsInOtherFiles = namedDecl(unless(isExpansionInMainFile())).bind("declOther");

    class IncludeCleanerMatchCallback : public MatchFinder::MatchCallback
    {
    private:
        std::map<std::string, int> *m_include_usages_count;
        std::set<std::string> *m_includes_marked_as_allowed;
        std::map<std::string, SourceLocation> *m_include_locations;
        std::set<std::string> m_files_whitelist;
        std::set<std::string> m_definitions;
        std::set<std::string> m_usages;
        std::map<std::string, std::deque<std::string>* > m_declarations;
        std::set<std::string> m_tag_definitions;
        std::map<std::string, std::deque<std::string>* > m_tag_declarations;
        std::map<std::string, std::deque<std::string>* > m_extern_declarations;
        bool m_done;
        DiagnosticsEngine *m_diag;

    public:
        IncludeCleanerMatchCallback(std::map<std::string, int> *include_usages_count,
                                    std::set<std::string> *includes_marked_as_allowed,
                                    std::map<std::string, SourceLocation> *include_locations,
                                    std::set<std::string> *files_whitelist)
        {
            m_done = false;
            m_include_usages_count = include_usages_count;
            m_includes_marked_as_allowed = includes_marked_as_allowed;
            m_include_locations = include_locations;
            m_files_whitelist = *files_whitelist;
        }

        void handle_source_location(std::string filename)
        {
            ON_DEBUG(std::cerr << "Found usage of: " << filename << std::endl);
            if (m_include_usages_count->count(filename) == 0) {
                (*m_include_usages_count)[filename] = 1;
            } else {
                (*m_include_usages_count)[filename] += 1;
            }
        }

        static void handle_declaration(std::map<std::string, std::deque<std::string>* > *ns,
                                       std::string name, std::string filename)
        {
            if (ns->count(name) == 0) {
                (*ns)[name] = new std::deque<std::string>();
            }
            (*ns)[name]->push_back(filename);
        }

        void handle_tag_usage(ASTContext *context, const TagDecl *tag_decl)
        {
            const std::string name = tag_decl->getTypeForDecl()->getCanonicalTypeInternal().getUnqualifiedType().getAsString(); // tag_decl->getNameAsString();
            SourceLocation loc = tag_decl->getLocation();
            if (loc.isValid()) {
                std::string filename = get_filename(&context->getSourceManager(), loc);
                ON_DEBUG(std::cerr << "Found type usage: " << name << " from " << filename << std::endl);
                m_tag_definitions.insert(name);
            }
        }

        virtual void run(const MatchFinder::MatchResult &Result)
        {
            ASTContext *context = Result.Context;
            DiagnosticsEngine &diagEngine = context->getDiagnostics();
            m_diag = &diagEngine;
            {
                const MemberExpr *expr = Result.Nodes.getNodeAs<MemberExpr>("memberExpr");
                if (expr) {
                    const FieldDecl *member_decl = dyn_cast<FieldDecl>(expr->getMemberDecl());
                    const RecordDecl *parent_decl = member_decl->getParent();
                    const Type *_type = parent_decl->getTypeForDecl();
                    const std::string name = QualType::getAsString(_type, Qualifiers());
                    ON_DEBUG(std::cerr << "Type Usage via member access: " << name << std::endl);
                    m_tag_definitions.insert(name);
                }
            }
            {
                const DeclRefExpr *expr = Result.Nodes.getNodeAs<DeclRefExpr>("declRef");
                if (expr) {
                    const ValueDecl *value_decl = expr->getDecl();
                    if (value_decl->getLocation().isValid()) {
                        const std::string name = expr->getNameInfo().getAsString();
                        ON_DEBUG(std::cerr << "Usage: " << name << std::endl);
                        m_usages.insert(name);
                    }
                }
            }
            {
                const ValueDecl *decl = Result.Nodes.getNodeAs<ValueDecl>("decl");
                if (decl) {
                    const std::string name = decl->getNameAsString();
                    ON_DEBUG(std::cerr << "Definition: " << name << std::endl);
                    m_definitions.insert(name);
                    return;
                }
            }
            {
                const NamedDecl *decl = Result.Nodes.getNodeAs<NamedDecl>("declOther");
                if (decl) {
                    if (decl->getLocation().isValid()) {
                        std::string filename = getFilenameFromDecl(&context->getSourceManager(), decl);
                        const TypedefNameDecl *_typedef = dyn_cast<TypedefNameDecl>(decl);
                        const TypeDecl *typeDecl = dyn_cast<TypeDecl>(decl);
                        const VarDecl *as_var = dyn_cast<VarDecl>(decl);
                        const FunctionDecl *as_func = dyn_cast<FunctionDecl>(decl);
                        const EnumConstantDecl *as_enum_const = dyn_cast<EnumConstantDecl>(decl);
                        if (_typedef) {
                            const std::string name = _typedef->getNameAsString();
                            ON_DEBUG(std::cerr << "Typedef Declaration: " << name << " in " << filename << std::endl);
                            handle_declaration(&m_tag_declarations, name, filename);
                        } else if (typeDecl) {
                            const Type *_type = typeDecl->getTypeForDecl();
                            const std::string name = QualType::getAsString(_type, Qualifiers());
                            ON_DEBUG(std::cerr << "Type Declaration: " << name << " in " << filename << std::endl);
                            handle_declaration(&m_tag_declarations, name, filename);
                        } else if (as_var) {
                            const std::string name = decl->getNameAsString();
                            if (as_var->hasExternalStorage()) {
                                ON_DEBUG(std::cerr << "Var Declaration (extern): " << name << std::endl);
                                handle_declaration(&m_extern_declarations, name, filename);
                            }
                            ON_DEBUG(std::cerr << "Var declaration: " << name << std::endl);
                            handle_declaration(&m_declarations, name, filename);
                        } else if (as_func) {
                            const std::string name = decl->getNameAsString();
                            // TODO: Add to extern_decl only if the function is extern.
                            // Currently this is disabled because we have sometimes:
                            // foo_internal.h
                            // foo.h
                            // foo.c -> defines some things from foo_internal.h
                            // if (as_func->getStorageClass() == clang::StorageClass::SC_Extern) {
                            //     ON_DEBUG(std::cerr << "Func Declaration (extern): " << name << std::endl);
                            handle_declaration(&m_extern_declarations, name, filename);
                            // }
                            ON_DEBUG(std::cerr << "Func Declaration: " << name << std::endl);
                            handle_declaration(&m_declarations, name, filename);
                        } else if (as_enum_const) {
                            const std::string name = decl->getNameAsString();
                            ON_DEBUG(std::cerr << "Enum Declaration: " << name << std::endl);
                            handle_declaration(&m_declarations, name, filename);
                        }
                        return;
                    }
                }
            }
            {
                const TypeLoc *type_loc = Result.Nodes.getNodeAs<TypeLoc>("typeLoc");
                if (type_loc) {
                    const Type *type = type_loc->getType().getTypePtr();
                    const TagDecl *tag_decl = type->getAsTagDecl();
                    if (tag_decl) {
                        handle_tag_usage(context, tag_decl);
                    }
                    const TypedefType *typedef_type = type->getAs<TypedefType>();
                    if (typedef_type) {
                        const TypedefNameDecl *_typedef = typedef_type->getDecl();
                        const std::string name = _typedef->getNameAsString();
                        SourceLocation loc = _typedef->getLocation();
                        if (loc.isValid()) {
                            std::string filename = get_filename(&context->getSourceManager(), loc);
                            ON_DEBUG(std::cerr << "Found Typedef usage: " << name << " from " << filename << std::endl);
                            m_tag_definitions.insert(name);
                        }
                        const TagDecl *other_tag_decl = _typedef->getAnonDeclWithTypedefName(true);
                        if (other_tag_decl) {
                            handle_tag_usage(context, other_tag_decl);
                        }
                    }
                }
            }
        }

        virtual void onStartOfTranslationUnit() {
        }

        void process_deps(
            std::set<std::string> definitions,
            std::map<std::string, std::deque<std::string>* > declarations)
        {
            for (auto it : definitions) {
                if (declarations.count(it) > 0) {
                    ON_DEBUG(std::cerr << "Found " << it << std::endl);
                    for (auto pos_it : *declarations[it]) {
                        handle_source_location(pos_it);
                    }
                }
            }
        }

        virtual void onEndOfTranslationUnit() {
            /* this funciton may be run several times */
            if (m_done) return;
            m_done = true;
            process_deps(m_usages, m_declarations);
            process_deps(m_definitions, m_extern_declarations);
            process_deps(m_tag_definitions, m_tag_declarations);

            // if a _private.h was used, allow include the _api.h
            std::vector<std::string> apis_allowed_because_of_privates;
            for (auto it : *m_include_usages_count) {
                auto filename = it.first;
                auto trimmed = trim_suffix(filename, "_private.h");
                if (!trimmed.hasValue()) continue;
                if (m_include_usages_count->count(filename) == 0) continue;
                if ((*m_include_usages_count)[filename] == 0) continue;
                apis_allowed_because_of_privates.push_back(trimmed.getValue() + "_api.h");
            }
            for (auto api : apis_allowed_because_of_privates) {
                if (m_include_usages_count->count(api) == 0) {
                    (*m_include_usages_count)[api] = 1;
                } else {
                    (*m_include_usages_count)[api] += 1;
                }
            }

            // verify no unused includes left
            for (auto it : *m_include_usages_count) {
                auto filename = it.first;
                auto count = it.second;
                const bool marked_as_allowed =
                    m_includes_marked_as_allowed->end() != m_includes_marked_as_allowed->find(filename);
                if (count == 0) {
                    if (marked_as_allowed) continue;
                    if (m_files_whitelist.count(filename) > 0) continue;
                    emit_unused_include_warn(*m_diag, (*m_include_locations)[filename], filename);
                } else {
                    if (marked_as_allowed) {
                        emit_redundant_allowed_warn(*m_diag, (*m_include_locations)[filename], filename);
                    }
                }
                ON_DEBUG(std::cerr << filename << " => " << count << std::endl);
            }
        }
    };


    class Find_Includes : public PPCallbacks
    {
    private:
        std::map<std::string, int> *m_include_usages_count;
        std::map<std::string, SourceLocation> *m_include_locations;
        std::set<std::string> *m_includes_marked_as_allowed;
        std::string m_main_filename;
        SourceManager *m_src_mgr;

    public:
        Find_Includes(
            SourceManager *src_mgr,
            std::map<std::string, int> *include_usages_count,
            std::set<std::string> *includes_marked_as_allowed,
            std::map<std::string, SourceLocation> *include_locations)
        {
            m_src_mgr = src_mgr;
            m_include_usages_count = include_usages_count;
            m_includes_marked_as_allowed = includes_marked_as_allowed;
            m_include_locations = include_locations;
            m_main_filename = src_mgr->getFileEntryForID(src_mgr->getMainFileID())->getName();
            auto trimmed = trim_suffix(m_main_filename, ".c");
            if (trimmed.hasValue()) {
                MarkFileUsed(trimmed.getValue() + ".h");
                MarkFileUsed(trimmed.getValue() + "_api.h");
            }
        }

        void MarkFileUsed(std::string name)
        {
            if (m_include_usages_count->count(name) == 0) {
                (*m_include_usages_count)[name] = 1;
            }
        }

        bool isIgnoredFile(std::string name)
        {
            std::string ignored_prefix = "/usr/";
            if (name.compare(0, ignored_prefix.length(), ignored_prefix) == 0) return true;
            std::string allowed_suffix = ".h";
            if (name.length() >= allowed_suffix.length()) {
                if (0 != name.compare(name.length() - allowed_suffix.length(), allowed_suffix.length(), allowed_suffix)) return true;
            }
            return false;
        }


        bool hasIncludeComment(CharSourceRange filename_range, SourceManager &sm, std::string expected)
        {
            const CharSourceRange range = CharSourceRange::getCharRange(
                filename_range.getEnd(),
                filename_range.getEnd().getLocWithOffset(expected.size()));
            std::string txt = Lexer::getSourceText(range, sm, LangOptions(), 0);
            ON_DEBUG(std::cerr << "Include line text: " << txt << std::endl);
            return txt == expected;
        }

        void InclusionDirective(
            SourceLocation hash_loc,
            const Token &UNUSED(include_token),
            StringRef UNUSED(file_name),
            bool UNUSED(is_angled),
            CharSourceRange filename_range,
            const FileEntry *file,
            StringRef UNUSED(search_path),
            StringRef UNUSED(relative_path),
            const Module *UNUSED(imported))
        {
            // do something with the include
            if (!hash_loc.isValid()) return;
            if (!file) return; // happens if file was not found (wrong #include)
            if (!m_src_mgr->isInMainFile(hash_loc)) return;
            std::string name = trim_filename(file->getName());
            ON_DEBUG(std::cerr << name << std::endl);
            if (m_include_usages_count->count(name) > 0) return;
            if (isIgnoredFile(name)) return;
            if (hasIncludeComment(filename_range, *m_src_mgr, " /* include:allowed */")) {
                (*m_includes_marked_as_allowed).insert(name);
            }
            (*m_include_usages_count)[name]
                = (hasIncludeComment(filename_range, *m_src_mgr, " /* include:optional */"))
                ? 1
                : 0;
            (*m_include_locations)[name] = hash_loc;
        }

        void MacroExpands(const Token &UNUSED(MacroNameTok),
                          const MacroDefinition &MD, SourceRange Range,
                          const MacroArgs *UNUSED(Args))
        {
            MacroInfo *macro_info = MD.getMacroInfo();
            SourceLocation macro_def_loc = macro_info->getDefinitionLoc();
            SourceLocation use_loc = Range.getBegin();
            std::string filename = get_filename(m_src_mgr, macro_def_loc);
            std::string use_filename = get_filename(m_src_mgr, use_loc);
            if (!use_loc.isValid()) return;
            if (!m_src_mgr->isInMainFile(use_loc)) return;
            if (isIgnoredFile(filename)) return;
            if (m_include_usages_count->count(filename) == 0) {
                (*m_include_usages_count)[filename] = 1;
            } else {
                (*m_include_usages_count)[filename] += 1;
            }
            (*m_include_locations)[filename] = use_loc;
        }
    };


    class IncludeCleanerAction : public PluginASTAction {
        std::set<std::string> files_whitelist;

    public:
        IncludeCleanerAction() {
        }

    protected:
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                       llvm::StringRef UNUSED(filename)) override
        {
            std::set<std::string> *includes_marked_as_allowed = new std::set<std::string>();
            std::map<std::string, int> *include_usages_count = new std::map<std::string, int>();
            std::map<std::string, SourceLocation> *include_locations = new std::map<std::string, SourceLocation>();
            ON_DEBUG(std::cerr << "Starting: " << filename.str() << std::endl);

            std::unique_ptr<Find_Includes> find_includes_callback(
                new Find_Includes(&CI.getSourceManager(), include_usages_count,
                                  includes_marked_as_allowed, include_locations));

            ON_DEBUG(std::cerr << filename.str() << std::endl);
            Preprocessor &pp = CI.getPreprocessor();
            pp.addPPCallbacks(std::move(find_includes_callback));

            MatchFinder *find = new MatchFinder();
            IncludeCleanerMatchCallback *cb = new IncludeCleanerMatchCallback(include_usages_count, includes_marked_as_allowed,
                                                                              include_locations, &files_whitelist);
            find->addMatcher(memberExprMatcher, cb);
            find->addMatcher(declInOtherFile, cb);
            find->addMatcher(declarationsInOtherFiles, cb);
            find->addMatcher(definitionsInMainFile, cb);
            find->addMatcher(typesInMainFile, cb);
            return find->newASTConsumer();
        }

        bool ParseArgs(const CompilerInstance &UNUSED(CI),
                       const std::vector<std::string> &args) override
        {
            for (auto filename : args) {
                ON_DEBUG(std::cerr << "Ignoring: " << filename << std::endl);
                files_whitelist.insert(filename);
            }
            return true;
        }

        void PrintHelp(llvm::raw_ostream& ros) {
            ros << "Help for switch_stmt plugin goes here\n";
        }

    };

}


static FrontendPluginRegistry::Add<IncludeCleanerAction> X("include_cleaner", "");
