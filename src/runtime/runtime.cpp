#include <xenon/runtime/runtime.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

#include <xenon/module/module_file.hpp>
#include <xenon/runtime/error.hpp>

xenon::Runtime::Runtime()
{
	_vm.setHeap(_heap);
}

// ---- modules ----

void xenon::Runtime::addModule(const xenon::Module& module)
{
	_modules.push_back(module);
	reloadModules();
}

void xenon::Runtime::loadModuleFile(const std::string& path)
{
	Module module;
	readModuleFile(path, module);
	addModule(module);
}

const xenon::Function* xenon::Runtime::findFunction(std::string_view name) const noexcept
{
	for (const auto& module : _modules)
	{
		if (const xenon::Function* function = module.findFunction(name))
		{
			return function;
		}
	}
	return nullptr;
}

const xenon::Method* xenon::Runtime::findMethod(std::string_view name) const noexcept
{
	for (const auto& module : _modules)
	{
		if (const xenon::Method* method = module.findMethod(name))
		{
			return method;
		}
	}
	return nullptr;
}

const xenon::Type* xenon::Runtime::findType(std::string_view name) const noexcept
{
	for (const auto& module : _modules)
	{
		if (const xenon::Type* type = module.findType(name))
		{
			return type;
		}
	}
	return nullptr;
}

bool xenon::Runtime::hasFunction(std::string_view name) const noexcept
{
	return _vm.hasFunction(name);
}

void xenon::Runtime::reloadModules()
{
	_vm.loadAll(_modules);
}

// ---- heap / objects ----

xenon::Ref xenon::Runtime::newObject(std::string_view className)
{
	return _heap.allocate(xenon::ObjectKind::Object, std::string(className));
}

xenon::Ref xenon::Runtime::newArray(std::size_t length)
{
	const Ref ref = _heap.allocate(ObjectKind::Array);
	_heap.get(ref)->Elements.assign(length, Null{});
	return ref;
}

xenon::Ref xenon::Runtime::newString(std::string_view text)
{
	const Ref ref = _heap.allocate(xenon::ObjectKind::String);
	_heap.get(ref)->Text = std::string(text);
	return ref;
}

xenon::ObjectKind xenon::Runtime::objectKind(xenon::Ref ref) const
{
	const xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr)
	{
		throw xenon::RuntimeError("invalid object reference");
	}
	return object->Kind;
}

std::string_view xenon::Runtime::className(xenon::Ref ref) const
{
	const xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::Object)
	{
		throw xenon::RuntimeError("not an object reference");
	}
	return object->ClassName;
}

void xenon::Runtime::setField(xenon::Ref ref, std::string_view name, Value value)
{
	xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::Object)
	{
		throw xenon::RuntimeError("not an object reference");
	}
	object->Fields[std::string(name)] = std::move(value);
}

xenon::Value xenon::Runtime::getField(xenon::Ref ref, std::string_view name) const
{
	const xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::Object)
	{
		throw xenon::RuntimeError("not an object reference");
	}
	const auto it = object->Fields.find(std::string(name));
	if (it == object->Fields.end())
	{
		throw xenon::RuntimeError("no such field: " + std::string(name));
	}
	return it->second;
}

std::size_t xenon::Runtime::arrayLength(Ref ref) const
{
	const xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::Array)
	{
		throw xenon::RuntimeError("not an array reference");
	}
	return object->Elements.size();
}

void xenon::Runtime::setElement(xenon::Ref ref, std::size_t index, Value value)
{
	xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::Array)
	{
		throw xenon::RuntimeError("not an array reference");
	}
	if (index >= object->Elements.size())
	{
		throw xenon::RuntimeError("array index out of range");
	}
	object->Elements[index] = std::move(value);
}

xenon::Value xenon::Runtime::getElement(xenon::Ref ref, std::size_t index) const
{
	const xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::Array)
	{
		throw xenon::RuntimeError("not an array reference");
	}
	if (index >= object->Elements.size())
	{
		throw xenon::RuntimeError("array index out of range");
	}
	return object->Elements[index];
}

