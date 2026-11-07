#include "xenon/env/string.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace xenon
{

String::String() = default;

String::String(std::string value) : _value(std::move(value))
{
}

String::String(const char* value) : _value(value != nullptr ? value : "")
{
}

std::string String::getType() const
{
    return "String";
}

std::size_t String::getHash() const
{
    return std::hash<std::string>{}(_value);
}

std::string String::toString() const
{
    return _value;
}

} // namespace xenon
