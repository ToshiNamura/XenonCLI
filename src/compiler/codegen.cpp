#include "xenon/compiler/codegen.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/value.hpp"

namespace xenon
{

CodeGenerator::CodeGenerator(const Program& program, const Module& hints, bool hintMode)
    : _program(program), _hints(hints), _hintMode(hintMode)
{
    std::size_t index = 0;
    for (const auto& function : _program.Functions)
    {
        _functionIndices[function.Name] = index++;
    }
    for (const auto& decl : _program.TypeDecls)
    {
        std::vector<std::string> fieldNames;
        for (const auto& field : decl.Fields)
        {
            fieldNames.push_back(field.Name);
        }
        _typeFields[decl.Name] = std::move(fieldNames);

        for (const auto& method : decl.Methods)
        {
            _functionIndices[decl.Name + "::" + method.Name] = index++;
        }
    }
    for (const auto& symbol : _hints.Symbols)
    {
        if (symbol.Kind == SymbolKind::Function)
        {
            _functionIndices[symbol.Name] = index++;
        }
        else if (symbol.Kind == SymbolKind::Struct || symbol.Kind == SymbolKind::Class)
        {
            _typeFields[symbol.Name] = symbol.Fields;
        }
    }
}

[[noreturn]] void CodeGenerator::fail(const std::string& message)
{
    _error = message;
    throw CodeGenError{};
}

bool CodeGenerator::generate(Module& out)
{
    _error.clear();
    out.Symbols.clear();

    try
    {
        for (const auto& function : _program.Functions)
        {
            compileFunction(function, out);
        }
        for (const auto& decl : _program.TypeDecls)
        {
            compileTypeDecl(decl, out);
        }
        for (const auto& symbol : _hints.Symbols)
        {
            out.Symbols.push_back(symbol);
        }
        return true;
    }
    catch (const CodeGenError&)
    {
        return false;
    }
}

void CodeGenerator::compileFunction(const FunctionDecl& function, Module& out,
                                    const std::string& qualifiedName)
{
    const std::string& name = qualifiedName.empty() ? function.Name : qualifiedName;

    Symbol symbol;
    symbol.Name = name;
    symbol.Kind = SymbolKind::Function;
    symbol.Signature = function.ReturnType.empty() ? "void " : function.ReturnType + " ";
    symbol.Signature += name;
    symbol.Signature += "(";
    for (std::size_t i = 0; i < function.Params.size(); ++i)
    {
        if (i != 0)
        {
            symbol.Signature += ", ";
        }
        symbol.Signature += function.Params[i].first;
    }
    symbol.Signature += ")";
    symbol.Arity = function.Params.size();
    symbol.Defined = function.HasBody;

    if (!function.HasBody)
    {
        out.Symbols.push_back(std::move(symbol));
        return;
    }

    _binary = &symbol.Body;
    _scopes.clear();
    _slotCounter = 0;
    _maxSlots = 0;

    enterScope();
    for (const auto& param : function.Params)
    {
        declareLocal(param.second, param.first);
    }
    compileBlock(*function.Body);

    emitConstant(Null{});
    emitOpcode(OpCode::Return);

    symbol.Locals = _maxSlots > symbol.Arity ? _maxSlots - symbol.Arity : 0;
    out.Symbols.push_back(std::move(symbol));
    _binary = nullptr;
}

void CodeGenerator::compileTypeDecl(const TypeDecl& decl, Module& out)
{
    Symbol symbol;
    symbol.Name = decl.Name;
    symbol.Kind = decl.Kind == DeclKind::Struct  ? SymbolKind::Struct
                  : decl.Kind == DeclKind::Class ? SymbolKind::Class
                                                 : SymbolKind::Enum;
    symbol.Signature = decl.Signature;
    symbol.Defined = !_hintMode;

    if (decl.Kind == DeclKind::Enum)
    {
        symbol.Enumerators = decl.Enumerators;
    }
    else
    {
        for (const auto& field : decl.Fields)
        {
            symbol.Fields.push_back(field.Name);
        }
    }
    out.Symbols.push_back(std::move(symbol));

    if (decl.Kind != DeclKind::Enum)
    {
        for (const auto& method : decl.Methods)
        {
            compileFunction(method, out, decl.Name + "::" + method.Name);
        }
    }
}

void CodeGenerator::enterScope()
{
    _scopes.push_back(Scope{{}, {}, _slotCounter});
}

void CodeGenerator::leaveScope()
{
    _slotCounter = _scopes.back().BaseSlot;
    _scopes.pop_back();
}

void CodeGenerator::declareLocal(const std::string& name, const std::string& type)
{
    _scopes.back().Names[name] = _slotCounter;
    if (!type.empty())
    {
        _scopes.back().Types[name] = type;
    }
    ++_slotCounter;
    _maxSlots = std::max(_maxSlots, _slotCounter);
}

std::size_t CodeGenerator::resolveLocal(const std::string& name)
{
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
    {
        const auto found = it->Names.find(name);
        if (found != it->Names.end())
        {
            return found->second;
        }
    }
    fail("undefined variable: " + name);
}

std::string CodeGenerator::localType(const std::string& name)
{
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
    {
        const auto found = it->Types.find(name);
        if (found != it->Types.end())
        {
            return found->second;
        }
    }
    fail("unknown variable type for: " + name);
}

std::size_t CodeGenerator::resolveField(const Expr& object, const std::string& fieldName)
{
    std::string typeName;
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&object))
    {
        typeName = localType(variable->Name);
    }
    else if (const auto* created = dynamic_cast<const NewExpr*>(&object))
    {
        typeName = created->TypeName;
    }
    else
    {
        fail("cannot determine the object type for field access");
    }

    const auto it = _typeFields.find(typeName);
    if (it == _typeFields.end())
    {
        fail("unknown type: " + typeName);
    }
    for (std::size_t i = 0; i < it->second.size(); ++i)
    {
        if (it->second[i] == fieldName)
        {
            return i;
        }
    }
    fail("no such field: " + typeName + "." + fieldName);
}

