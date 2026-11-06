#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "xenon/bytecode/chunk.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/value.hpp"

namespace xenon
{

enum class InterpretResult
{
    Ok,
    RuntimeError,
};

/// The Xenon execution engine: loads symbols from a compiled `.module` and
/// executes function bodies against an external operand stack.
///
/// Execution uses a call stack of frames; each frame has its own instruction
/// pointer and local variable slots.
class VM
{
  public:
    /// Loads the symbols of `module` into the VM.
    void load(const Module& module);

    /// Looks up a loaded symbol by name. Returns `nullptr` if not found.
    [[nodiscard]] const Symbol* find(std::string_view name) const noexcept;

    /// The symbols currently loaded into the VM.
    [[nodiscard]] const std::vector<Symbol>& symbols() const noexcept
    {
        return symbols_;
    }

    /// Executes the function symbol named `name` on `stack`. Throws `RuntimeError`.
    [[nodiscard]] InterpretResult run(std::string_view name, std::vector<Value>& stack);

    /// Low-level: executes raw bytecode on `stack`. Throws `RuntimeError`.
    [[nodiscard]] InterpretResult interpret(const Chunk& chunk, std::vector<Value>& stack);

    /// The value produced by the most recent `OpCode::Return`.
    [[nodiscard]] const Value& last_value() const noexcept
    {
        return last_value_;
    }

    /// Clears the execution state (frames), not the caller's stack.
    void reset();

  private:
    struct Frame
    {
        const Chunk* chunk = nullptr;
        std::size_t ip = 0;
        std::vector<Value> locals;
    };

    void call(const Symbol& symbol, std::vector<Value>& stack);
    void execute(std::vector<Value>& stack);

    std::vector<Symbol> symbols_;
    std::vector<const Symbol*> functions_;
    std::vector<Frame> frames_;
    Value last_value_{Null{}};
};

} // namespace xenon
