#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "xenon/runtime/value.hpp"

namespace xenon
{

/// The kinds of heap objects.
enum class ObjectKind : std::uint8_t
{
    Object, ///< A class/struct instance with named fields.
    Array,  ///< An indexed list of values.
    String, ///< An immutable text value.
};

/// A single object living on the managed heap.
struct Object
{
    ObjectKind kind = ObjectKind::Object;
    std::string class_name;                        ///< `Object`: the type name.
    std::unordered_map<std::string, Value> fields; ///< `Object`: named fields.
    std::vector<Value> elements;                   ///< `Array`: indexed elements.
    std::string text;                              ///< `String`: the text.
};

/// The managed heap: owns all runtime objects and hands out `Ref` handles.
class Heap
{
  public:
    /// Allocates a new object of `kind` and returns a reference to it.
    Ref allocate(ObjectKind kind, std::string class_name = {});

    /// Returns the object for `ref`, or `nullptr` if the reference is invalid.
    [[nodiscard]] Object* get(Ref ref) noexcept;
    [[nodiscard]] const Object* get(Ref ref) const noexcept;

    /// True when `ref` points at a live object.
    [[nodiscard]] bool contains(Ref ref) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return objects_.size();
    }

    void clear();

  private:
    std::vector<Object> objects_;
};

} // namespace xenon
