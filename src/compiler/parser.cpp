#include <xenon/compiler/parser.hpp>

#include <charconv>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

xenon::Parser::Parser(Lexer& lexer) : _lexer(lexer)
{
	_current = _lexer.next();
	if (_current.Type == xenon::TokenType::Invalid)
	{
		fail("unexpected character");
	}
}

[[noreturn]] void xenon::Parser::fail(const std::string& message)
{
	_error = message + " (line " + std::to_string(_current.Line) + ")";
	throw ParseError{};
}

const xenon::Token& xenon::Parser::advance()
{
	_previous = _current;
	if (_hasPeek)
	{
		_current = _peek;
		_hasPeek = false;
	}
	else
	{
		_current = _lexer.next();
	}
	if (_current.Type == xenon::TokenType::Invalid)
	{
		fail("unexpected character '" + std::string(_current.Lexeme) + "'");
	}
	return _previous;
}

const xenon::Token& xenon::Parser::peekNext()
{
	if (!_hasPeek)
	{
		_peek = _lexer.next();
		_hasPeek = true;
	}
	return _peek;
}

bool xenon::Parser::check(xenon::TokenType type) const
{
	return _current.Type == type;
}

bool xenon::Parser::match(xenon::TokenType type)
{
	if (!check(type))
	{
		return false;
	}
	advance();
	return true;
}

const xenon::Token& xenon::Parser::consume(xenon::TokenType type, const std::string& message)
{
	if (check(type))
	{
		return advance();
	}
	fail(message);
}

bool xenon::Parser::checkKeyword(std::string_view keyword) const
{
	return _current.Type == xenon::TokenType::Identifier && _current.Lexeme == keyword;
}

bool xenon::Parser::matchKeyword(std::string_view keyword)
{
	if (!checkKeyword(keyword))
	{
		return false;
	}
	advance();
	return true;
}

bool xenon::Parser::isType(const Token& token) const
{
	if (token.Type != xenon::TokenType::Identifier)
	{
		return false;
	}
	if (token.Lexeme == "int" || token.Lexeme == "double" || token.Lexeme == "bool" ||
		token.Lexeme == "string" || token.Lexeme == "void")
	{
		return true;
	}
	return _typeNames.contains(std::string(token.Lexeme));
}

bool xenon::Parser::parse(xenon::Program& out)
{
	try
	{
		parseProgram(out);
		return true;
	}
	catch (const ParseError&)
	{
		return false;
	}
}

void xenon::Parser::parseProgram(xenon::Program& out)
{
	while (!check(xenon::TokenType::End))
	{
		if (checkKeyword("template"))
		{
			out.Templates.push_back(parseTemplate());
		}
		else if (checkKeyword("struct") || checkKeyword("class") || checkKeyword("enum"))
		{
			out.TypeDecls.push_back(parseTypeDecl());
		}
		else
		{
			parseTopLevelDeclaration(out);
		}
	}
}

void xenon::Parser::parseTopLevelDeclaration(xenon::Program& out)
{
	const bool exported = matchKeyword("export");
	const bool constant = matchKeyword("const");

	const std::string type = parseType();
	const std::string name = std::string(consume(TokenType::Identifier, "expected a declaration name").Lexeme);

	if (match(xenon::TokenType::LeftParen))
	{
		xenon::FunctionDecl function;
		function.Exported   = exported;
		function.ReturnType = type;
		function.Name       = name;
		parseFunctionRest(function);
		out.Functions.push_back(std::move(function));
		return;
	}

	xenon::GlobalVarDecl global;
	global.Type     = type;
	global.Name     = name;
	global.Constant = constant;
	if (match(xenon::TokenType::Assign))
	{
		global.Init = parseExpression();
	}
	consume(xenon::TokenType::Semicolon, "expected ';' after global variable declaration");
	out.Globals.push_back(std::move(global));
}

