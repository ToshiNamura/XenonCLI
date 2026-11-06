#pragma once

#include <string_view>

namespace xenon
{

enum class TokenType
{
    End,
    Invalid,
    Identifier,
    Number,
    String,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Comma,
    Semicolon,
    Plus,
    Minus,
    Star,
    Slash,
    Bang,
    Assign,       // =
    EqualEqual,   // ==
    BangEqual,    // !=
    Less,         // <
    LessEqual,    // <=
    Greater,      // >
    GreaterEqual, // >=
    AndAnd,       // &&
    OrOr,         // ||
};

struct Token
{
    TokenType type = TokenType::End;
    std::string_view lexeme;
    int line = 1;
};

/// Tokenizes C++-like Xenon source text.
class Lexer
{
  public:
    explicit Lexer(std::string_view source);

    /// Returns the next token. Repeats `End` at end of input.
    Token next();

  private:
    void skip_trivia();
    char peek(std::size_t ahead = 0) const;
    char advance();

    std::string_view source_;
    std::size_t pos_ = 0;
    int line_ = 1;
};

} // namespace xenon
