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
    int Line = 0;
};

struct NumberExpr : Expr
{
    double Value = 0.0;
};

struct BoolExpr : Expr
{
    bool Value = false;
};

struct StringExpr : Expr
{
    std::string Value;
};

struct NullExpr : Expr
{
};

struct VariableExpr : Expr
{
    std::string Name;
};

struct UnaryExpr : Expr
{
    enum class Op
    {
        Negate,
        Not,
    };
    Op Operation = Op::Negate;
    std::unique_ptr<Expr> Operand;
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
    Op Operation = Op::Add;
    std::unique_ptr<Expr> Left;
    std::unique_ptr<Expr> Right;
};

struct AssignExpr : Expr
{
    std::string Name;
    std::unique_ptr<Expr> Value;
};

struct CallExpr : Expr
{
    std::string Callee;
    std::vector<std::unique_ptr<Expr>> Args;
};

// ---- statements ----

struct Stmt
{
    virtual ~Stmt() = default;
    int Line = 0;
};

struct ExprStmt : Stmt
{
    std::unique_ptr<Expr> Expr;
};

struct VarDeclStmt : Stmt
{
    std::string Type;
    std::string Name;
    std::unique_ptr<Expr> Init; // may be null
};

struct ReturnStmt : Stmt
{
    std::unique_ptr<Expr> Value; // may be null (void)
};

struct BlockStmt : Stmt
{
    std::vector<std::unique_ptr<Stmt>> Statements;
};

struct IfStmt : Stmt
{
    std::unique_ptr<Expr> Condition;
    std::unique_ptr<Stmt> ThenBranch;
    std::unique_ptr<Stmt> ElseBranch; // may be null
};

struct WhileStmt : Stmt
{
    std::unique_ptr<Expr> Condition;
    std::unique_ptr<Stmt> Body;
};

// ---- top-level ----

struct FunctionDecl
{
    std::string ReturnType; // empty means void
    std::string Name;
    std::vector<std::pair<std::string, std::string>> Params; // type, name
    std::unique_ptr<BlockStmt> Body;                         // null for a prototype
    bool Exported = false;
    bool HasBody = true;
};

/// Kind of a declaration-only type (from `.hint` files or forward declarations).
enum class DeclKind
{
    Struct,
    Class,
    Enum,
};

struct TypeDecl
{
    DeclKind Kind = DeclKind::Struct;
    std::string Name;
    std::string Signature;
};

struct Program
{
    std::vector<FunctionDecl> Functions;
    std::vector<TypeDecl> TypeDecls;
};

} // namespace xenon