void CodeGenerator::compileBlock(const BlockStmt& block)
{
    enterScope();
    for (const auto& statement : block.Statements)
    {
        compileStatement(*statement);
    }
    leaveScope();
}

void CodeGenerator::compileStatement(const Stmt& stmt)
{
    if (const auto* e = dynamic_cast<const ExprStmt*>(&stmt))
    {
        compileExpression(*e->Expr);
        emitOpcode(OpCode::Pop);
    }
    else if (const auto* v = dynamic_cast<const VarDeclStmt*>(&stmt))
    {
        if (v->Init)
        {
            compileExpression(*v->Init);
        }
        declareLocal(v->Name, v->Type);
        if (v->Init)
        {
            emitOpcode(OpCode::StoreLocal);
            emitByte(static_cast<std::uint8_t>(resolveLocal(v->Name)));
        }
    }
    else if (const auto* r = dynamic_cast<const ReturnStmt*>(&stmt))
    {
        if (r->Value)
        {
            compileExpression(*r->Value);
        }
        else
        {
            emitConstant(Null{});
        }
        emitOpcode(OpCode::Return);
    }
    else if (const auto* b = dynamic_cast<const BlockStmt*>(&stmt))
    {
        compileBlock(*b);
    }
    else if (const auto* i = dynamic_cast<const IfStmt*>(&stmt))
    {
        compileExpression(*i->Condition);
        const auto elseJump = emitJump(OpCode::JumpIfFalse);
        compileStatement(*i->ThenBranch);
        const auto endJump = emitJump(OpCode::Jump);
        patchJump(elseJump, _binary->codePosition());
        if (i->ElseBranch)
        {
            compileStatement(*i->ElseBranch);
        }
        patchJump(endJump, _binary->codePosition());
    }
    else if (const auto* w = dynamic_cast<const WhileStmt*>(&stmt))
    {
        const auto loopStart = _binary->codePosition();
        compileExpression(*w->Condition);
        const auto exitJump = emitJump(OpCode::JumpIfFalse);
        compileStatement(*w->Body);
        const auto backJump = emitJump(OpCode::Jump);
        patchJump(backJump, loopStart);
        patchJump(exitJump, _binary->codePosition());
    }
    else
    {
        fail("unknown statement");
    }
}

