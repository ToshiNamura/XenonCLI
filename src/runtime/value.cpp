#include <xenon/runtime/value.hpp>

#include <format>

std::string xenon::toString(const xenon::Value& value)
{
	if (const auto* number = std::get_if<double>(&value))
	{
		return std::format("{}", *number);
	}
	if (const auto* boolean = std::get_if<bool>(&value))
	{
		return *boolean ? "true" : "false";
	}
	if (const auto* ref = std::get_if<xenon::Ref>(&value))
	{
		return std::format("object#{}", ref->id);
	}
	return "null";
}
