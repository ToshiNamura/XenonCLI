#include "xenon/compiler/lexer.hpp"

#include <cctype>

namespace xenon
{

Lexer::Lexer(std::string_view source) : source_(source)
{
}

char Lexer::peek(std::size_t ahead) const
{
    return pos_ + ahead < source_.size() ? source_[pos_ + ahead] : '\0';
}

char Lexer::advance()
{
    if (pos_ >= source_.size())
    {
        return '\0';
    }
    const char c = source_[pos_++];
    if (c == '\n')
    {
        ++line_;
    }
    return c;
}

void Lexer::skip_trivia()
{
    while (pos_ < source_.size())
    {
        const char c = peek();
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/')
        {
            while (pos_ < source_.size() && peek() != '\n')
            {
                advance();
            }
            continue;
        }
        if (c == '/' && peek(1) == '*')
        {
            advance();
            advance();
            while (pos_ < source_.size() && !(peek() == '*' && peek(1) == '/'))
            {
                advance();
            }
            if (pos_ < source_.size())
            {
                advance();
                advance();
            }
            continue;
        }
        break;
    }
}

Token Lexer::next()
{
    skip_trivia();

    Token token;
    token.line = line_;

    if (pos_ >= source_.size())
    {
        token.type = TokenType::End;
        return token;
    }

    const std::size_t start = pos_;
    const char c = advance();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
    {
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
        {
            advance();
        }
        token.type = TokenType::Identifier;
        token.lexeme = source_.substr(start, pos_ - start);
        return token;
    }

    if (std::isdigit(static_cast<unsigned char>(c)))
    {
        while (std::isdigit(static_cast<unsigned char>(peek())))
        {
            advance();
        }
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))
        {
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek())))
            {
                advance();
            }
        }
        token.type = TokenType::Number;
        token.lexeme = source_.substr(start, pos_ - start);
        return token;
    }

    if (c == '"')
    {
        while (pos_ < source_.size() && peek() != '"')
        {
            advance();
        }
        if (pos_ < source_.size())
        {
            advance(); // closing quote
        }
        token.type = TokenType::String;
        token.lexeme = source_.substr(start, pos_ - start);
        return token;
    }

    token.lexeme = source_.substr(start, 1);

    switch (c)
    {
    case '(':
        token.type = TokenType::LeftParen;
        break;
    case ')':
        token.type = TokenType::RightParen;
        break;
    case '{':
        token.type = TokenType::LeftBrace;
        break;
    case '}':
        token.type = TokenType::RightBrace;
        break;
    case ',':
        token.type = TokenType::Comma;
        break;
    case ';':
        token.type = TokenType::Semicolon;
        break;
    case '+':
        token.type = TokenType::Plus;
        break;
    case '-':
        token.type = TokenType::Minus;
        break;
    case '*':
        token.type = TokenType::Star;
        break;
    case '/':
        token.type = TokenType::Slash;
        break;
    case '!':
        if (peek() == '=')
        {
            advance();
            token.lexeme = source_.substr(start, 2);
            token.type = TokenType::BangEqual;
        }
        else
        {
            token.type = TokenType::Bang;
        }
        break;
    case '=':
        if (peek() == '=')
        {
            advance();
            token.lexeme = source_.substr(start, 2);
            token.type = TokenType::EqualEqual;
        }
        else
        {
            token.type = TokenType::Assign;
        }
        break;
    case '<':
        if (peek() == '=')
        {
            advance();
            token.lexeme = source_.substr(start, 2);
            token.type = TokenType::LessEqual;
        }
        else
        {
            token.type = TokenType::Less;
        }
        break;
    case '>':
        if (peek() == '=')
        {
            advance();
            token.lexeme = source_.substr(start, 2);
            token.type = TokenType::GreaterEqual;
        }
        else
        {
            token.type = TokenType::Greater;
        }
        break;
    case '&':
        if (peek() == '&')
        {
            advance();
            token.lexeme = source_.substr(start, 2);
            token.type = TokenType::AndAnd;
        }
        else
        {
            token.type = TokenType::Invalid;
        }
        break;
    case '|':
        if (peek() == '|')
        {
            advance();
            token.lexeme = source_.substr(start, 2);
            token.type = TokenType::OrOr;
        }
        else
        {
            token.type = TokenType::Invalid;
        }
        break;
    default:
        token.type = TokenType::Invalid;
        break;
    }

    return token;
}

} // namespace xenon
