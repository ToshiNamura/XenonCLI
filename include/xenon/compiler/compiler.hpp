#pragma once

#include <string>
#include <string_view>

#include "xenon/compiler/module.hpp"
#include "xenon/module/module.hpp"

namespace xenon
{

/// The Xenon front end: translates a C++-like source module into a compiled
/// `.module` artifact (packed symbols for the VM).
///
/// Pipeline: source -> module header -> lexer -> parser -> code generator -> Module.
/// The module header (`export module`, `import`, `#include`) is parsed first;
/// the body compiler is not implemented yet.
class Compiler
{
  public:
    /// Compiles `source` into the compiled `out` module. Returns `true` on success.
    [[nodiscard]] bool compile(std::string_view source, Module& out);

    /// The source-level module header parsed during the last `compile` call.
    [[nodiscard]] const ModuleHeader& header() const noexcept
    {
        return header_;
    }

    /// Human-readable description of the last compilation error.
    [[nodiscard]] const std::string& last_error() const noexcept
    {
        return error_;
    }

  private:
    ModuleHeader header_;
    std::string error_;
};

} // namespace xenon