xenon::TemplateDecl xenon::Parser::parseTemplate()
{
	xenon::TemplateDecl templ;

	advance(); // consume 'template'
	consume(xenon::TokenType::Less, "expected '<' after 'template'");

	if (!checkGreater())
	{
		do
		{
			if (!matchKeyword("typename") && !matchKeyword("class"))
			{
				fail("expected 'typename' or 'class' in template parameters");
			}
			templ.Params.push_back(std::string(consume(xenon::TokenType::Identifier, "expected a template parameter name").Lexeme));
		} while (match(xenon::TokenType::Comma));
	}
	consumeGreater("expected '>' after template parameters");

	if (checkKeyword("enum"))
	{
		fail("template enums are not supported");
	}

	for (const auto& param : templ.Params)
	{
		_typeNames.insert(param);
	}

	if (checkKeyword("struct") || checkKeyword("class"))
	{
		templ.Type = parseTypeDecl();
		templ.IsFunction = false;
	}
	else
	{
		templ.Function = parseFunction();
		templ.IsFunction = true;
	}

	for (const auto& param : templ.Params)
	{
		_typeNames.erase(param);
	}

	return templ;
}

xenon::FunctionDecl xenon::Parser::parseFunction()
{
	xenon::FunctionDecl function;
	function.Exported = matchKeyword("export");

	function.ReturnType = parseType();
	function.Name       = std::string(consume(xenon::TokenType::Identifier, "expected a function name").Lexeme);

	consume(xenon::TokenType::LeftParen, "expected '(' after function name");
	parseFunctionRest(function);
	return function;
}

void xenon::Parser::parseFunctionRest(xenon::FunctionDecl& function)
{
	if (!check(xenon::TokenType::RightParen))
	{
		do
		{
			const std::string paramType = parseType();
			const std::string paramName = std::string(consume(xenon::TokenType::Identifier, "expected a parameter name").Lexeme);
			function.Params.emplace_back(paramType, paramName);
		} while (match(xenon::TokenType::Comma));
	}

	consume(xenon::TokenType::RightParen, "expected ')' after parameters");

	if (check(xenon::TokenType::LeftBrace))
	{
		function.HasBody = true;
		function.Body = parseBlock();
	}
	else
	{
		function.HasBody = false;
		consume(xenon::TokenType::Semicolon, "expected '{' or ';' after function signature");
	}
}

