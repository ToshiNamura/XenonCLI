#include "xenon/env/object.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace xenon
{

std::string Object::getType() const
{
    return "Object";
}

std::size_t Object::getHash() const
{
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(this));
}

std::string Object::toString() const
{
    return "Object";
}

} // namespace xenon
