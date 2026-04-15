#include "cdb.h"

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Lex/Lexer.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/raw_ostream.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

using namespace clang;
using namespace clang::tooling;

static llvm::SmallVector<std::pair<std::string,std::string>, 256> g_files;
static llvm::SmallVector<std::string, 512> g_hook_names;
static std::string g_project_root;
static llvm::SmallSet<uintptr_t, 32> g_instrumented_funcs;

/* ---------------- path filters ---------------- */

static bool is_ccbpf_path(const char *path) {
    const char *p = path;
    while ((p = strstr(p, "ccbpf")) != 0) {
        bool ok_front = (p == path) || (*(p - 1) == '/');
        const char *q = p + 5;
        bool ok_back = (*q == 0) || (*q == '/');
        if (ok_front && ok_back)
            return true;
        p++;
    }
    return false;
}

static bool is_lib_path(const char *path) {
    const char *p = path;
    while ((p = strstr(p, "lib")) != 0) {
        bool ok_front = (p == path) || (*(p - 1) == '/');
        const char *q = p + 3;
        bool ok_back = (*q == 0) || (*q == '/');
        if (ok_front && ok_back)
            return true;
        p++;
    }
    return false;
}

static bool is_mg_path(const char *path) {
    const char *p = path;
    while ((p = strstr(p, "mg")) != 0) {
        bool ok_front = (p == path) || (*(p - 1) == '/');
        const char *q = p + 2;
        bool ok_back = (*q == 0) || (*q == '/');
        if (ok_front && ok_back)
            return true;
        p++;
    }
    return false;
}

/* ---------------- output ---------------- */

static std::string make_relative(const std::string &path) {
    if (path.compare(0, g_project_root.size(), g_project_root) == 0) {
        std::string rel = path.substr(g_project_root.size());
        if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
            rel.erase(0, 1);
        return rel;
    }
    return path;
}

void CdbOutput::store(const char *path, const std::string &content) {
    g_files.push_back(std::make_pair(make_relative(path), content));
}

static void copy_tree(const char *src_root, const char *dst_root) {
    DIR *dp = opendir(src_root);
    if (!dp) return;

    mkdir(dst_root, 0755);

    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;

        std::string src = std::string(src_root) + "/" + ep->d_name;
        std::string dst = std::string(dst_root) + "/" + ep->d_name;

        struct stat st;
        if (stat(src.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            copy_tree(src.c_str(), dst.c_str());
        } else if (S_ISREG(st.st_mode)) {
            FILE *in = fopen(src.c_str(), "rb");
            if (!in) continue;
            FILE *out = fopen(dst.c_str(), "wb");
            if (!out) { fclose(in); continue; }

            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                fwrite(buf, 1, n, out);

            fclose(in);
            fclose(out);
        }
    }

    closedir(dp);
}

void CdbOutput::writeAll(const char *project_root) {
    char abs_root[1024];
    realpath(project_root, abs_root);
    g_project_root = abs_root;

    mkdir("process", 0755);
    copy_tree(g_project_root.c_str(), "process");

    for (size_t i = 0; i < g_files.size(); ++i) {
        const std::string &rel = g_files[i].first;
        const std::string &content = g_files[i].second;

        std::string out = "process/" + rel;

        char tmp[1024];
        strcpy(tmp, out.c_str());
        for (char *q = tmp + 1; *q; q++) {
            if (*q == '/') {
                *q = 0;
                mkdir(tmp, 0755);
                *q = '/';
            }
        }

        FILE *fp = fopen(out.c_str(), "w");
        if (!fp) continue;

        fwrite(content.data(), 1, content.size(), fp);
        fclose(fp);
    }
}

/* ---------------- scan ---------------- */

static void scan_include_dirs(const char *root, std::vector<std::string> &incs) {
    DIR *dp = opendir(root);
    if (!dp) return;

    bool has_header = false;

    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;

        std::string full = std::string(root) + "/" + ep->d_name;

        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_include_dirs(full.c_str(), incs);
        } else if (S_ISREG(st.st_mode)) {
            size_t len = strlen(ep->d_name);
            if (len > 2 && ep->d_name[len - 2] == '.' && ep->d_name[len - 1] == 'h')
                has_header = true;
        }
    }

    closedir(dp);

    if (has_header)
        incs.push_back(std::string("-I") + root);
}

