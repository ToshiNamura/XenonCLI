#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "xenon/compiler/ast.hpp"
#include "xenon/module/module.hpp"

namespace xenon
{

/// Compiles a parsed program into a compiled `Module` (packed symbols).
class CodeGenerator
{
  public:
    CodeGenerator(const Program& program, const Module& hints, bool hintMode = false);

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
        std::unordered_map<std::string, std::string> Types;
        std::size_t BaseSlot = 0;
    };

    struct LoopContext
    {
        std::vector<std::size_t> breaks;
        std::vector<std::size_t> continues;
    };
    struct SwitchContext
    {
        std::vector<std::size_t> breaks;
    };

    [[noreturn]] void fail(const std::string& message);

    void compileFunction(const FunctionDecl& function, Module& out, const std::string& qualifiedName = {});
    void compileTypeDecl(const TypeDecl& decl, Module& out);
    void compileGlobalVar(const GlobalVarDecl& global, Module& out);
    void compileStatement(const Stmt& stmt);
    void compileFor(const ForStmt& stmt);
    void compileDoWhile(const DoWhileStmt& stmt);
    void compileSwitch(const SwitchStmt& stmt);
    void compileBlock(const BlockStmt& block);
    void compileExpression(const Expr& expr);

    void enterScope();
    void leaveScope();
    void declareLocal(const std::string& name, const std::string& type = {});
    [[nodiscard]] std::size_t allocateTemp();
    [[nodiscard]] std::size_t resolveLocal(const std::string& name);
    [[nodiscard]] bool hasLocal(const std::string& name) const;
    [[nodiscard]] std::size_t resolveGlobal(const std::string& name);
    [[nodiscard]] Value evaluateConstant(const Expr* init);
    [[nodiscard]] std::string localType(const std::string& name);
    [[nodiscard]] std::size_t resolveField(const Expr& object, const std::string& fieldName);

    [[nodiscard]] std::string substituteType(const std::string& name) const;
    [[nodiscard]] std::size_t resolveFunction(const std::string& callee);
    void ensureTypeInstantiated(const std::string& typeName);
    void emitTypeSymbol(const TypeDecl& decl, const std::string& name, Module& out);

    void emitOpcode(OpCode op);
    void emitByte(std::uint8_t byte);
    void emitU16(std::uint16_t value);
    [[nodiscard]] std::size_t emitJump(OpCode op);
    void patchJump(std::size_t operandOffset, std::size_t target);
    void emitConstant(Value value);
    void emitString(const std::string& value);

    const Program& _program;
    Module _hints;
    bool _hintMode = false;
    XenonBinary* _binary = nullptr;
    std::vector<Scope> _scopes;
    std::vector<LoopContext> _loops;
    std::vector<SwitchContext> _switches;
    std::size_t _slotCounter = 0;
    std::size_t _maxSlots = 0;
    struct FunctionTemplate
    {
        std::vector<std::string> Params;
        const FunctionDecl* Decl = nullptr;
    };
    struct TypeTemplate
    {
        std::vector<std::string> Params;
        const TypeDecl* Decl = nullptr;
    };
    struct PendingFunction
    {
        std::string Name;
        const FunctionDecl* Decl = nullptr;
        std::unordered_map<std::string, std::string> Substitutions;
    };
    struct PendingType
    {
        std::string Name;
        const TypeDecl* Decl = nullptr;
    };

    std::unordered_map<std::string, std::size_t> _functionIndices;
    std::unordered_map<std::string, std::size_t> _globalIndices;
    std::unordered_map<std::string, std::vector<std::string>> _typeFields;
    std::unordered_map<std::string, FunctionTemplate> _functionTemplates;
    std::unordered_map<std::string, TypeTemplate> _typeTemplates;
    std::vector<PendingFunction> _pendingFunctions;
    std::vector<PendingType> _pendingTypes;
    std::unordered_map<std::string, std::string> _substitutions;
    std::string _error;
};

} // namespace xenon
