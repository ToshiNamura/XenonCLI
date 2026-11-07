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
namespace
{

/// Splits `text` on `sep`, ignoring separators nested inside `<...>`.
std::vector<std::string> splitTopLevel(const std::string& text, char sep)
{
    std::vector<std::string> parts;
    std::size_t depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '<')
        {
            ++depth;
        }
        else if (text[i] == '>')
        {
            --depth;
        }
        else if (text[i] == sep && depth == 0)
        {
            parts.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(text.substr(start));
    return parts;
}

/// Builds a type substitution map for an instantiation `base<args...>`.
std::unordered_map<std::string, std::string> buildSubstitutions(
    const std::vector<std::string>& params, const std::string& fullName,
    const std::string& base, const std::string& selfType)
{
    std::unordered_map<std::string, std::string> map;
    const auto less = fullName.find('<');
    const auto greater = fullName.rfind('>');
    if (less == std::string::npos || greater == std::string::npos || greater <= less)
    {
        return map;
    }

    const std::string inner = fullName.substr(less + 1, greater - less - 1);
    const auto args = splitTopLevel(inner, ',');
    for (std::size_t i = 0; i < params.size() && i < args.size(); ++i)
    {
        map[params[i]] = args[i];
    }
    if (!selfType.empty())
    {
        map[base] = selfType;
    }
    return map;
}

} // namespace

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
    for (const auto& templ : _program.Templates)
    {
        if (templ.IsFunction)
        {
            _functionTemplates[templ.Function.Name] = FunctionTemplate{templ.Params, &templ.Function};
        }
        else
        {
            _typeTemplates[templ.Type.Name] = TypeTemplate{templ.Params, &templ.Type};
        }
    }
    std::size_t globalIndex = 0;
    for (const auto& global : _program.Globals)
    {
        _globalIndices[global.Name] = globalIndex++;
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
        for (const auto& global : _program.Globals)
        {
            compileGlobalVar(global, out);
        }
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

        while (!_pendingFunctions.empty() || !_pendingTypes.empty())
        {
            auto functions = std::move(_pendingFunctions);
            _pendingFunctions.clear();
            for (const auto& pending : functions)
            {
                _substitutions = pending.Substitutions;
                compileFunction(*pending.Decl, out, pending.Name);
            }
            _substitutions.clear();

            auto types = std::move(_pendingTypes);
            _pendingTypes.clear();
            for (const auto& pending : types)
            {
                emitTypeSymbol(*pending.Decl, pending.Name, out);
            }
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

void CodeGenerator::compileGlobalVar(const GlobalVarDecl& global, Module& out)
{
    Symbol symbol;
    symbol.Name = global.Name;
    symbol.Kind = SymbolKind::Variable;
    symbol.Signature = global.Type + " " + global.Name;
    symbol.Defined = true;
    symbol.InitialValue = evaluateConstant(global.Init.get());
    out.Symbols.push_back(std::move(symbol));
}

Value CodeGenerator::evaluateConstant(const Expr* init)
{
    if (init == nullptr)
    {
        return Null{};
    }
    if (const auto* number = dynamic_cast<const NumberExpr*>(init))
    {
        return Value{number->Value};
    }
    if (const auto* boolean = dynamic_cast<const BoolExpr*>(init))
    {
        return Value{boolean->Value};
    }
    if (dynamic_cast<const NullExpr*>(init) != nullptr)
    {
        return Null{};
    }
    if (dynamic_cast<const StringExpr*>(init) != nullptr)
    {
        fail("global string variables are not supported yet");
    }
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(init))
    {
        if (unary->Operation == UnaryExpr::Op::Negate)
        {
            if (const auto* number = dynamic_cast<const NumberExpr*>(unary->Operand.get()))
            {
                return Value{-number->Value};
            }
        }
    }
    fail("global initializer must be a constant expression");
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
        _scopes.back().Types[name] = substituteType(type);
    }
    ++_slotCounter;
    _maxSlots = std::max(_maxSlots, _slotCounter);
}

