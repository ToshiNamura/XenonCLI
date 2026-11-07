#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "xenon/runtime/heap_object.hpp"
#include "xenon/runtime/value.hpp"

namespace xenon
{

/// The managed heap of the virtual machine: owns all runtime objects and hands
/// out `Ref` handles.
class VirtualHeap
{
  public:
    /// Allocates a new object of `kind` and returns a reference to it.
    Ref allocate(ObjectKind kind, std::string className = {});

    /// Returns the object for `ref`, or `nullptr` if the reference is invalid.
    [[nodiscard]] HeapObject* get(Ref ref) noexcept;
    [[nodiscard]] const HeapObject* get(Ref ref) const noexcept;

    /// True when `ref` points at a live object.
    [[nodiscard]] bool contains(Ref ref) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return _objects.size();
    }

    void clear();

  private:
    std::vector<HeapObject> _objects;
};

} // namespace xenon
