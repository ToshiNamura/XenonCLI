#include "xenon/runtime/virtual_machine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/error.hpp"
#include "xenon/runtime/virtual_heap.hpp"

namespace xenon
{
namespace
{

bool truthy(const Value& value)
{
    if (const auto* boolean = std::get_if<bool>(&value))
    {
        return *boolean;
    }
    if (const auto* number = std::get_if<double>(&value))
    {
        return *number != 0.0;
    }
    if (std::holds_alternative<Null>(value))
    {
        return false;
    }
    return true; // Ref
}

bool valuesEqual(const Value& a, const Value& b)
{
    if (const auto* na = std::get_if<double>(&a))
    {
        if (const auto* nb = std::get_if<double>(&b))
        {
            return *na == *nb;
        }
        return false;
    }
    if (const auto* ba = std::get_if<bool>(&a))
    {
        if (const auto* bb = std::get_if<bool>(&b))
        {
            return *ba == *bb;
        }
        return false;
    }
    if (const auto* ra = std::get_if<Ref>(&a))
    {
        if (const auto* rb = std::get_if<Ref>(&b))
        {
            return ra->id == rb->id;
        }
        return false;
    }
    return std::holds_alternative<Null>(a) && std::holds_alternative<Null>(b);
}

} // namespace

void VirtualMachine::reset()
{
    _frames.clear();
    _lastValue = Null{};
}

void VirtualMachine::load(const Module& module)
{
    _symbols = module.Symbols;
    rebuildFunctionTable();
}

void VirtualMachine::loadAll(const std::vector<Module>& modules)
{
    _symbols.clear();
    for (const auto& module : modules)
    {
        _symbols.insert(_symbols.end(), module.Symbols.begin(), module.Symbols.end());
    }
    rebuildFunctionTable();
}

void VirtualMachine::rebuildFunctionTable()
{
    _functions.clear();
    for (const auto& symbol : _symbols)
    {
        if (symbol.Kind == SymbolKind::Function)
        {
            _functions.push_back(&symbol);
        }
    }
}

const Symbol* VirtualMachine::find(std::string_view name) const noexcept
{
    for (const auto& symbol : _symbols)
    {
        if (symbol.Name == name)
        {
            return &symbol;
        }
    }
    return nullptr;
}

void VirtualMachine::call(const Symbol& symbol, VirtualStack& stack)
{
    Frame frame;
    frame.binary = &symbol.Body;
    frame.ip = 0;
    frame.locals.resize(symbol.Arity + symbol.Locals);
    for (std::size_t i = symbol.Arity; i-- > 0;)
    {
        frame.locals[i] = stack.pop();
    }
    _frames.push_back(std::move(frame));
}

void VirtualMachine::registerNative(std::string name, NativeFunction function)
{
    _natives[std::move(name)] = std::move(function);
}

void VirtualMachine::callExternal(const Symbol& symbol, VirtualStack& stack)
{
    std::vector<Value> args(symbol.Arity);
    for (std::size_t i = symbol.Arity; i-- > 0;)
    {
        args[i] = stack.pop();
    }

    const auto it = _natives.find(symbol.Name);
    if (it == _natives.end())
    {
        throw RuntimeError("external function not bound: " + symbol.Name);
    }
    stack.push(it->second(args));
}

InterpretResult VirtualMachine::run(std::string_view name, VirtualStack& stack)
{
    const Symbol* symbol = find(name);
    if (symbol == nullptr)
    {
        throw RuntimeError("unknown symbol: " + std::string(name));
    }
    if (symbol->Kind != SymbolKind::Function)
    {
        throw RuntimeError("symbol is not a function: " + std::string(name));
    }

    reset();

    if (!symbol->Defined)
    {
        callExternal(*symbol, stack);
        _lastValue = stack.pop();
        return InterpretResult::Ok;
    }

    call(*symbol, stack);
    execute(stack);
    return InterpretResult::Ok;
}

InterpretResult VirtualMachine::interpret(const XenonBinary& binary, VirtualStack& stack)
{
    reset();

    Frame frame;
    frame.binary = &binary;
    frame.ip = 0;
    _frames.push_back(std::move(frame));

    execute(stack);
    return InterpretResult::Ok;
}

void VirtualMachine::execute(VirtualStack& stack)
{
    while (true)
    {
        Frame& frame = _frames.back();
        const XenonBinary& binary = *frame.binary;
        const auto& code = binary.code();
        const auto& constants = binary.constants();

        auto readByte = [&]() -> std::uint8_t
        {
            if (frame.ip >= code.size())
            {
                throw RuntimeError("unexpected end of bytecode");
            }
            return code[frame.ip++];
        };

        auto readI16 = [&]() -> std::int16_t
        {
            if (frame.ip + 2 > code.size())
            {
                throw RuntimeError("unexpected end of bytecode");
            }
            const auto value = static_cast<std::int16_t>(
                code[frame.ip] | (static_cast<std::uint16_t>(code[frame.ip + 1]) << 8));
            frame.ip += 2;
            return value;
        };

        auto readConstant = [&]() -> const Value&
        {
            const auto index = readByte();
            if (index >= constants.size())
            {
                throw RuntimeError("constant index out of range");
            }
            return constants[index];
        };

        auto push = [&stack](Value value)
        {
            stack.push(std::move(value));
        };

        auto pop = [&stack]() -> Value
        {
            return stack.pop();
        };

        auto expectNumber = [](const Value& value) -> double
        {
            if (const auto* number = std::get_if<double>(&value))
            {
                return *number;
            }
            throw RuntimeError("operand must be a number");
        };

        const auto op = static_cast<OpCode>(readByte());

        switch (op)
        {
        case OpCode::Constant:
            push(readConstant());
            break;

        case OpCode::String:
        {
            const auto index = readByte();
            const auto& strings = binary.strings();
            if (index >= strings.size())
            {
                throw RuntimeError("string index out of range");
            }
            if (_heap == nullptr)
            {
                throw RuntimeError("no heap available for string literal");
            }
            const Ref ref = _heap->allocate(ObjectKind::String);
            _heap->get(ref)->Text = strings[index];
            push(Value{ref});
            break;
        }

        case OpCode::LoadLocal:
        {
            const auto slot = readByte();
            if (slot >= frame.locals.size())
            {
                throw RuntimeError("local slot out of range");
            }
            push(frame.locals[slot]);
            break;
        }

        case OpCode::StoreLocal:
        {
            const auto slot = readByte();
            if (slot >= frame.locals.size())
            {
                throw RuntimeError("local slot out of range");
            }
            frame.locals[slot] = pop();
            break;
        }

        case OpCode::Pop:
            pop();
            break;

        case OpCode::Negate:
            push(-expectNumber(pop()));
            break;

        case OpCode::Not:
            push(Value{!truthy(pop())});
            break;

        case OpCode::Add:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(a + b);
            break;
        }

        case OpCode::Subtract:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(a - b);
            break;
        }

        case OpCode::Multiply:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(a * b);
            break;
        }

