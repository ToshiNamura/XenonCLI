#include "xenon/runtime/env.hpp"

#include <utility>
#include <variant>

#include "xenon/runtime/error.hpp"

namespace xenon
{

Ref Env::new_object(std::string_view class_name)
{
    return heap_.allocate(ObjectKind::Object, std::string(class_name));
}

Ref Env::new_array(std::size_t length)
{
    const Ref ref = heap_.allocate(ObjectKind::Array);
    heap_.get(ref)->elements.assign(length, Null{});
    return ref;
}

Ref Env::new_string(std::string_view text)
{
    const Ref ref = heap_.allocate(ObjectKind::String);
    heap_.get(ref)->text = std::string(text);
    return ref;
}

ObjectKind Env::object_kind(Ref ref) const
{
    const Object* object = heap_.get(ref);
    if (object == nullptr)
    {
        throw RuntimeError("invalid object reference");
    }
    return object->kind;
}

std::string_view Env::class_name(Ref ref) const
{
    const Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::Object)
    {
        throw RuntimeError("not an object reference");
    }
    return object->class_name;
}

void Env::set_field(Ref ref, std::string_view name, Value value)
{
    Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::Object)
    {
        throw RuntimeError("not an object reference");
    }
    object->fields[std::string(name)] = std::move(value);
}

Value Env::get_field(Ref ref, std::string_view name) const
{
    const Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::Object)
    {
        throw RuntimeError("not an object reference");
    }
    const auto it = object->fields.find(std::string(name));
    if (it == object->fields.end())
    {
        throw RuntimeError("no such field: " + std::string(name));
    }
    return it->second;
}

std::size_t Env::array_length(Ref ref) const
{
    const Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::Array)
    {
        throw RuntimeError("not an array reference");
    }
    return object->elements.size();
}

void Env::set_element(Ref ref, std::size_t index, Value value)
{
    Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::Array)
    {
        throw RuntimeError("not an array reference");
    }
    if (index >= object->elements.size())
    {
        throw RuntimeError("array index out of range");
    }
    object->elements[index] = std::move(value);
}

Value Env::get_element(Ref ref, std::size_t index) const
{
    const Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::Array)
    {
        throw RuntimeError("not an array reference");
    }
    if (index >= object->elements.size())
    {
        throw RuntimeError("array index out of range");
    }
    return object->elements[index];
}

std::string_view Env::string_value(Ref ref) const
{
    const Object* object = heap_.get(ref);
    if (object == nullptr || object->kind != ObjectKind::String)
    {
        throw RuntimeError("not a string reference");
    }
    return object->text;
}

void Env::push(Value value)
{
    stack_.push_back(std::move(value));
}

Value Env::pop()
{
    if (stack_.empty())
    {
        throw RuntimeError("stack underflow");
    }
    auto value = std::move(stack_.back());
    stack_.pop_back();
    return value;
}

const Value& Env::peek(std::size_t depth) const
{
    if (depth >= stack_.size())
    {
        throw RuntimeError("stack peek out of range");
    }
    return stack_[stack_.size() - 1 - depth];
}

std::size_t Env::stack_size() const noexcept
{
    return stack_.size();
}

void Env::load(const Module& module)
{
    vm_.load(module);
}

const Symbol* Env::find(std::string_view name) const noexcept
{
    return vm_.find(name);
}

InterpretResult Env::run(std::string_view name)
{
    return vm_.run(name, stack_);
}

InterpretResult Env::run_chunk(const Chunk& chunk)
{
    return vm_.interpret(chunk, stack_);
}

const Value& Env::last_value() const noexcept
{
    return vm_.last_value();
}

bool Env::is_number(const Value& value)
{
    return std::holds_alternative<double>(value);
}

bool Env::is_boolean(const Value& value)
{
    return std::holds_alternative<bool>(value);
}

bool Env::is_null(const Value& value)
{
    return std::holds_alternative<Null>(value);
}

bool Env::is_ref(const Value& value)
{
    return std::holds_alternative<Ref>(value);
}

} // namespace xenon
