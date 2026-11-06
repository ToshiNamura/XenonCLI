#include "xenon/runtime/object.hpp"

#include <utility>

namespace xenon
{

Ref Heap::allocate(ObjectKind kind, std::string class_name)
{
    Object object;
    object.kind = kind;
    object.class_name = std::move(class_name);
    objects_.push_back(std::move(object));
    return Ref{static_cast<std::uint32_t>(objects_.size() - 1)};
}

Object* Heap::get(Ref ref) noexcept
{
    return ref.id < objects_.size() ? &objects_[ref.id] : nullptr;
}

const Object* Heap::get(Ref ref) const noexcept
{
    return ref.id < objects_.size() ? &objects_[ref.id] : nullptr;
}

bool Heap::contains(Ref ref) const noexcept
{
    return ref.id < objects_.size();
}

void Heap::clear()
{
    objects_.clear();
}

} // namespace xenon