static void scan_src(const char *root, std::vector<std::string> &out) {
    DIR *dp = opendir(root);
    if (!dp) return;

    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;

        std::string full = std::string(root) + "/" + ep->d_name;

        if (is_ccbpf_path(full.c_str()))
            continue;
        if (is_lib_path(full.c_str()))
            continue;
        if (is_mg_path(full.c_str()))
            continue;

        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_src(full.c_str(), out);
        } else if (S_ISREG(st.st_mode)) {
            size_t len = strlen(ep->d_name);
            if (len > 2 && ep->d_name[len - 2] == '.' && ep->d_name[len - 1] == 'c')
                out.push_back(full);
        }
    }

    closedir(dp);
}

/* ---------------- collect hooks ---------------- */

class CollectHooksVisitor : public RecursiveASTVisitor<CollectHooksVisitor> {
public:
    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (!FD->hasBody()) return true;
        if (FD->isImplicit()) return true;
        if (FD != FD->getCanonicalDecl())
            return true;

        std::string func = FD->getNameAsString();
        if (func.empty()) return true;

        if (g_hook_names.empty())
            g_hook_names.push_back("hook_trace");

        return true;
    }
};

class CollectHooksConsumer : public ASTConsumer {
public:
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
private:
    CollectHooksVisitor Visitor;
};

class CollectHooksAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance &CI, llvm::StringRef file) override {
        return std::unique_ptr<ASTConsumer>(new CollectHooksConsumer());
    }
};

/* ---------------- InsertContext ---------------- */

InsertContext InsertContext::build(FunctionDecl *FD, SourceManager &SM) {
    InsertContext ctx;
    ctx.FD = FD;
    ctx.Body = dyn_cast<CompoundStmt>(FD->getBody());

    ctx.filename = SM.getFilename(FD->getLocation()).str();

    const char *name_c = ctx.filename.c_str();
    size_t len = ctx.filename.size();
    if (len >= 6 && strcmp(name_c + (len - 6), "main.c") == 0)
        ctx.is_main_file = true;
    else
        ctx.is_main_file = false;

    ctx.is_main_func = FD->isMain();

    if (ctx.Body) {
        for (CompoundStmt::body_iterator it = ctx.Body->body_begin();
             it != ctx.Body->body_end(); ++it) {
            Stmt *S = *it;
            if (!isa<DeclStmt>(S)) {
                ctx.FirstNonDecl = S;
                break;
            }
        }

        class RetCollector : public RecursiveASTVisitor<RetCollector> {
        public:
            std::vector<ReturnStmt*> &Out;
            RetCollector(std::vector<ReturnStmt*> &O) : Out(O) {}
            bool VisitReturnStmt(ReturnStmt *RS) {
                Out.push_back(RS);
                return true;
            }
        } RC(ctx.Returns);

        RC.TraverseStmt(ctx.Body);
    }

    return ctx;
}

/* ---------------- RuleEngine ---------------- */

void RuleEngine::addRule(const InsertRule &rule) {
    rules.push_back(rule);
}

void RuleEngine::applyOne(const InsertRule &rule,
                          const InsertContext &ctx,
                          Rewriter &R) {
    if (!rule.cond->match(ctx))
        return;

    std::string code = rule.temp->generate(ctx);
    if (code.empty())
        return;

    switch (rule.pos) {
    case InsertPos::BeforeFirstStmt:
    if (ctx.FirstNonDecl)
        R.InsertText(ctx.FirstNonDecl->getBeginLoc(), code, true, true);
    else if (ctx.Body) {
        SourceLocation L = ctx.Body->getLBracLoc().getLocWithOffset(1);
        R.InsertText(L, code, true, true);
    }
    break;

    case InsertPos::BeforeEachReturn:
        for (size_t i = 0; i < ctx.Returns.size(); ++i) {
            ReturnStmt *RS = ctx.Returns[i];

            R.InsertText(RS->getBeginLoc(), "{ " + code, true, true);
            SourceLocation afterSemi = Lexer::findLocationAfterToken(
                RS->getEndLoc(),
                tok::semi,
                R.getSourceMgr(),
                R.getLangOpts(),
                false
            );
            R.InsertText(afterSemi, " }", true, true);
        }
        break;

    case InsertPos::FuncEnd:
        if (ctx.Body)
            R.InsertText(ctx.Body->getRBracLoc(), code, true, true);
        break;
    }
}

