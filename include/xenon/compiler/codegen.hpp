#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "xenon/compiler/ast.hpp"
#include "xenon/module/module.hpp"

namespace xenon
{

/// Compiles a parsed program into a compiled `Module` (packed symbols).
class CodeGenerator
{
  public:
    explicit CodeGenerator(const Program& program);

    /// Generates bytecode into `out`. Returns `true` on success.
    bool generate(Module& out);

    [[nodiscard]] const std::string& last_error() const noexcept
    {
        return error_;
    }

  private:
    struct CodeGenError
    {
    };

    struct Scope
    {
        std::unordered_map<std::string, std::size_t> names;
        std::size_t base_slot = 0;
    };

    [[noreturn]] void fail(const std::string& message);

    void compile_function(const FunctionDecl& function, Module& out);
    void compile_statement(const Stmt& stmt);
    void compile_block(const BlockStmt& block);
    void compile_expression(const Expr& expr);

    void enter_scope();
    void leave_scope();
    void declare_local(const std::string& name);
    [[nodiscard]] std::size_t resolve_local(const std::string& name);

    void emit_opcode(OpCode op);
    void emit_byte(std::uint8_t byte);
    void emit_u16(std::uint16_t value);
    [[nodiscard]] std::size_t emit_jump(OpCode op);
    void patch_jump(std::size_t operand_offset, std::size_t target);
    void emit_constant(Value value);

    const Program& program_;
    Chunk* chunk_ = nullptr;
    std::vector<Scope> scopes_;
    std::size_t slot_counter_ = 0;
    std::size_t max_slots_ = 0;
    std::unordered_map<std::string, std::size_t> function_indices_;
    std::string error_;
};

} // namespace xenon
