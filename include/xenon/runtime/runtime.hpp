#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "xenon/bytecode/xenon_binary.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/virtual_heap.hpp"
#include "xenon/runtime/virtual_machine.hpp"
#include "xenon/runtime/virtual_stack.hpp"

namespace xenon
{

/// The Xenon executor: the environment that hosts loaded modules and the
/// virtual machine, exposing a JNI-like API to create objects, access fields,
/// manipulate the operand stack and run functions.
///
/// The executor holds an array of `Module` objects. Modules can be loaded from
/// a `.module` file or constructed by hand. `xenon.env` classes (Object, String,
/// Vector, Environment) live in a separate component and are described by the
/// module produced by `environmentModule()`.
class Runtime
{
  public:
    Runtime();

    // ---- modules ----
    void addModule(const Module& module);
    void loadModuleFile(const std::string& path);

    [[nodiscard]] std::size_t moduleCount() const noexcept
    {
        return _modules.size();
    }
    [[nodiscard]] const std::vector<Module>& modules() const noexcept
    {
        return _modules;
    }
    [[nodiscard]] const Symbol* find(std::string_view name) const noexcept;

    // ---- heap / objects ----
    Ref newObject(std::string_view className);
    Ref newArray(std::size_t length);
    Ref newString(std::string_view text);

    [[nodiscard]] ObjectKind objectKind(Ref ref) const;
    [[nodiscard]] std::string_view className(Ref ref) const;

    // ---- fields ----
    void setField(Ref ref, std::string_view name, Value value);
    [[nodiscard]] Value getField(Ref ref, std::string_view name) const;

    // ---- arrays ----
    [[nodiscard]] std::size_t arrayLength(Ref ref) const;
    void setElement(Ref ref, std::size_t index, Value value);
    [[nodiscard]] Value getElement(Ref ref, std::size_t index) const;

    // ---- strings ----
    [[nodiscard]] std::string_view stringValue(Ref ref) const;

    // ---- operand stack ----
    void push(Value value);
    [[nodiscard]] Value pop();
    [[nodiscard]] const Value& peek(std::size_t depth = 0) const;
    [[nodiscard]] std::size_t stackSize() const noexcept;

    // ---- execution ----
    [[nodiscard]] InterpretResult run(std::string_view name);
    void registerNative(std::string name, NativeFunction function);
    [[nodiscard]] InterpretResult runChunk(const XenonBinary& binary);
    [[nodiscard]] const Value& lastValue() const noexcept;

    /// Runs the program entry point, marshalling CLI arguments to the supported
    /// main() signatures and returning the process exit code.
    ///
    /// Supported signatures:
    ///   function(void)
    ///   function([const] int argc, [const] char* argv[])
    ///   function([const] int argc, [const] char* argv[], [const] char* envp[])
    ///   function(xenon::Vector<xenon::String> arguments)
    ///   function(xenon::Vector<xenon::String> arguments, xenon::Vector<xenon::String> environment)
    ///
    /// `argv`/`envp` are passed as heap arrays of strings; `Vector<String>`
    /// parameters are passed the same way (a single `Ref` to a string array).
    [[nodiscard]] int runMain(std::string_view name,
                              const std::vector<std::string>& arguments,
                              const std::vector<std::string>& environment);

    // ---- accessors ----
    [[nodiscard]] VirtualHeap& heap() noexcept
    {
        return _heap;
    }
    [[nodiscard]] const VirtualHeap& heap() const noexcept
    {
        return _heap;
    }
    [[nodiscard]] VirtualStack& stack() noexcept
    {
        return _stack;
    }
    [[nodiscard]] const VirtualStack& stack() const noexcept
    {
        return _stack;
    }

    // ---- type predicates ----
    [[nodiscard]] static bool isNumber(const Value& value);
    [[nodiscard]] static bool isBoolean(const Value& value);
    [[nodiscard]] static bool isNull(const Value& value);
    [[nodiscard]] static bool isRef(const Value& value);

  private:
    Ref newStringArray(const std::vector<std::string>& values);
    static int valueToExitCode(const Value& value);
    void reloadModules();

    std::vector<Module> _modules;
    VirtualHeap _heap;
    VirtualStack _stack;
    VirtualMachine _vm;
};

} // namespace xenon
