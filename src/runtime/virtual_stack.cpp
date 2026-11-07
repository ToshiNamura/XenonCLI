#include "xenon/runtime/virtual_stack.hpp"

#include <utility>

#include "xenon/runtime/error.hpp"

namespace xenon
{

void VirtualStack::push(Value value)
{
    _values.push_back(std::move(value));
}

Value VirtualStack::pop()
{
    if (_values.empty())
    {
        throw RuntimeError("stack underflow");
    }
    auto value = std::move(_values.back());
    _values.pop_back();
    return value;
}

const Value& VirtualStack::peek(std::size_t depth) const
{
    if (depth >= _values.size())
    {
        throw RuntimeError("stack peek out of range");
    }
    return _values[_values.size() - 1 - depth];
}

} // namespace xenon
