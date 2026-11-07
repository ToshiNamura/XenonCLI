#pragma once

#ifndef XENON__ERROR_HPP
#define XENON__ERROR_HPP 1

#include <stdexcept>

namespace xenon
{

/// Raised when the VM hits a runtime fault (bad opcode, stack under/overflow,
/// type error, division by zero, ...).
class RuntimeError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

} // namespace xenon

#endif // !XENON__ERROR_HPP
