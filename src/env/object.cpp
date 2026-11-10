#include <xenon/env/object.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

std::string xenon::Object::getType() const
{
	return "Object";
}

std::size_t xenon::Object::getHash() const
{
	return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(this));
}

std::string xenon::Object::toString() const
{
	return "Object";
}
