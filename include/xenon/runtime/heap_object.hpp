#pragma once

#ifndef XENON__HEAP_OBJECT_HPP
#define XENON__HEAP_OBJECT_HPP 1

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "xenon/runtime/value.hpp"

namespace xenon
{

/// The kinds of objects living on the virtual heap.
enum class ObjectKind : std::uint8_t
{
    Object, ///< A class/struct instance with named fields.
    Array,  ///< An indexed list of values.
    String, ///< An immutable text value.
};

/// A single object living on the managed heap of the virtual machine.
struct HeapObject
{
    ObjectKind Kind = ObjectKind::Object;
    std::string ClassName;                        ///< `Object`: the type name.
    std::unordered_map<std::string, Value> Fields; ///< `Object`: named fields.
    std::vector<Value> Elements;                   ///< `Array`: indexed elements.
    std::string Text;                              ///< `String`: the text.
};

} // namespace xenon

#endif // !XENON__HEAP_OBJECT_HPP