xenon::TypeDecl xenon::Parser::parseTypeDecl()
{
	xenon::TypeDecl decl;
	const std::string keyword = std::string(advance().Lexeme);

	decl.Kind = keyword == "struct"  ? xenon::DeclKind::Struct : keyword == "class" ? xenon::DeclKind::Class : xenon::DeclKind::Enum;

	decl.Name = std::string(consume(xenon::TokenType::Identifier, "expected a type name").Lexeme);
	_typeNames.insert(decl.Name);

	decl.Signature = keyword + " " + decl.Name;

	if (decl.Kind == xenon::DeclKind::Enum)
	{
		consume(xenon::TokenType::LeftBrace, "expected '{' after enum name");
		while (!check(xenon::TokenType::RightBrace) && !check(xenon::TokenType::End))
		{
			decl.Enumerators.push_back(std::string(consume(xenon::TokenType::Identifier, "expected an enumerator name").Lexeme));
			if (!match(xenon::TokenType::Comma))
			{
				break;
			}
		}
		consume(xenon::TokenType::RightBrace, "expected '}' after enumerators");
		consume(xenon::TokenType::Semicolon, "expected ';' after enum");
		return decl;
	}

	consume(xenon::TokenType::LeftBrace, "expected '{' after type name");

	while (!check(xenon::TokenType::RightBrace) && !check(xenon::TokenType::End))
	{
		const std::string memberType = parseType();
		const std::string memberName = std::string(consume(xenon::TokenType::Identifier, "expected a member name").Lexeme);

		if (match(xenon::TokenType::LeftParen))
		{
			xenon::FunctionDecl method;
			method.ReturnType = memberType;
			method.Name       = memberName;

			if (!check(TokenType::RightParen))
			{
				do
				{
					const std::string paramType = parseType();
					const std::string paramName = std::string(consume(xenon::TokenType::Identifier, "expected a parameter name").Lexeme);
					method.Params.emplace_back(paramType, paramName);
				} while (match(xenon::TokenType::Comma));
			}
			consume(xenon::TokenType::RightParen, "expected ')' after parameters");

			if (check(xenon::TokenType::LeftBrace))
			{
				method.HasBody = true;
				method.Body = parseBlock();
			}
			else
			{
				method.HasBody = false;
				consume(xenon::TokenType::Semicolon, "expected '{' or ';' after method signature");
			}
			decl.Methods.push_back(std::move(method));
		}
		else
		{
			decl.Fields.push_back(FieldDecl{memberType, memberName});
			if (match(xenon::TokenType::Assign))
			{
				while (!check(xenon::TokenType::Semicolon) && !check(xenon::TokenType::Comma) &&
					   !check(xenon::TokenType::End))
				{
					advance();
				}
			}
			while (match(xenon::TokenType::Comma))
			{
				const std::string name = std::string(
					consume(xenon::TokenType::Identifier, "expected a field name").Lexeme);
				decl.Fields.push_back(FieldDecl{memberType, name});
				if (match(xenon::TokenType::Assign))
				{
					while (!check(xenon::TokenType::Semicolon) && !check(xenon::TokenType::Comma) &&
						   !check(xenon::TokenType::End))
					{
						advance();
					}
				}
			}
			consume(xenon::TokenType::Semicolon, "expected ';' after field declaration");
		}
	}

	consume(xenon::TokenType::RightBrace, "expected '}' after type body");
	consume(xenon::TokenType::Semicolon, "expected ';' after type declaration");
	return decl;
}

std::string xenon::Parser::parseType()
{
	if (!isType(_current))
	{
		fail("expected a type");
	}
	std::string type(_current.Lexeme);
	advance();

	if (match(xenon::TokenType::Less))
	{
		type += '<';
		do
		{
			type += parseType();
		} while (match(xenon::TokenType::Comma));
		consumeGreater("expected '>' after template arguments");
		type += '>';
	}

	return type;
}

std::unique_ptr<xenon::BlockStmt> xenon::Parser::parseBlock()
{
	auto block = std::make_unique<xenon::BlockStmt>();
	block->Line = _current.Line;

	consume(xenon::TokenType::LeftBrace, "expected '{'");
	while (!check(xenon::TokenType::RightBrace) && !check(xenon::TokenType::End))
	{
		block->Statements.push_back(parseStatement());
	}
	consume(xenon::TokenType::RightBrace, "expected '}'");
	return block;
}

