#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>

#include "xenon/module/module.hpp"

namespace xenon
{

/// Raised when a `.module` file cannot be read or has an invalid format.
class ModuleFileError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

/// Serializes `module` into `os` in the binary `.module` format.
void writeModuleFile(std::ostream& os, const Module& module);

/// Deserializes a module from `is` into `out`. Throws `ModuleFileError`.
void readModuleFile(std::istream& is, Module& out);

/// Convenience overloads for file paths.
void writeModuleFile(const std::string& path, const Module& module);
void readModuleFile(const std::string& path, Module& out);

} // namespace xenon
