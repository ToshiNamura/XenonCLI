#include <xenon/runtime/virtual_heap.hpp>

#include <utility>

xenon::Ref xenon::VirtualHeap::allocate(xenon::ObjectKind kind, std::string className)
{
	xenon::HeapObject object;
	object.Kind      = kind;
	object.ClassName = std::move(className);
	_objects.push_back(std::move(object));
	return xenon::Ref{static_cast<std::uint32_t>(_objects.size() - 1)};
}

xenon::HeapObject* xenon::VirtualHeap::get(xenon::Ref ref) noexcept
{
	return ref.id < _objects.size() ? &_objects[ref.id] : nullptr;
}

const xenon::HeapObject* xenon::VirtualHeap::get(xenon::Ref ref) const noexcept
{
	return ref.id < _objects.size() ? &_objects[ref.id] : nullptr;
}

bool xenon::VirtualHeap::contains(xenon::Ref ref) const noexcept
{
	return ref.id < _objects.size();
}

void xenon::VirtualHeap::clear()
{
	_objects.clear();
}