std::unique_ptr<xenon::Stmt> xenon::Parser::parseStatement()
{
	if (check(xenon::TokenType::LeftBrace))
	{
		return parseBlock();
	}

	if (matchKeyword("return"))
	{
		auto stmt = std::make_unique<xenon::ReturnStmt>();
		stmt->Line = _previous.Line;
		if (!check(xenon::TokenType::Semicolon))
		{
			stmt->Value = parseExpression();
		}
		consume(xenon::TokenType::Semicolon, "expected ';' after return");
		return stmt;
	}

	if (matchKeyword("if"))
	{
		auto stmt = std::make_unique<IfStmt>();
		stmt->Line = _previous.Line;
		consume(xenon::TokenType::LeftParen, "expected '(' after 'if'");
		stmt->Condition = parseExpression();
		consume(xenon::TokenType::RightParen, "expected ')' after condition");
		stmt->ThenBranch = parseStatement();
		if (matchKeyword("else"))
		{
			stmt->ElseBranch = parseStatement();
		}
		return stmt;
	}

	if (matchKeyword("while"))
	{
		auto stmt = std::make_unique<WhileStmt>();
		stmt->Line = _previous.Line;
		consume(xenon::TokenType::LeftParen, "expected '(' after 'while'");
		stmt->Condition = parseExpression();
		consume(xenon::TokenType::RightParen, "expected ')' after condition");
		stmt->Body = parseStatement();
		return stmt;
	}

	if (matchKeyword("for"))
	{
		return parseForStatement();
	}

	if (matchKeyword("do"))
	{
		return parseDoWhileStatement();
	}

	if (matchKeyword("switch"))
	{
		return parseSwitchStatement();
	}

	if (matchKeyword("break"))
	{
		auto stmt = std::make_unique<xenon::BreakStmt>();
		stmt->Line = _previous.Line;
		consume(xenon::TokenType::Semicolon, "expected ';' after 'break'");
		return stmt;
	}

	if (matchKeyword("continue"))
	{
		auto stmt = std::make_unique<xenon::ContinueStmt>();
		stmt->Line = _previous.Line;
		consume(xenon::TokenType::Semicolon, "expected ';' after 'continue'");
		return stmt;
	}

	if (isType(_current))
	{
		return parseVarDecl();
	}

	auto stmt = std::make_unique<xenon::ExprStmt>();
	stmt->Line = _current.Line;
	stmt->Expression = parseExpression();
	consume(xenon::TokenType::Semicolon, "expected ';' after expression");
	return stmt;
}

std::unique_ptr<xenon::Stmt> xenon::Parser::parseVarDecl()
{
	auto stmt = std::make_unique<xenon::VarDeclStmt>();
	stmt->Line = _current.Line;

	stmt->Type = parseType();
	if (stmt->Type == "void")
	{
		fail("'void' is not a valid variable type");
	}
	stmt->Name = std::string(consume(xenon::TokenType::Identifier, "expected a variable name").Lexeme);

	if (match(xenon::TokenType::Assign))
	{
		stmt->Init = parseExpression();
	}
	consume(xenon::TokenType::Semicolon, "expected ';' after variable declaration");
	return stmt;
}

std::unique_ptr<xenon::Stmt> xenon::Parser::parseForStatement()
{
	auto stmt = std::make_unique<xenon::ForStmt>();
	stmt->Line = _previous.Line;

	consume(xenon::TokenType::LeftParen, "expected '(' after 'for'");

	if (!check(xenon::TokenType::Semicolon))
	{
		if (isType(_current))
		{
			stmt->Init = parseVarDecl();
		}
		else
		{
			auto expr = std::make_unique<xenon::ExprStmt>();
			expr->Expression = parseExpression();
			consume(xenon::TokenType::Semicolon, "expected ';' after for initializer");
			stmt->Init = std::move(expr);
		}
	}
	else
	{
		advance(); // consume ';'
	}

	if (!check(xenon::TokenType::Semicolon))
	{
		stmt->Condition = parseExpression();
	}
	consume(xenon::TokenType::Semicolon, "expected ';' after for condition");

	if (!check(xenon::TokenType::RightParen))
	{
		stmt->Increment = parseExpression();
	}
	consume(xenon::TokenType::RightParen, "expected ')' after for clauses");

	stmt->Body = parseStatement();
	return stmt;
}

std::unique_ptr<xenon::Stmt> xenon::Parser::parseDoWhileStatement()
{
	auto stmt = std::make_unique<xenon::DoWhileStmt>();
	stmt->Line = _previous.Line;

	stmt->Body = parseStatement();

	if (!matchKeyword("while"))
	{
		fail("expected 'while' after do body");
	}
	consume(xenon::TokenType::LeftParen, "expected '(' after 'while'");
	stmt->Condition = parseExpression();
	consume(xenon::TokenType::RightParen, "expected ')' after condition");
	consume(xenon::TokenType::Semicolon, "expected ';' after do/while");
	return stmt;
}

