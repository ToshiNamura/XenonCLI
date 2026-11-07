#pragma once

#include <cstddef>
#include <vector>

#include "xenon/runtime/value.hpp"

namespace xenon
{

/// The operand stack used by the virtual machine.
///
/// Function arguments are pushed onto it before a call, instructions pop and
/// push operands, and the top of the stack holds the result of a `Return`.
class VirtualStack
{
  public:
    void push(Value value);
    Value pop();
    [[nodiscard]] const Value& peek(std::size_t depth = 0) const;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return _values.size();
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return _values.empty();
    }
    void clear() noexcept
    {
        _values.clear();
    }

    [[nodiscard]] std::vector<Value>& data() noexcept
    {
        return _values;
    }
    [[nodiscard]] const std::vector<Value>& data() const noexcept
    {
        return _values;
    }

  private:
    std::vector<Value> _values;
};

} // namespace xenon
