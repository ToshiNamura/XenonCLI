#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xenon
{

// ---- expressions ----

struct Expr
{
    virtual ~Expr() = default;
    int line = 0;
};

struct NumberExpr : Expr
{
    double value = 0.0;
};

struct BoolExpr : Expr
{
    bool value = false;
};

struct NullExpr : Expr
{
};

struct VariableExpr : Expr
{
    std::string name;
};

struct UnaryExpr : Expr
{
    enum class Op
    {
        Negate,
        Not,
    };
    Op op = Op::Negate;
    std::unique_ptr<Expr> operand;
};

struct BinaryExpr : Expr
{
    enum class Op
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        And,
        Or,
    };
    Op op = Op::Add;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct AssignExpr : Expr
{
    std::string name;
    std::unique_ptr<Expr> value;
};

struct CallExpr : Expr
{
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

// ---- statements ----

struct Stmt
{
    virtual ~Stmt() = default;
    int line = 0;
};

struct ExprStmt : Stmt
{
    std::unique_ptr<Expr> expr;
};

struct VarDeclStmt : Stmt
{
    std::string type;
    std::string name;
    std::unique_ptr<Expr> init; // may be null
};

struct ReturnStmt : Stmt
{
    std::unique_ptr<Expr> value; // may be null (void)
};

struct BlockStmt : Stmt
{
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt : Stmt
{
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch; // may be null
};

struct WhileStmt : Stmt
{
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
};

// ---- top-level ----

struct FunctionDecl
{
    std::string return_type; // empty means void
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; // type, name
    std::unique_ptr<BlockStmt> body;
    bool exported = false;
};

struct Program
{
    std::vector<FunctionDecl> functions;
};

} // namespace xenon
