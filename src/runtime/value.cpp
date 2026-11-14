#include <xenon/runtime/value.hpp>

#include <array>
#include <charconv>

std::string xenon::toNumberString(double value)
{
	std::array<char, 64> buffer{};
	const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
	return std::string(buffer.data(), result.ptr);
}

std::string xenon::toString(const xenon::Value& value)
{
	if (const auto* number = std::get_if<double>(&value))
	{
		return xenon::toNumberString(*number);
	}
	if (const auto* boolean = std::get_if<bool>(&value))
	{
		return *boolean ? "true" : "false";
	}
	if (const auto* ref = std::get_if<xenon::Ref>(&value))
	{
		return "object#" + std::to_string(ref->id);
	}
	return "null";
}
