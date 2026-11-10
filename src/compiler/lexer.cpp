#include <xenon/compiler/lexer.hpp>

#include <cctype>
#include <cstddef>

xenon::Lexer::Lexer(std::string_view source) : _source(source) { }

char xenon::Lexer::peek(std::size_t ahead) const
{
	return _pos + ahead < _source.size() ? _source[_pos + ahead] : '\0';
}

char xenon::Lexer::advance()
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

void xenon::Lexer::skipTrivia()
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

xenon::Token xenon::Lexer::next()
{
	skipTrivia();

	xenon::Token token;
	token.Line = _line;

	if (_pos >= _source.size())
	{
		token.Type = xenon::TokenType::End;
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
		token.Type   = xenon::TokenType::Identifier;
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
		token.Type   = xenon::TokenType::Number;
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
		token.Type   = xenon::TokenType::String;
		token.Lexeme = _source.substr(start, _pos - start);
		return token;
	}

	token.Lexeme = _source.substr(start, 1);

	#define XENON_TOKEN_CASE(tokenItem,tokenName) case tokenItem: token.Type = xenon::TokenType::tokenName; break
	#define XENON_DUO_TOKEN_CASE(tokenItem1,tokenItem2,tokenName1,tokenName2) \
	case tokenItem1: \
		if (peek() == tokenItem2) { advance(); token.Lexeme = _source.substr(start, 2); token.Type = xenon::TokenType::tokenName1; } \
		else { token.Type = xenon::TokenType::tokenName2; } break

	switch (c)
	{
		XENON_TOKEN_CASE('(', LeftParen);
		XENON_TOKEN_CASE(')', RightParen);
		XENON_TOKEN_CASE('{', LeftBrace);
		XENON_TOKEN_CASE('}', RightBrace);
		XENON_TOKEN_CASE(',', Comma);
		XENON_TOKEN_CASE(';', Semicolon);
		XENON_TOKEN_CASE('+', Plus);
		XENON_TOKEN_CASE('-', Minus);
		XENON_TOKEN_CASE('*', Star);
		XENON_TOKEN_CASE('/', Slash);
		XENON_DUO_TOKEN_CASE('!', '=', BangEqual, Bang);
		XENON_DUO_TOKEN_CASE('=', '=', EqualEqual, Assign);
		XENON_DUO_TOKEN_CASE('<', '=', LessEqual, Less);
		XENON_DUO_TOKEN_CASE('>', '=', GreaterEqual, Greater);
		XENON_DUO_TOKEN_CASE('&', '&', AndAnd, Invalid);
		XENON_DUO_TOKEN_CASE('|', '|', OrOr, Invalid);
		XENON_DUO_TOKEN_CASE(':', ':', ColonColon, Colon);
		XENON_TOKEN_CASE('.', Dot);
	default:
		token.Type = xenon::TokenType::Invalid;
		break;
	}

	#undef XENON_TOKEN_CASE
	#undef XENON_DUO_TOKEN_CASE

	return token;
}