std::size_t CodeGenerator::allocateTemp()
{
    const auto slot = _slotCounter++;
    _maxSlots = std::max(_maxSlots, _slotCounter);
    return slot;
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

bool CodeGenerator::hasLocal(const std::string& name) const
{
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
    {
        if (it->Names.find(name) != it->Names.end())
        {
            return true;
        }
    }
    return false;
}

std::size_t CodeGenerator::resolveGlobal(const std::string& name)
{
    const auto it = _globalIndices.find(name);
    if (it == _globalIndices.end())
    {
        fail("undefined variable: " + name);
    }
    return it->second;
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

    ensureTypeInstantiated(typeName);

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

std::string CodeGenerator::substituteType(const std::string& name) const
{
    const auto it = _substitutions.find(name);
    return it != _substitutions.end() ? it->second : name;
}

std::size_t CodeGenerator::resolveFunction(const std::string& callee)
{
    const auto existing = _functionIndices.find(callee);
    if (existing != _functionIndices.end())
    {
        return existing->second;
    }

    const auto separator = callee.rfind("::");
    if (separator != std::string::npos)
    {
        ensureTypeInstantiated(callee.substr(0, separator));
        const auto after = _functionIndices.find(callee);
        if (after != _functionIndices.end())
        {
            return after->second;
        }
    }

    const auto less = callee.find('<');
    if (less == std::string::npos)
    {
        fail("unknown function: " + callee);
    }
    const std::string base = callee.substr(0, less);
    const auto templ = _functionTemplates.find(base);
    if (templ == _functionTemplates.end())
    {
        fail("unknown function: " + callee);
    }

    const auto index = _functionIndices.size();
    _functionIndices[callee] = index;

    PendingFunction pending;
    pending.Name = callee;
    pending.Decl = templ->second.Decl;
    pending.Substitutions = buildSubstitutions(templ->second.Params, callee, base, {});
    _pendingFunctions.push_back(std::move(pending));
    return index;
}

void CodeGenerator::ensureTypeInstantiated(const std::string& typeName)
{
    if (_typeFields.find(typeName) != _typeFields.end())
    {
        return;
    }

    const auto less = typeName.find('<');
    if (less == std::string::npos)
    {
        return;
    }

    const auto templ = _typeTemplates.find(typeName.substr(0, less));
    if (templ == _typeTemplates.end())
    {
        return;
    }

    const TypeDecl* decl = templ->second.Decl;

    std::vector<std::string> fieldNames;
    for (const auto& field : decl->Fields)
    {
        fieldNames.push_back(field.Name);
    }
    _typeFields[typeName] = std::move(fieldNames);

    _pendingTypes.push_back(PendingType{typeName, decl});

    const auto substitutions =
        buildSubstitutions(templ->second.Params, typeName, typeName.substr(0, less), typeName);

    for (const auto& method : decl->Methods)
    {
        const std::string mangled = typeName + "::" + method.Name;
        const auto index = _functionIndices.size();
        _functionIndices[mangled] = index;

        PendingFunction pending;
        pending.Name = mangled;
        pending.Decl = &method;
        pending.Substitutions = substitutions;
        _pendingFunctions.push_back(std::move(pending));
    }
}

void CodeGenerator::emitTypeSymbol(const TypeDecl& decl, const std::string& name, Module& out)
{
    Symbol symbol;
    symbol.Name = name;
    symbol.Kind = decl.Kind == DeclKind::Struct ? SymbolKind::Struct : SymbolKind::Class;
    symbol.Signature = (decl.Kind == DeclKind::Struct ? "struct " : "class ") + name;
    symbol.Defined = !_hintMode;
    for (const auto& field : decl.Fields)
    {
        symbol.Fields.push_back(field.Name);
    }
    out.Symbols.push_back(std::move(symbol));
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
        _loops.push_back(LoopContext{});
        compileExpression(*w->Condition);
        const auto exitJump = emitJump(OpCode::JumpIfFalse);
        compileStatement(*w->Body);
        const auto backJump = emitJump(OpCode::Jump);
        patchJump(backJump, loopStart);

        const auto end = _binary->codePosition();
        patchJump(exitJump, end);

        const auto loop = std::move(_loops.back());
        _loops.pop_back();
        for (const auto offset : loop.breaks)
        {
            patchJump(offset, end);
        }
        for (const auto offset : loop.continues)
        {
            patchJump(offset, loopStart);
        }
    }
    else if (const auto* dw = dynamic_cast<const DoWhileStmt*>(&stmt))
    {
        compileDoWhile(*dw);
    }
    else if (const auto* f = dynamic_cast<const ForStmt*>(&stmt))
    {
        compileFor(*f);
    }
    else if (const auto* s = dynamic_cast<const SwitchStmt*>(&stmt))
    {
        compileSwitch(*s);
    }
    else if (dynamic_cast<const BreakStmt*>(&stmt) != nullptr)
    {
        if (!_switches.empty())
        {
            _switches.back().breaks.push_back(emitJump(OpCode::Jump));
        }
        else if (!_loops.empty())
        {
            _loops.back().breaks.push_back(emitJump(OpCode::Jump));
        }
        else
        {
            fail("'break' outside a loop or switch");
        }
    }
    else if (dynamic_cast<const ContinueStmt*>(&stmt) != nullptr)
    {
        if (_loops.empty())
        {
            fail("'continue' outside a loop");
        }
        _loops.back().continues.push_back(emitJump(OpCode::Jump));
    }
    else
    {
        fail("unknown statement");
    }
}

void CodeGenerator::compileFor(const ForStmt& stmt)
{
    if (stmt.Init)
    {
        compileStatement(*stmt.Init);
    }

    const auto conditionStart = _binary->codePosition();
    _loops.push_back(LoopContext{});

    const bool hasCondition = stmt.Condition != nullptr;
    std::size_t exitJump = 0;
    if (hasCondition)
    {
        compileExpression(*stmt.Condition);
        exitJump = emitJump(OpCode::JumpIfFalse);
    }

    compileStatement(*stmt.Body);

    const auto incrementStart = _binary->codePosition();
    if (stmt.Increment)
    {
        compileExpression(*stmt.Increment);
        emitOpcode(OpCode::Pop);
    }

    const auto backJump = emitJump(OpCode::Jump);
    patchJump(backJump, conditionStart);

    const auto end = _binary->codePosition();
    if (hasCondition)
    {
        patchJump(exitJump, end);
    }

    const auto continueTarget = stmt.Increment ? incrementStart : conditionStart;

    const auto loop = std::move(_loops.back());
    _loops.pop_back();
    for (const auto offset : loop.breaks)
    {
        patchJump(offset, end);
    }
    for (const auto offset : loop.continues)
    {
        patchJump(offset, continueTarget);
    }
}

void CodeGenerator::compileDoWhile(const DoWhileStmt& stmt)
{
    const auto loopStart = _binary->codePosition();
    _loops.push_back(LoopContext{});

    compileStatement(*stmt.Body);

    const auto conditionStart = _binary->codePosition();
    compileExpression(*stmt.Condition);
    const auto exitJump = emitJump(OpCode::JumpIfFalse);
    const auto backJump = emitJump(OpCode::Jump);
    patchJump(backJump, loopStart);

    const auto end = _binary->codePosition();
    patchJump(exitJump, end);

    const auto loop = std::move(_loops.back());
    _loops.pop_back();
    for (const auto offset : loop.breaks)
    {
        patchJump(offset, end);
    }
    for (const auto offset : loop.continues)
    {
        patchJump(offset, conditionStart);
    }
}

void CodeGenerator::compileSwitch(const SwitchStmt& stmt)
{
    compileExpression(*stmt.Condition);
    const auto tempSlot = allocateTemp();
    emitOpcode(OpCode::StoreLocal);
    emitByte(static_cast<std::uint8_t>(tempSlot));

    _switches.push_back(SwitchContext{});

    std::vector<std::size_t> bodyJumpForCase(stmt.Cases.size(), 0);
    std::size_t defaultIndex = static_cast<std::size_t>(-1);

    for (std::size_t i = 0; i < stmt.Cases.size(); ++i)
    {
        const CaseClause& clause = stmt.Cases[i];
        if (clause.Value == nullptr)
        {
            defaultIndex = i;
            continue;
        }

        emitOpcode(OpCode::LoadLocal);
        emitByte(static_cast<std::uint8_t>(tempSlot));
        emitConstant(evaluateConstant(clause.Value.get()));
        emitOpcode(OpCode::Equal);
        const auto falseJump = emitJump(OpCode::JumpIfFalse);
        bodyJumpForCase[i] = emitJump(OpCode::Jump);
        patchJump(falseJump, _binary->codePosition());
    }

    const bool hasDefault = defaultIndex != static_cast<std::size_t>(-1);
    std::size_t defaultJump = 0;
    if (hasDefault)
    {
        defaultJump = emitJump(OpCode::Jump);
    }
    const auto endJump = emitJump(OpCode::Jump);

    for (std::size_t i = 0; i < stmt.Cases.size(); ++i)
    {
        const CaseClause& clause = stmt.Cases[i];
        const auto bodyOffset = _binary->codePosition();

        if (clause.Value != nullptr)
        {
            patchJump(bodyJumpForCase[i], bodyOffset);
        }
        else if (hasDefault)
        {
            patchJump(defaultJump, bodyOffset);
        }

        for (const auto& statement : clause.Statements)
        {
            compileStatement(*statement);
        }
    }

    const auto end = _binary->codePosition();
    patchJump(endJump, end);

    const auto context = std::move(_switches.back());
    _switches.pop_back();
    for (const auto offset : context.breaks)
    {
        patchJump(offset, end);
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
        const std::string typeName = substituteType(n->TypeName);
        ensureTypeInstantiated(typeName);
        const auto index = _binary->addString(typeName);
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
        if (hasLocal(v->Name))
        {
            emitOpcode(OpCode::LoadLocal);
            emitByte(static_cast<std::uint8_t>(resolveLocal(v->Name)));
        }
        else
        {
            emitOpcode(OpCode::LoadGlobal);
            emitByte(static_cast<std::uint8_t>(resolveGlobal(v->Name)));
        }
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
        if (hasLocal(a->Name))
        {
            const auto slot = resolveLocal(a->Name);
            emitOpcode(OpCode::StoreLocal);
            emitByte(static_cast<std::uint8_t>(slot));
            emitOpcode(OpCode::LoadLocal);
            emitByte(static_cast<std::uint8_t>(slot));
        }
        else
        {
            emitOpcode(OpCode::StoreGlobal);
            emitByte(static_cast<std::uint8_t>(resolveGlobal(a->Name)));
        }
        return;
    }
    if (const auto* c = dynamic_cast<const CallExpr*>(&expr))
    {
        for (const auto& arg : c->Args)
        {
            compileExpression(*arg);
        }
        const auto index = resolveFunction(c->Callee);
        emitOpcode(OpCode::Call);
        emitByte(static_cast<std::uint8_t>(index));
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
