#pragma once

#ifndef XENON__PARSER_HPP
#define XENON__PARSER_HPP 1

#include <string>
#include <unordered_set>

#include <xenon/compiler/ast.hpp>
#include <xenon/compiler/lexer.hpp>

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
		struct ParseError { };

		[[noreturn]] void fail(const std::string& message);

		const Token& advance();

		const Token& peekNext();

		bool check(TokenType type) const;

		bool match(TokenType type);

		const Token& consume(TokenType type, const std::string& message);

		/// True when the current token can close a template argument list
		/// (`>` or the first half of `>>`).
		bool checkGreater() const;

		/// Consumes one `>`, splitting a `>>` token when template arguments nest.
		void consumeGreater(const std::string& message);

		bool checkKeyword(std::string_view keyword) const;

		bool matchKeyword(std::string_view keyword);

		bool isType(const Token& token) const;

		void parseProgram(Program& out);

		void parseTopLevelDeclaration(Program& out);

		TemplateDecl parseTemplate();

		FunctionDecl parseFunction();

		void parseFunctionRest(FunctionDecl& function);

		TypeDecl parseTypeDecl();

		std::string parseType();

		/// Parses `[const] type` and reports whether the qualifier was present.
		std::string parseQualifiedType(bool& constant);

		std::unique_ptr<BlockStmt> parseBlock();

		std::unique_ptr<Stmt> parseStatement();

		std::unique_ptr<Stmt> parseVarDecl();

		std::unique_ptr<Stmt> parseForStatement();

		std::unique_ptr<Stmt> parseDoWhileStatement();

		std::unique_ptr<Stmt> parseSwitchStatement();

		std::unique_ptr<Expr> parseExpression();

		std::unique_ptr<Expr> parseAssignment();

		std::unique_ptr<Expr> parseTernary();

		std::unique_ptr<Expr> parseOr();

		std::unique_ptr<Expr> parseAnd();

		std::unique_ptr<Expr> parseBitOr();

		std::unique_ptr<Expr> parseBitXor();

		std::unique_ptr<Expr> parseBitAnd();

		std::unique_ptr<Expr> parseEquality();

		std::unique_ptr<Expr> parseComparison();

		std::unique_ptr<Expr> parseShift();

		std::unique_ptr<Expr> parseTerm();

		std::unique_ptr<Expr> parseFactor();

		std::unique_ptr<Expr> parseUnary();

		std::unique_ptr<Expr> parseCall();

		std::unique_ptr<Expr> parsePrimary();

		Lexer&                          _lexer;
		Token                           _current;
		Token                           _previous;
		Token                           _peek;
		bool                            _hasPeek = false;
		std::unordered_set<std::string> _typeNames;
		std::string                     _error;
	};
}

#endif // !XENON__PARSER_HPP