std::string_view xenon::Runtime::stringValue(xenon::Ref ref) const
{
	const xenon::HeapObject* object = _heap.get(ref);
	if (object == nullptr || object->Kind != xenon::ObjectKind::String)
	{
		throw xenon::RuntimeError("not a string reference");
	}
	return object->Text;
}

// ---- operand stack ----

void xenon::Runtime::push(xenon::Value value)
{
	_stack.push(std::move(value));
}

xenon::Value xenon::Runtime::pop()
{
	return _stack.pop();
}

const xenon::Value& xenon::Runtime::peek(std::size_t depth) const
{
	return _stack.peek(depth);
}

std::size_t xenon::Runtime::stackSize() const noexcept
{
	return _stack.size();
}

// ---- execution ----

xenon::InterpretResult xenon::Runtime::run(std::string_view name)
{
	return _vm.run(name, _stack);
}

void xenon::Runtime::registerNative(std::string name, xenon::NativeFunction function)
{
	_vm.registerNative(std::move(name), std::move(function));
}

xenon::InterpretResult xenon::Runtime::runChunk(const xenon::XenonBinary& binary)
{
	return _vm.interpret(binary, _stack);
}

const xenon::Value& xenon::Runtime::lastValue() const noexcept
{
	return _vm.lastValue();
}

// ---- entry point ----

xenon::Ref xenon::Runtime::newStringArray(const std::vector<std::string>& values)
{
	const xenon::Ref ref = newArray(values.size());
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		setElement(ref, i, xenon::Value{newString(values[i])});
	}
	return ref;
}

int xenon::Runtime::valueToExitCode(const xenon::Value& value)
{
	if (const auto* number = std::get_if<double>(&value))
	{
		return static_cast<int>(*number);
	}
	if (const auto* boolean = std::get_if<bool>(&value))
	{
		return *boolean ? 0 : 1;
	}
	return 0;
}

int xenon::Runtime::runMain(std::string_view name, const std::vector<std::string>& arguments, const std::vector<std::string>& environment)
{
	std::size_t arity = 0;
	std::string signature;
	if (const xenon::Function* function = findFunction(name))
	{
		arity     = function->Arity;
		signature = function->Signature;
	}
	else if (const xenon::Method* method = findMethod(name))
	{
		arity     = method->Arity;
		signature = method->Signature;
	}
	else
	{
		throw xenon::RuntimeError("unknown entry point: " + std::string(name));
	}

	const bool vectorStyle = signature.find("Vector") != std::string::npos;

	if (vectorStyle)
	{
		// function(xenon::Vector<xenon::String> arguments[, environment])
		if (arity >= 1)
		{
			push(xenon::Value{newStringArray(arguments)});
		}
		if (arity >= 2)
		{
			push(xenon::Value{newStringArray(environment)});
		}
	}
	else
	{
		// function(void) / function(int argc[, char* argv[] [, char* envp[]]])
		if (arity >= 1)
		{
			push(xenon::Value{static_cast<double>(arguments.size())});
		}
		if (arity >= 2)
		{
			push(xenon::Value{newStringArray(arguments)});
		}
		if (arity >= 3)
		{
			push(xenon::Value{newStringArray(environment)});
		}
	}

	if (run(name) != xenon::InterpretResult::Ok)
	{
		throw xenon::RuntimeError("entry point failed: " + std::string(name));
	}

	return valueToExitCode(lastValue());
}

// ---- type predicates ----

bool xenon::Runtime::isNumber(const xenon::Value& value)
{
	return std::holds_alternative<double>(value);
}

bool xenon::Runtime::isBoolean(const xenon::Value& value)
{
	return std::holds_alternative<bool>(value);
}

bool xenon::Runtime::isNull(const xenon::Value& value)
{
	return std::holds_alternative<xenon::Null>(value);
}

bool xenon::Runtime::isRef(const xenon::Value& value)
{
	return std::holds_alternative<xenon::Ref>(value);
}
