#include <xenon/runtime/virtual_machine.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include <xenon/bytecode/opcode.hpp>
#include <xenon/runtime/error.hpp>
#include <xenon/runtime/virtual_heap.hpp>

namespace xenon
{
	bool truthy(const xenon::Value& value)
	{
		if (const auto* boolean = std::get_if<bool>(&value))
		{
			return *boolean;
		}
		if (const auto* number = std::get_if<double>(&value))
		{
			return *number != 0.0;
		}
		if (std::holds_alternative<xenon::Null>(value))
		{
			return false;
		}
		return true; // Ref
	}

	bool valuesEqual(const xenon::Value& a, const xenon::Value& b)
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
		if (const auto* ra = std::get_if<xenon::Ref>(&a))
		{
			if (const auto* rb = std::get_if<xenon::Ref>(&b))
			{
				return ra->id == rb->id;
			}
			return false;
		}
		return std::holds_alternative<xenon::Null>(a) && std::holds_alternative<xenon::Null>(b);
	}
}

void xenon::VirtualMachine::reset()
{
	_frames.clear();
	_lastValue = xenon::Null{};
}

void xenon::VirtualMachine::load(const xenon::Module& module)
{
	_symbols = module.Symbols;
	rebuildFunctionTable();
	rebuildGlobals();
}

void xenon::VirtualMachine::loadAll(const std::vector<xenon::Module>& modules)
{
	_symbols.clear();
	for (const auto& module : modules)
	{
		_symbols.insert(_symbols.end(), module.Symbols.begin(), module.Symbols.end());
	}
	rebuildFunctionTable();
	rebuildGlobals();
}

void xenon::VirtualMachine::rebuildGlobals()
{
	_globals.clear();
	for (const auto& symbol : _symbols)
	{
		if (symbol.Kind == xenon::SymbolKind::Variable)
		{
			_globals.push_back(symbol.InitialValue);
		}
	}
}

void xenon::VirtualMachine::rebuildFunctionTable()
{
	_functions.clear();
	for (const auto& symbol : _symbols)
	{
		if (symbol.Kind == xenon::SymbolKind::Function)
		{
			_functions.push_back(&symbol);
		}
	}
}

const xenon::Symbol* xenon::VirtualMachine::find(std::string_view name) const noexcept
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

void xenon::VirtualMachine::call(const xenon::Symbol& symbol, xenon::VirtualStack& stack)
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

void xenon::VirtualMachine::registerNative(std::string name, xenon::NativeFunction function)
{
	_natives[std::move(name)] = std::move(function);
}

void xenon::VirtualMachine::callExternal(const xenon::Symbol& symbol, xenon::VirtualStack& stack)
{
	std::vector<xenon::Value> args(symbol.Arity);
	for (std::size_t i = symbol.Arity; i-- > 0;)
	{
		args[i] = stack.pop();
	}

	const auto it = _natives.find(symbol.Name);
	if (it == _natives.end())
	{
		throw xenon::RuntimeError("external function not bound: " + symbol.Name);
	}
	stack.push(it->second(args));
}

xenon::InterpretResult xenon::VirtualMachine::run(std::string_view name, xenon::VirtualStack& stack)
{
	const xenon::Symbol* symbol = find(name);
	if (symbol == nullptr)
	{
		throw xenon::RuntimeError("unknown symbol: " + std::string(name));
	}
	if (symbol->Kind != xenon::SymbolKind::Function)
	{
		throw xenon::RuntimeError("symbol is not a function: " + std::string(name));
	}

	reset();

	if (!symbol->Defined)
	{
		callExternal(*symbol, stack);
		_lastValue = stack.pop();
		return xenon::InterpretResult::Ok;
	}

	call(*symbol, stack);
	execute(stack);
	return xenon::InterpretResult::Ok;
}

xenon::InterpretResult xenon::VirtualMachine::interpret(const xenon::XenonBinary& binary, xenon::VirtualStack& stack)
{
	reset();

	Frame frame;
	frame.binary = &binary;
	frame.ip     = 0;
	_frames.push_back(std::move(frame));

	execute(stack);
	return xenon::InterpretResult::Ok;
}

