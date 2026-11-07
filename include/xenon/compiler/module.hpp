#pragma once

#ifndef XENON__COMPILER_MODULE_HPP
#define XENON__COMPILER_MODULE_HPP 1

#include <string>
#include <vector>

namespace xenon
{

/// The source-level header of a Xenon module (a `.ixx` interface unit).
///
/// The file layout is:
///   export module Name;      // module declaration (required, first statement)
///   import Dependency;       // module dependencies (C#-style `import`)
///   #include <header>        // host includes
///   ...body...               // code; `export` marks visible declarations
struct ModuleHeader
{
    std::string Name;                  ///< Module name from `export module Name;`.
    std::vector<std::string> Imports;  ///< Imported module dependencies, in order.
    std::vector<std::string> Includes; ///< Host `#include` directives, in order.
    std::size_t BodyOffset = 0;        ///< Byte offset where the module body begins.

    [[nodiscard]] bool empty() const noexcept
    {
        return Name.empty();
    }
};

} // namespace xenon

#endif // !XENON__COMPILER_MODULE_HPP
