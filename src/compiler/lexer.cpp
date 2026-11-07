#include "xenon/compiler/lexer.hpp"

#include <cctype>
#include <cstddef>

namespace xenon
{

Lexer::Lexer(std::string_view source) : _source(source)
{
}

char Lexer::peek(std::size_t ahead) const
{
    return _pos + ahead < _source.size() ? _source[_pos + ahead] : '\0';
}

char Lexer::advance()
{
    if (_pos >= _source.size())
    {
        return '\0';
    }
    const char c = _source[_pos++];
    if (c == '\n')
    {
        ++_line;
    }
    return c;
}

void Lexer::skipTrivia()
{
    while (_pos < _source.size())
    {
        const char c = peek();
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/')
        {
            while (_pos < _source.size() && peek() != '\n')
            {
                advance();
            }
            continue;
        }
        if (c == '/' && peek(1) == '*')
        {
            advance();
            advance();
            while (_pos < _source.size() && !(peek() == '*' && peek(1) == '/'))
            {
                advance();
            }
            if (_pos < _source.size())
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
    skipTrivia();

    Token token;
    token.Line = _line;

    if (_pos >= _source.size())
    {
        token.Type = TokenType::End;
        return token;
    }

    const std::size_t start = _pos;
    const char c = advance();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
    {
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
        {
            advance();
        }
        token.Type = TokenType::Identifier;
        token.Lexeme = _source.substr(start, _pos - start);
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
        token.Type = TokenType::Number;
        token.Lexeme = _source.substr(start, _pos - start);
        return token;
    }

    if (c == '"')
    {
        while (_pos < _source.size() && peek() != '"')
        {
            advance();
        }
        if (_pos < _source.size())
        {
            advance(); // closing quote
        }
        token.Type = TokenType::String;
        token.Lexeme = _source.substr(start, _pos - start);
        return token;
    }

    token.Lexeme = _source.substr(start, 1);

    switch (c)
    {
    case '(':
        token.Type = TokenType::LeftParen;
        break;
    case ')':
        token.Type = TokenType::RightParen;
        break;
    case '{':
        token.Type = TokenType::LeftBrace;
        break;
    case '}':
        token.Type = TokenType::RightBrace;
        break;
    case ',':
        token.Type = TokenType::Comma;
        break;
    case ';':
        token.Type = TokenType::Semicolon;
        break;
    case '+':
        token.Type = TokenType::Plus;
        break;
    case '-':
        token.Type = TokenType::Minus;
        break;
    case '*':
        token.Type = TokenType::Star;
        break;
    case '/':
        token.Type = TokenType::Slash;
        break;
    case '!':
        if (peek() == '=')
        {
            advance();
            token.Lexeme = _source.substr(start, 2);
            token.Type = TokenType::BangEqual;
        }
        else
        {
            token.Type = TokenType::Bang;
        }
        break;
    case '=':
        if (peek() == '=')
        {
            advance();
            token.Lexeme = _source.substr(start, 2);
            token.Type = TokenType::EqualEqual;
        }
        else
        {
            token.Type = TokenType::Assign;
        }
        break;
    case '<':
        if (peek() == '=')
        {
            advance();
            token.Lexeme = _source.substr(start, 2);
            token.Type = TokenType::LessEqual;
        }
        else
        {
            token.Type = TokenType::Less;
        }
        break;
    case '>':
        if (peek() == '=')
        {
            advance();
            token.Lexeme = _source.substr(start, 2);
            token.Type = TokenType::GreaterEqual;
        }
        else
        {
            token.Type = TokenType::Greater;
        }
        break;
    case '&':
        if (peek() == '&')
        {
            advance();
            token.Lexeme = _source.substr(start, 2);
            token.Type = TokenType::AndAnd;
        }
        else
        {
            token.Type = TokenType::Invalid;
        }
        break;
    case '|':
        if (peek() == '|')
        {
            advance();
            token.Lexeme = _source.substr(start, 2);
            token.Type = TokenType::OrOr;
        }
        else
        {
            token.Type = TokenType::Invalid;
        }
        break;
    default:
        token.Type = TokenType::Invalid;
        break;
    }

    return token;
}

} // namespace xenon
