#include <xenon/compiler/codegen.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace xenon
{
	namespace
	{
		/// Splits `text` on `sep`, ignoring separators nested inside `<...>`.
		std::vector<std::string> splitTopLevel(const std::string& text, char sep)
		{
			std::vector<std::string> parts;
			std::size_t              depth = 0;
			std::size_t              start = 0;
			for (std::size_t i = 0; i < text.size(); ++i)
			{
				if (text[i] == '<')
				{
					++depth;
				}
				else if (text[i] == '>')
				{
					--depth;
				}
				else if (text[i] == sep && depth == 0)
				{
					parts.push_back(text.substr(start, i - start));
					start = i + 1;
				}
			}
			parts.push_back(text.substr(start));
			return parts;
		}

		/// Builds a type substitution map for an instantiation `base<args...>`.
		std::unordered_map<std::string, std::string> buildSubstitutions(const std::vector<std::string>& params,
																	   const std::string& fullName,
																	   const std::string& base,
																	   const std::string& selfType)
		{
			std::unordered_map<std::string, std::string> map;
			const auto less    = fullName.find('<');
			const auto greater = fullName.rfind('>');

			if (less == std::string::npos || greater == std::string::npos || greater <= less)
			{
				return map;
			}

			const std::string inner = fullName.substr(less + 1, greater - less - 1);
			const auto        args  = splitTopLevel(inner, ',');
			for (std::size_t i = 0; i < params.size() && i < args.size(); ++i)
			{
				map[params[i]] = args[i];
			}

			if (!selfType.empty())
			{
				map[base] = selfType;
			}

			return map;
		}

		TypeKind typeKindOf(DeclKind kind) noexcept
		{
			switch (kind)
			{
				case DeclKind::Struct: return TypeKind::Struct;
				case DeclKind::Class:  return TypeKind::Class;
				case DeclKind::Enum:   return TypeKind::Enum;
			}
			return TypeKind::Class;
		}
	}

	xenon::CodeGenerator::CodeGenerator(const Program& program, const Module& hints, bool hintMode) :
		_program(program),
		_hints(hints),
		_hintMode(hintMode)
	{
		for (const auto& function : _program.Functions)
		{
			_functions.insert(function.Name);
		}

		for (const auto& decl : _program.TypeDecls)
		{
			std::vector<std::string> fieldNames;
			std::vector<std::string> fieldTypes;
			std::unordered_set<std::string> constFields;
			for (const auto& field : decl.Fields)
			{
				fieldNames.push_back(field.Name);
				fieldTypes.push_back(field.Type);
				if (field.Constant)
				{
					constFields.insert(field.Name);
				}
			}
			_typeFields[decl.Name]     = std::move(fieldNames);
			_typeFieldTypes[decl.Name] = std::move(fieldTypes);
			if (!constFields.empty())
			{
				_constFields[decl.Name] = std::move(constFields);
			}

			for (const auto& method : decl.Methods)
			{
				const std::string qualified = decl.Name + "::" + method.Name;
				_methods.insert(qualified);
				registerSignature(qualified, method);
			}
		}

		for (const auto& function : _program.Functions)
		{
			registerSignature(function.Name, function);
		}

		for (const auto& global : _program.Globals)
		{
			_globalTypes[global.Name] = global.Type;
			if (global.Constant)
			{
				_constGlobals.insert(global.Name);
			}
		}

		for (const auto& function : _hints.Functions)
		{
			_functions.insert(function.Name);
			registerSignature(function.Name, function.Params, function.Signature);
		}

		for (const auto& type : _hints.Types)
		{
			std::vector<std::string> fieldNames;
			std::vector<std::string> fieldTypes;
			std::unordered_set<std::string> constFields;
			for (const auto& field : type.Fields)
			{
				fieldNames.push_back(field.Name);
				fieldTypes.push_back(field.Type);
				if (field.Constant)
				{
					constFields.insert(field.Name);
				}
			}
			_typeFields[type.Name]     = std::move(fieldNames);
			_typeFieldTypes[type.Name] = std::move(fieldTypes);
			if (!constFields.empty())
			{
				_constFields[type.Name] = std::move(constFields);
			}

			for (const auto& method : type.Methods)
			{
				const std::string qualified = type.Name + "::" + method.Name;
				_methods.insert(qualified);
				registerSignature(qualified, method.Params, method.Signature);
			}
		}

		for (const auto& templ : _program.Templates)
		{
			if (templ.IsFunction)
			{
				_functionTemplates[templ.Function.Name] = FunctionTemplate{templ.Params, &templ.Function};
			}
			else
			{
				_typeTemplates[templ.Type.Name] = TypeTemplate{templ.Params, &templ.Type};
			}
		}

		for (const auto& global : _program.Globals)
		{
			_globals.insert(global.Name);
		}
		for (const auto& global : _hints.Globals)
		{
			_globals.insert(global.Name);
			_globalTypes[global.Name] = global.Type;
			if (global.Constant)
			{
				_constGlobals.insert(global.Name);
			}
		}
	}

	void xenon::CodeGenerator::registerSignature(const std::string& name, const std::vector<std::string>& params,
												 const std::string& signature)
	{
		_functionParams[name] = params;

		std::string returnType = "void";
		const auto  space      = signature.find(' ');
		if (space != std::string::npos && signature.compare(0, space, "void") != 0)
		{
			returnType = signature.substr(0, space);
		}
		_functionReturns[name] = std::move(returnType);
	}

	void xenon::CodeGenerator::registerSignature(const std::string& name, const xenon::FunctionDecl& function)
	{
		std::vector<std::string> params;
		params.reserve(function.Params.size());
		for (const auto& param : function.Params)
		{
			params.push_back(param.first);
		}
		_functionParams[name] = std::move(params);
		_functionReturns[name] = function.ReturnType.empty() ? "void" : function.ReturnType;
	}

	[[noreturn]] void xenon::CodeGenerator::fail(const std::string& message)
	{
		_error = message;
		throw CodeGenError{};
	}

	[[noreturn]] void xenon::CodeGenerator::fail(const std::string& message, int line)
	{
		_error = line > 0 ? message + " (line " + std::to_string(line) + ")" : message;
		throw CodeGenError{};
	}

	std::string xenon::CodeGenerator::baseType(const std::string& name)
	{
		const auto less = name.find('<');
		return less == std::string::npos ? name : name.substr(0, less);
	}

	bool xenon::CodeGenerator::isAssignable(const std::string& target, const std::string& value)
	{
		if (target.empty() || value.empty() || target == "any" || value == "any")
		{
			return true; // type could not be inferred
		}
		if (target == value || baseType(target) == baseType(value))
		{
			return true;
		}

		const auto numeric = [](const std::string& type)
		{
			return type == "double" || type == "int" || type == "float" || type == "long";
		};

		if (numeric(target) && numeric(value))
		{
			return true;
		}
		if (value == "null")
		{
			return !numeric(target) && target != "bool";
		}
		return false;
	}

	bool xenon::CodeGenerator::generate(xenon::Module& out)
	{
		_error.clear();
		out.Functions.clear();
		out.Types.clear();
		out.Globals.clear();

		try
		{
			for (const auto& global : _program.Globals)
			{
				compileGlobalVar(global, out);
			}

			for (const auto& function : _program.Functions)
			{
				compileFunction(function, out);
			}

			for (const auto& decl : _program.TypeDecls)
			{
				compileTypeDecl(decl, out);
			}

			for (const auto& function : _hints.Functions)
			{
				if (out.findFunction(function.Name) == nullptr)
				{
					out.Functions.push_back(function);
				}
			}

			for (const auto& type : _hints.Types)
			{
				if (out.findType(type.Name) == nullptr)
				{
					out.Types.push_back(type);
				}
			}

			for (const auto& global : _hints.Globals)
			{
				if (out.findGlobal(global.Name) == nullptr)
				{
					out.Globals.push_back(global);
				}
			}

			while (!_pendingFunctions.empty() || !_pendingTypes.empty())
			{
				auto functions = std::move(_pendingFunctions);
				_pendingFunctions.clear();
				for (const auto& pending : functions)
				{
					_substitutions = pending.Substitutions;
					compileFunction(*pending.Decl, out, pending.Name);
				}
				_substitutions.clear();

				auto types = std::move(_pendingTypes);
				_pendingTypes.clear();
				for (const auto& pending : types)
				{
					_substitutions = pending.Substitutions;
					emitType(*pending.Decl, pending.Name, out);
					_substitutions.clear();
				}
			}

			return true;
		}
		catch (const CodeGenError&)
		{
			return false;
		}
	}

	std::string xenon::CodeGenerator::buildSignature(const std::string& returnType, const std::string& name,
													 const std::vector<std::pair<std::string, std::string>>& params)
	{
		std::string signature = returnType.empty() ? "void " : returnType + " ";
		signature += name;
		signature += "(";
		for (std::size_t i = 0; i < params.size(); ++i)
		{
			if (i != 0)
			{
				signature += ", ";
			}
			signature += params[i].first;
		}
		signature += ")";
		return signature;
	}

	void xenon::CodeGenerator::compileFunction(const xenon::FunctionDecl& function, xenon::Module& out,
											   const std::string& qualifiedName)
	{
		const std::string& name = qualifiedName.empty() ? function.Name : qualifiedName;

		xenon::Function compiled;
		compiled.Name      = name;
		compiled.Signature = buildSignature(function.ReturnType, name, function.Params);
		compiled.Exported  = function.Exported;
		compiled.Arity     = function.Params.size();
		compiled.Defined   = function.HasBody;
		for (const auto& param : function.Params)
		{
			compiled.Params.push_back(substituteType(param.first));
		}
		registerSignature(name, compiled.Params, compiled.Signature);

		if (!function.HasBody)
		{
			out.Functions.push_back(std::move(compiled));
			return;
		}

		_binary      = &compiled.Body;
		_scopes.clear();
		_slotCounter = 0;
		_maxSlots    = 0;

		enterScope();
		for (std::size_t i = 0; i < function.Params.size(); ++i)
		{
			const bool constant = i < function.ParamConstants.size() && function.ParamConstants[i];
			declareLocal(function.Params[i].second, function.Params[i].first, constant);
		}

		_currentReturnType   = function.ReturnType.empty() ? "void" : function.ReturnType;
		_currentFunctionName = name;
		compileBlock(*function.Body);
		_currentReturnType.clear();
		_currentFunctionName.clear();

		emitConstant(Null{});
		emitOpcode(Instruction::Ret);

		compiled.Locals = _maxSlots > compiled.Arity ? _maxSlots - compiled.Arity : 0;
		out.Functions.push_back(std::move(compiled));
		_binary = nullptr;
	}

	void xenon::CodeGenerator::compileMethod(const xenon::FunctionDecl& method, xenon::Type& type)
	{
		xenon::Method compiled;
		compiled.Name           = method.Name;
		compiled.Signature      = buildSignature(method.ReturnType, method.Name, method.Params);
		compiled.AccessModifier = Access::Public;
		compiled.Arity          = method.Params.size();
		compiled.Defined        = method.HasBody;
		for (const auto& param : method.Params)
		{
			compiled.Params.push_back(substituteType(param.first));
		}
		registerSignature(type.Name + "::" + method.Name, compiled.Params, compiled.Signature);

		if (!method.HasBody)
		{
			type.Methods.push_back(std::move(compiled));
			return;
		}

		_binary      = &compiled.Body;
		_scopes.clear();
		_slotCounter = 0;
		_maxSlots    = 0;

		enterScope();
		for (std::size_t i = 0; i < method.Params.size(); ++i)
		{
			const bool constant = i < method.ParamConstants.size() && method.ParamConstants[i];
			declareLocal(method.Params[i].second, method.Params[i].first, constant);
		}

		_currentReturnType   = method.ReturnType.empty() ? "void" : method.ReturnType;
		_currentFunctionName = type.Name + "::" + method.Name;
		compileBlock(*method.Body);
		_currentReturnType.clear();
		_currentFunctionName.clear();

		emitConstant(Null{});
		emitOpcode(Instruction::Ret);

		compiled.Locals = _maxSlots > compiled.Arity ? _maxSlots - compiled.Arity : 0;
		type.Methods.push_back(std::move(compiled));
		_binary = nullptr;
	}

	void xenon::CodeGenerator::compileTypeDecl(const xenon::TypeDecl& decl, xenon::Module& out)
	{
		xenon::Type type;
		type.Name    = decl.Name;
		type.Kind    = typeKindOf(decl.Kind);
		type.Defined = !_hintMode;

		if (decl.Kind == xenon::DeclKind::Enum)
		{
			type.Enumerators = decl.Enumerators;
		}
		else
		{
			for (const auto& field : decl.Fields)
			{
				xenon::Field compiled;
				compiled.Type           = field.Type;
				compiled.Name           = field.Name;
				compiled.AccessModifier = decl.Kind == xenon::DeclKind::Struct ? Access::Public : Access::Private;
				compiled.Constant       = field.Constant;
				type.Fields.push_back(std::move(compiled));
			}

			for (const auto& method : decl.Methods)
			{
				compileMethod(method, type);
			}
		}

		out.Types.push_back(std::move(type));
	}

	void xenon::CodeGenerator::emitType(const xenon::TypeDecl& decl, const std::string& name, xenon::Module& out)
	{
		xenon::Type type;
		type.Name    = name;
		type.Kind    = typeKindOf(decl.Kind);
		type.Defined = !_hintMode;

		if (decl.Kind == xenon::DeclKind::Enum)
		{
			type.Enumerators = decl.Enumerators;
		}
		else
		{
			for (const auto& field : decl.Fields)
			{
				xenon::Field compiled;
				compiled.Type           = substituteType(field.Type);
				compiled.Name           = field.Name;
				compiled.AccessModifier = decl.Kind == xenon::DeclKind::Struct ? Access::Public : Access::Private;
				compiled.Constant       = field.Constant;
				type.Fields.push_back(std::move(compiled));
			}

			for (const auto& method : decl.Methods)
			{
				compileMethod(method, type);
			}
		}

		out.Types.push_back(std::move(type));
	}

	void xenon::CodeGenerator::compileGlobalVar(const xenon::GlobalVarDecl& global, xenon::Module& out)
	{
		xenon::Global compiled;
		compiled.Name         = global.Name;
		compiled.Type         = global.Type;
		compiled.Constant     = global.Constant;
		compiled.InitialValue = evaluateConstant(global.Init.get());
		out.Globals.push_back(std::move(compiled));
	}

	xenon::Value xenon::CodeGenerator::evaluateConstant(const xenon::Expr* init)
	{
		if (init == nullptr)
		{
			return xenon::Null{};
		}

		if (const auto* number = dynamic_cast<const xenon::NumberExpr*>(init))
		{
			return xenon::Value{number->Value};
		}

		if (const auto* boolean = dynamic_cast<const xenon::BoolExpr*>(init))
		{
			return xenon::Value{boolean->Value};
		}

		if (dynamic_cast<const xenon::NullExpr*>(init) != nullptr)
		{
			return xenon::Null{};
		}

		if (dynamic_cast<const xenon::StringExpr*>(init) != nullptr)
		{
			fail("global string variables are not supported yet");
		}

		if (const auto* unary = dynamic_cast<const xenon::UnaryExpr*>(init))
		{
			if (unary->Operation == xenon::UnaryExpr::Op::Negate)
			{
				const xenon::Value operand = evaluateConstant(unary->Operand.get());
				if (const auto* number = std::get_if<double>(&operand))
				{
					return xenon::Value{-*number};
				}
			}
		}

		if (const auto* binary = dynamic_cast<const xenon::BinaryExpr*>(init))
		{
			const xenon::Value left  = evaluateConstant(binary->Left.get());
			const xenon::Value right = evaluateConstant(binary->Right.get());
			const auto*        a     = std::get_if<double>(&left);
			const auto*        b     = std::get_if<double>(&right);
			if (a == nullptr || b == nullptr)
			{
				fail("global initializer must be a constant expression");
			}

			switch (binary->Operation)
			{
				case xenon::BinaryExpr::Op::Add:      return xenon::Value{*a + *b};
				case xenon::BinaryExpr::Op::Subtract: return xenon::Value{*a - *b};
				case xenon::BinaryExpr::Op::Multiply: return xenon::Value{*a * *b};
				case xenon::BinaryExpr::Op::Divide:
					if (*b == 0.0)
					{
						fail("division by zero in a constant expression");
					}
					return xenon::Value{*a / *b};
				case xenon::BinaryExpr::Op::Modulo:
					if (*b == 0.0)
					{
						fail("division by zero in a constant expression");
					}
					return xenon::Value{std::fmod(*a, *b)};
				case xenon::BinaryExpr::Op::BitAnd:
					return xenon::Value{static_cast<double>(static_cast<std::int64_t>(*a) & static_cast<std::int64_t>(*b))};
				case xenon::BinaryExpr::Op::BitOr:
					return xenon::Value{static_cast<double>(static_cast<std::int64_t>(*a) | static_cast<std::int64_t>(*b))};
				case xenon::BinaryExpr::Op::BitXor:
					return xenon::Value{static_cast<double>(static_cast<std::int64_t>(*a) ^ static_cast<std::int64_t>(*b))};
				case xenon::BinaryExpr::Op::ShiftLeft:
					return xenon::Value{static_cast<double>(static_cast<std::int64_t>(
						static_cast<std::uint64_t>(static_cast<std::int64_t>(*a))
						<< static_cast<std::uint64_t>(static_cast<std::int64_t>(*b) & 63)))};
				case xenon::BinaryExpr::Op::ShiftRight:
					return xenon::Value{static_cast<double>(static_cast<std::int64_t>(*a) >>
														   (static_cast<std::int64_t>(*b) & 63))};
				default:
					fail("global initializer must be a constant expression");
			}
		}

		fail("global initializer must be a constant expression");
	}

	void xenon::CodeGenerator::enterScope()
	{
		_scopes.push_back(Scope{{}, {}, {}, _slotCounter});
	}

	void xenon::CodeGenerator::leaveScope()
	{
		_slotCounter = _scopes.back().BaseSlot;
		_scopes.pop_back();
	}

	void xenon::CodeGenerator::declareLocal(const std::string& name, const std::string& type, bool constant)
	{
		_scopes.back().Names[name] = _slotCounter;
		if (!type.empty())
		{
			_scopes.back().Types[name] = substituteType(type);
		}
		if (constant)
		{
			_scopes.back().Constants.insert(name);
		}
		++_slotCounter;
		_maxSlots = std::max(_maxSlots, _slotCounter);
	}

	void xenon::CodeGenerator::checkConstWrite(const std::string& name, int line)
	{
		for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
		{
			if (it->Names.find(name) != it->Names.end())
			{
				if (it->Constants.contains(name))
				{
					fail("cannot assign to the 'const' variable '" + name + "'", line);
				}
				return;
			}
		}
		if (_constGlobals.contains(name))
		{
			fail("cannot assign to the 'const' global '" + name + "'", line);
		}
	}

	void xenon::CodeGenerator::checkFieldConstant(const std::string& typeName, const std::string& fieldName, int line)
	{
		const auto isConstant = [this, &fieldName](const std::string& type)
		{
			const auto found = _constFields.find(type);
			return found != _constFields.end() && found->second.contains(fieldName);
		};

		if (isConstant(typeName) || isConstant(baseType(typeName)))
		{
			fail("cannot assign to the 'const' field '" + typeName + "." + fieldName + "'", line);
		}
	}

	std::string xenon::CodeGenerator::typeOf(const xenon::Expr& expr)
	{
		if (dynamic_cast<const xenon::NumberExpr*>(&expr) != nullptr)
		{
			return "double";
		}
		if (dynamic_cast<const xenon::BoolExpr*>(&expr) != nullptr)
		{
			return "bool";
		}
		if (dynamic_cast<const xenon::StringExpr*>(&expr) != nullptr)
		{
			return "string";
		}
		if (dynamic_cast<const xenon::NullExpr*>(&expr) != nullptr)
		{
			return "null";
		}
		if (const auto* created = dynamic_cast<const xenon::NewExpr*>(&expr))
		{
			return substituteType(created->TypeName);
		}
		if (const auto* variable = dynamic_cast<const xenon::VariableExpr*>(&expr))
		{
			for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
			{
				const auto found = it->Types.find(variable->Name);
				if (found != it->Types.end())
				{
					return found->second;
				}
			}
			const auto global = _globalTypes.find(variable->Name);
			return global != _globalTypes.end() ? global->second : std::string{};
		}
		if (const auto* field = dynamic_cast<const xenon::FieldAccessExpr*>(&expr))
		{
			const std::string owner = typeOf(*field->Object);
			if (owner.empty())
			{
				return {};
			}
			ensureTypeInstantiated(owner);
			const auto fields = _typeFields.find(owner);
			const auto types  = _typeFieldTypes.find(owner);
			if (fields == _typeFields.end() || types == _typeFieldTypes.end())
			{
				return {};
			}
			for (std::size_t i = 0; i < fields->second.size() && i < types->second.size(); ++i)
			{
				if (fields->second[i] == field->FieldName)
				{
					return types->second[i];
				}
			}
			return {};
		}
		if (const auto* fieldAssign = dynamic_cast<const xenon::FieldAssignExpr*>(&expr))
		{
			return typeOf(*fieldAssign->Value);
		}
		if (const auto* assign = dynamic_cast<const xenon::AssignExpr*>(&expr))
		{
			return typeOf(*assign->Value);
		}
		if (const auto* call = dynamic_cast<const xenon::CallExpr*>(&expr))
		{
			const auto found = _functionReturns.find(call->Callee);
			if (found != _functionReturns.end())
			{
				return substituteType(found->second);
			}
			const auto separator = call->Callee.rfind("::");
			if (separator != std::string::npos)
			{
				const auto method = _functionReturns.find(call->Callee);
				if (method != _functionReturns.end())
				{
					return substituteType(method->second);
				}
			}
			return {};
		}
		if (const auto* unary = dynamic_cast<const xenon::UnaryExpr*>(&expr))
		{
			return unary->Operation == xenon::UnaryExpr::Op::Not ? "bool" : "double";
		}
		if (dynamic_cast<const xenon::PostfixExpr*>(&expr) != nullptr)
		{
			return "double";
		}
		if (const auto* binary = dynamic_cast<const xenon::BinaryExpr*>(&expr))
		{
			switch (binary->Operation)
			{
				case xenon::BinaryExpr::Op::Equal:
				case xenon::BinaryExpr::Op::NotEqual:
				case xenon::BinaryExpr::Op::Less:
				case xenon::BinaryExpr::Op::LessEqual:
				case xenon::BinaryExpr::Op::Greater:
				case xenon::BinaryExpr::Op::GreaterEqual:
				case xenon::BinaryExpr::Op::And:
				case xenon::BinaryExpr::Op::Or:
					return "bool";
				default:
					return "double";
			}
		}
		if (const auto* ternary = dynamic_cast<const xenon::TernaryExpr*>(&expr))
		{
			const std::string thenType = typeOf(*ternary->Then);
			const std::string elseType = typeOf(*ternary->Else);
			if (thenType == elseType)
			{
				return thenType;
			}
			if (thenType == "null")
			{
				return elseType;
			}
			if (elseType == "null")
			{
				return thenType;
			}
			return {};
		}
		return {};
	}

	std::size_t xenon::CodeGenerator::allocateTemp()
	{
		const auto slot = _slotCounter++;
		_maxSlots = std::max(_maxSlots, _slotCounter);
		return slot;
	}

	std::size_t xenon::CodeGenerator::resolveLocal(const std::string& name)
	{
		for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
		{
			const auto found = it->Names.find(name);
			if (found != it->Names.end())
			{
				return found->second;
			}
		}
		fail("undefined variable: " + name);
	}

	bool xenon::CodeGenerator::hasLocal(const std::string& name) const
	{
		for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
		{
			if (it->Names.find(name) != it->Names.end())
			{
				return true;
			}
		}
		return false;
	}

	void xenon::CodeGenerator::resolveGlobal(const std::string& name)
	{
		if (!_globals.contains(name))
		{
			fail("undefined variable: " + name);
		}
	}

	std::string xenon::CodeGenerator::globalType(const std::string& name) const
	{
		const auto found = _globalTypes.find(name);
		return found != _globalTypes.end() ? found->second : std::string{};
	}

	std::string xenon::CodeGenerator::fieldType(const std::string& typeName, const std::string& fieldName)
	{
		if (typeName.empty())
		{
			return {};
		}
		ensureTypeInstantiated(typeName);

		const auto fields = _typeFields.find(typeName);
		const auto types  = _typeFieldTypes.find(typeName);
		if (fields == _typeFields.end() || types == _typeFieldTypes.end())
		{
			return {};
		}
		for (std::size_t i = 0; i < fields->second.size() && i < types->second.size(); ++i)
		{
			if (fields->second[i] == fieldName)
			{
				return types->second[i];
			}
		}
		return {};
	}

	void xenon::CodeGenerator::checkCall(const xenon::CallExpr& call)
	{
		const auto params = _functionParams.find(call.Callee);
		if (params == _functionParams.end())
		{
			return; // declaration unknown here; resolveFunction reports unknown names
		}

		if (call.Args.size() != params->second.size())
		{
			fail("'" + call.Callee + "' expects " + std::to_string(params->second.size()) +
					 " argument(s), got " + std::to_string(call.Args.size()),
				 call.Line);
		}

		for (std::size_t i = 0; i < call.Args.size(); ++i)
		{
			const std::string expected = substituteType(params->second[i]);
			const std::string actual   = typeOf(*call.Args[i]);
			if (!isAssignable(expected, actual))
			{
				fail("argument " + std::to_string(i + 1) + " of '" + call.Callee + "' expects " +
						 expected + ", got " + actual,
					 call.Args[i]->Line);
			}
		}
	}

	std::string xenon::CodeGenerator::localType(const std::string& name)
	{
		for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
		{
			const auto found = it->Types.find(name);
			if (found != it->Types.end())
			{
				return found->second;
			}
		}
		fail("unknown variable type for: " + name);
	}

	std::string xenon::CodeGenerator::objectType(const xenon::Expr& object)
	{
		const std::string type = typeOf(object);
		if (type.empty())
		{
			fail("cannot determine the object type for field access");
		}
		return type;
	}

	std::size_t xenon::CodeGenerator::resolveField(const xenon::Expr& object, const std::string& fieldName)
	{
		const std::string typeName = objectType(object);

		ensureTypeInstantiated(typeName);

		const auto it = _typeFields.find(typeName);
		if (it == _typeFields.end())
		{
			fail("unknown type: " + typeName);
		}

		for (std::size_t i = 0; i < it->second.size(); ++i)
		{
			if (it->second[i] == fieldName)
			{
				return i;
			}
		}
		fail("no such field: " + typeName + "." + fieldName);
	}

	std::string xenon::CodeGenerator::substituteType(const std::string& name) const
	{
		const auto it = _substitutions.find(name);
		return it != _substitutions.end() ? it->second : name;
	}

	void xenon::CodeGenerator::resolveFunction(const std::string& callee)
	{
		if (_functions.contains(callee) || _methods.contains(callee))
		{
			return;
		}

		const auto separator = callee.rfind("::");
		if (separator != std::string::npos)
		{
			ensureTypeInstantiated(callee.substr(0, separator));
			if (_methods.contains(callee))
			{
				return;
			}
			fail("unknown function: " + callee);
		}

		if (ensureFunctionInstantiated(callee))
		{
			return;
		}

		fail("unknown function: " + callee);
	}

	bool xenon::CodeGenerator::ensureFunctionInstantiated(const std::string& callee)
	{
		if (_instantiatedFunctions.contains(callee))
		{
			return true;
		}

		const auto less = callee.find('<');
		if (less == std::string::npos)
		{
			return false;
		}

		const std::string base = callee.substr(0, less);
		const auto        templ = _functionTemplates.find(base);
		if (templ == _functionTemplates.end())
		{
			return false;
		}

		_instantiatedFunctions.insert(callee);
		_functions.insert(callee);

		PendingFunction pending;
		pending.Name          = callee;
		pending.Decl          = templ->second.Decl;
		pending.Substitutions = buildSubstitutions(templ->second.Params, callee, base, {});
		_pendingFunctions.push_back(std::move(pending));
		return true;
	}

	void xenon::CodeGenerator::ensureTypeInstantiated(const std::string& typeName)
	{
		if (_typeFields.contains(typeName) || _instantiatedTypes.contains(typeName))
		{
			return;
		}

		const auto less = typeName.find('<');
		if (less == std::string::npos)
		{
			return;
		}

		const auto templ = _typeTemplates.find(typeName.substr(0, less));
		if (templ == _typeTemplates.end())
		{
			return;
		}

		const xenon::TypeDecl* decl = templ->second.Decl;

		const auto substitutions = buildSubstitutions(templ->second.Params, typeName,
													 typeName.substr(0, less), typeName);

		std::vector<std::string>        fieldNames;
		std::vector<std::string>        fieldTypes;
		std::unordered_set<std::string> constFields;
		for (const auto& field : decl->Fields)
		{
			fieldNames.push_back(field.Name);
			const auto replaced = substitutions.find(field.Type);
			fieldTypes.push_back(replaced != substitutions.end() ? replaced->second : field.Type);
			if (field.Constant)
			{
				constFields.insert(field.Name);
			}
		}
		_typeFields[typeName]     = std::move(fieldNames);
		_typeFieldTypes[typeName] = std::move(fieldTypes);
		if (!constFields.empty())
		{
			_constFields[typeName] = std::move(constFields);
		}

		for (const auto& method : decl->Methods)
		{
			_methods.insert(typeName + "::" + method.Name);
		}

		_instantiatedTypes.insert(typeName);
		_pendingTypes.push_back(PendingType{typeName, decl, substitutions});
	}

	void xenon::CodeGenerator::compileBlock(const xenon::BlockStmt& block)
	{
		enterScope();
		for (const auto& statement : block.Statements)
		{
			compileStatement(*statement);
		}
		leaveScope();
	}

	void xenon::CodeGenerator::compileStatement(const xenon::Stmt& stmt)
	{
		if (const auto* e = dynamic_cast<const xenon::ExprStmt*>(&stmt))
		{
			compileExpression(*e->Expression);
			emitOpcode(Instruction::Pop);
		}
		else if (const auto* v = dynamic_cast<const xenon::VarDeclStmt*>(&stmt))
		{
			if (v->Init)
			{
				compileExpression(*v->Init);
				const std::string valueType = typeOf(*v->Init);
				if (!isAssignable(substituteType(v->Type), valueType))
				{
					fail("cannot initialize '" + v->Name + "' of type " + v->Type + " with a value of type " +
							 valueType,
						 v->Line);
				}
			}
			declareLocal(v->Name, v->Type, v->Constant);
			if (v->Init)
			{
				emitOpcode(Instruction::Stl);
				emitU16(resolveLocal(v->Name));
			}
		}
		else if (const auto* r = dynamic_cast<const xenon::ReturnStmt*>(&stmt))
		{
			const bool voidFunction = _currentReturnType.empty() || _currentReturnType == "void";
			if (r->Value)
			{
				if (voidFunction)
				{
					fail("'" + _currentFunctionName + "' does not return a value", r->Line);
				}
				compileExpression(*r->Value);
				const std::string valueType = typeOf(*r->Value);
				if (!isAssignable(substituteType(_currentReturnType), valueType))
				{
					fail("'" + _currentFunctionName + "' returns " + _currentReturnType +
							 " but the expression has type " + valueType,
						 r->Line);
				}
			}
			else
			{
				if (!voidFunction)
				{
					fail("'" + _currentFunctionName + "' must return a value of type " + _currentReturnType,
						 r->Line);
				}
				emitConstant(xenon::Null{});
			}
			emitOpcode(Instruction::Ret);
		}
		else if (const auto* b = dynamic_cast<const xenon::BlockStmt*>(&stmt))
		{
			compileBlock(*b);
		}
		else if (const auto* i = dynamic_cast<const xenon::IfStmt*>(&stmt))
		{
			compileExpression(*i->Condition);
			const auto elseJump = emitJump(Instruction::Jif);
			compileStatement(*i->ThenBranch);
			const auto endJump = emitJump(Instruction::Jmp);
			patchJump(elseJump, _binary->codePosition());
			if (i->ElseBranch)
			{
				compileStatement(*i->ElseBranch);
			}
			patchJump(endJump, _binary->codePosition());
		}
		else if (const auto* w = dynamic_cast<const xenon::WhileStmt*>(&stmt))
		{
			const auto loopStart = _binary->codePosition();
			_loops.push_back(LoopContext{});
			compileExpression(*w->Condition);
			const auto exitJump = emitJump(Instruction::Jif);
			compileStatement(*w->Body);
			const auto backJump = emitJump(Instruction::Jmp);
			patchJump(backJump, loopStart);

			const auto end = _binary->codePosition();
			patchJump(exitJump, end);

			const auto loop = std::move(_loops.back());
			_loops.pop_back();
			for (const auto offset : loop.breaks)
			{
				patchJump(offset, end);
			}
			for (const auto offset : loop.continues)
			{
				patchJump(offset, loopStart);
			}
		}
		else if (const auto* dw = dynamic_cast<const xenon::DoWhileStmt*>(&stmt))
		{
			compileDoWhile(*dw);
		}
		else if (const auto* f = dynamic_cast<const xenon::ForStmt*>(&stmt))
		{
			compileFor(*f);
		}
		else if (const auto* s = dynamic_cast<const xenon::SwitchStmt*>(&stmt))
		{
			compileSwitch(*s);
		}
		else if (dynamic_cast<const xenon::BreakStmt*>(&stmt) != nullptr)
		{
			if (!_switches.empty())
			{
				_switches.back().breaks.push_back(emitJump(Instruction::Jmp));
			}
			else if (!_loops.empty())
			{
				_loops.back().breaks.push_back(emitJump(Instruction::Jmp));
			}
			else
			{
				fail("'break' outside a loop or switch");
			}
		}
		else if (dynamic_cast<const xenon::ContinueStmt*>(&stmt) != nullptr)
		{
			if (_loops.empty())
			{
				fail("'continue' outside a loop");
			}
			_loops.back().continues.push_back(emitJump(Instruction::Jmp));
		}
		else
		{
			fail("unknown statement");
		}
	}

	void xenon::CodeGenerator::compileFor(const xenon::ForStmt& stmt)
	{
		if (stmt.Init)
		{
			compileStatement(*stmt.Init);
		}

		const auto conditionStart = _binary->codePosition();
		_loops.push_back(LoopContext{});

		const bool  hasCondition = stmt.Condition != nullptr;
		std::size_t exitJump     = 0;
		if (hasCondition)
		{
			compileExpression(*stmt.Condition);
			exitJump = emitJump(Instruction::Jif);
		}

		compileStatement(*stmt.Body);

		const auto incrementStart = _binary->codePosition();
		if (stmt.Increment)
		{
			compileExpression(*stmt.Increment);
			emitOpcode(Instruction::Pop);
		}

		const auto backJump = emitJump(Instruction::Jmp);
		patchJump(backJump, conditionStart);

		const auto end = _binary->codePosition();
		if (hasCondition)
		{
			patchJump(exitJump, end);
		}

		const auto continueTarget = stmt.Increment ? incrementStart : conditionStart;

		const auto loop = std::move(_loops.back());
		_loops.pop_back();
		for (const auto offset : loop.breaks)
		{
			patchJump(offset, end);
		}
		for (const auto offset : loop.continues)
		{
			patchJump(offset, continueTarget);
		}
	}

	void xenon::CodeGenerator::compileDoWhile(const xenon::DoWhileStmt& stmt)
	{
		const auto loopStart = _binary->codePosition();
		_loops.push_back(LoopContext{});

		compileStatement(*stmt.Body);

		const auto conditionStart = _binary->codePosition();
		compileExpression(*stmt.Condition);
		const auto exitJump = emitJump(Instruction::Jif);
		const auto backJump = emitJump(Instruction::Jmp);
		patchJump(backJump, loopStart);

		const auto end = _binary->codePosition();
		patchJump(exitJump, end);

		const auto loop = std::move(_loops.back());
		_loops.pop_back();
		for (const auto offset : loop.breaks)
		{
			patchJump(offset, end);
		}
		for (const auto offset : loop.continues)
		{
			patchJump(offset, conditionStart);
		}
	}

	void xenon::CodeGenerator::compileSwitch(const xenon::SwitchStmt& stmt)
	{
		compileExpression(*stmt.Condition);
		const auto tempSlot = allocateTemp();
		emitOpcode(Instruction::Stl);
		emitU16(tempSlot);

		_switches.push_back(SwitchContext{});

		std::vector<std::size_t> bodyJumpForCase(stmt.Cases.size(), 0);
		std::size_t              defaultIndex = static_cast<std::size_t>(-1);

		for (std::size_t i = 0; i < stmt.Cases.size(); ++i)
		{
			const xenon::CaseClause& clause = stmt.Cases[i];
			if (clause.Value == nullptr)
			{
				defaultIndex = i;
				continue;
			}

			emitOpcode(Instruction::Ldl);
			emitU16(tempSlot);
			emitConstant(evaluateConstant(clause.Value.get()));
			emitOpcode(Instruction::Equ);
			const auto falseJump = emitJump(Instruction::Jif);
			bodyJumpForCase[i]   = emitJump(Instruction::Jmp);
			patchJump(falseJump, _binary->codePosition());
		}

		const bool  hasDefault = defaultIndex != static_cast<std::size_t>(-1);
		std::size_t defaultJump = 0;
		if (hasDefault)
		{
			defaultJump = emitJump(Instruction::Jmp);
		}
		const auto endJump = emitJump(Instruction::Jmp);

		for (std::size_t i = 0; i < stmt.Cases.size(); ++i)
		{
			const xenon::CaseClause& clause = stmt.Cases[i];
			const auto bodyOffset = _binary->codePosition();

			if (clause.Value != nullptr)
			{
				patchJump(bodyJumpForCase[i], bodyOffset);
			}
			else if (hasDefault)
			{
				patchJump(defaultJump, bodyOffset);
			}

			for (const auto& statement : clause.Statements)
			{
				compileStatement(*statement);
			}
		}

		const auto end = _binary->codePosition();
		patchJump(endJump, end);

		const auto context = std::move(_switches.back());
		_switches.pop_back();
		for (const auto offset : context.breaks)
		{
			patchJump(offset, end);
		}
	}

	void xenon::CodeGenerator::compileExpression(const xenon::Expr& expr)
	{
		if (const auto* n = dynamic_cast<const xenon::NumberExpr*>(&expr))
		{
			emitConstant(n->Value);
			return;
		}
		if (const auto* b = dynamic_cast<const xenon::BoolExpr*>(&expr))
		{
			emitConstant(b->Value);
			return;
		}
		if (dynamic_cast<const xenon::NullExpr*>(&expr) != nullptr)
		{
			emitConstant(xenon::Null{});
			return;
		}
		if (const auto* s = dynamic_cast<const xenon::StringExpr*>(&expr))
		{
			emitString(s->Value);
			return;
		}
		if (const auto* n = dynamic_cast<const xenon::NewExpr*>(&expr))
		{
			const std::string typeName = substituteType(n->TypeName);
			ensureTypeInstantiated(typeName);
			emitOpcode(Instruction::New);
			emitName(typeName);
			return;
		}
		if (const auto* f = dynamic_cast<const xenon::FieldAccessExpr*>(&expr))
		{
			compileExpression(*f->Object);
			const auto index = resolveField(*f->Object, f->FieldName);
			emitOpcode(Instruction::Getf);
			emitU16(index);
			return;
		}
		if (const auto* fa = dynamic_cast<const xenon::FieldAssignExpr*>(&expr))
		{
			const std::string ownerType = objectType(*fa->Object);
			checkFieldConstant(ownerType, fa->FieldName, fa->Line);
			if (!isAssignable(fieldType(ownerType, fa->FieldName), typeOf(*fa->Value)))
			{
				fail("cannot assign a value of type " + typeOf(*fa->Value) + " to the field '" +
						 fa->FieldName + "' of type " + fieldType(ownerType, fa->FieldName),
					 fa->Line);
			}

			if (fa->Compound)
			{
				compileFieldCompoundAssignment(*fa);
				return;
			}
			compileExpression(*fa->Object);
			compileExpression(*fa->Value);
			const auto index = resolveField(*fa->Object, fa->FieldName);
			emitOpcode(Instruction::Setf);
			emitU16(index);
			return;
		}
		if (const auto* v = dynamic_cast<const xenon::VariableExpr*>(&expr))
		{
			if (hasLocal(v->Name))
			{
				emitOpcode(Instruction::Ldl);
				emitU16(resolveLocal(v->Name));
			}
			else
			{
				resolveGlobal(v->Name);
				emitOpcode(Instruction::Ldg);
				emitName(v->Name);
			}
			return;
		}
		if (const auto* u = dynamic_cast<const xenon::UnaryExpr*>(&expr))
		{
			switch (u->Operation)
			{
				case xenon::UnaryExpr::Op::Negate:
					compileExpression(*u->Operand);
					emitOpcode(Instruction::Neg);
					return;
				case xenon::UnaryExpr::Op::Not:
					compileExpression(*u->Operand);
					emitOpcode(Instruction::Not);
					return;
				case xenon::UnaryExpr::Op::BitNot:
					compileExpression(*u->Operand);
					emitOpcode(Instruction::BNot);
					return;
				case xenon::UnaryExpr::Op::PreIncrement:
					compileIncrement(*u->Operand, true, false);
					return;
				case xenon::UnaryExpr::Op::PreDecrement:
					compileIncrement(*u->Operand, false, false);
					return;
			}
			fail("unknown unary operator");
		}
		if (const auto* p = dynamic_cast<const xenon::PostfixExpr*>(&expr))
		{
			compileIncrement(*p->Operand, p->Increment, true);
			return;
		}
		if (const auto* ternary = dynamic_cast<const xenon::TernaryExpr*>(&expr))
		{
			const std::string thenType = typeOf(*ternary->Then);
			const std::string elseType = typeOf(*ternary->Else);
			if (!isAssignable(thenType, elseType) && !isAssignable(elseType, thenType))
			{
				fail("the branches of '?:' have incompatible types " + thenType + " and " + elseType,
					 ternary->Line);
			}

			compileExpression(*ternary->Condition);
			const auto elseJump = emitJump(Instruction::Jif);
			compileExpression(*ternary->Then);
			const auto endJump = emitJump(Instruction::Jmp);
			patchJump(elseJump, _binary->codePosition());
			compileExpression(*ternary->Else);
			patchJump(endJump, _binary->codePosition());
			return;
		}
		if (const auto* a = dynamic_cast<const xenon::AssignExpr*>(&expr))
		{
			checkConstWrite(a->Name, a->Line);
			const std::string targetType = substituteType(hasLocal(a->Name) ? localType(a->Name)
																		   : globalType(a->Name));
			if (!isAssignable(targetType, typeOf(*a->Value)))
			{
				fail("cannot assign a value of type " + typeOf(*a->Value) + " to '" + a->Name +
						 "' of type " + targetType,
					 a->Line);
			}

			if (a->Compound)
			{
				compileCompoundAssignment(a->Name, *a->Value, a->Operation);
				return;
			}
			compileExpression(*a->Value);
			if (hasLocal(a->Name))
			{
				const auto slot = resolveLocal(a->Name);
				emitOpcode(Instruction::Stl);
				emitU16(slot);
				emitOpcode(Instruction::Ldl);
				emitU16(slot);
			}
			else
			{
				resolveGlobal(a->Name);
				emitOpcode(Instruction::Stg);
				emitName(a->Name);
			}
			return;
		}
		if (const auto* c = dynamic_cast<const xenon::CallExpr*>(&expr))
		{
			checkCall(*c);
			for (const auto& arg : c->Args)
			{
				compileExpression(*arg);
			}
			resolveFunction(c->Callee);
			emitOpcode(Instruction::Call);
			emitName(c->Callee);
			return;
		}
		if (const auto* bin = dynamic_cast<const xenon::BinaryExpr*>(&expr))
		{
			if (bin->Operation == xenon::BinaryExpr::Op::And)
			{
				compileExpression(*bin->Left);
				const auto falseJump = emitJump(Instruction::Jif);
				compileExpression(*bin->Right);
				const auto endJump = emitJump(Instruction::Jmp);
				patchJump(falseJump, _binary->codePosition());
				emitConstant(false);
				patchJump(endJump, _binary->codePosition());
				return;
			}
			if (bin->Operation == xenon::BinaryExpr::Op::Or)
			{
				compileExpression(*bin->Left);
				const auto evalRight = emitJump(Instruction::Jif);
				emitConstant(true);
				const auto endJump = emitJump(Instruction::Jmp);
				patchJump(evalRight, _binary->codePosition());
				compileExpression(*bin->Right);
				patchJump(endJump, _binary->codePosition());
				return;
			}

			compileExpression(*bin->Left);
			compileExpression(*bin->Right);
			emitBinaryOperation(bin->Operation);
			return;
		}
		fail("unknown expression");
	}

	void xenon::CodeGenerator::emitBinaryOperation(xenon::CompoundOp operation)
	{
		switch (operation)
		{
			case xenon::CompoundOp::Add:          emitOpcode(Instruction::Add); break;
			case xenon::CompoundOp::Subtract:     emitOpcode(Instruction::Sub); break;
			case xenon::CompoundOp::Multiply:     emitOpcode(Instruction::Mul); break;
			case xenon::CompoundOp::Divide:       emitOpcode(Instruction::Div); break;
			case xenon::CompoundOp::Modulo:       emitOpcode(Instruction::Mod); break;
			case xenon::CompoundOp::Equal:        emitOpcode(Instruction::Equ); break;
			case xenon::CompoundOp::NotEqual:     emitOpcode(Instruction::Neq); break;
			case xenon::CompoundOp::Less:         emitOpcode(Instruction::Lss); break;
			case xenon::CompoundOp::LessEqual:    emitOpcode(Instruction::Leq); break;
			case xenon::CompoundOp::Greater:      emitOpcode(Instruction::Gtr); break;
			case xenon::CompoundOp::GreaterEqual: emitOpcode(Instruction::Geq); break;
			case xenon::CompoundOp::BitAnd:       emitOpcode(Instruction::BAnd); break;
			case xenon::CompoundOp::BitOr:        emitOpcode(Instruction::BOr); break;
			case xenon::CompoundOp::BitXor:       emitOpcode(Instruction::BXor); break;
			case xenon::CompoundOp::ShiftLeft:    emitOpcode(Instruction::Shl); break;
			case xenon::CompoundOp::ShiftRight:   emitOpcode(Instruction::Shr); break;
			case xenon::CompoundOp::And:
			case xenon::CompoundOp::Or:
				fail("logical operators are not supported in compound assignments");
		}
	}

	void xenon::CodeGenerator::compileCompoundAssignment(const std::string& name, const xenon::Expr& value,
														 xenon::CompoundOp operation)
	{
		if (hasLocal(name))
		{
			const auto slot = resolveLocal(name);
			emitOpcode(Instruction::Ldl);
			emitU16(slot);
			compileExpression(value);
			emitBinaryOperation(operation);
			emitOpcode(Instruction::Stl);
			emitU16(slot);
			emitOpcode(Instruction::Ldl);
			emitU16(slot);
			return;
		}

		resolveGlobal(name);
		emitOpcode(Instruction::Ldg);
		emitName(name);
		compileExpression(value);
		emitBinaryOperation(operation);
		emitOpcode(Instruction::Stg);
		emitName(name);
	}

	void xenon::CodeGenerator::compileFieldCompoundAssignment(const xenon::FieldAssignExpr& expr)
	{
		const auto fieldIndex = resolveField(*expr.Object, expr.FieldName);
		const auto objectSlot = allocateTemp();
		const auto valueSlot  = allocateTemp();

		compileExpression(*expr.Object);
		emitOpcode(Instruction::Stl);
		emitU16(objectSlot);

		emitOpcode(Instruction::Ldl);
		emitU16(objectSlot);
		emitOpcode(Instruction::Getf);
		emitU16(fieldIndex);
		compileExpression(*expr.Value);
		emitBinaryOperation(expr.Operation);
		emitOpcode(Instruction::Stl);
		emitU16(valueSlot);

		emitOpcode(Instruction::Ldl);
		emitU16(objectSlot);
		emitOpcode(Instruction::Ldl);
		emitU16(valueSlot);
		emitOpcode(Instruction::Setf);
		emitU16(fieldIndex);
	}

	void xenon::CodeGenerator::compileIncrement(const xenon::Expr& target, bool increment, bool postfix)
	{
		const auto emitStep = [this, increment]()
		{
			emitConstant(1.0);
			emitOpcode(increment ? Instruction::Add : Instruction::Sub);
		};

		if (const auto* variable = dynamic_cast<const xenon::VariableExpr*>(&target))
		{
			checkConstWrite(variable->Name, target.Line);
			if (hasLocal(variable->Name))
			{
				const auto slot = resolveLocal(variable->Name);
				if (postfix)
				{
					emitOpcode(Instruction::Ldl);
					emitU16(slot);
				}
				emitOpcode(Instruction::Ldl);
				emitU16(slot);
				emitStep();
				emitOpcode(Instruction::Stl);
				emitU16(slot);
				if (!postfix)
				{
					emitOpcode(Instruction::Ldl);
					emitU16(slot);
				}
				return;
			}

			resolveGlobal(variable->Name);
			if (postfix)
			{
				emitOpcode(Instruction::Ldg);
				emitName(variable->Name);
			}
			emitOpcode(Instruction::Ldg);
			emitName(variable->Name);
			emitStep();
			emitOpcode(Instruction::Stg);
			emitName(variable->Name);
			if (postfix)
			{
				emitOpcode(Instruction::Pop);
			}
			return;
		}

		if (const auto* field = dynamic_cast<const xenon::FieldAccessExpr*>(&target))
		{
			checkFieldConstant(objectType(*field->Object), field->FieldName, target.Line);
			const auto fieldIndex = resolveField(*field->Object, field->FieldName);
			const auto objectSlot = allocateTemp();
			const auto valueSlot  = allocateTemp();

			compileExpression(*field->Object);
			emitOpcode(Instruction::Stl);
			emitU16(objectSlot);

			if (postfix)
			{
				emitOpcode(Instruction::Ldl);
				emitU16(objectSlot);
				emitOpcode(Instruction::Getf);
				emitU16(fieldIndex);
			}

			emitOpcode(Instruction::Ldl);
			emitU16(objectSlot);
			emitOpcode(Instruction::Getf);
			emitU16(fieldIndex);
			emitStep();
			emitOpcode(Instruction::Stl);
			emitU16(valueSlot);

			emitOpcode(Instruction::Ldl);
			emitU16(objectSlot);
			emitOpcode(Instruction::Ldl);
			emitU16(valueSlot);
			emitOpcode(Instruction::Setf);
			emitU16(fieldIndex);
			if (postfix)
			{
				emitOpcode(Instruction::Pop);
			}
			return;
		}

		fail("'++' and '--' require a variable or a field");
	}

	void xenon::CodeGenerator::emitOpcode(Instruction op)
	{
		_binary->write(op, 0);
	}

	void xenon::CodeGenerator::emitByte(std::uint8_t byte)
	{
		_binary->write(byte, 0);
	}

	void xenon::CodeGenerator::emitU16(std::size_t value)
	{
		if (value > XenonBinary::MaxOperand)
		{
			fail("operand does not fit into 16 bits");
		}
		emitByte(static_cast<std::uint8_t>(value & 0xff));
		emitByte(static_cast<std::uint8_t>((value >> 8) & 0xff));
	}

	std::size_t xenon::CodeGenerator::emitJump(Instruction op)
	{
		emitOpcode(op);
		const auto offset = _binary->codePosition();
		emitU16(0);
		return offset;
	}

	void xenon::CodeGenerator::patchJump(std::size_t operandOffset, std::size_t target)
	{
		const auto relative = static_cast<std::int16_t>(target - (operandOffset + 2));
		_binary->patchU16(operandOffset, static_cast<std::uint16_t>(relative));
	}

	void xenon::CodeGenerator::emitConstant(Value value)
	{
		const auto index = _binary->addConstant(std::move(value));
		emitOpcode(Instruction::Const);
		emitU16(index);
	}

	void xenon::CodeGenerator::emitString(const std::string& value)
	{
		const auto index = _binary->addString(value);
		emitOpcode(Instruction::Str);
		emitU16(index);
	}

	void xenon::CodeGenerator::emitName(const std::string& name)
	{
		const auto index = _binary->addString(name);
		emitU16(index);
	}
}
