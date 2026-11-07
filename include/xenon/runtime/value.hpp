#pragma once

#ifndef XENON__VALUE_HPP
#define XENON__VALUE_HPP 1

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>

namespace xenon
{

/// Sentinel type for the `null` runtime value.
struct Null
{
    constexpr bool operator==(Null) const noexcept
    {
        return true;
    }
};

/// A reference to a heap object (like `jobject` in JNI).
struct Ref
{
    std::uint32_t id = 0;

    constexpr bool operator==(const Ref&) const noexcept = default;
};

/// The set of types a Xenon runtime value can hold.
using Value = std::variant<double, bool, Null, Ref>;

/// Human-readable representation of a value (used by the CLI and the debugger).
[[nodiscard]] std::string toString(const Value& value);

inline std::ostream& operator<<(std::ostream& os, const Value& value)
{
    return os << toString(value);
}

} // namespace xenon

#endif // !XENON__VALUE_HPP
