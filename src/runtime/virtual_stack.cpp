#include <xenon/runtime/virtual_stack.hpp>

#include <utility>

#include <xenon/runtime/error.hpp>

void xenon::VirtualStack::push(xenon::Value value)
{
	_values.push_back(std::move(value));
}

xenon::Value xenon::VirtualStack::pop()
{
	if (_values.empty())
	{
		throw xenon::RuntimeError("stack underflow");
	}
	auto value = std::move(_values.back());
	_values.pop_back();
	return value;
}

const xenon::Value& xenon::VirtualStack::peek(std::size_t depth) const
{
	if (depth >= _values.size())
	{
		throw xenon::RuntimeError("stack peek out of range");
	}
	return _values[_values.size() - 1 - depth];
}
