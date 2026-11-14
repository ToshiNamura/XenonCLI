#pragma once

#ifndef XENON__COMPILER_HPP
#define XENON__COMPILER_HPP 1

#include <string>
#include <string_view>
#include <vector>

#include <xenon/asm/model.hpp>
#include <xenon/compiler/module_header.hpp>

namespace xenon
{
	/// The Xenon front end: translates a C++-like source module into a compiled
	/// `Module` (Xenon ASM declarations and code for the virtual machine).
	///
	/// Pipeline: source -> module header -> lexer -> parser -> code generator -> Module.
	class Compiler
	{
	public:
		/// Compiles `source` into the compiled `out` module. `hints` supplies
		/// external declarations (`.hint` files). Returns `true` on success.
		[[nodiscard]] bool compile(std::string_view source, Module& out, const std::vector<Module>& hints = {});

		/// Compiles a `.hint` file (declarations only) into `out`.
		[[nodiscard]] bool compileHints(std::string_view source, Module& out);

		/// The source-level module header parsed during the last `compile` call.
		[[nodiscard]] const ModuleHeader& header() const noexcept
		{
			return _header;
		}

		/// Human-readable description of the last compilation error.
		[[nodiscard]] const std::string& lastError() const noexcept
		{
			return _error;
		}

	private:
		ModuleHeader _header;
		std::string  _error;
	};
}

#endif // !XENON__COMPILER_HPP
