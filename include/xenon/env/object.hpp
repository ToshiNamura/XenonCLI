#pragma once

#ifndef XENON__OBJECT_HPP
#define XENON__OBJECT_HPP 1

#include <cstddef>
#include <string>

namespace xenon
{

/// Base class of every Xenon API object (the host-side `xenon.env` hierarchy).
///
/// Environment classes (`String`, `Vector<Type>`, `Environment`, ...) derive
/// from `Object` and can live on their own; the `Environment` class stores and
/// manages them.
class Object
{
  public:
    virtual ~Object() = default;

    /// Returns the Xenon type name of the object (e.g. `"String"`, `"Vector"`).
    [[nodiscard]] virtual std::string getType() const;

    /// Returns the hash code of the object.
    [[nodiscard]] virtual std::size_t getHash() const;

    /// Returns the string representation of the object.
    [[nodiscard]] virtual std::string toString() const;
};

} // namespace xenon

#endif // !XENON__OBJECT_HPP
