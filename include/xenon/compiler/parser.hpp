#pragma once

#include <string>
#include <unordered_set>

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

    [[nodiscard]] const std::string& lastError() const noexcept
    {
        return _error;
    }

  private:
    struct ParseError
    {
    };

    [[noreturn]] void fail(const std::string& message);

    const Token& advance();
    const Token& peekNext();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& message);
    bool checkKeyword(std::string_view keyword) const;
    bool matchKeyword(std::string_view keyword);
    bool isType(const Token& token) const;

    void parseProgram(Program& out);
    TemplateDecl parseTemplate();
    FunctionDecl parseFunction();
    TypeDecl parseTypeDecl();
    std::string parseType();
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Stmt> parseVarDecl();
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAssignment();
    std::unique_ptr<Expr> parseOr();
    std::unique_ptr<Expr> parseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseTerm();
    std::unique_ptr<Expr> parseFactor();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parseCall();
    std::unique_ptr<Expr> parsePrimary();

    Lexer& _lexer;
    Token _current;
    Token _previous;
    Token _peek;
    bool _hasPeek = false;
    std::unordered_set<std::string> _typeNames;
    std::string _error;
};

} // namespace xenon
