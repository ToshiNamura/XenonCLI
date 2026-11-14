#include <xenon/runtime/virtual_machine.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include <xenon/runtime/error.hpp>
#include <xenon/runtime/virtual_heap.hpp>

namespace xenon
{
	namespace
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
		_modules.clear();
		_modules.push_back(module);
		rebuildTables();
	}

	void xenon::VirtualMachine::loadAll(const std::vector<xenon::Module>& modules)
	{
		_modules = modules;
		rebuildTables();
	}

	void xenon::VirtualMachine::rebuildTables()
	{
		_executables.clear();
		_types.clear();
		_globals.clear();

		const auto insertExecutable = [this](const std::string& name, const XenonBinary* body,
											 std::size_t arity, std::size_t locals, bool defined)
		{
			Executable executable;
			executable.Name    = name;
			executable.Body    = body;
			executable.Arity   = arity;
			executable.Locals  = locals;
			executable.Defined = defined;

			const auto [position, inserted] = _executables.emplace(name, std::move(executable));
			if (!inserted && !position->second.Defined && defined)
			{
				position->second = Executable{name, body, arity, locals, defined};
			}
		};

		for (const auto& module : _modules)
		{
			for (const auto& function : module.Functions)
			{
				insertExecutable(function.Name, &function.Body, function.Arity, function.Locals, function.Defined);
			}

			for (const auto& type : module.Types)
			{
				const auto [position, inserted] = _types.emplace(type.Name, &type);
				if (!inserted && !position->second->Defined && type.Defined)
				{
					position->second = &type;
				}

				for (const auto& method : type.Methods)
				{
					insertExecutable(type.Name + "::" + method.Name, &method.Body, method.Arity,
									 method.Locals, method.Defined);
				}
			}

			for (const auto& global : module.Globals)
			{
				_globals.emplace(global.Name, global.InitialValue);
			}
		}
	}

	const xenon::VirtualMachine::Executable* xenon::VirtualMachine::findExecutable(std::string_view name) const noexcept
	{
		const auto position = _executables.find(std::string(name));
		if (position != _executables.end())
		{
			return &position->second;
		}

		// Allow an unqualified method name (e.g. `length`) to resolve to `Type::length`.
		for (const auto& module : _modules)
		{
			for (const auto& type : module.Types)
			{
				for (const auto& method : type.Methods)
				{
					if (method.Name == name)
					{
						const auto qualified = _executables.find(type.Name + "::" + method.Name);
						if (qualified != _executables.end())
						{
							return &qualified->second;
						}
					}
				}
			}
		}
		return nullptr;
	}

	const xenon::Function* xenon::VirtualMachine::findFunction(std::string_view name) const noexcept
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

	const xenon::Method* xenon::VirtualMachine::findMethod(std::string_view name) const noexcept
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

	const xenon::Type* xenon::VirtualMachine::findType(std::string_view name) const noexcept
	{
		const auto position = _types.find(std::string(name));
		return position != _types.end() ? position->second : nullptr;
	}

	bool xenon::VirtualMachine::hasFunction(std::string_view name) const noexcept
	{
		return findExecutable(name) != nullptr;
	}

	void xenon::VirtualMachine::call(const xenon::VirtualMachine::Executable& executable, xenon::VirtualStack& stack)
	{
		Frame frame;
		frame.binary = executable.Body;
		frame.ip     = 0;
		frame.locals.resize(executable.Arity + executable.Locals);
		for (std::size_t i = executable.Arity; i-- > 0;)
		{
			frame.locals[i] = stack.pop();
		}
		_frames.push_back(std::move(frame));
	}

	void xenon::VirtualMachine::registerNative(std::string name, xenon::NativeFunction function)
	{
		_natives[std::move(name)] = std::move(function);
	}

	void xenon::VirtualMachine::callExternal(const xenon::VirtualMachine::Executable& executable,
											 xenon::VirtualStack& stack)
	{
		std::vector<xenon::Value> args(executable.Arity);
		for (std::size_t i = executable.Arity; i-- > 0;)
		{
			args[i] = stack.pop();
		}

		const auto position = _natives.find(executable.Name);
		if (position == _natives.end())
		{
			throw xenon::RuntimeError("external function not bound: " + executable.Name);
		}
		stack.push(position->second(args));
	}

	xenon::InterpretResult xenon::VirtualMachine::run(std::string_view name, xenon::VirtualStack& stack)
	{
		const xenon::VirtualMachine::Executable* executable = findExecutable(name);
		if (executable == nullptr)
		{
			throw xenon::RuntimeError("unknown symbol: " + std::string(name));
		}

		reset();

		if (!executable->Defined)
		{
			callExternal(*executable, stack);
			_lastValue = stack.pop();
			return xenon::InterpretResult::Ok;
		}

		call(*executable, stack);
		execute(stack);
		return xenon::InterpretResult::Ok;
	}

	xenon::InterpretResult xenon::VirtualMachine::interpret(const xenon::XenonBinary& binary,
															xenon::VirtualStack& stack)
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
			Frame& frame                    = _frames.back();
			const xenon::XenonBinary& binary = *frame.binary;
			const auto& code                 = binary.code();
			const auto& constants            = binary.constants();
			const auto& strings              = binary.strings();

			auto readByte = [&]() -> std::uint8_t
			{
				if (frame.ip >= code.size())
				{
					throw xenon::RuntimeError("unexpected end of bytecode");
				}
				return code[frame.ip++];
			};

			auto readU16 = [&]() -> std::uint16_t
			{
				if (frame.ip + 2 > code.size())
				{
					throw xenon::RuntimeError("unexpected end of bytecode");
				}
				const auto value = static_cast<std::uint16_t>(
					code[frame.ip] | (static_cast<std::uint16_t>(code[frame.ip + 1]) << 8));
				frame.ip += 2;
				return value;
			};

			auto readI16 = [&]() -> std::int16_t
			{
				return static_cast<std::int16_t>(readU16());
			};

			auto readU32 = [&]() -> std::uint32_t
			{
				if (frame.ip + 4 > code.size())
				{
					throw xenon::RuntimeError("unexpected end of bytecode");
				}
				const auto value = static_cast<std::uint32_t>(code[frame.ip]) |
								   (static_cast<std::uint32_t>(code[frame.ip + 1]) << 8) |
								   (static_cast<std::uint32_t>(code[frame.ip + 2]) << 16) |
								   (static_cast<std::uint32_t>(code[frame.ip + 3]) << 24);
				frame.ip += 4;
				return value;
			};

			auto readConstant = [&]() -> const Value&
			{
				const auto index = readU16();
				if (index >= constants.size())
				{
					throw xenon::RuntimeError("constant index out of range");
				}
				return constants[index];
			};

			auto readString = [&]() -> const std::string&
			{
				const auto index = readU16();
				if (index >= strings.size())
				{
					throw xenon::RuntimeError("string index out of range");
				}
				return strings[index];
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

			const auto opcode = static_cast<xenon::Instruction>(readByte());

			switch (opcode)
			{
			case xenon::Instruction::Const:
				push(readConstant());
				break;

			case xenon::Instruction::Str:
			{
				const std::string& text = readString();
				if (_heap == nullptr)
				{
					throw xenon::RuntimeError("no heap available for string literal");
				}
				const xenon::Ref ref = _heap->allocate(xenon::ObjectKind::String);
				_heap->get(ref)->Text = text;
				push(xenon::Value{ref});
				break;
			}

			case xenon::Instruction::New:
			{
				const std::string& typeName = readString();
				if (_heap == nullptr)
				{
					throw xenon::RuntimeError("no heap available");
				}
				const xenon::Type* type = findType(typeName);
				if (type == nullptr || type->Kind == xenon::TypeKind::Enum)
				{
					throw xenon::RuntimeError("unknown type: " + typeName);
				}
				const xenon::Ref ref      = _heap->allocate(xenon::ObjectKind::Object, typeName);
				xenon::HeapObject* object = _heap->get(ref);
				for (const auto& field : type->Fields)
				{
					object->Fields[field.Name] = xenon::Null{};
				}
				push(xenon::Value{ref});
				break;
			}

			case xenon::Instruction::Getf:
			{
				const auto         index       = readU16();
				const xenon::Value objectValue = pop();
				const auto*        ref         = std::get_if<xenon::Ref>(&objectValue);
				if (ref == nullptr || _heap == nullptr)
				{
					throw xenon::RuntimeError("GetField on a non-object");
				}
				const xenon::HeapObject* object = _heap->get(*ref);
				if (object == nullptr || object->Kind != xenon::ObjectKind::Object)
				{
					throw xenon::RuntimeError("GetField on a non-object");
				}
				const xenon::Type* type = findType(object->ClassName);
				if (type == nullptr || index >= type->Fields.size())
				{
					throw xenon::RuntimeError("field index out of range");
				}
				const auto field = object->Fields.find(type->Fields[index].Name);
				push(field != object->Fields.end() ? field->second : xenon::Value{Null{}});
				break;
			}

			case xenon::Instruction::Setf:
			{
				const auto         index       = readU16();
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
				const xenon::Type* type = findType(object->ClassName);
				if (type == nullptr || index >= type->Fields.size())
				{
					throw xenon::RuntimeError("field index out of range");
				}
				object->Fields[type->Fields[index].Name] = value;
				push(value);
				break;
			}

			case xenon::Instruction::Ldg:
			{
				const std::string& name = readString();
				const auto position     = _globals.find(name);
				if (position == _globals.end())
				{
					throw xenon::RuntimeError("unknown global: " + name);
				}
				push(position->second);
				break;
			}

			case xenon::Instruction::Stg:
			{
				const std::string& name = readString();
				const auto position     = _globals.find(name);
				if (position == _globals.end())
				{
					throw xenon::RuntimeError("unknown global: " + name);
				}
				const xenon::Value value = pop();
				position->second         = value;
				push(value);
				break;
			}

			case xenon::Instruction::Ldl:
			{
				const auto slot = readU16();
				if (slot >= frame.locals.size())
				{
					throw xenon::RuntimeError("local slot out of range");
				}
				push(frame.locals[slot]);
				break;
			}

			case xenon::Instruction::Stl:
			{
				const auto slot = readU16();
				if (slot >= frame.locals.size())
				{
					throw xenon::RuntimeError("local slot out of range");
				}
				frame.locals[slot] = pop();
				break;
			}

			case xenon::Instruction::Pop:
				pop();
				break;

			case xenon::Instruction::Neg:
				push(-expectNumber(pop()));
				break;

			case xenon::Instruction::Not:
				push(xenon::Value{!truthy(pop())});
				break;

			case xenon::Instruction::Add:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(a + b);
				break;
			}

			case xenon::Instruction::Sub:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(a - b);
				break;
			}

			case xenon::Instruction::Mul:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(a * b);
				break;
			}

			case xenon::Instruction::Div:
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

			case xenon::Instruction::Equ:
			{
				const auto b = pop();
				const auto a = pop();
				push(xenon::Value{valuesEqual(a, b)});
				break;
			}

			case xenon::Instruction::Neq:
			{
				const auto b = pop();
				const auto a = pop();
				push(xenon::Value{!valuesEqual(a, b)});
				break;
			}

			case xenon::Instruction::Lss:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(xenon::Value{a < b});
				break;
			}

			case xenon::Instruction::Leq:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(xenon::Value{a <= b});
				break;
			}

			case xenon::Instruction::Gtr:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(xenon::Value{a > b});
				break;
			}

			case xenon::Instruction::Geq:
			{
				const auto b = expectNumber(pop());
				const auto a = expectNumber(pop());
				push(xenon::Value{a >= b});
				break;
			}

			case xenon::Instruction::Jmp:
				frame.ip += readI16();
				break;

			case xenon::Instruction::Jif:
			{
				const auto offset = readI16();
				if (!truthy(pop()))
				{
					frame.ip += offset;
				}
				break;
			}

			case xenon::Instruction::Jmpa:
				frame.ip = readU32();
				break;

			case xenon::Instruction::Jifa:
			{
				const auto address = readU32();
				if (!truthy(pop()))
				{
					frame.ip = address;
				}
				break;
			}

			case xenon::Instruction::Call:
			{
				const std::string& name = readString();
				const xenon::VirtualMachine::Executable* executable = findExecutable(name);
				if (executable == nullptr)
				{
					throw xenon::RuntimeError("unknown function: " + name);
				}
				if (!executable->Defined)
				{
					callExternal(*executable, stack);
				}
				else
				{
					call(*executable, stack);
				}
				break;
			}

			case xenon::Instruction::Ret:
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
}
