#include "xenon/runtime/runtime.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

#include "xenon/module/module_file.hpp"
#include "xenon/runtime/error.hpp"

namespace xenon
{

Runtime::Runtime()
{
    _vm.setHeap(_heap);
}

// ---- modules ----

void Runtime::addModule(const Module& module)
{
    _modules.push_back(module);
    reloadModules();
}

void Runtime::loadModuleFile(const std::string& path)
{
    Module module;
    readModuleFile(path, module);
    addModule(module);
}

const Symbol* Runtime::find(std::string_view name) const noexcept
{
    for (const auto& module : _modules)
    {
        const Symbol* symbol = module.find(name);
        if (symbol != nullptr)
        {
            return symbol;
        }
    }
    return nullptr;
}

void Runtime::reloadModules()
{
    _vm.loadAll(_modules);
}

// ---- heap / objects ----

Ref Runtime::newObject(std::string_view className)
{
    return _heap.allocate(ObjectKind::Object, std::string(className));
}

Ref Runtime::newArray(std::size_t length)
{
    const Ref ref = _heap.allocate(ObjectKind::Array);
    _heap.get(ref)->Elements.assign(length, Null{});
    return ref;
}

Ref Runtime::newString(std::string_view text)
{
    const Ref ref = _heap.allocate(ObjectKind::String);
    _heap.get(ref)->Text = std::string(text);
    return ref;
}

ObjectKind Runtime::objectKind(Ref ref) const
{
    const HeapObject* object = _heap.get(ref);
    if (object == nullptr)
    {
        throw RuntimeError("invalid object reference");
    }
    return object->Kind;
}

std::string_view Runtime::className(Ref ref) const
{
    const HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::Object)
    {
        throw RuntimeError("not an object reference");
    }
    return object->ClassName;
}

void Runtime::setField(Ref ref, std::string_view name, Value value)
{
    HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::Object)
    {
        throw RuntimeError("not an object reference");
    }
    object->Fields[std::string(name)] = std::move(value);
}

Value Runtime::getField(Ref ref, std::string_view name) const
{
    const HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::Object)
    {
        throw RuntimeError("not an object reference");
    }
    const auto it = object->Fields.find(std::string(name));
    if (it == object->Fields.end())
    {
        throw RuntimeError("no such field: " + std::string(name));
    }
    return it->second;
}

std::size_t Runtime::arrayLength(Ref ref) const
{
    const HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::Array)
    {
        throw RuntimeError("not an array reference");
    }
    return object->Elements.size();
}

void Runtime::setElement(Ref ref, std::size_t index, Value value)
{
    HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::Array)
    {
        throw RuntimeError("not an array reference");
    }
    if (index >= object->Elements.size())
    {
        throw RuntimeError("array index out of range");
    }
    object->Elements[index] = std::move(value);
}

Value Runtime::getElement(Ref ref, std::size_t index) const
{
    const HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::Array)
    {
        throw RuntimeError("not an array reference");
    }
    if (index >= object->Elements.size())
    {
        throw RuntimeError("array index out of range");
    }
    return object->Elements[index];
}

std::string_view Runtime::stringValue(Ref ref) const
{
    const HeapObject* object = _heap.get(ref);
    if (object == nullptr || object->Kind != ObjectKind::String)
    {
        throw RuntimeError("not a string reference");
    }
    return object->Text;
}

// ---- operand stack ----

void Runtime::push(Value value)
{
    _stack.push(std::move(value));
}

Value Runtime::pop()
{
    return _stack.pop();
}

const Value& Runtime::peek(std::size_t depth) const
{
    return _stack.peek(depth);
}

std::size_t Runtime::stackSize() const noexcept
{
    return _stack.size();
}

// ---- execution ----

InterpretResult Runtime::run(std::string_view name)
{
    return _vm.run(name, _stack);
}

void Runtime::registerNative(std::string name, NativeFunction function)
{
    _vm.registerNative(std::move(name), std::move(function));
}

InterpretResult Runtime::runChunk(const XenonBinary& binary)
{
    return _vm.interpret(binary, _stack);
}

const Value& Runtime::lastValue() const noexcept
{
    return _vm.lastValue();
}

// ---- entry point ----

Ref Runtime::newStringArray(const std::vector<std::string>& values)
{
    const Ref ref = newArray(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        setElement(ref, i, Value{newString(values[i])});
    }
    return ref;
}

int Runtime::valueToExitCode(const Value& value)
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

int Runtime::runMain(std::string_view name, const std::vector<std::string>& arguments,
                     const std::vector<std::string>& environment)
{
    const Symbol* symbol = find(name);
    if (symbol == nullptr)
    {
        throw RuntimeError("unknown entry point: " + std::string(name));
    }
    if (symbol->Kind != SymbolKind::Function)
    {
        throw RuntimeError("entry point is not a function: " + std::string(name));
    }

    const bool vectorStyle = symbol->Signature.find("Vector") != std::string::npos;

    if (vectorStyle)
    {
        // function(xenon::Vector<xenon::String> arguments[, environment])
        if (symbol->Arity >= 1)
        {
            push(Value{newStringArray(arguments)});
        }
        if (symbol->Arity >= 2)
        {
            push(Value{newStringArray(environment)});
        }
    }
    else
    {
        // function(void) / function(int argc[, char* argv[] [, char* envp[]]])
        if (symbol->Arity >= 1)
        {
            push(Value{static_cast<double>(arguments.size())});
        }
        if (symbol->Arity >= 2)
        {
            push(Value{newStringArray(arguments)});
        }
        if (symbol->Arity >= 3)
        {
            push(Value{newStringArray(environment)});
        }
    }

    if (run(name) != InterpretResult::Ok)
    {
        throw RuntimeError("entry point failed: " + std::string(name));
    }

    return valueToExitCode(lastValue());
}

// ---- type predicates ----

bool Runtime::isNumber(const Value& value)
{
    return std::holds_alternative<double>(value);
}

bool Runtime::isBoolean(const Value& value)
{
    return std::holds_alternative<bool>(value);
}

bool Runtime::isNull(const Value& value)
{
    return std::holds_alternative<Null>(value);
}

bool Runtime::isRef(const Value& value)
{
    return std::holds_alternative<Ref>(value);
}

} // namespace xenon
