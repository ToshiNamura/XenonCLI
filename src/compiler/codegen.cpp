#include "xenon/compiler/codegen.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/value.hpp"

namespace xenon
{

CodeGenerator::CodeGenerator(const Program& program) : program_(program)
{
    for (std::size_t i = 0; i < program_.functions.size(); ++i)
    {
        function_indices_[program_.functions[i].name] = i;
    }
}

[[noreturn]] void CodeGenerator::fail(const std::string& message)
{
    error_ = message;
    throw CodeGenError{};
}

bool CodeGenerator::generate(Module& out)
{
    error_.clear();
    out.symbols.clear();

    try
    {
        for (const auto& function : program_.functions)
        {
            compile_function(function, out);
        }
        return true;
    }
    catch (const CodeGenError&)
    {
        return false;
    }
}

void CodeGenerator::compile_function(const FunctionDecl& function, Module& out)
{
    Symbol symbol;
    symbol.name = function.name;
    symbol.kind = SymbolKind::Function;
    symbol.signature = function.return_type.empty() ? "void " : function.return_type + " ";
    symbol.signature += function.name;
    symbol.signature += "(";
    for (std::size_t i = 0; i < function.params.size(); ++i)
    {
        if (i != 0)
        {
            symbol.signature += ", ";
        }
        symbol.signature += function.params[i].first;
    }
    symbol.signature += ")";
    symbol.arity = function.params.size();

    chunk_ = &symbol.body;
    scopes_.clear();
    slot_counter_ = 0;
    max_slots_ = 0;

    enter_scope();
    for (const auto& param : function.params)
    {
        declare_local(param.second);
    }
    compile_block(*function.body);

    emit_constant(Null{});
    emit_opcode(OpCode::Return);

    symbol.locals = max_slots_ > symbol.arity ? max_slots_ - symbol.arity : 0;
    out.symbols.push_back(std::move(symbol));
    chunk_ = nullptr;
}

void CodeGenerator::enter_scope()
{
    scopes_.push_back(Scope{{}, slot_counter_});
}

void CodeGenerator::leave_scope()
{
    slot_counter_ = scopes_.back().base_slot;
    scopes_.pop_back();
}

void CodeGenerator::declare_local(const std::string& name)
{
    scopes_.back().names[name] = slot_counter_;
    ++slot_counter_;
    max_slots_ = std::max(max_slots_, slot_counter_);
}

std::size_t CodeGenerator::resolve_local(const std::string& name)
{
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
    {
        const auto found = it->names.find(name);
        if (found != it->names.end())
        {
            return found->second;
        }
    }
    fail("undefined variable: " + name);
}

void CodeGenerator::compile_block(const BlockStmt& block)
{
    enter_scope();
    for (const auto& statement : block.statements)
    {
        compile_statement(*statement);
    }
    leave_scope();
}

void CodeGenerator::compile_statement(const Stmt& stmt)
{
    if (const auto* e = dynamic_cast<const ExprStmt*>(&stmt))
    {
        compile_expression(*e->expr);
        emit_opcode(OpCode::Pop);
    }
    else if (const auto* v = dynamic_cast<const VarDeclStmt*>(&stmt))
    {
        if (v->init)
        {
            compile_expression(*v->init);
        }
        declare_local(v->name);
        if (v->init)
        {
            emit_opcode(OpCode::StoreLocal);
            emit_byte(static_cast<std::uint8_t>(resolve_local(v->name)));
        }
    }
    else if (const auto* r = dynamic_cast<const ReturnStmt*>(&stmt))
    {
        if (r->value)
        {
            compile_expression(*r->value);
        }
        else
        {
            emit_constant(Null{});
        }
        emit_opcode(OpCode::Return);
    }
    else if (const auto* b = dynamic_cast<const BlockStmt*>(&stmt))
    {
        compile_block(*b);
    }
    else if (const auto* i = dynamic_cast<const IfStmt*>(&stmt))
    {
        compile_expression(*i->condition);
        const auto else_jump = emit_jump(OpCode::JumpIfFalse);
        compile_statement(*i->then_branch);
        const auto end_jump = emit_jump(OpCode::Jump);
        patch_jump(else_jump, chunk_->code_position());
        if (i->else_branch)
        {
            compile_statement(*i->else_branch);
        }
        patch_jump(end_jump, chunk_->code_position());
    }
    else if (const auto* w = dynamic_cast<const WhileStmt*>(&stmt))
    {
        const auto loop_start = chunk_->code_position();
        compile_expression(*w->condition);
        const auto exit_jump = emit_jump(OpCode::JumpIfFalse);
        compile_statement(*w->body);
        const auto back_jump = emit_jump(OpCode::Jump);
        patch_jump(back_jump, loop_start);
        patch_jump(exit_jump, chunk_->code_position());
    }
    else
    {
        fail("unknown statement");
    }
}

void CodeGenerator::compile_expression(const Expr& expr)
{
    if (const auto* n = dynamic_cast<const NumberExpr*>(&expr))
    {
        emit_constant(n->value);
        return;
    }
    if (const auto* b = dynamic_cast<const BoolExpr*>(&expr))
    {
        emit_constant(b->value);
        return;
    }
    if (dynamic_cast<const NullExpr*>(&expr) != nullptr)
    {
        emit_constant(Null{});
        return;
    }
    if (const auto* v = dynamic_cast<const VariableExpr*>(&expr))
    {
        emit_opcode(OpCode::LoadLocal);
        emit_byte(static_cast<std::uint8_t>(resolve_local(v->name)));
        return;
    }
    if (const auto* u = dynamic_cast<const UnaryExpr*>(&expr))
    {
        compile_expression(*u->operand);
        emit_opcode(u->op == UnaryExpr::Op::Negate ? OpCode::Negate : OpCode::Not);
        return;
    }
    if (const auto* a = dynamic_cast<const AssignExpr*>(&expr))
    {
        compile_expression(*a->value);
        const auto slot = resolve_local(a->name);
        emit_opcode(OpCode::StoreLocal);
        emit_byte(static_cast<std::uint8_t>(slot));
        emit_opcode(OpCode::LoadLocal);
        emit_byte(static_cast<std::uint8_t>(slot));
        return;
    }
    if (const auto* c = dynamic_cast<const CallExpr*>(&expr))
    {
        for (const auto& arg : c->args)
        {
            compile_expression(*arg);
        }
        const auto it = function_indices_.find(c->callee);
        if (it == function_indices_.end())
        {
            fail("unknown function: " + c->callee);
        }
        emit_opcode(OpCode::Call);
        emit_byte(static_cast<std::uint8_t>(it->second));
        return;
    }
    if (const auto* bin = dynamic_cast<const BinaryExpr*>(&expr))
    {
        if (bin->op == BinaryExpr::Op::And)
        {
            compile_expression(*bin->left);
            const auto false_jump = emit_jump(OpCode::JumpIfFalse);
            compile_expression(*bin->right);
            const auto end_jump = emit_jump(OpCode::Jump);
            patch_jump(false_jump, chunk_->code_position());
            emit_constant(false);
            patch_jump(end_jump, chunk_->code_position());
            return;
        }
        if (bin->op == BinaryExpr::Op::Or)
        {
            compile_expression(*bin->left);
            const auto eval_right = emit_jump(OpCode::JumpIfFalse);
            emit_constant(true);
            const auto end_jump = emit_jump(OpCode::Jump);
            patch_jump(eval_right, chunk_->code_position());
            compile_expression(*bin->right);
            patch_jump(end_jump, chunk_->code_position());
            return;
        }

        compile_expression(*bin->left);
        compile_expression(*bin->right);
        switch (bin->op)
        {
        case BinaryExpr::Op::Add:
            emit_opcode(OpCode::Add);
            break;
        case BinaryExpr::Op::Subtract:
            emit_opcode(OpCode::Subtract);
            break;
        case BinaryExpr::Op::Multiply:
            emit_opcode(OpCode::Multiply);
            break;
        case BinaryExpr::Op::Divide:
            emit_opcode(OpCode::Divide);
            break;
        case BinaryExpr::Op::Equal:
            emit_opcode(OpCode::Equal);
            break;
        case BinaryExpr::Op::NotEqual:
            emit_opcode(OpCode::NotEqual);
            break;
        case BinaryExpr::Op::Less:
            emit_opcode(OpCode::Less);
            break;
        case BinaryExpr::Op::LessEqual:
            emit_opcode(OpCode::LessEqual);
            break;
        case BinaryExpr::Op::Greater:
            emit_opcode(OpCode::Greater);
            break;
        case BinaryExpr::Op::GreaterEqual:
            emit_opcode(OpCode::GreaterEqual);
            break;
        default:
            fail("unsupported binary operator");
        }
        return;
    }
    fail("unknown expression");
}

void CodeGenerator::emit_opcode(OpCode op)
{
    chunk_->write(op, 0);
}

void CodeGenerator::emit_byte(std::uint8_t byte)
{
    chunk_->write(byte, 0);
}

void CodeGenerator::emit_u16(std::uint16_t value)
{
    emit_byte(static_cast<std::uint8_t>(value & 0xff));
    emit_byte(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

std::size_t CodeGenerator::emit_jump(OpCode op)
{
    emit_opcode(op);
    const auto offset = chunk_->code_position();
    emit_u16(0);
    return offset;
}

void CodeGenerator::patch_jump(std::size_t operand_offset, std::size_t target)
{
    const auto relative = static_cast<std::int16_t>(target - (operand_offset + 2));
    chunk_->patch_u16(operand_offset, static_cast<std::uint16_t>(relative));
}

void CodeGenerator::emit_constant(Value value)
{
    const auto index = chunk_->add_constant(std::move(value));
    emit_opcode(OpCode::Constant);
    emit_byte(static_cast<std::uint8_t>(index));
}

} // namespace xenon
