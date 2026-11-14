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
			for (const auto& field : decl.Fields)
			{
				fieldNames.push_back(field.Name);
			}
			_typeFields[decl.Name] = std::move(fieldNames);

			for (const auto& method : decl.Methods)
			{
				_methods.insert(decl.Name + "::" + method.Name);
			}
		}

		for (const auto& function : _hints.Functions)
		{
			_functions.insert(function.Name);
		}

		for (const auto& type : _hints.Types)
		{
			std::vector<std::string> fieldNames;
			for (const auto& field : type.Fields)
			{
				fieldNames.push_back(field.Name);
			}
			_typeFields[type.Name] = std::move(fieldNames);

			for (const auto& method : type.Methods)
			{
				_methods.insert(type.Name + "::" + method.Name);
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
		}
	}

	[[noreturn]] void xenon::CodeGenerator::fail(const std::string& message)
	{
		_error = message;
		throw CodeGenError{};
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
		for (const auto& param : function.Params)
		{
			declareLocal(param.second, param.first);
		}
		compileBlock(*function.Body);

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
		for (const auto& param : method.Params)
		{
			declareLocal(param.second, param.first);
		}
		compileBlock(*method.Body);

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
		_scopes.push_back(Scope{{}, {}, _slotCounter});
	}

	void xenon::CodeGenerator::leaveScope()
	{
		_slotCounter = _scopes.back().BaseSlot;
		_scopes.pop_back();
	}

	void xenon::CodeGenerator::declareLocal(const std::string& name, const std::string& type)
	{
		_scopes.back().Names[name] = _slotCounter;
		if (!type.empty())
		{
			_scopes.back().Types[name] = substituteType(type);
		}
		++_slotCounter;
		_maxSlots = std::max(_maxSlots, _slotCounter);
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

	std::size_t xenon::CodeGenerator::resolveField(const xenon::Expr& object, const std::string& fieldName)
	{
		std::string typeName;
		if (const auto* variable = dynamic_cast<const xenon::VariableExpr*>(&object))
		{
			typeName = localType(variable->Name);
		}
		else if (const auto* created = dynamic_cast<const xenon::NewExpr*>(&object))
		{
			typeName = created->TypeName;
		}
		else
		{
			fail("cannot determine the object type for field access");
		}

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

		std::vector<std::string> fieldNames;
		for (const auto& field : decl->Fields)
		{
			fieldNames.push_back(field.Name);
		}
		_typeFields[typeName] = std::move(fieldNames);

		for (const auto& method : decl->Methods)
		{
			_methods.insert(typeName + "::" + method.Name);
		}

		_instantiatedTypes.insert(typeName);
		_pendingTypes.push_back(PendingType{
			typeName,
			decl,
			buildSubstitutions(templ->second.Params, typeName, typeName.substr(0, less), typeName),
		});
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
			}
			declareLocal(v->Name, v->Type);
			if (v->Init)
			{
				emitOpcode(Instruction::Stl);
				emitU16(resolveLocal(v->Name));
			}
		}
		else if (const auto* r = dynamic_cast<const xenon::ReturnStmt*>(&stmt))
		{
			if (r->Value)
			{
				compileExpression(*r->Value);
			}
			else
			{
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
