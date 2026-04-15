// cdb.h
#pragma once
#include <string>
#include <vector>

namespace clang {
class FunctionDecl;
class CompoundStmt;
class Stmt;
class ReturnStmt;
class SourceManager;
class Rewriter;
}

// Context for a single function instrumentation
class InsertContext {
public:
    clang::FunctionDecl *FD = nullptr;
    clang::CompoundStmt *Body = nullptr;

    clang::Stmt *FirstNonDecl = nullptr;
    std::vector<clang::ReturnStmt*> Returns;

    std::string filename;
    bool is_main_file = false;
    bool is_main_func = false;

    static InsertContext build(clang::FunctionDecl *FD,
                               clang::SourceManager &SM);
};

enum class InsertPos {
    BeforeFirstStmt,
    BeforeEachReturn,
    FuncEnd
};

class Condition {
public:
    virtual ~Condition() {}
    virtual bool match(const InsertContext &ctx) const = 0;
};

class TemplateGen {
public:
    virtual ~TemplateGen() {}
    virtual std::string generate(const InsertContext &ctx) const = 0;
};

class InsertRule {
public:
    InsertRule(Condition *c, TemplateGen *t, InsertPos p)
        : cond(c), temp(t), pos(p) {}

    Condition *cond;
    TemplateGen *temp;
    InsertPos pos;
};

class RuleEngine {
public:
    void addRule(const InsertRule &rule);
    void applyAll(const InsertContext &ctx, clang::Rewriter &R);

private:
    void applyOne(const InsertRule &rule,
                  const InsertContext &ctx,
                  clang::Rewriter &R);

    std::vector<InsertRule> rules;
};

class CdbOutput {
public:
    static void store(const char *path, const std::string &content);
    static void writeAll(const char *project_root);
};

class CdbTool {
public:
    static int run(const char *project_root);
};