void xenon::VirtualMachine::execute(xenon::VirtualStack& stack)
{
	while (true)
	{
		Frame& frame              = _frames.back();
		const xenon::XenonBinary& binary = *frame.binary;
		const auto& code                 = binary.code();
		const auto& constants            = binary.constants();

		auto readByte = [&]() -> std::uint8_t
		{
			if (frame.ip >= code.size())
			{
				throw xenon::RuntimeError("unexpected end of bytecode");
			}
			return code[frame.ip++];
		};

		auto readI16 = [&]() -> std::int16_t
		{
			if (frame.ip + 2 > code.size())
			{
				throw xenon::RuntimeError("unexpected end of bytecode");
			}
			const auto value = static_cast<std::int16_t>(code[frame.ip] | (static_cast<std::uint16_t>(code[frame.ip + 1]) << 8));
			frame.ip += 2;
			return value;
		};

		auto readConstant = [&]() -> const Value&
		{
			const auto index = readByte();
			if (index >= constants.size())
			{
				throw xenon::RuntimeError("constant index out of range");
			}
			return constants[index];
		};

		auto push = [&stack](xenon::Value value)
		{
			stack.push(std::move(value));
		};

		auto pop = [&stack]() -> xenon::Value
		{
			return stack.pop();
		};

		auto expectNumber = [](const xenon::Value& value) -> double
		{
			if (const auto* number = std::get_if<double>(&value))
			{
				return *number;
			}
			throw xenon::RuntimeError("operand must be a number");
		};

		const auto op = static_cast<xenon::OpCode>(readByte());

		switch (op)
		{
		case xenon::OpCode::Constant:
			push(readConstant());
			break;

		case xenon::OpCode::String:
		{
			const auto index = readByte();
			const auto& strings = binary.strings();
			if (index >= strings.size())
			{
				throw xenon::RuntimeError("string index out of range");
			}
			if (_heap == nullptr)
			{
				throw xenon::RuntimeError("no heap available for string literal");
			}
			const xenon::Ref ref = _heap->allocate(xenon::ObjectKind::String);
			_heap->get(ref)->Text = strings[index];
			push(xenon::Value{ref});
			break;
		}

		case xenon::OpCode::New:
		{
			const auto index    = readByte();
			const auto& strings = binary.strings();
			if (index >= strings.size())
			{
				throw xenon::RuntimeError("type index out of range");
			}
			if (_heap == nullptr)
			{
				throw xenon::RuntimeError("no heap available");
			}
			const xenon::Symbol* classSymbol = find(strings[index]);
			if (classSymbol == nullptr || (classSymbol->Kind != xenon::SymbolKind::Struct && classSymbol->Kind != xenon::SymbolKind::Class))
			{
				throw xenon::RuntimeError("unknown type: " + strings[index]);
			}
			const xenon::Ref ref      = _heap->allocate(xenon::ObjectKind::Object, strings[index]);
			xenon::HeapObject* object = _heap->get(ref);
			for (const auto& field : classSymbol->Fields)
			{
				object->Fields[field] = xenon::Null{};
			}
			push(xenon::Value{ref});
			break;
		}

		case xenon::OpCode::GetField:
		{
			const auto index               = readByte();
			const xenon::Value objectValue = pop();
			const auto* ref                = std::get_if<xenon::Ref>(&objectValue);
			if (ref == nullptr || _heap == nullptr)
			{
				throw xenon::RuntimeError("GetField on a non-object");
			}
			const xenon::HeapObject* object = _heap->get(*ref);
			if (object == nullptr || object->Kind != xenon::ObjectKind::Object)
			{
				throw xenon::RuntimeError("GetField on a non-object");
			}
			const xenon::Symbol* classSymbol = find(object->ClassName);
			if (classSymbol == nullptr || index >= classSymbol->Fields.size())
			{
				throw xenon::RuntimeError("field index out of range");
			}
			const auto field = object->Fields.find(classSymbol->Fields[index]);
			push(field != object->Fields.end() ? field->second : xenon::Value{Null{}});
			break;
		}

		case xenon::OpCode::SetField:
		{
			const auto         index       = readByte();
			const xenon::Value value       = pop();
			const xenon::Value objectValue = pop();
			const auto*        ref         = std::get_if<xenon::Ref>(&objectValue);
			if (ref == nullptr || _heap == nullptr)
			{
				throw xenon::RuntimeError("SetField on a non-object");
			}
			xenon::HeapObject* object = _heap->get(*ref);
			if (object == nullptr || object->Kind != xenon::ObjectKind::Object)
			{
				throw xenon::RuntimeError("SetField on a non-object");
			}
			const xenon::Symbol* classSymbol = find(object->ClassName);
			if (classSymbol == nullptr || index >= classSymbol->Fields.size())
			{
				throw xenon::RuntimeError("field index out of range");
			}
			object->Fields[classSymbol->Fields[index]] = value;
			push(value);
			break;
		}

		case xenon::OpCode::LoadGlobal:
		{
			const auto index = readByte();
			if (index >= _globals.size())
			{
				throw xenon::RuntimeError("global index out of range");
			}
			push(_globals[index]);
			break;
		}

		case xenon::OpCode::StoreGlobal:
		{
			const auto index = readByte();
			if (index >= _globals.size())
			{
				throw xenon::RuntimeError("global index out of range");
			}
			const xenon::Value value = pop();
			_globals[index] = value;
			push(value);
			break;
		}

		case xenon::OpCode::LoadLocal:
		{
			const auto slot = readByte();
			if (slot >= frame.locals.size())
			{
				throw xenon::RuntimeError("local slot out of range");
			}
			push(frame.locals[slot]);
			break;
		}

		case xenon::OpCode::StoreLocal:
		{
			const auto slot = readByte();
			if (slot >= frame.locals.size())
			{
				throw xenon::RuntimeError("local slot out of range");
			}
			frame.locals[slot] = pop();
			break;
		}

		case xenon::OpCode::Pop:
			pop();
			break;

		case xenon::OpCode::Negate:
			push(-expectNumber(pop()));
			break;

		case xenon::OpCode::Not:
			push(xenon::Value{!xenon::truthy(pop())});
			break;

		case xenon::OpCode::Add:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(a + b);
			break;
		}

		case xenon::OpCode::Subtract:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(a - b);
			break;
		}

		case xenon::OpCode::Multiply:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(a * b);
			break;
		}

		case xenon::OpCode::Divide:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			if (b == 0.0)
			{
				throw xenon::RuntimeError("division by zero");
			}
			push(a / b);
			break;
		}

		case xenon::OpCode::Equal:
		{
			const auto b = pop();
			const auto a = pop();
			push(xenon::Value{xenon::valuesEqual(a, b)});
			break;
		}

		case xenon::OpCode::NotEqual:
		{
			const auto b = pop();
			const auto a = pop();
			push(xenon::Value{!xenon::valuesEqual(a, b)});
			break;
		}

		case xenon::OpCode::Less:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(xenon::Value{a < b});
			break;
		}

		case xenon::OpCode::LessEqual:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(xenon::Value{a <= b});
			break;
		}

		case xenon::OpCode::Greater:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(xenon::Value{a > b});
			break;
		}

		case xenon::OpCode::GreaterEqual:
		{
			const auto b = expectNumber(pop());
			const auto a = expectNumber(pop());
			push(xenon::Value{a >= b});
			break;
		}

		case xenon::OpCode::Jump:
			frame.ip += readI16();
			break;

		case xenon::OpCode::JumpIfFalse:
		{
			const auto offset = readI16();
			if (!xenon::truthy(pop()))
			{
				frame.ip += offset;
			}
			break;
		}

		case xenon::OpCode::Call:
		{
			const auto index = readByte();
			if (index >= _functions.size())
			{
				throw xenon::RuntimeError("function index out of range");
			}
			const xenon::Symbol* function = _functions[index];
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

		case xenon::OpCode::Return:
		{
			xenon::Value result = pop();
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
			throw xenon::RuntimeError("unknown opcode");
		}
	}
}
