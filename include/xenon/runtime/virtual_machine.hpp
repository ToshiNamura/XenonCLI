#pragma once

#ifndef XENON__VIRTUAL_MACHINE_HPP
#define XENON__VIRTUAL_MACHINE_HPP 1

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <xenon/asm/binary.hpp>
#include <xenon/asm/model.hpp>
#include <xenon/runtime/value.hpp>
#include <xenon/runtime/virtual_stack.hpp>

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

	/// The Xenon execution engine: executes `XenonBinary` bodies of functions and
	/// methods loaded from compiled modules.
	///
	/// Instructions refer to functions, types and globals by name (string-pool
	/// indices), so modules are linked at runtime and can call each other across
	/// imports. Execution uses a call stack of frames; each frame has its own
	/// instruction pointer and local variable slots.
	class VirtualMachine
	{
	public:
		/// Loads `module`, replacing any previously loaded modules.
		void load(const Module& module);

		/// Loads every module in `modules`, replacing any previously loaded modules.
		void loadAll(const std::vector<Module>& modules);

		/// Returns the top-level function named `name` from the loaded modules, or `nullptr`.
		[[nodiscard]] const Function* findFunction(std::string_view name) const noexcept;

		/// Returns the method `Type::name` from the loaded modules, or `nullptr`.
		[[nodiscard]] const Method* findMethod(std::string_view name) const noexcept;

		/// Returns the type named `name` from the loaded modules, or `nullptr`.
		[[nodiscard]] const Type* findType(std::string_view name) const noexcept;

		/// True when an executable (function or method) named `name` is loaded.
		[[nodiscard]] bool hasFunction(std::string_view name) const noexcept;

		/// Registers a host function to satisfy a `.hint` external declaration.
		void registerNative(std::string name, NativeFunction function);

		/// Executes the function or method named `name` on `stack`. Throws `RuntimeError`.
		[[nodiscard]] InterpretResult run(std::string_view name, VirtualStack& stack);

		/// Low-level: executes raw binary code on `stack`. Throws `RuntimeError`.
		[[nodiscard]] InterpretResult interpret(const XenonBinary& binary, VirtualStack& stack);

		/// The value produced by the most recent `Return`.
		[[nodiscard]] const Value& lastValue() const noexcept
		{
			return _lastValue;
		}

		/// Clears the execution state (frames), not the caller's stack.
		void reset();

		/// Attaches the heap used to allocate objects.
		void setHeap(VirtualHeap& heap) noexcept
		{
			_heap = &heap;
		}

	private:
		struct Executable
		{
			std::string        Name;
			std::string        Module;    ///< Module that defines the executable.
			const XenonBinary* Body      = nullptr;
			std::size_t        Arity     = 0;
			std::size_t        Locals    = 0;
			bool               Defined   = true;
			bool               Exported  = true;
		};

		struct Frame
		{
			const XenonBinary* binary = nullptr;
			std::string        module; ///< Module of the running function.
			std::size_t        ip = 0;
			std::vector<Value> locals;
		};

		void rebuildTables();

		const Executable* findExecutable(std::string_view name) const noexcept;

		/// Allocates a heap string for a constant initializer.
		[[nodiscard]] Ref allocateString(const std::string& text);

		/// Throws unless the symbol may be used from `fromModule`.
		static void checkVisible(const std::string& name, const std::string& module, bool exported,
								 std::string_view fromModule);

		void call(const Executable& executable, VirtualStack& stack);

		void callExternal(const Executable& executable, VirtualStack& stack);

		void execute(VirtualStack& stack);

		std::vector<Module>                              _modules;
		struct SymbolInfo
		{
			std::string Module;
			bool        Exported = true;
		};

		std::unordered_map<std::string, Executable>      _executables;
		std::unordered_map<std::string, const Type*>     _types;
		std::unordered_map<std::string, SymbolInfo>      _typeInfo;
		std::unordered_map<std::string, Value>           _globals;
		std::unordered_map<std::string, SymbolInfo>      _globalInfo;
		std::unordered_map<std::string, const std::vector<std::string>*> _stringsByModule;
		std::unordered_map<std::string, NativeFunction>  _natives;
		std::vector<Frame>                               _frames;
		VirtualHeap*                                     _heap = nullptr;
		Value                                            _lastValue{Null{}};
	};
}

#endif // !XENON__VIRTUAL_MACHINE_HPP