void RuleEngine::applyAll(const InsertContext &ctx, Rewriter &R) {
    for (size_t i = 0; i < rules.size(); ++i)
        applyOne(rules[i], ctx, R);
}

/* ---------------- Rules ---------------- */

// main.c: only main() gets init+register_hooks at function entry
class CondMainInit : public Condition {
public:
    bool match(const InsertContext &ctx) const {
        return ctx.is_main_file && ctx.is_main_func && ctx.Body != 0;
    }
};

class TempMainInit : public TemplateGen {
public:
    std::string generate(const InsertContext &) const {
        std::string code;
        code += "    ccbpf_system_init();\n";
        code += "    register_hooks();\n";
        return code;
    }
};

// non-main.c: function entry hook, once per function
class CondFuncEntry : public Condition {
public:
    bool match(const InsertContext &ctx) const {
        if (ctx.is_main_file) return false;
        if (!ctx.Body) return false;

        uintptr_t key = reinterpret_cast<uintptr_t>(ctx.FD->getCanonicalDecl());

        if (g_instrumented_funcs.count(key))
            return false;
        g_instrumented_funcs.insert(key);
        return true;
    }
};

class TempFuncEntry : public TemplateGen {
public:
    std::string generate(const InsertContext &ctx) const {
        CompoundStmt *Body = ctx.Body;
        if (!Body) return std::string();

        Stmt *InsertBefore = 0;
        for (CompoundStmt::body_iterator it = Body->body_begin();
             it != Body->body_end(); ++it) {
            Stmt *S = *it;
            if (!isa<DeclStmt>(S)) {
                InsertBefore = S;
                break;
            }
        }
        if (!InsertBefore) return std::string();

        std::string func = ctx.FD->getNameAsString();
        if (func.empty()) return std::string();

        std::string code;
        code += "    void* __ccbpf_ctx[3];\n";
        code += "    __ccbpf_ctx[0] = (void*)1;\n";
        code += "    __ccbpf_ctx[1] = (void*)" + func + ";\n";
        code += "    __ccbpf_ctx[2] = (void*)__func__;\n";
        code += "    HOOK_LINE(__ccbpf_ctx, \"hook_trace\");\n";

        return code;
    }
};


// exit hook: before each return
class CondFuncExitBeforeReturn : public Condition {
public:
    bool match(const InsertContext &ctx) const override {
        if (ctx.is_main_file) return false;
        if (!ctx.Body) return false;
        return !ctx.Returns.empty();
    }
};

// exit hook: at function end for void functions with no return
class CondFuncExitAtEnd : public Condition {
public:
    bool match(const InsertContext &ctx) const override {
        if (ctx.is_main_file) return false;
        if (!ctx.Body) return false;

        if (!ctx.FD->getReturnType()->isVoidType())
            return false;

        if (!ctx.FirstNonDecl)
            return false;

        return true;
    }
};

class TempFuncExit : public TemplateGen {
public:
    std::string generate(const InsertContext &ctx) const override {
        std::string func = ctx.FD->getNameAsString();
        if (func.empty()) return std::string();

        std::string code;
        code += "    __ccbpf_ctx[0] = (void*)2;\n";
        code += "    __ccbpf_ctx[1] = (void*)" + func + ";\n";
        code += "    __ccbpf_ctx[2] = (void*)__func__;\n";
        code += "    HOOK_LINE(__ccbpf_ctx, \"hook_trace\");\n";
        return code;
    }
};

/* ---------------- Rule registration ---------------- */

