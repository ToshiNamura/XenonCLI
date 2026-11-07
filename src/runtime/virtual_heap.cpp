#include "xenon/runtime/virtual_heap.hpp"

#include <utility>

namespace xenon
{

Ref VirtualHeap::allocate(ObjectKind kind, std::string className)
{
    HeapObject object;
    object.Kind = kind;
    object.ClassName = std::move(className);
    _objects.push_back(std::move(object));
    return Ref{static_cast<std::uint32_t>(_objects.size() - 1)};
}

HeapObject* VirtualHeap::get(Ref ref) noexcept
{
    return ref.id < _objects.size() ? &_objects[ref.id] : nullptr;
}

const HeapObject* VirtualHeap::get(Ref ref) const noexcept
{
    return ref.id < _objects.size() ? &_objects[ref.id] : nullptr;
}

bool VirtualHeap::contains(Ref ref) const noexcept
{
    return ref.id < _objects.size();
}

void VirtualHeap::clear()
{
    _objects.clear();
}

} // namespace xenon
