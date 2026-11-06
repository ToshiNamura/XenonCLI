#include "xenon/compiler/parser.hpp"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace xenon
{

Parser::Parser(Lexer& lexer) : lexer_(lexer)
{
    current_ = lexer_.next();
    if (current_.type == TokenType::Invalid)
    {
        fail("unexpected character");
    }
}

[[noreturn]] void Parser::fail(const std::string& message)
{
    error_ = message + " (line " + std::to_string(current_.line) + ")";
    throw ParseError{};
}

const Token& Parser::advance()
{
    previous_ = current_;
    current_ = lexer_.next();
    if (current_.type == TokenType::Invalid)
    {
        fail("unexpected character '" + std::string(current_.lexeme) + "'");
    }
    return previous_;
}

bool Parser::check(TokenType type) const
{
    return current_.type == type;
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

bool Parser::check_keyword(std::string_view keyword) const
{
    return current_.type == TokenType::Identifier && current_.lexeme == keyword;
}

bool Parser::match_keyword(std::string_view keyword)
{
    if (!check_keyword(keyword))
    {
        return false;
    }
    advance();
    return true;
}

bool Parser::is_type(const Token& token) const
{
    if (token.type != TokenType::Identifier)
    {
        return false;
    }
    return token.lexeme == "int" || token.lexeme == "double" || token.lexeme == "bool" ||
           token.lexeme == "void";
}

bool Parser::parse(Program& out)
{
    try
    {
        parse_program(out);
        return true;
    }
    catch (const ParseError&)
    {
        return false;
    }
}

void Parser::parse_program(Program& out)
{
    while (!check(TokenType::End))
    {
        out.functions.push_back(parse_function());
    }
}

FunctionDecl Parser::parse_function()
{
    FunctionDecl function;
    function.exported = match_keyword("export");

    function.return_type = parse_type();
    function.name = std::string(consume(TokenType::Identifier, "expected a function name").lexeme);

    consume(TokenType::LeftParen, "expected '(' after function name");

    if (!check(TokenType::RightParen))
    {
        do
        {
            const std::string param_type = parse_type();
            const std::string param_name =
                std::string(consume(TokenType::Identifier, "expected a parameter name").lexeme);
            function.params.emplace_back(param_type, param_name);
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected ')' after parameters");
    function.body = parse_block();
    return function;
}

std::string Parser::parse_type()
{
    if (!is_type(current_))
    {
        fail("expected a type");
    }
    const std::string type(current_.lexeme);
    advance();
    return type;
}

std::unique_ptr<BlockStmt> Parser::parse_block()
{
    auto block = std::make_unique<BlockStmt>();
    block->line = current_.line;

    consume(TokenType::LeftBrace, "expected '{'");
    while (!check(TokenType::RightBrace) && !check(TokenType::End))
    {
        block->statements.push_back(parse_statement());
    }
    consume(TokenType::RightBrace, "expected '}'");
    return block;
}

std::unique_ptr<Stmt> Parser::parse_statement()
{
    if (check(TokenType::LeftBrace))
    {
        return parse_block();
    }

    if (match_keyword("return"))
    {
        auto stmt = std::make_unique<ReturnStmt>();
        stmt->line = previous_.line;
        if (!check(TokenType::Semicolon))
        {
            stmt->value = parse_expression();
        }
        consume(TokenType::Semicolon, "expected ';' after return");
        return stmt;
    }

    if (match_keyword("if"))
    {
        auto stmt = std::make_unique<IfStmt>();
        stmt->line = previous_.line;
        consume(TokenType::LeftParen, "expected '(' after 'if'");
        stmt->condition = parse_expression();
        consume(TokenType::RightParen, "expected ')' after condition");
        stmt->then_branch = parse_statement();
        if (match_keyword("else"))
        {
            stmt->else_branch = parse_statement();
        }
        return stmt;
    }

    if (match_keyword("while"))
    {
        auto stmt = std::make_unique<WhileStmt>();
        stmt->line = previous_.line;
        consume(TokenType::LeftParen, "expected '(' after 'while'");
        stmt->condition = parse_expression();
        consume(TokenType::RightParen, "expected ')' after condition");
        stmt->body = parse_statement();
        return stmt;
    }

    if (is_type(current_))
    {
        return parse_var_decl();
    }

    auto stmt = std::make_unique<ExprStmt>();
    stmt->line = current_.line;
    stmt->expr = parse_expression();
    consume(TokenType::Semicolon, "expected ';' after expression");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parse_var_decl()
{
    auto stmt = std::make_unique<VarDeclStmt>();
    stmt->line = current_.line;

    stmt->type = parse_type();
    if (stmt->type == "void")
    {
        fail("'void' is not a valid variable type");
    }
    stmt->name = std::string(consume(TokenType::Identifier, "expected a variable name").lexeme);

    if (match(TokenType::Assign))
    {
        stmt->init = parse_expression();
    }
    consume(TokenType::Semicolon, "expected ';' after variable declaration");
    return stmt;
}

std::unique_ptr<Expr> Parser::parse_expression()
{
    return parse_assignment();
}

std::unique_ptr<Expr> Parser::parse_assignment()
{
    auto expr = parse_or();

    if (match(TokenType::Assign))
    {
        auto* variable = dynamic_cast<VariableExpr*>(expr.get());
        if (variable == nullptr)
        {
            fail("invalid assignment target");
        }
        auto assign = std::make_unique<AssignExpr>();
        assign->line = previous_.line;
        assign->name = std::move(variable->name);
        assign->value = parse_assignment();
        return assign;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_or()
{
    auto expr = parse_and();

    while (match(TokenType::OrOr))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->op = BinaryExpr::Op::Or;
        binary->left = std::move(expr);
        binary->right = parse_and();
        binary->line = binary->right->line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_and()
{
    auto expr = parse_equality();

    while (match(TokenType::AndAnd))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->op = BinaryExpr::Op::And;
        binary->left = std::move(expr);
        binary->right = parse_equality();
        binary->line = binary->right->line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_equality()
{
    auto expr = parse_comparison();

    while (match(TokenType::EqualEqual) || match(TokenType::BangEqual))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->op = previous_.type == TokenType::EqualEqual ? BinaryExpr::Op::Equal
                                                             : BinaryExpr::Op::NotEqual;
        binary->left = std::move(expr);
        binary->right = parse_comparison();
        binary->line = binary->right->line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_comparison()
{
    auto expr = parse_term();

    while (match(TokenType::Less) || match(TokenType::LessEqual) || match(TokenType::Greater) ||
           match(TokenType::GreaterEqual))
    {
        BinaryExpr::Op op = BinaryExpr::Op::Less;
        switch (previous_.type)
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
        binary->op = op;
        binary->left = std::move(expr);
        binary->right = parse_term();
        binary->line = binary->right->line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_term()
{
    auto expr = parse_factor();

    while (match(TokenType::Plus) || match(TokenType::Minus))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->op =
            previous_.type == TokenType::Plus ? BinaryExpr::Op::Add : BinaryExpr::Op::Subtract;
        binary->left = std::move(expr);
        binary->right = parse_factor();
        binary->line = binary->right->line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_factor()
{
    auto expr = parse_unary();

    while (match(TokenType::Star) || match(TokenType::Slash))
    {
        auto binary = std::make_unique<BinaryExpr>();
        binary->op =
            previous_.type == TokenType::Star ? BinaryExpr::Op::Multiply : BinaryExpr::Op::Divide;
        binary->left = std::move(expr);
        binary->right = parse_unary();
        binary->line = binary->right->line;
        expr = std::move(binary);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_unary()
{
    if (match(TokenType::Minus))
    {
        auto unary = std::make_unique<UnaryExpr>();
        unary->op = UnaryExpr::Op::Negate;
        unary->line = previous_.line;
        unary->operand = parse_unary();
        return unary;
    }
    if (match(TokenType::Bang))
    {
        auto unary = std::make_unique<UnaryExpr>();
        unary->op = UnaryExpr::Op::Not;
        unary->line = previous_.line;
        unary->operand = parse_unary();
        return unary;
    }
    return parse_call();
}

std::unique_ptr<Expr> Parser::parse_call()
{
    auto expr = parse_primary();

    while (match(TokenType::LeftParen))
    {
        auto* variable = dynamic_cast<VariableExpr*>(expr.get());
        if (variable == nullptr)
        {
            fail("only named functions can be called");
        }

        auto call = std::make_unique<CallExpr>();
        call->callee = std::move(variable->name);
        call->line = previous_.line;

        if (!check(TokenType::RightParen))
        {
            do
            {
                call->args.push_back(parse_expression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightParen, "expected ')' after arguments");

        expr = std::move(call);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_primary()
{
    if (match(TokenType::Number))
    {
        auto expr = std::make_unique<NumberExpr>();
        expr->line = previous_.line;

        double value = 0.0;
        const auto result = std::from_chars(
            previous_.lexeme.data(), previous_.lexeme.data() + previous_.lexeme.size(), value);
        if (result.ec != std::errc())
        {
            fail("invalid number literal");
        }
        expr->value = value;
        return expr;
    }

    if (match(TokenType::String))
    {
        fail("string literals are not supported yet");
    }

    if (match_keyword("true"))
    {
        auto expr = std::make_unique<BoolExpr>();
        expr->value = true;
        expr->line = previous_.line;
        return expr;
    }

    if (match_keyword("false"))
    {
        auto expr = std::make_unique<BoolExpr>();
        expr->value = false;
        expr->line = previous_.line;
        return expr;
    }

    if (match_keyword("null"))
    {
        auto expr = std::make_unique<NullExpr>();
        expr->line = previous_.line;
        return expr;
    }

    if (match(TokenType::Identifier))
    {
        auto expr = std::make_unique<VariableExpr>();
        expr->name = std::string(previous_.lexeme);
        expr->line = previous_.line;
        return expr;
    }

    if (match(TokenType::LeftParen))
    {
        auto expr = parse_expression();
        consume(TokenType::RightParen, "expected ')' after expression");
        return expr;
    }

    fail("expected an expression");
}

} // namespace xenon