static RuleEngine &getRuleEngine() {
    static RuleEngine engine;
    static bool inited = false;

    if (!inited) {
        inited = true;

        engine.addRule(InsertRule(
            new CondMainInit(),
            new TempMainInit(),
            InsertPos::BeforeFirstStmt
        ));

        engine.addRule(InsertRule(
            new CondFuncEntry(),
            new TempFuncEntry(),
            InsertPos::BeforeFirstStmt
        ));

        engine.addRule(InsertRule(
            new CondFuncExitBeforeReturn(),
            new TempFuncExit(),
            InsertPos::BeforeEachReturn
        ));

        engine.addRule(InsertRule(
            new CondFuncExitAtEnd(),
            new TempFuncExit(),
            InsertPos::FuncEnd
        ));
    }

    return engine;
}

/* ---------------- Instrument pass ---------------- */

class InstrumentVisitor : public RecursiveASTVisitor<InstrumentVisitor> {
public:
    explicit InstrumentVisitor(Rewriter &R) : TheRewriter(R) {}

    bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD->hasBody()) return true;
    if (FD->isImplicit()) return true;

    FunctionDecl *Def = FD->getDefinition();
    if (!Def) return true;
    if (FD != Def) return true;  

    SourceManager &SM = TheRewriter.getSourceMgr();
    if (!SM.isWrittenInMainFile(Def->getLocation()))
        return true;

    InsertContext ctx = InsertContext::build(Def, SM);
    getRuleEngine().applyAll(ctx, TheRewriter);
    return true;
}


private:
    Rewriter &TheRewriter;
};

class InstrumentConsumer : public ASTConsumer {
public:
    explicit InstrumentConsumer(Rewriter &R) : Visitor(R) {}
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
private:
    InstrumentVisitor Visitor;
};

class InstrumentAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance &CI, llvm::StringRef file) override {

        filename = file.str();
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::unique_ptr<ASTConsumer>(new InstrumentConsumer(TheRewriter));
    }

    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        FileID id = SM.getMainFileID();
        const RewriteBuffer *buf = TheRewriter.getRewriteBufferFor(id);
        if (!buf) return;

        std::string out;
        llvm::raw_string_ostream os(out);
        buf->write(os);
        os.flush();

        std::string header;
        header += "#include \"ccbpf.h\"\n";
        header += "#define HOOK_LINE(ctx,name) ccbpf_hook_line(ctx,__LINE__,name)\n";
        header += "static inline void ccbpf_hook_line(void*ctx,int line,const char*name){hook_run(name,(uint8_t*)ctx,256);} \n\n";
        out = header + out;

        if (out.find("register_hooks();") != std::string::npos) {
            std::string reg;
            reg += "extern void hook_register(const char *name);\n";
            reg += "static void register_hooks(void) {\n";
            for (size_t i = 0; i < g_hook_names.size(); ++i)
                reg += "    hook_register(\"" + g_hook_names[i] + "\");\n";
            reg += "}\n\n";

            out = reg + out;
        }

        CdbOutput::store(filename.c_str(), out);
    }

private:
    Rewriter TheRewriter;
    std::string filename;
};

/* ---------------- CdbTool ---------------- */

int CdbTool::run(const char *project_root) {
    char abs_root[1024];
    realpath(project_root, abs_root);
    g_project_root = abs_root;

    std::vector<std::string> files;
    scan_src(g_project_root.c_str(), files);
    if (files.empty())
        return 0;

    std::vector<std::string> incs;
    incs.push_back("-I.");
    scan_include_dirs(g_project_root.c_str(), incs);

    FixedCompilationDatabase Compilations(".", incs);

    {
        ClangTool Tool1(Compilations, files);
        int r1 = Tool1.run(newFrontendActionFactory<CollectHooksAction>().get());
        if (r1 != 0) return r1;
    }

    {
        ClangTool Tool2(Compilations, files);
        int r2 = Tool2.run(newFrontendActionFactory<InstrumentAction>().get());
        if (r2 != 0) return r2;
    }

    CdbOutput::writeAll(g_project_root.c_str());
    return 0;
}
