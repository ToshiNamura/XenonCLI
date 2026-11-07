#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "xenon/compiler/ast.hpp"
#include "xenon/module/module.hpp"

namespace xenon
{

/// Compiles a parsed program into a compiled `Module` (packed symbols).
class CodeGenerator
{
  public:
    CodeGenerator(const Program& program, const Module& hints);

    /// Generates bytecode into `out`. Returns `true` on success.
    bool generate(Module& out);

    [[nodiscard]] const std::string& lastError() const noexcept
    {
        return _error;
    }

  private:
    struct CodeGenError
    {
    };

    struct Scope
    {
        std::unordered_map<std::string, std::size_t> Names;
        std::size_t BaseSlot = 0;
    };

    [[noreturn]] void fail(const std::string& message);

    void compileFunction(const FunctionDecl& function, Module& out);
    void compileTypeDecl(const TypeDecl& decl, Module& out);
    void compileStatement(const Stmt& stmt);
    void compileBlock(const BlockStmt& block);
    void compileExpression(const Expr& expr);

    void enterScope();
    void leaveScope();
    void declareLocal(const std::string& name);
    [[nodiscard]] std::size_t resolveLocal(const std::string& name);

    void emitOpcode(OpCode op);
    void emitByte(std::uint8_t byte);
    void emitU16(std::uint16_t value);
    [[nodiscard]] std::size_t emitJump(OpCode op);
    void patchJump(std::size_t operandOffset, std::size_t target);
    void emitConstant(Value value);

    const Program& _program;
    Module _hints;
    XenonBinary* _binary = nullptr;
    std::vector<Scope> _scopes;
    std::size_t _slotCounter = 0;
    std::size_t _maxSlots = 0;
    std::unordered_map<std::string, std::size_t> _functionIndices;
    std::string _error;
};

} // namespace xenon
