#pragma once

#include <string>
#include <string_view>

#include "xenon/env/object.hpp"

namespace xenon
{

/// The Xenon string class (host-side analogue of `std::string`).
class String : public Object
{
  public:
    String();
    explicit String(std::string value);
    String(const char* value);

    [[nodiscard]] std::string getType() const override;
    [[nodiscard]] std::size_t getHash() const override;
    [[nodiscard]] std::string toString() const override;

    [[nodiscard]] const std::string& value() const noexcept
    {
        return _value;
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return _value.size();
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return _value.empty();
    }

    bool operator==(const String& other) const
    {
        return _value == other._value;
    }
    bool operator==(std::string_view other) const
    {
        return _value == other;
    }

  private:
    std::string _value;
};

} // namespace xenon
