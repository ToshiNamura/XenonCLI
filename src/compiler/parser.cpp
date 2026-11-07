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
    if (_hasPeek)
    {
        _current = _peek;
        _hasPeek = false;
    }
    else
    {
        _current = _lexer.next();
    }
    if (_current.Type == TokenType::Invalid)
    {
        fail("unexpected character '" + std::string(_current.Lexeme) + "'");
    }
    return _previous;
}

const Token& Parser::peekNext()
{
    if (!_hasPeek)
    {
        _peek = _lexer.next();
        _hasPeek = true;
    }
    return _peek;
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
    if (token.Lexeme == "int" || token.Lexeme == "double" || token.Lexeme == "bool" ||
        token.Lexeme == "string" || token.Lexeme == "void")
    {
        return true;
    }
    return _typeNames.contains(std::string(token.Lexeme));
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

void Parser::parseTopLevelDeclaration(Program& out)
{
    const bool exported = matchKeyword("export");
    const bool constant = matchKeyword("const");

    const std::string type = parseType();
    const std::string name =
        std::string(consume(TokenType::Identifier, "expected a declaration name").Lexeme);

    if (match(TokenType::LeftParen))
    {
        FunctionDecl function;
        function.Exported = exported;
        function.ReturnType = type;
        function.Name = name;
        parseFunctionRest(function);
        out.Functions.push_back(std::move(function));
        return;
    }

    GlobalVarDecl global;
    global.Type = type;
    global.Name = name;
    global.Constant = constant;
    if (match(TokenType::Assign))
    {
        global.Init = parseExpression();
    }
    consume(TokenType::Semicolon, "expected ';' after global variable declaration");
    out.Globals.push_back(std::move(global));
}

TemplateDecl Parser::parseTemplate()
{
    TemplateDecl templ;

    advance(); // consume 'template'
    consume(TokenType::Less, "expected '<' after 'template'");

    if (!check(TokenType::Greater))
    {
        do
        {
            if (!matchKeyword("typename") && !matchKeyword("class"))
            {
                fail("expected 'typename' or 'class' in template parameters");
            }
            templ.Params.push_back(std::string(
                consume(TokenType::Identifier, "expected a template parameter name").Lexeme));
        } while (match(TokenType::Comma));
    }
    consume(TokenType::Greater, "expected '>' after template parameters");

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

FunctionDecl Parser::parseFunction()
{
    FunctionDecl function;
    function.Exported = matchKeyword("export");

    function.ReturnType = parseType();
    function.Name = std::string(consume(TokenType::Identifier, "expected a function name").Lexeme);

    consume(TokenType::LeftParen, "expected '(' after function name");
    parseFunctionRest(function);
    return function;
}

void Parser::parseFunctionRest(FunctionDecl& function)
{
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
}

TypeDecl Parser::parseTypeDecl()
{
    TypeDecl decl;
    const std::string keyword = std::string(advance().Lexeme);

    decl.Kind = keyword == "struct"  ? DeclKind::Struct
                : keyword == "class" ? DeclKind::Class
                                     : DeclKind::Enum;

    decl.Name = std::string(consume(TokenType::Identifier, "expected a type name").Lexeme);
    _typeNames.insert(decl.Name);

    decl.Signature = keyword + " " + decl.Name;

    if (decl.Kind == DeclKind::Enum)
    {
        consume(TokenType::LeftBrace, "expected '{' after enum name");
        if (!check(TokenType::RightBrace))
        {
            do
            {
                decl.Enumerators.push_back(std::string(
                    consume(TokenType::Identifier, "expected an enumerator name").Lexeme));
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightBrace, "expected '}' after enumerators");
        consume(TokenType::Semicolon, "expected ';' after enum");
        return decl;
    }

    consume(TokenType::LeftBrace, "expected '{' after type name");

    while (!check(TokenType::RightBrace) && !check(TokenType::End))
    {
        const std::string memberType = parseType();
        const std::string memberName =
            std::string(consume(TokenType::Identifier, "expected a member name").Lexeme);

        if (match(TokenType::LeftParen))
        {
            FunctionDecl method;
            method.ReturnType = memberType;
            method.Name = memberName;

            if (!check(TokenType::RightParen))
            {
                do
                {
                    const std::string paramType = parseType();
                    const std::string paramName = std::string(
                        consume(TokenType::Identifier, "expected a parameter name").Lexeme);
                    method.Params.emplace_back(paramType, paramName);
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RightParen, "expected ')' after parameters");

            if (check(TokenType::LeftBrace))
            {
                method.HasBody = true;
                method.Body = parseBlock();
            }
            else
            {
                method.HasBody = false;
                consume(TokenType::Semicolon, "expected '{' or ';' after method signature");
            }
            decl.Methods.push_back(std::move(method));
        }
        else
        {
            decl.Fields.push_back(FieldDecl{memberType, memberName});
            if (match(TokenType::Assign))
            {
                while (!check(TokenType::Semicolon) && !check(TokenType::Comma) &&
                       !check(TokenType::End))
                {
                    advance();
                }
            }
            while (match(TokenType::Comma))
            {
                const std::string name = std::string(
                    consume(TokenType::Identifier, "expected a field name").Lexeme);
                decl.Fields.push_back(FieldDecl{memberType, name});
                if (match(TokenType::Assign))
                {
                    while (!check(TokenType::Semicolon) && !check(TokenType::Comma) &&
                           !check(TokenType::End))
                    {
                        advance();
                    }
                }
            }
            consume(TokenType::Semicolon, "expected ';' after field declaration");
        }
    }

    consume(TokenType::RightBrace, "expected '}' after type body");
    consume(TokenType::Semicolon, "expected ';' after type declaration");
    return decl;
}

std::string Parser::parseType()
{
    if (!isType(_current))
    {
        fail("expected a type");
    }
    std::string type(_current.Lexeme);
    advance();

    if (match(TokenType::Less))
    {
        type += '<';
        do
        {
            type += parseType();
        } while (match(TokenType::Comma));
        consume(TokenType::Greater, "expected '>' after template arguments");
        type += '>';
    }

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
        auto stmt = std::make_unique<BreakStmt>();
        stmt->Line = _previous.Line;
        consume(TokenType::Semicolon, "expected ';' after 'break'");
        return stmt;
    }

    if (matchKeyword("continue"))
    {
        auto stmt = std::make_unique<ContinueStmt>();
        stmt->Line = _previous.Line;
        consume(TokenType::Semicolon, "expected ';' after 'continue'");
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

std::unique_ptr<Stmt> Parser::parseForStatement()
{
    auto stmt = std::make_unique<ForStmt>();
    stmt->Line = _previous.Line;

    consume(TokenType::LeftParen, "expected '(' after 'for'");

    if (!check(TokenType::Semicolon))
    {
        if (isType(_current))
        {
            stmt->Init = parseVarDecl();
        }
        else
        {
            auto expr = std::make_unique<ExprStmt>();
            expr->Expr = parseExpression();
            consume(TokenType::Semicolon, "expected ';' after for initializer");
            stmt->Init = std::move(expr);
        }
    }
    else
    {
        advance(); // consume ';'
    }

    if (!check(TokenType::Semicolon))
    {
        stmt->Condition = parseExpression();
    }
    consume(TokenType::Semicolon, "expected ';' after for condition");

    if (!check(TokenType::RightParen))
    {
        stmt->Increment = parseExpression();
    }
    consume(TokenType::RightParen, "expected ')' after for clauses");

    stmt->Body = parseStatement();
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseDoWhileStatement()
{
    auto stmt = std::make_unique<DoWhileStmt>();
    stmt->Line = _previous.Line;

    stmt->Body = parseStatement();

    if (!matchKeyword("while"))
    {
        fail("expected 'while' after do body");
    }
    consume(TokenType::LeftParen, "expected '(' after 'while'");
    stmt->Condition = parseExpression();
    consume(TokenType::RightParen, "expected ')' after condition");
    consume(TokenType::Semicolon, "expected ';' after do/while");
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseSwitchStatement()
{
    auto stmt = std::make_unique<SwitchStmt>();
    stmt->Line = _previous.Line;

    consume(TokenType::LeftParen, "expected '(' after 'switch'");
    stmt->Condition = parseExpression();
    consume(TokenType::RightParen, "expected ')' after switch condition");

    consume(TokenType::LeftBrace, "expected '{' after switch");

    CaseClause* current = nullptr;
    while (!check(TokenType::RightBrace) && !check(TokenType::End))
    {
        if (matchKeyword("case"))
        {
            stmt->Cases.push_back(CaseClause{});
            current = &stmt->Cases.back();
            current->Value = parseExpression();
            consume(TokenType::Colon, "expected ':' after case value");
        }
        else if (matchKeyword("default"))
        {
            stmt->Cases.push_back(CaseClause{});
            current = &stmt->Cases.back();
            consume(TokenType::Colon, "expected ':' after 'default'");
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

    consume(TokenType::RightBrace, "expected '}' after switch");
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
        if (auto* variable = dynamic_cast<VariableExpr*>(expr.get()))
        {
            auto assign = std::make_unique<AssignExpr>();
            assign->Line = _previous.Line;
            assign->Name = std::move(variable->Name);
            assign->Value = parseAssignment();
            return assign;
        }
        if (auto* field = dynamic_cast<FieldAccessExpr*>(expr.get()))
        {
            auto assign = std::make_unique<FieldAssignExpr>();
            assign->Line = _previous.Line;
            assign->Object = std::move(field->Object);
            assign->FieldName = std::move(field->FieldName);
            assign->Value = parseAssignment();
            return assign;
        }
        fail("invalid assignment target");
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
    if (matchKeyword("new"))
    {
        auto expr = std::make_unique<NewExpr>();
        expr->TypeName = parseType();
        consume(TokenType::LeftParen, "expected '(' after type name in 'new'");
        consume(TokenType::RightParen, "expected ')' (constructor arguments not supported yet)");
        expr->Line = _previous.Line;
        return expr;
    }
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

    while (true)
    {
        if (match(TokenType::LeftParen))
        {
            auto* variable = dynamic_cast<VariableExpr*>(expr.get());
            if (variable == nullptr)
            {
                fail("only named functions and types can be called");
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
        else if (match(TokenType::Dot))
        {
            auto access = std::make_unique<FieldAccessExpr>();
            access->Object = std::move(expr);
            access->FieldName =
                std::string(consume(TokenType::Identifier, "expected a field name").Lexeme);
            access->Line = _previous.Line;
            expr = std::move(access);
        }
        else if (match(TokenType::ColonColon))
        {
            auto* variable = dynamic_cast<VariableExpr*>(expr.get());
            if (variable == nullptr)
            {
                fail("expected a type or module name before '::'");
            }
            const std::string memberName =
                std::string(consume(TokenType::Identifier, "expected a member name").Lexeme);

            auto target = std::make_unique<VariableExpr>();
            target->Name = variable->Name + "::" + memberName;
            target->Line = _previous.Line;
            expr = std::move(target);
        }
        else if (check(TokenType::Less))
        {
            auto* variable = dynamic_cast<VariableExpr*>(expr.get());
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
            } while (match(TokenType::Comma));
            consume(TokenType::Greater, "expected '>' after template arguments");
            name += '>';

            auto target = std::make_unique<VariableExpr>();
            target->Name = std::move(name);
            target->Line = _previous.Line;
            expr = std::move(target);
        }
        else
        {
            break;
        }
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
        auto expr = std::make_unique<StringExpr>();
        expr->Line = _previous.Line;
        const std::string_view lexeme = _previous.Lexeme;
        expr->Value = lexeme.size() >= 2 ? std::string(lexeme.substr(1, lexeme.size() - 2))
                                         : std::string();
        return expr;
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
