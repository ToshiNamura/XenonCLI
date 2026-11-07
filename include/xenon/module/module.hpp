#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xenon/module/symbol.hpp"

namespace xenon
{

/// A compiled module: the packed artifact stored in a `.module` file.
///
/// It contains the module name, its dependencies and the symbols exported by
/// the module (classes, functions, structs, templates, enums, variables).
struct Module
{
    std::string Name;                 ///< Module name.
    std::vector<std::string> Imports; ///< Module dependencies, in order.
    std::vector<Symbol> Symbols;      ///< Packed symbols, in declaration order.

    /// Returns the symbol with the given name, or `nullptr`.
    [[nodiscard]] const Symbol* find(std::string_view symbolName) const noexcept
    {
        for (const auto& symbol : Symbols)
        {
            if (symbol.Name == symbolName)
            {
                return &symbol;
            }
        }
        return nullptr;
    }
};

} // namespace xenon
