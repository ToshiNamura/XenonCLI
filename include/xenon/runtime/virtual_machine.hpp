#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "xenon/bytecode/xenon_binary.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/value.hpp"
#include "xenon/runtime/virtual_stack.hpp"

namespace xenon
{

class VirtualHeap;

/// A host-provided function that satisfies a `.hint` external declaration.
using NativeFunction = std::function<Value(const std::vector<Value>&)>;

enum class InterpretResult
{
    Ok,
    RuntimeError,
};

/// The Xenon execution engine: executes `XenonBinary` bodies of function
/// symbols loaded from compiled modules.
///
/// Execution uses a call stack of frames; each frame has its own instruction
/// pointer and local variable slots. Instructions interact with the `VirtualStack`.
class VirtualMachine
{
  public:
    /// Loads the symbols of `module`, replacing any previously loaded symbols.
    void load(const Module& module);

    /// Loads the symbols of every module, replacing any previously loaded symbols.
    void loadAll(const std::vector<Module>& modules);

    /// Looks up a loaded symbol by name. Returns `nullptr` if not found.
    [[nodiscard]] const Symbol* find(std::string_view name) const noexcept;

    /// The symbols currently loaded into the VM.
    [[nodiscard]] const std::vector<Symbol>& symbols() const noexcept
    {
        return _symbols;
    }

    /// Registers a host function to satisfy a `.hint` external declaration.
    void registerNative(std::string name, NativeFunction function);

    /// Executes the function symbol named `name` on `stack`. Throws `RuntimeError`.
    [[nodiscard]] InterpretResult run(std::string_view name, VirtualStack& stack);

    /// Low-level: executes raw binary code on `stack`. Throws `RuntimeError`.
    [[nodiscard]] InterpretResult interpret(const XenonBinary& binary, VirtualStack& stack);

    /// The value produced by the most recent `OpCode::Return`.
    [[nodiscard]] const Value& lastValue() const noexcept
    {
        return _lastValue;
    }

    /// Clears the execution state (frames), not the caller's stack.
    void reset();

    /// Attaches the heap used to allocate objects for string literals.
    void setHeap(VirtualHeap& heap) noexcept
    {
        _heap = &heap;
    }

  private:
    struct Frame
    {
        const XenonBinary* binary = nullptr;
        std::size_t ip = 0;
        std::vector<Value> locals;
    };

    void rebuildFunctionTable();
    void call(const Symbol& symbol, VirtualStack& stack);
    void callExternal(const Symbol& symbol, VirtualStack& stack);
    void execute(VirtualStack& stack);

    std::vector<Symbol> _symbols;
    std::vector<const Symbol*> _functions;
    std::unordered_map<std::string, NativeFunction> _natives;
    std::vector<Frame> _frames;
    VirtualHeap* _heap = nullptr;
    Value _lastValue{Null{}};
};

} // namespace xenon
