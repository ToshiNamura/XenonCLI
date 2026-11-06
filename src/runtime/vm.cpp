#include "xenon/runtime/vm.hpp"

#include <utility>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/runtime/error.hpp"

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

bool values_equal(const Value& a, const Value& b)
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

void VM::reset()
{
    frames_.clear();
    last_value_ = Null{};
}

void VM::load(const Module& module)
{
    symbols_ = module.symbols;
    functions_.clear();
    for (const auto& symbol : symbols_)
    {
        if (symbol.kind == SymbolKind::Function)
        {
            functions_.push_back(&symbol);
        }
    }
}

const Symbol* VM::find(std::string_view name) const noexcept
{
    for (const auto& symbol : symbols_)
    {
        if (symbol.name == name)
        {
            return &symbol;
        }
    }
    return nullptr;
}

void VM::call(const Symbol& symbol, std::vector<Value>& stack)
{
    Frame frame;
    frame.chunk = &symbol.body;
    frame.ip = 0;
    frame.locals.resize(symbol.arity + symbol.locals);
    for (std::size_t i = symbol.arity; i-- > 0;)
    {
        if (stack.empty())
        {
            throw RuntimeError("stack underflow while passing arguments");
        }
        frame.locals[i] = std::move(stack.back());
        stack.pop_back();
    }
    frames_.push_back(std::move(frame));
}

InterpretResult VM::run(std::string_view name, std::vector<Value>& stack)
{
    const Symbol* symbol = find(name);
    if (symbol == nullptr)
    {
        throw RuntimeError("unknown symbol: " + std::string(name));
    }
    if (symbol->kind != SymbolKind::Function)
    {
        throw RuntimeError("symbol is not a function: " + std::string(name));
    }
    reset();
    call(*symbol, stack);
    execute(stack);
    return InterpretResult::Ok;
}

InterpretResult VM::interpret(const Chunk& chunk, std::vector<Value>& stack)
{
    reset();
    Frame frame;
    frame.chunk = &chunk;
    frame.ip = 0;
    frames_.push_back(std::move(frame));
    execute(stack);
    return InterpretResult::Ok;
}

void VM::execute(std::vector<Value>& stack)
{
    while (true)
    {
        Frame& frame = frames_.back();
        const Chunk& chunk = *frame.chunk;
        const auto& code = chunk.code();
        const auto& constants = chunk.constants();

        auto read_byte = [&]() -> std::uint8_t
        {
            if (frame.ip >= code.size())
            {
                throw RuntimeError("unexpected end of bytecode");
            }
            return code[frame.ip++];
        };

        auto read_i16 = [&]() -> std::int16_t
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

        auto read_constant = [&]() -> const Value&
        {
            const auto index = read_byte();
            if (index >= constants.size())
            {
                throw RuntimeError("constant index out of range");
            }
            return constants[index];
        };

        auto push = [&stack](Value value)
        {
            stack.push_back(std::move(value));
        };

        auto pop = [&stack]() -> Value
        {
            if (stack.empty())
            {
                throw RuntimeError("stack underflow");
            }
            auto value = std::move(stack.back());
            stack.pop_back();
            return value;
        };

        auto expect_number = [](const Value& value) -> double
        {
            if (const auto* number = std::get_if<double>(&value))
            {
                return *number;
            }
            throw RuntimeError("operand must be a number");
        };

        const auto op = static_cast<OpCode>(read_byte());

        switch (op)
        {
        case OpCode::Constant:
            push(read_constant());
            break;

        case OpCode::LoadLocal:
        {
            const auto slot = read_byte();
            if (slot >= frame.locals.size())
            {
                throw RuntimeError("local slot out of range");
            }
            push(frame.locals[slot]);
            break;
        }

        case OpCode::StoreLocal:
        {
            const auto slot = read_byte();
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
            push(-expect_number(pop()));
            break;

        case OpCode::Not:
            push(Value{!truthy(pop())});
            break;

        case OpCode::Add:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(a + b);
            break;
        }

        case OpCode::Subtract:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(a - b);
            break;
        }

        case OpCode::Multiply:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(a * b);
            break;
        }

        case OpCode::Divide:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
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
            push(Value{values_equal(a, b)});
            break;
        }

        case OpCode::NotEqual:
        {
            const auto b = pop();
            const auto a = pop();
            push(Value{!values_equal(a, b)});
            break;
        }

        case OpCode::Less:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(Value{a < b});
            break;
        }

        case OpCode::LessEqual:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(Value{a <= b});
            break;
        }

        case OpCode::Greater:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(Value{a > b});
            break;
        }

        case OpCode::GreaterEqual:
        {
            const auto b = expect_number(pop());
            const auto a = expect_number(pop());
            push(Value{a >= b});
            break;
        }

        case OpCode::Jump:
            frame.ip += read_i16();
            break;

        case OpCode::JumpIfFalse:
        {
            const auto offset = read_i16();
            if (!truthy(pop()))
            {
                frame.ip += offset;
            }
            break;
        }

        case OpCode::Call:
        {
            const auto index = read_byte();
            if (index >= functions_.size())
            {
                throw RuntimeError("function index out of range");
            }
            call(*functions_[index], stack);
            break;
        }

        case OpCode::Return:
        {
            Value result = pop();
            frames_.pop_back();
            if (frames_.empty())
            {
                last_value_ = std::move(result);
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
