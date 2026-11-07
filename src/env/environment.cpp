#include "xenon/env/environment.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace xenon
{

void Environment::registerClass(std::string typeName, Factory factory)
{
    _classes[std::move(typeName)] = std::move(factory);
}

bool Environment::hasClass(std::string_view typeName) const
{
    return _classes.find(std::string(typeName)) != _classes.end();
}

std::shared_ptr<Object> Environment::create(std::string_view typeName) const
{
    const auto it = _classes.find(std::string(typeName));
    if (it == _classes.end())
    {
        throw std::runtime_error("unknown Xenon class: " + std::string(typeName));
    }
    return it->second();
}

std::string Environment::getType() const
{
    return "Environment";
}

std::string Environment::toString() const
{
    return "Environment(" + std::to_string(_classes.size()) + " classes)";
}

} // namespace xenon
