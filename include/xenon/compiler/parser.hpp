#pragma once

#include <string>

#include "xenon/compiler/ast.hpp"
#include "xenon/compiler/lexer.hpp"

namespace xenon
{

/// Recursive-descent parser for the C++-like Xenon subset.
class Parser
{
  public:
    explicit Parser(Lexer& lexer);

    /// Parses a program. Returns `true` on success and fills `out`.
    bool parse(Program& out);

    [[nodiscard]] const std::string& last_error() const noexcept
    {
        return error_;
    }

  private:
    struct ParseError
    {
    };

    [[noreturn]] void fail(const std::string& message);

    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& message);
    bool check_keyword(std::string_view keyword) const;
    bool match_keyword(std::string_view keyword);
    bool is_type(const Token& token) const;

    void parse_program(Program& out);
    FunctionDecl parse_function();
    std::string parse_type();
    std::unique_ptr<BlockStmt> parse_block();
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Stmt> parse_var_decl();
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_assignment();
    std::unique_ptr<Expr> parse_or();
    std::unique_ptr<Expr> parse_and();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_call();
    std::unique_ptr<Expr> parse_primary();

    Lexer& lexer_;
    Token current_;
    Token previous_;
    std::string error_;
};

} // namespace xenon
