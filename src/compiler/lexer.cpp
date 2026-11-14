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

	const auto accept = [this](char expected) -> bool
	{
		if (peek() != expected)
		{
			return false;
		}
		advance();
		return true;
	};

	const auto complete = [this, &token, start](xenon::TokenType type) -> xenon::Token
	{
		token.Type   = type;
		token.Lexeme = _source.substr(start, _pos - start);
		return token;
	};

	switch (c)
	{
		case '(': return complete(xenon::TokenType::LeftParen);
		case ')': return complete(xenon::TokenType::RightParen);
		case '{': return complete(xenon::TokenType::LeftBrace);
		case '}': return complete(xenon::TokenType::RightBrace);
		case ',': return complete(xenon::TokenType::Comma);
		case ';': return complete(xenon::TokenType::Semicolon);
		case '.': return complete(xenon::TokenType::Dot);
		case '?': return complete(xenon::TokenType::Question);
		case '~': return complete(xenon::TokenType::Tilde);

		case '+':
			if (accept('+'))
			{
				return complete(xenon::TokenType::PlusPlus);
			}
			return complete(accept('=') ? xenon::TokenType::PlusAssign : xenon::TokenType::Plus);

		case '-':
			if (accept('-'))
			{
				return complete(xenon::TokenType::MinusMinus);
			}
			return complete(accept('=') ? xenon::TokenType::MinusAssign : xenon::TokenType::Minus);

		case '*':
			return complete(accept('=') ? xenon::TokenType::StarAssign : xenon::TokenType::Star);

		case '/':
			return complete(accept('=') ? xenon::TokenType::SlashAssign : xenon::TokenType::Slash);

		case '%':
			return complete(accept('=') ? xenon::TokenType::PercentAssign : xenon::TokenType::Percent);

		case '!':
			return complete(accept('=') ? xenon::TokenType::BangEqual : xenon::TokenType::Bang);

		case '=':
			return complete(accept('=') ? xenon::TokenType::EqualEqual : xenon::TokenType::Assign);

		case '<':
			if (accept('<'))
			{
				return complete(accept('=') ? xenon::TokenType::ShiftLeftAssign : xenon::TokenType::ShiftLeft);
			}
			return complete(accept('=') ? xenon::TokenType::LessEqual : xenon::TokenType::Less);

		case '>':
			if (accept('>'))
			{
				return complete(accept('=') ? xenon::TokenType::ShiftRightAssign : xenon::TokenType::ShiftRight);
			}
			return complete(accept('=') ? xenon::TokenType::GreaterEqual : xenon::TokenType::Greater);

		case '&':
			if (accept('&'))
			{
				return complete(xenon::TokenType::AndAnd);
			}
			return complete(accept('=') ? xenon::TokenType::AmpAssign : xenon::TokenType::Ampersand);

		case '|':
			if (accept('|'))
			{
				return complete(xenon::TokenType::OrOr);
			}
			return complete(accept('=') ? xenon::TokenType::PipeAssign : xenon::TokenType::Pipe);

		case '^':
			return complete(accept('=') ? xenon::TokenType::CaretAssign : xenon::TokenType::Caret);

		case ':':
			return complete(accept(':') ? xenon::TokenType::ColonColon : xenon::TokenType::Colon);

		default:
			token.Type   = xenon::TokenType::Invalid;
			token.Lexeme = _source.substr(start, _pos - start);
			return token;
	}
}