std::unique_ptr<xenon::Stmt> xenon::Parser::parseSwitchStatement()
{
	auto stmt = std::make_unique<xenon::SwitchStmt>();
	stmt->Line = _previous.Line;

	consume(xenon::TokenType::LeftParen, "expected '(' after 'switch'");
	stmt->Condition = parseExpression();
	consume(xenon::TokenType::RightParen, "expected ')' after switch condition");

	consume(xenon::TokenType::LeftBrace, "expected '{' after switch");

	CaseClause* current = nullptr;
	while (!check(xenon::TokenType::RightBrace) && !check(xenon::TokenType::End))
	{
		if (matchKeyword("case"))
		{
			stmt->Cases.push_back(CaseClause{});
			current = &stmt->Cases.back();
			current->Value = parseExpression();
			consume(xenon::TokenType::Colon, "expected ':' after case value");
		}
		else if (matchKeyword("default"))
		{
			stmt->Cases.push_back(CaseClause{});
			current = &stmt->Cases.back();
			consume(xenon::TokenType::Colon, "expected ':' after 'default'");
		}
		else
		{
			if (current == nullptr)
			{
				fail("statement outside of a case");
			}
			current->Statements.push_back(parseStatement());
		}
	}

	consume(xenon::TokenType::RightBrace, "expected '}' after switch");
	return stmt;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseExpression()
{
	return parseAssignment();
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseAssignment()
{
	auto expr = parseTernary();

	xenon::CompoundOp operation = xenon::CompoundOp::Add;
	bool              compound  = false;

	switch (_current.Type)
	{
		case xenon::TokenType::PlusAssign:      operation = xenon::CompoundOp::Add;      compound = true; break;
		case xenon::TokenType::MinusAssign:     operation = xenon::CompoundOp::Subtract; compound = true; break;
		case xenon::TokenType::StarAssign:      operation = xenon::CompoundOp::Multiply; compound = true; break;
		case xenon::TokenType::SlashAssign:     operation = xenon::CompoundOp::Divide;   compound = true; break;
		case xenon::TokenType::PercentAssign:   operation = xenon::CompoundOp::Modulo;   compound = true; break;
		case xenon::TokenType::AmpAssign:       operation = xenon::CompoundOp::BitAnd;   compound = true; break;
		case xenon::TokenType::PipeAssign:      operation = xenon::CompoundOp::BitOr;    compound = true; break;
		case xenon::TokenType::CaretAssign:     operation = xenon::CompoundOp::BitXor;   compound = true; break;
		case xenon::TokenType::ShiftLeftAssign: operation = xenon::CompoundOp::ShiftLeft;  compound = true; break;
		case xenon::TokenType::ShiftRightAssign: operation = xenon::CompoundOp::ShiftRight; compound = true; break;
		default:
			break;
	}

	if (compound)
	{
		advance(); // consume the compound assignment operator
	}
	else if (!match(xenon::TokenType::Assign))
	{
		return expr;
	}

	{
		const int line = _previous.Line;

		if (auto* variable = dynamic_cast<xenon::VariableExpr*>(expr.get()))
		{
			auto assign      = std::make_unique<xenon::AssignExpr>();
			assign->Line     = line;
			assign->Name     = std::move(variable->Name);
			assign->Compound = compound;
			assign->Operation = operation;
			assign->Value    = parseAssignment();
			return assign;
		}
		if (auto* field = dynamic_cast<xenon::FieldAccessExpr*>(expr.get()))
		{
			auto assign        = std::make_unique<xenon::FieldAssignExpr>();
			assign->Line       = line;
			assign->Object     = std::move(field->Object);
			assign->FieldName  = std::move(field->FieldName);
			assign->Compound   = compound;
			assign->Operation  = operation;
			assign->Value      = parseAssignment();
			return assign;
		}
		fail("invalid assignment target");
	}
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseTernary()
{
	auto expr = parseOr();

	if (match(xenon::TokenType::Question))
	{
		auto ternary       = std::make_unique<xenon::TernaryExpr>();
		ternary->Line      = _previous.Line;
		ternary->Condition = std::move(expr);
		ternary->Then      = parseAssignment();
		consume(xenon::TokenType::Colon, "expected ':' in the ternary operator");
		ternary->Else      = parseAssignment();
		return ternary;
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseOr()
{
	auto expr = parseAnd();

	while (match(xenon::TokenType::OrOr))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = xenon::BinaryExpr::Op::Or;
		binary->Left      = std::move(expr);
		binary->Right     = parseAnd();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseAnd()
{
	auto expr = parseBitOr();

	while (match(xenon::TokenType::AndAnd))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = xenon::BinaryExpr::Op::And;
		binary->Left      = std::move(expr);
		binary->Right     = parseBitOr();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseBitOr()
{
	auto expr = parseBitXor();

	while (match(xenon::TokenType::Pipe))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = xenon::BinaryExpr::Op::BitOr;
		binary->Left      = std::move(expr);
		binary->Right     = parseBitXor();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseBitXor()
{
	auto expr = parseBitAnd();

	while (match(xenon::TokenType::Caret))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = xenon::BinaryExpr::Op::BitXor;
		binary->Left      = std::move(expr);
		binary->Right     = parseBitAnd();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseBitAnd()
{
	auto expr = parseEquality();

	while (match(xenon::TokenType::Ampersand))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = xenon::BinaryExpr::Op::BitAnd;
		binary->Left      = std::move(expr);
		binary->Right     = parseEquality();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseEquality()
{
	auto expr = parseComparison();

	while (match(xenon::TokenType::EqualEqual) || match(xenon::TokenType::BangEqual))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = _previous.Type == xenon::TokenType::EqualEqual ? xenon::BinaryExpr::Op::Equal : xenon::BinaryExpr::Op::NotEqual;
		binary->Left      = std::move(expr);
		binary->Right     = parseComparison();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseComparison()
{
	auto expr = parseShift();

	while (match(xenon::TokenType::Less) || match(xenon::TokenType::LessEqual) || match(xenon::TokenType::Greater) || match(xenon::TokenType::GreaterEqual))
	{
		xenon::BinaryExpr::Op op = xenon::BinaryExpr::Op::Less;
		switch (_previous.Type)
		{
		case xenon::TokenType::Less:
			op = xenon::BinaryExpr::Op::Less;
			break;
		case xenon::TokenType::LessEqual:
			op = xenon::BinaryExpr::Op::LessEqual;
			break;
		case xenon::TokenType::Greater:
			op = xenon::BinaryExpr::Op::Greater;
			break;
		default:
			op = xenon::BinaryExpr::Op::GreaterEqual;
			break;
		}

		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = op;
		binary->Left      = std::move(expr);
		binary->Right     = parseShift();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseShift()
{
	auto expr = parseTerm();

	while (match(xenon::TokenType::ShiftLeft) || match(xenon::TokenType::ShiftRight))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = _previous.Type == xenon::TokenType::ShiftLeft ? xenon::BinaryExpr::Op::ShiftLeft
																		  : xenon::BinaryExpr::Op::ShiftRight;
		binary->Left      = std::move(expr);
		binary->Right     = parseTerm();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseTerm()
{
	auto expr = parseFactor();

	while (match(xenon::TokenType::Plus) || match(xenon::TokenType::Minus))
	{
		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = _previous.Type == xenon::TokenType::Plus ? xenon::BinaryExpr::Op::Add : xenon::BinaryExpr::Op::Subtract;
		binary->Left      = std::move(expr);
		binary->Right     = parseFactor();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseFactor()
{
	auto expr = parseUnary();

	while (match(xenon::TokenType::Star) || match(xenon::TokenType::Slash) || match(xenon::TokenType::Percent))
	{
		xenon::BinaryExpr::Op op = xenon::BinaryExpr::Op::Multiply;
		if (_previous.Type == xenon::TokenType::Slash)
		{
			op = xenon::BinaryExpr::Op::Divide;
		}
		else if (_previous.Type == xenon::TokenType::Percent)
		{
			op = xenon::BinaryExpr::Op::Modulo;
		}

		auto binary       = std::make_unique<xenon::BinaryExpr>();
		binary->Operation = op;
		binary->Left      = std::move(expr);
		binary->Right     = parseUnary();
		binary->Line      = binary->Right->Line;
		expr = std::move(binary);
	}

	return expr;
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseUnary()
{
	if (matchKeyword("new"))
	{
		auto expr      = std::make_unique<xenon::NewExpr>();
		expr->TypeName = parseType();
		consume(xenon::TokenType::LeftParen, "expected '(' after type name in 'new'");
		consume(xenon::TokenType::RightParen, "expected ')' (constructor arguments not supported yet)");
		expr->Line = _previous.Line;
		return expr;
	}
	if (match(xenon::TokenType::Minus))
	{
		auto unary       = std::make_unique<xenon::UnaryExpr>();
		unary->Operation = xenon::UnaryExpr::Op::Negate;
		unary->Line      = _previous.Line;
		unary->Operand   = parseUnary();
		return unary;
	}
	if (match(xenon::TokenType::Bang))
	{
		auto unary       = std::make_unique<xenon::UnaryExpr>();
		unary->Operation = xenon::UnaryExpr::Op::Not;
		unary->Line      = _previous.Line;
		unary->Operand   = parseUnary();
		return unary;
	}
	if (match(xenon::TokenType::Tilde))
	{
		auto unary       = std::make_unique<xenon::UnaryExpr>();
		unary->Operation = xenon::UnaryExpr::Op::BitNot;
		unary->Line      = _previous.Line;
		unary->Operand   = parseUnary();
		return unary;
	}
	if (match(xenon::TokenType::PlusPlus) || match(xenon::TokenType::MinusMinus))
	{
		auto unary       = std::make_unique<xenon::UnaryExpr>();
		unary->Operation = _previous.Type == xenon::TokenType::PlusPlus ? xenon::UnaryExpr::Op::PreIncrement
																	   : xenon::UnaryExpr::Op::PreDecrement;
		unary->Line      = _previous.Line;
		unary->Operand   = parseUnary();
		return unary;
	}
	return parseCall();
}

std::unique_ptr<xenon::Expr> xenon::Parser::parseCall()
{
	auto expr = parsePrimary();

	while (true)
	{
		if (match(xenon::TokenType::LeftParen))
		{
			auto* variable = dynamic_cast<xenon::VariableExpr*>(expr.get());
			if (variable == nullptr)
			{
				fail("only named functions and types can be called");
			}

			auto call    = std::make_unique<xenon::CallExpr>();
			call->Callee = std::move(variable->Name);
			call->Line   = _previous.Line;

			if (!check(xenon::TokenType::RightParen))
			{
				do
				{
					call->Args.push_back(parseExpression());
				} while (match(xenon::TokenType::Comma));
			}
			consume(xenon::TokenType::RightParen, "expected ')' after arguments");

			expr = std::move(call);
		}
		else if (match(xenon::TokenType::Dot))
		{
			auto access       = std::make_unique<FieldAccessExpr>();
			access->Object    = std::move(expr);
			access->FieldName = std::string(consume(xenon::TokenType::Identifier, "expected a field name").Lexeme);
			access->Line      = _previous.Line;
			expr = std::move(access);
		}
		else if (match(xenon::TokenType::ColonColon))
		{
			auto* variable = dynamic_cast<xenon::VariableExpr*>(expr.get());
			if (variable == nullptr)
			{
				fail("expected a type or module name before '::'");
			}
			const std::string memberName = std::string(consume(xenon::TokenType::Identifier, "expected a member name").Lexeme);

			auto target  = std::make_unique<xenon::VariableExpr>();
			target->Name = variable->Name + "::" + memberName;
			target->Line = _previous.Line;
			expr         = std::move(target);
		}
		else if (check(xenon::TokenType::Less))
		{
			auto* variable = dynamic_cast<xenon::VariableExpr*>(expr.get());
			if (variable == nullptr || !isType(peekNext()))
			{
				break;
			}

			advance(); // consume '<'
			std::string name = std::move(variable->Name);
			name += '<';
			do
			{
				name += parseType();
			} while (match(xenon::TokenType::Comma));
			consumeGreater("expected '>' after template arguments");
			name += '>';

			auto target  = std::make_unique<xenon::VariableExpr>();
			target->Name = std::move(name);
			target->Line = _previous.Line;
			expr         = std::move(target);
		}
		else if (match(xenon::TokenType::PlusPlus) || match(xenon::TokenType::MinusMinus))
		{
			auto postfix      = std::make_unique<xenon::PostfixExpr>();
			postfix->Increment = _previous.Type == xenon::TokenType::PlusPlus;
			postfix->Line      = _previous.Line;
			postfix->Operand   = std::move(expr);
			expr               = std::move(postfix);
		}
		else
		{
			break;
		}
	}

	return expr;
}

bool xenon::Parser::checkGreater() const
{
	return _current.Type == xenon::TokenType::Greater || _current.Type == xenon::TokenType::ShiftRight;
}

void xenon::Parser::consumeGreater(const std::string& message)
{
	if (_current.Type == xenon::TokenType::Greater)
	{
		advance();
		return;
	}
	if (_current.Type == xenon::TokenType::ShiftRight)
	{
		// Split `>>` so nested template argument lists close one level at a time.
		_current.Type   = xenon::TokenType::Greater;
		_current.Lexeme = _current.Lexeme.substr(0, 1);
		_previous       = _current;
		return;
	}
	fail(message);
}

std::unique_ptr<xenon::Expr> xenon::Parser::parsePrimary()
{
	if (match(xenon::TokenType::Number))
	{
		auto expr  = std::make_unique<xenon::NumberExpr>();
		expr->Line = _previous.Line;

		double value = 0.0;
		const auto result = std::from_chars(_previous.Lexeme.data(), _previous.Lexeme.data() + _previous.Lexeme.size(), value);
		if (result.ec != std::errc())
		{
			fail("invalid number literal");
		}
		expr->Value = value;
		return expr;
	}

	if (match(xenon::TokenType::String))
	{
		auto expr = std::make_unique<xenon::StringExpr>();
		expr->Line = _previous.Line;
		const std::string_view lexeme = _previous.Lexeme;
		expr->Value = lexeme.size() >= 2 ? std::string(lexeme.substr(1, lexeme.size() - 2)) : std::string();
		return expr;
	}

	if (matchKeyword("true"))
	{
		auto expr = std::make_unique<xenon::BoolExpr>();
		expr->Value = true;
		expr->Line = _previous.Line;
		return expr;
	}

	if (matchKeyword("false"))
	{
		auto expr = std::make_unique<xenon::BoolExpr>();
		expr->Value = false;
		expr->Line = _previous.Line;
		return expr;
	}

	if (matchKeyword("null"))
	{
		auto expr = std::make_unique<xenon::NullExpr>();
		expr->Line = _previous.Line;
		return expr;
	}

	if (match(xenon::TokenType::Identifier))
	{
		auto expr  = std::make_unique<xenon::VariableExpr>();
		expr->Name = std::string(_previous.Lexeme);
		expr->Line = _previous.Line;
		return expr;
	}

	if (match(xenon::TokenType::LeftParen))
	{
		auto expr = parseExpression();
		consume(xenon::TokenType::RightParen, "expected ')' after expression");
		return expr;
	}

	fail("expected an expression");
}
