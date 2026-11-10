#include <xenon/env/string.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

xenon::String::String() = default;

xenon::String::String(std::string value) : _value(std::move(value)) { }

xenon::String::String(const char* value) : _value(value != nullptr ? value : "") { }

std::string xenon::String::getType() const
{
	return "String";
}

std::size_t xenon::String::getHash() const
{
	return std::hash<std::string>{}(_value);
}

std::string xenon::String::toString() const
{
	return _value;
}