        case OpCode::Divide:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            if (b == 0.0)
            {
                throw RuntimeError("division by zero");
            }
            push(a / b);
            break;
        }

        case OpCode::Equal:
        {
            const auto b = pop();
            const auto a = pop();
            push(Value{valuesEqual(a, b)});
            break;
        }

        case OpCode::NotEqual:
        {
            const auto b = pop();
            const auto a = pop();
            push(Value{!valuesEqual(a, b)});
            break;
        }

        case OpCode::Less:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(Value{a < b});
            break;
        }

        case OpCode::LessEqual:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(Value{a <= b});
            break;
        }

        case OpCode::Greater:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(Value{a > b});
            break;
        }

        case OpCode::GreaterEqual:
        {
            const auto b = expectNumber(pop());
            const auto a = expectNumber(pop());
            push(Value{a >= b});
            break;
        }

        case OpCode::Jump:
            frame.ip += readI16();
            break;

        case OpCode::JumpIfFalse:
        {
            const auto offset = readI16();
            if (!truthy(pop()))
            {
                frame.ip += offset;
            }
            break;
        }

        case OpCode::Call:
        {
            const auto index = readByte();
            if (index >= _functions.size())
            {
                throw RuntimeError("function index out of range");
            }
            const Symbol* function = _functions[index];
            if (!function->Defined)
            {
                callExternal(*function, stack);
            }
            else
            {
                call(*function, stack);
            }
            break;
        }

        case OpCode::Return:
        {
            Value result = pop();
            _frames.pop_back();
            if (_frames.empty())
            {
                _lastValue = std::move(result);
                return;
            }
            push(std::move(result));
            break;
        }

        default:
            throw RuntimeError("unknown opcode");
        }
    }
}

} // namespace xenon
