#pragma once

#ifndef XENON__SYMBOL_HPP
#define XENON__SYMBOL_HPP 1

#include <cstdint>
#include <string>
#include <vector>

#include <xenon/bytecode/xenon_binary.hpp>

namespace xenon
{
	/// The kinds of symbols a compiled `.module` file can contain.
	enum class SymbolKind : std::uint8_t
	{
		Function, ///< Executable function (has a bytecode body).
		Class,    ///< Class declaration.
		Struct,   ///< Struct declaration.
		Enum,     ///< Enum declaration.
		Template, ///< Template declaration.
		Variable, ///< Global variable / constant.
	};

	/// A single packed symbol from a compiled module.
	struct Symbol
	{
		std::string              Name;                        ///< Symbol name.
		SymbolKind               Kind = SymbolKind::Variable;
		std::string              Signature;                   ///< Human-readable signature, e.g. `int add(int, int)`.
		std::size_t              Arity = 0;                   ///< `Function`: number of parameters.
		std::size_t              Locals = 0;                  ///< `Function`: local slots beyond parameters.
		bool                     Defined = true;              ///< False for `.hint` declarations without a body.
		XenonBinary              Body;                        ///< Packed executable body; meaningful for `Function`.
		Value                    InitialValue = Null{};       ///< `Variable`: the initial value.
		std::vector<std::string> Fields;                      ///< Class/Struct: ordered field names.
		std::vector<std::string> Enumerators;                 ///< Enum: enumerator names.
	};
}

#endif // !XENON__SYMBOL_HPP
