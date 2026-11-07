#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "xenon/env/object.hpp"

namespace xenon
{

/// The Xenon API environment: stores and manages the classes that derive from
/// `Object`, and can instantiate them by type name.
class Environment : public Object
{
  public:
    /// A factory that creates an instance of a Xenon class.
    using Factory = std::function<std::shared_ptr<Object>()>;

    /// Registers `factory` as the provider of the Xenon class `typeName`.
    void registerClass(std::string typeName, Factory factory);

    /// True when a class named `typeName` is registered.
    [[nodiscard]] bool hasClass(std::string_view typeName) const;

    /// Creates an instance of the registered class `typeName`.
    [[nodiscard]] std::shared_ptr<Object> create(std::string_view typeName) const;

    /// The number of registered classes.
    [[nodiscard]] std::size_t size() const noexcept
    {
        return _classes.size();
    }

    [[nodiscard]] std::string getType() const override;
    [[nodiscard]] std::string toString() const override;

  private:
    std::unordered_map<std::string, Factory> _classes;
};

} // namespace xenon