void CodeGenerator::compileExpression(const Expr& expr)
{
    if (const auto* n = dynamic_cast<const NumberExpr*>(&expr))
    {
        emitConstant(n->Value);
        return;
    }
    if (const auto* b = dynamic_cast<const BoolExpr*>(&expr))
    {
        emitConstant(b->Value);
        return;
    }
    if (dynamic_cast<const NullExpr*>(&expr) != nullptr)
    {
        emitConstant(Null{});
        return;
    }
    if (const auto* s = dynamic_cast<const StringExpr*>(&expr))
    {
        emitString(s->Value);
        return;
    }
    if (const auto* n = dynamic_cast<const NewExpr*>(&expr))
    {
        const auto index = _binary->addString(n->TypeName);
        emitOpcode(OpCode::New);
        emitByte(static_cast<std::uint8_t>(index));
        return;
    }
    if (const auto* f = dynamic_cast<const FieldAccessExpr*>(&expr))
    {
        compileExpression(*f->Object);
        const auto index = resolveField(*f->Object, f->FieldName);
        emitOpcode(OpCode::GetField);
        emitByte(static_cast<std::uint8_t>(index));
        return;
    }
    if (const auto* fa = dynamic_cast<const FieldAssignExpr*>(&expr))
    {
        compileExpression(*fa->Object);
        compileExpression(*fa->Value);
        const auto index = resolveField(*fa->Object, fa->FieldName);
        emitOpcode(OpCode::SetField);
        emitByte(static_cast<std::uint8_t>(index));
        return;
    }
    if (const auto* v = dynamic_cast<const VariableExpr*>(&expr))
    {
        emitOpcode(OpCode::LoadLocal);
        emitByte(static_cast<std::uint8_t>(resolveLocal(v->Name)));
        return;
    }
    if (const auto* u = dynamic_cast<const UnaryExpr*>(&expr))
    {
        compileExpression(*u->Operand);
        emitOpcode(u->Operation == UnaryExpr::Op::Negate ? OpCode::Negate : OpCode::Not);
        return;
    }
    if (const auto* a = dynamic_cast<const AssignExpr*>(&expr))
    {
        compileExpression(*a->Value);
        const auto slot = resolveLocal(a->Name);
        emitOpcode(OpCode::StoreLocal);
        emitByte(static_cast<std::uint8_t>(slot));
        emitOpcode(OpCode::LoadLocal);
        emitByte(static_cast<std::uint8_t>(slot));
        return;
    }
    if (const auto* c = dynamic_cast<const CallExpr*>(&expr))
    {
        for (const auto& arg : c->Args)
        {
            compileExpression(*arg);
        }
        const auto it = _functionIndices.find(c->Callee);
        if (it == _functionIndices.end())
        {
            fail("unknown function: " + c->Callee);
        }
        emitOpcode(OpCode::Call);
        emitByte(static_cast<std::uint8_t>(it->second));
        return;
    }
    if (const auto* bin = dynamic_cast<const BinaryExpr*>(&expr))
    {
        if (bin->Operation == BinaryExpr::Op::And)
        {
            compileExpression(*bin->Left);
            const auto falseJump = emitJump(OpCode::JumpIfFalse);
            compileExpression(*bin->Right);
            const auto endJump = emitJump(OpCode::Jump);
            patchJump(falseJump, _binary->codePosition());
            emitConstant(false);
            patchJump(endJump, _binary->codePosition());
            return;
        }
        if (bin->Operation == BinaryExpr::Op::Or)
        {
            compileExpression(*bin->Left);
            const auto evalRight = emitJump(OpCode::JumpIfFalse);
            emitConstant(true);
            const auto endJump = emitJump(OpCode::Jump);
            patchJump(evalRight, _binary->codePosition());
            compileExpression(*bin->Right);
            patchJump(endJump, _binary->codePosition());
            return;
        }

        compileExpression(*bin->Left);
        compileExpression(*bin->Right);
        switch (bin->Operation)
        {
        case BinaryExpr::Op::Add:
            emitOpcode(OpCode::Add);
            break;
        case BinaryExpr::Op::Subtract:
            emitOpcode(OpCode::Subtract);
            break;
        case BinaryExpr::Op::Multiply:
            emitOpcode(OpCode::Multiply);
            break;
        case BinaryExpr::Op::Divide:
            emitOpcode(OpCode::Divide);
            break;
        case BinaryExpr::Op::Equal:
            emitOpcode(OpCode::Equal);
            break;
        case BinaryExpr::Op::NotEqual:
            emitOpcode(OpCode::NotEqual);
            break;
        case BinaryExpr::Op::Less:
            emitOpcode(OpCode::Less);
            break;
        case BinaryExpr::Op::LessEqual:
            emitOpcode(OpCode::LessEqual);
            break;
        case BinaryExpr::Op::Greater:
            emitOpcode(OpCode::Greater);
            break;
        case BinaryExpr::Op::GreaterEqual:
            emitOpcode(OpCode::GreaterEqual);
            break;
        default:
            fail("unsupported binary operator");
        }
        return;
    }
    fail("unknown expression");
}

void CodeGenerator::emitOpcode(OpCode op)
{
    _binary->write(op, 0);
}

void CodeGenerator::emitByte(std::uint8_t byte)
{
    _binary->write(byte, 0);
}

void CodeGenerator::emitU16(std::uint16_t value)
{
    emitByte(static_cast<std::uint8_t>(value & 0xff));
    emitByte(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

std::size_t CodeGenerator::emitJump(OpCode op)
{
    emitOpcode(op);
    const auto offset = _binary->codePosition();
    emitU16(0);
    return offset;
}

void CodeGenerator::patchJump(std::size_t operandOffset, std::size_t target)
{
    const auto relative = static_cast<std::int16_t>(target - (operandOffset + 2));
    _binary->patchU16(operandOffset, static_cast<std::uint16_t>(relative));
}

void CodeGenerator::emitConstant(Value value)
{
    const auto index = _binary->addConstant(std::move(value));
    emitOpcode(OpCode::Constant);
    emitByte(static_cast<std::uint8_t>(index));
}

void CodeGenerator::emitString(const std::string& value)
{
    const auto index = _binary->addString(value);
    emitOpcode(OpCode::String);
    emitByte(static_cast<std::uint8_t>(index));
}

} // namespace xenon
