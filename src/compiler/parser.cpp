#include "xenon/compiler/parser.hpp"

#include <charconv>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace xenon
{

Parser::Parser(Lexer& lexer) : _lexer(lexer)
{
    _current = _lexer.next();
    if (_current.Type == TokenType::Invalid)
    {
        fail("unexpected character");
    }
}

[[noreturn]] void Parser::fail(const std::string& message)
{
    _error = message + " (line " + std::to_string(_current.Line) + ")";
    throw ParseError{};
}

const Token& Parser::advance()
{
    _previous = _current;
    _current = _lexer.next();
    if (_current.Type == TokenType::Invalid)
    {
        fail("unexpected character '" + std::string(_current.Lexeme) + "'");
    }
    return _previous;
}

bool Parser::check(TokenType type) const
{
    return _current.Type == type;
}

bool Parser::match(TokenType type)
{
    if (!check(type))
    {
        return false;
    }
    advance();
    return true;
}

const Token& Parser::consume(TokenType type, const std::string& message)
{
    if (check(type))
    {
        return advance();
    }
    fail(message);
}

bool Parser::checkKeyword(std::string_view keyword) const
{
    return _current.Type == TokenType::Identifier && _current.Lexeme == keyword;
}

bool Parser::matchKeyword(std::string_view keyword)
{
    if (!checkKeyword(keyword))
    {
        return false;
    }
    advance();
    return true;
}

bool Parser::isType(const Token& token) const
{
    if (token.Type != TokenType::Identifier)
    {
        return false;
    }
    return token.Lexeme == "int" || token.Lexeme == "double" || token.Lexeme == "bool" ||
           token.Lexeme == "void";
}

bool Parser::parse(Program& out)
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

void Parser::parseProgram(Program& out)
{
    while (!check(TokenType::End))
    {
        if (checkKeyword("struct") || checkKeyword("class") || checkKeyword("enum"))
        {
            out.TypeDecls.push_back(parseTypeDecl());
        }
        else
        {
            out.Functions.push_back(parseFunction());
        }
    }
}

FunctionDecl Parser::parseFunction()
{
    FunctionDecl function;
    function.Exported = matchKeyword("export");

    function.ReturnType = parseType();
    function.Name = std::string(consume(TokenType::Identifier, "expected a function name").Lexeme);

    consume(TokenType::LeftParen, "expected '(' after function name");

    if (!check(TokenType::RightParen))
    {
        do
        {
            const std::string paramType = parseType();
            const std::string paramName =
                std::string(consume(TokenType::Identifier, "expected a parameter name").Lexeme);
            function.Params.emplace_back(paramType, paramName);
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected ')' after parameters");

    if (check(TokenType::LeftBrace))
    {
        function.HasBody = true;
        function.Body = parseBlock();
    }
    else
    {
        function.HasBody = false;
        consume(TokenType::Semicolon, "expected '{' or ';' after function signature");
    }
    return function;
}

TypeDecl Parser::parseTypeDecl()
{
    TypeDecl decl;
    const std::string keyword = std::string(advance().Lexeme);

    decl.Kind = keyword == "struct"  ? DeclKind::Struct
                : keyword == "class" ? DeclKind::Class
                                     : DeclKind::Enum;

    decl.Name = std::string(consume(TokenType::Identifier, "expected a type name").Lexeme);

    std::string signature = keyword + " " + decl.Name;
    if (match(TokenType::LeftBrace))
    {
        int depth = 1;
        while (depth > 0 && !check(TokenType::End))
        {
            if (match(TokenType::LeftBrace))
            {
                ++depth;
            }
            else if (match(TokenType::RightBrace))
            {
                --depth;
            }
            else
            {
                advance();
            }
        }
        signature += " { ... }";
    }
    consume(TokenType::Semicolon, "expected ';' after declaration");
    decl.Signature = signature;
    return decl;
}

std::string Parser::parseType()
{
    if (!isType(_current))
    {
        fail("expected a type");
    }
    const std::string type(_current.Lexeme);
    advance();
    return type;
}

std::unique_ptr<BlockStmt> Parser::parseBlock()
{
    auto block = std::make_unique<BlockStmt>();
    block->Line = _current.Line;

    consume(TokenType::LeftBrace, "expected '{'");
    while (!check(TokenType::RightBrace) && !check(TokenType::End))
    {
        block->Statements.push_back(parseStatement());
    }
    consume(TokenType::RightBrace, "expected '}'");
    return block;
}

std::unique_ptr<Stmt> Parser::parseStatement()
{
    if (check(TokenType::LeftBrace))
    {
        return parseBlock();
    }

    if (matchKeyword("return"))
    {
        auto stmt = std::make_unique<ReturnStmt>();
        stmt->Line = _previous.Line;
        if (!check(TokenType::Semicolon))
        {
            stmt->Value = parseExpression();
        }
        consume(TokenType::Semicolon, "expected ';' after return");
        return stmt;
    }

    if (matchKeyword("if"))
    {
        auto stmt = std::make_unique<IfStmt>();
        stmt->Line = _previous.Line;
        consume(TokenType::LeftParen, "expected '(' after 'if'");
        stmt->Condition = parseExpression();
        consume(TokenType::RightParen, "expected ')' after condition");
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
        consume(TokenType::LeftParen, "expected '(' after 'while'");
        stmt->Condition = parseExpression();
        consume(TokenType::RightParen, "expected ')' after condition");
        stmt->Body = parseStatement();
        return stmt;
    }

    if (isType(_current))
    {
        return parseVarDecl();
    }

    auto stmt = std::make_unique<ExprStmt>();
    stmt->Line = _current.Line;
    stmt->Expr = parseExpression();
    consume(TokenType::Semicolon, "expected ';' after expression");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseVarDecl()
{
    auto stmt = std::make_unique<VarDeclStmt>();
    stmt->Line = _current.Line;

    stmt->Type = parseType();
    if (stmt->Type == "void")
    {
        fail("'void' is not a valid variable type");
    }
    stmt->Name = std::string(consume(TokenType::Identifier, "expected a variable name").Lexeme);

    if (match(TokenType::Assign))
    {
        stmt->Init = parseExpression();
    }
    consume(TokenType::Semicolon, "expected ';' after variable declaration");
    return stmt;
}

std::unique_ptr<Expr> Parser::parseExpression()
{
    return parseAssignment();
}

std::unique_ptr<Expr> Parser::parseAssignment()
{
    auto expr = parseOr();

    if (match(TokenType::Assign))
    {
        auto* variable = dynamic_cast<VariableExpr*>(expr.get());
        if (variable == nullptr)
        {
            fail("invalid assignment target");
        }
        auto assign = std::make_unique<AssignExpr>();
        assign->Line = _previous.Line;
        assign->Name = std::move(variable->Name);
        assign->Value = parseAssignment();
        return assign;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseOr()
{
    auto expr = parseAnd();

    while (match(TokenType::OrOr))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->Operation = BinaryExpr::Op::Or;
        binary->Left = std::move(expr);
        binary->Right = parseAnd();
        binary->Line = binary->Right->Line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseAnd()
{
    auto expr = parseEquality();

    while (match(TokenType::AndAnd))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->Operation = BinaryExpr::Op::And;
        binary->Left = std::move(expr);
        binary->Right = parseEquality();
        binary->Line = binary->Right->Line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality()
{
    auto expr = parseComparison();

    while (match(TokenType::EqualEqual) || match(TokenType::BangEqual))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->Operation = _previous.Type == TokenType::EqualEqual ? BinaryExpr::Op::Equal
                                                                     : BinaryExpr::Op::NotEqual;
        binary->Left = std::move(expr);
        binary->Right = parseComparison();
        binary->Line = binary->Right->Line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseComparison()
{
    auto expr = parseTerm();

    while (match(TokenType::Less) || match(TokenType::LessEqual) || match(TokenType::Greater) ||
           match(TokenType::GreaterEqual))
    {
        BinaryExpr::Op op = BinaryExpr::Op::Less;
        switch (_previous.Type)
        {
        case TokenType::Less:
            op = BinaryExpr::Op::Less;
            break;
        case TokenType::LessEqual:
            op = BinaryExpr::Op::LessEqual;
            break;
        case TokenType::Greater:
            op = BinaryExpr::Op::Greater;
            break;
        default:
            op = BinaryExpr::Op::GreaterEqual;
            break;
        }

        auto binary = std::make_unique<BinaryExpr>();
        binary->Operation = op;
        binary->Left = std::move(expr);
        binary->Right = parseTerm();
        binary->Line = binary->Right->Line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseTerm()
{
    auto expr = parseFactor();

    while (match(TokenType::Plus) || match(TokenType::Minus))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->Operation = _previous.Type == TokenType::Plus ? BinaryExpr::Op::Add
                                                               : BinaryExpr::Op::Subtract;
        binary->Left = std::move(expr);
        binary->Right = parseFactor();
        binary->Line = binary->Right->Line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseFactor()
{
    auto expr = parseUnary();

    while (match(TokenType::Star) || match(TokenType::Slash))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->Operation = _previous.Type == TokenType::Star ? BinaryExpr::Op::Multiply
                                                               : BinaryExpr::Op::Divide;
        binary->Left = std::move(expr);
        binary->Right = parseUnary();
        binary->Line = binary->Right->Line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary()
{
    if (match(TokenType::Minus))
    {
        auto unary = std::make_unique<UnaryExpr>();
        unary->Operation = UnaryExpr::Op::Negate;
        unary->Line = _previous.Line;
        unary->Operand = parseUnary();
        return unary;
    }
    if (match(TokenType::Bang))
    {
        auto unary = std::make_unique<UnaryExpr>();
        unary->Operation = UnaryExpr::Op::Not;
        unary->Line = _previous.Line;
        unary->Operand = parseUnary();
        return unary;
    }
    return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall()
{
    auto expr = parsePrimary();

    while (match(TokenType::LeftParen))
    {
        auto* variable = dynamic_cast<VariableExpr*>(expr.get());
        if (variable == nullptr)
        {
            fail("only named functions can be called");
        }

        auto call = std::make_unique<CallExpr>();
        call->Callee = std::move(variable->Name);
        call->Line = _previous.Line;

        if (!check(TokenType::RightParen))
        {
            do
            {
                call->Args.push_back(parseExpression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightParen, "expected ')' after arguments");

        expr = std::move(call);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary()
{
    if (match(TokenType::Number))
    {
        auto expr = std::make_unique<NumberExpr>();
        expr->Line = _previous.Line;

        double value = 0.0;
        const auto result = std::from_chars(
            _previous.Lexeme.data(), _previous.Lexeme.data() + _previous.Lexeme.size(), value);
        if (result.ec != std::errc())
        {
            fail("invalid number literal");
        }
        expr->Value = value;
        return expr;
    }

    if (match(TokenType::String))
    {
        fail("string literals are not supported yet");
    }

    if (matchKeyword("true"))
    {
        auto expr = std::make_unique<BoolExpr>();
        expr->Value = true;
        expr->Line = _previous.Line;
        return expr;
    }

    if (matchKeyword("false"))
    {
        auto expr = std::make_unique<BoolExpr>();
        expr->Value = false;
        expr->Line = _previous.Line;
        return expr;
    }

    if (matchKeyword("null"))
    {
        auto expr = std::make_unique<NullExpr>();
        expr->Line = _previous.Line;
        return expr;
    }

    if (match(TokenType::Identifier))
    {
        auto expr = std::make_unique<VariableExpr>();
        expr->Name = std::string(_previous.Lexeme);
        expr->Line = _previous.Line;
        return expr;
    }

    if (match(TokenType::LeftParen))
    {
        auto expr = parseExpression();
        consume(TokenType::RightParen, "expected ')' after expression");
        return expr;
    }

    fail("expected an expression");
}

} // namespace xenon
