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
    std::string name;                 ///< Module name.
    std::vector<std::string> imports; ///< Module dependencies, in order.
    std::vector<Symbol> symbols;      ///< Packed symbols, in declaration order.

    /// Returns the symbol with the given name, or `nullptr`.
    [[nodiscard]] const Symbol* find(std::string_view symbol_name) const noexcept
    {
        for (const auto& symbol : symbols)
        {
            if (symbol.name == symbol_name)
            {
                return &symbol;
            }
        }
        return nullptr;
    }
};

} // namespace xenon
