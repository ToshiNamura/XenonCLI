#pragma once

#include <cstdint>
#include <string>

#include "xenon/bytecode/chunk.hpp"

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
    std::string name; ///< Symbol name.
    SymbolKind kind = SymbolKind::Variable;
    std::string signature;  ///< Human-readable signature, e.g. `int add(int, int)`.
    std::size_t arity = 0;  ///< `Function`: number of parameters.
    std::size_t locals = 0; ///< `Function`: number of local slots beyond parameters.
    Chunk body;             ///< Packed executable body; meaningful for `Function`.
};

} // namespace xenon
