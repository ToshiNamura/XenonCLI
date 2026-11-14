#include <xenon/asm/assembler.hpp>

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <xenon/asm/opcode.hpp>

[[noreturn]] void xenon::Assembler::fail(const std::string& message)
{
	_error = message + " (line " + std::to_string(_line) + ")";
	throw Error{};
}

std::vector<xenon::Assembler::Token> xenon::Assembler::tokenize(std::string_view line)
{
	std::vector<Token> tokens;
	std::size_t i = 0;
	while (i < line.size())
	{
		const char c = line[i];
		if (std::isspace(static_cast<unsigned char>(c)))
		{
			++i;
			continue;
		}
		if (c == '"')
		{
			++i;
			std::string value;
			while (i < line.size() && line[i] != '"')
			{
				if (line[i] == '\\' && i + 1 < line.size())
				{
					switch (line[i + 1])
					{
						case 'n': value.push_back('\n'); break;
						case 'r': value.push_back('\r'); break;
						case 't': value.push_back('\t'); break;
						case '\\': value.push_back('\\'); break;
						case '"': value.push_back('"'); break;
						default:
							value.push_back(line[i + 1]);
							break;
					}
					i += 2;
					continue;
				}
				value.push_back(line[i++]);
			}
			if (i >= line.size())
			{
				fail("unterminated string literal");
			}
			++i; // closing quote
			Token token;
			token.Text = std::move(value);
			token.IsString = true;
			tokens.push_back(std::move(token));
			continue;
		}
		if (std::isdigit(static_cast<unsigned char>(c)) ||
			(c == '-' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1]))))
		{
			const std::size_t start = i;
			++i;
			while (i < line.size() &&
				   (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.'))
			{
				++i;
			}
			Token token;
			token.Text = std::string(line.substr(start, i - start));
			token.IsNumber = true;
			tokens.push_back(std::move(token));
			continue;
		}
		if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.')
		{
			const std::size_t start = i;
			while (i < line.size() &&
				   (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_' || line[i] == '.'))
			{
				++i;
			}
			Token token;
			token.Text = std::string(line.substr(start, i - start));
			token.IsIdentifier = true;
			tokens.push_back(std::move(token));
			continue;
		}
		Token token;
		token.Text = std::string(1, c);
		tokens.push_back(std::move(token));
		++i;
	}
	return tokens;
}

void xenon::Assembler::clearPending()
{
	_pendingAttributes.clear();
	_pendingTemplates.clear();
	_pendingParents.clear();
}

bool xenon::Assembler::parseAccess(const std::string& text, Access& out)
{
	if (text == "private")
	{
		out = Access::Private;
		return true;
	}
	if (text == "protected")
	{
		out = Access::Protected;
		return true;
	}
	if (text == "public")
	{
		out = Access::Public;
		return true;
	}
	return false;
}

void xenon::Assembler::finishParamGroup(std::vector<std::string>& group,
									 std::vector<std::string>& paramTypes,
									 std::size_t& arity)
{
	if (group.empty())
	{
		return;
	}
	if (group.size() == 1 && group[0] == "void")
	{
		return; // (void) — нет параметров
	}
	paramTypes.push_back(group[0]);
	++arity;
}

std::string xenon::Assembler::join(const std::vector<std::string>& items, std::string_view sep)
{
	std::string result;
	for (std::size_t i = 0; i < items.size(); ++i)
	{
		if (i != 0)
		{
			result += sep;
		}
		result += items[i];
	}
	return result;
}

std::size_t xenon::Assembler::parseParams(const std::vector<Token>& tokens, std::size_t openIdx,
									   std::vector<std::string>& paramTypes)
{
	std::vector<std::string> group;
	std::size_t arity = 0;
	for (std::size_t i = openIdx + 1; i < tokens.size(); ++i)
	{
		const std::string& text = tokens[i].Text;
		if (text == ")")
		{
			finishParamGroup(group, paramTypes, arity);
			return arity;
		}
		if (text == ",")
		{
			finishParamGroup(group, paramTypes, arity);
			group.clear();
			continue;
		}
		group.push_back(text);
	}
	fail("expected ')' in signature");
}

xenon::Value xenon::Assembler::parseValue(const Token& token)
{
	if (token.IsNumber)
	{
		return Value{std::strtod(token.Text.c_str(), nullptr)};
	}
	if (token.IsIdentifier)
	{
		if (token.Text == "true")
		{
			return Value{true};
		}
		if (token.Text == "false")
		{
			return Value{false};
		}
		if (token.Text == "null")
		{
			return Value{Null{}};
		}
	}
	if (token.IsString)
	{
		fail("string initial values are not supported yet");
	}
	fail("invalid initial value: " + token.Text);
}

bool xenon::Assembler::assemble(std::string_view source, Module& out)
{
	_error.clear();
	out = Module{};
	_out = &out;
	_line = 0;
	_section = Section::None;
	_scopes.clear();
	_currentType = nullptr;
	_currentFunction = nullptr;
	_currentMethod = nullptr;
	clearPending();

	try
	{
		std::size_t pos = 0;
		while (pos <= source.size())
		{
			const std::size_t end = source.find('\n', pos);
			std::string_view raw = (end == std::string_view::npos)
				? source.substr(pos)
				: source.substr(pos, end - pos);
			++_line;

			std::string_view content = raw;
			const std::size_t comment = content.find(';');
			if (comment != std::string_view::npos)
			{
				content = content.substr(0, comment);
			}
			std::size_t first = 0;
			while (first < content.size() && std::isspace(static_cast<unsigned char>(content[first])))
			{
				++first;
			}
			std::size_t last = content.size();
			while (last > first && std::isspace(static_cast<unsigned char>(content[last - 1])))
			{
				--last;
			}
			content = content.substr(first, last - first);

			if (!content.empty())
			{
				processLine(content);
			}

			if (end == std::string_view::npos)
			{
				break;
			}
			pos = end + 1;
		}

		if (!_scopes.empty())
		{
			fail("unexpected end of file: unclosed scope");
		}

		_out = nullptr;
		return true;
	}
	catch (const Error&)
	{
		_out = nullptr;
		return false;
	}
}

void xenon::Assembler::processLine(std::string_view line)
{
	const auto tokens = tokenize(line);
	if (tokens.empty())
	{
		return;
	}
	if (tokens[0].IsIdentifier && tokens[0].Text[0] == '.')
	{
		parseDirective(tokens);
	}
	else
	{
		parseInstruction(tokens);
	}
}

void xenon::Assembler::parseDirective(const std::vector<Token>& tokens)
{
	const std::string_view name = tokens[0].Text;

	if (name == ".meta" || name == ".types" || name == ".data" || name == ".text" || name == ".extern")
	{
		if (!_scopes.empty())
		{
			fail("section must be opened at top level");
		}
		if (name == ".meta")
		{
			_section = Section::Meta;
		}
		else if (name == ".types")
		{
			_section = Section::Types;
		}
		else if (name == ".data")
		{
			_section = Section::Data;
		}
		else if (name == ".text")
		{
			_section = Section::Text;
		}
		else
		{
			_section = Section::Extern;
		}
		clearPending();
		_scopes.push_back(Scope::Section);
		return;
	}

	if (name == ".end")
	{
		closeScope();
		return;
	}

	if (name == ".module")
	{
		if (_section != Section::Meta)
		{
			fail("'.module' is allowed only in .meta");
		}
		if (tokens.size() != 2)
		{
			fail("expected '.module <name>'");
		}
		_out->Name = tokens[1].Text;
		return;
	}

	if (name == ".import")
	{
		if (_section != Section::Meta)
		{
			fail("'.import' is allowed only in .meta");
		}
		if (tokens.size() != 2)
		{
			fail("expected '.import <name>'");
		}
		_out->Imports.push_back(tokens[1].Text);
		return;
	}

	if (name == ".attribute")
	{
		parseAttribute(tokens);
		return;
	}
	if (name == ".template")
	{
		parseTemplate(tokens);
		return;
	}
	if (name == ".parent")
	{
		parseParent(tokens);
		return;
	}

	if (name == ".class" || name == ".struct" || name == ".enum")
	{
		parseTypeDecl(tokens);
		return;
	}
	if (name == ".field")
	{
		parseField(tokens);
		return;
	}
	if (name == ".method")
	{
		parseMethod(tokens);
		return;
	}
	if (name == ".item")
	{
		parseItem(tokens);
		return;
	}
	if (name == ".function")
	{
		parseFunction(tokens);
		return;
	}
	if (name == ".variable")
	{
		parseVariable(tokens);
		return;
	}
	if (name == ".constant")
	{
		parseConstant(tokens);
		return;
	}
	if (name == ".local")
	{
		parseLocal(tokens);
		return;
	}
	if (name == ".locals")
	{
		parseLocals(tokens);
		return;
	}

	fail("unknown directive: " + std::string(name));
}

void xenon::Assembler::closeScope()
{
	if (_scopes.empty())
	{
		fail("unexpected '.end'");
	}
	const Scope scope = _scopes.back();
	_scopes.pop_back();
	switch (scope)
	{
		case Scope::Section:
			_section = Section::None;
			break;
		case Scope::Type:
			_currentType = nullptr;
			break;
		case Scope::Function:
			_currentFunction = nullptr;
			break;
		case Scope::Method:
			_currentMethod = nullptr;
			break;
	}
}

void xenon::Assembler::parseTypeDecl(const std::vector<Token>& tokens)
{
	if (_section != Section::Types && _section != Section::Extern)
	{
		fail("type declaration is allowed only in .types or .extern");
	}

	std::size_t i = 1;
	bool exported = false;
	if (i < tokens.size() && tokens[i].Text == "export")
	{
		exported = true;
		++i;
	}
	if (i + 1 != tokens.size())
	{
		fail("expected '.class/.struct/.enum [export] <name>'");
	}

	Type type;
	type.Name = tokens[i].Text;
	type.Exported = exported;
	if (tokens[0].Text == ".class")
	{
		type.Kind = TypeKind::Class;
	}
	else if (tokens[0].Text == ".struct")
	{
		type.Kind = TypeKind::Struct;
	}
	else
	{
		type.Kind = TypeKind::Enum;
	}

	if (type.Kind == TypeKind::Enum && !_pendingParents.empty())
	{
		fail("'.parent' is not allowed for an enum");
	}

	type.Attributes = std::move(_pendingAttributes);
	type.TemplateParams = std::move(_pendingTemplates);
	type.Parents = std::move(_pendingParents);
	clearPending();

	_out->Types.push_back(std::move(type));
	_currentType = &_out->Types.back();
	_scopes.push_back(Scope::Type);
}

void xenon::Assembler::parseField(const std::vector<Token>& tokens)
{
	if (_currentType == nullptr)
	{
		fail("'.field' outside of a type");
	}

	std::size_t i = 1;
	bool exported = false;
	if (i < tokens.size() && tokens[i].Text == "export")
	{
		exported = true;
		++i;
	}
	bool constant = false;
	if (i < tokens.size() && tokens[i].Text == "const")
	{
		constant = true;
		++i;
	}
	Access access = (_currentType->Kind == TypeKind::Struct) ? Access::Public : Access::Private;
	if (i < tokens.size() && parseAccess(tokens[i].Text, access))
	{
		++i;
	}
	if (i + 2 != tokens.size())
	{
		fail("expected '.field [export] [const] [access] <type> <name>'");
	}
	if (!_pendingParents.empty())
	{
		fail("'.parent' is not allowed for a field");
	}

	Field field;
	field.Type = tokens[i].Text;
	field.Name = tokens[i + 1].Text;
	field.AccessModifier = access;
	field.Exported = exported;
	field.Constant = constant;
	field.Attributes = std::move(_pendingAttributes);
	field.TemplateParams = std::move(_pendingTemplates);
	clearPending();

	_currentType->Fields.push_back(std::move(field));
}

void xenon::Assembler::parseMethod(const std::vector<Token>& tokens)
{
	if (_currentType == nullptr)
	{
		fail("'.method' outside of a type");
	}

	std::size_t i = 1;
	bool exported = false;
	if (i < tokens.size() && tokens[i].Text == "export")
	{
		exported = true;
		++i;
	}
	Access access = (_currentType->Kind == TypeKind::Struct) ? Access::Public : Access::Private;
	if (i < tokens.size() && parseAccess(tokens[i].Text, access))
	{
		++i;
	}
	if (i + 2 >= tokens.size() || tokens[i + 2].Text != "(")
	{
		fail("expected '.method <type> <name>(<params>)'");
	}
	if (!_pendingParents.empty())
	{
		fail("'.parent' is not allowed for a method");
	}

	const std::string returnType = tokens[i].Text;
	const std::string name = tokens[i + 1].Text;

	std::vector<std::string> paramTypes;
	const std::size_t arity = parseParams(tokens, i + 2, paramTypes);

	Method method;
	method.Name = name;
	method.Signature = returnType + " " + name + "(" + join(paramTypes, ", ") + ")";
	method.AccessModifier = access;
	method.Exported = exported;
	method.Arity = arity;
	method.Defined = (_section != Section::Extern);
	method.Attributes = std::move(_pendingAttributes);
	method.TemplateParams = std::move(_pendingTemplates);
	clearPending();

	_currentType->Methods.push_back(std::move(method));
	_currentMethod = &_currentType->Methods.back();

	if (_section == Section::Extern)
	{
		_currentMethod = nullptr; // объявление без тела
	}
	else
	{
		_scopes.push_back(Scope::Method);
	}
}

void xenon::Assembler::parseItem(const std::vector<Token>& tokens)
{
	if (_currentType == nullptr || _currentType->Kind != TypeKind::Enum)
	{
		fail("'.item' is allowed only inside .enum");
	}
	if (tokens.size() != 2)
	{
		fail("expected '.item <name>'");
	}
	_currentType->Enumerators.push_back(tokens[1].Text);
}

void xenon::Assembler::parseFunction(const std::vector<Token>& tokens)
{
	if (_section != Section::Text && _section != Section::Extern)
	{
		fail("'.function' is allowed only in .text or .extern");
	}

	std::size_t i = 1;
	bool exported = false;
	if (i < tokens.size() && tokens[i].Text == "export")
	{
		exported = true;
		++i;
	}
	if (i + 2 >= tokens.size() || tokens[i + 2].Text != "(")
	{
		fail("expected '.function <type> <name>(<params>)'");
	}
	if (!_pendingParents.empty())
	{
		fail("'.parent' is not allowed for a function");
	}

	const std::string returnType = tokens[i].Text;
	const std::string name = tokens[i + 1].Text;

	std::vector<std::string> paramTypes;
	const std::size_t arity = parseParams(tokens, i + 2, paramTypes);

	Function function;
	function.Name = name;
	function.Signature = returnType + " " + name + "(" + join(paramTypes, ", ") + ")";
	function.Exported = exported;
	function.Arity = arity;
	function.Defined = (_section != Section::Extern);
	function.Attributes = std::move(_pendingAttributes);
	function.TemplateParams = std::move(_pendingTemplates);
	clearPending();

	_out->Functions.push_back(std::move(function));
	_currentFunction = &_out->Functions.back();

	if (_section == Section::Extern)
	{
		_currentFunction = nullptr; // объявление без тела
	}
	else
	{
		_scopes.push_back(Scope::Function);
	}
}

void xenon::Assembler::parseVariable(const std::vector<Token>& tokens)
{
	if (_section != Section::Data && _section != Section::Extern)
	{
		fail("'.variable' is allowed only in .data or .extern");
	}

	std::size_t i = 1;
	bool exported = false;
	if (i < tokens.size() && tokens[i].Text == "export")
	{
		exported = true;
		++i;
	}
	if (i + 2 > tokens.size())
	{
		fail("expected '.variable [export] <type> <name> [<value>]'");
	}
	if (!_pendingParents.empty())
	{
		fail("'.parent' is not allowed for a variable");
	}

	Global global;
	global.Type = tokens[i].Text;
	global.Name = tokens[i + 1].Text;
	global.Exported = exported;
	global.Constant = false;
	if (i + 2 < tokens.size())
	{
		global.InitialValue = parseValue(tokens[i + 2]);
	}
	global.Attributes = std::move(_pendingAttributes);
	global.TemplateParams = std::move(_pendingTemplates);
	clearPending();

	_out->Globals.push_back(std::move(global));
}

void xenon::Assembler::parseConstant(const std::vector<Token>& tokens)
{
	if (_section != Section::Data && _section != Section::Extern)
	{
		fail("'.constant' is allowed only in .data or .extern");
	}

	std::size_t i = 1;
	bool exported = false;
	if (i < tokens.size() && tokens[i].Text == "export")
	{
		exported = true;
		++i;
	}
	if (i + 3 != tokens.size())
	{
		fail("expected '.constant [export] <type> <name> <value>'");
	}
	if (!_pendingParents.empty())
	{
		fail("'.parent' is not allowed for a constant");
	}

	Global global;
	global.Type = tokens[i].Text;
	global.Name = tokens[i + 1].Text;
	global.Exported = exported;
	global.Constant = true;
	global.InitialValue = parseValue(tokens[i + 2]);
	global.Attributes = std::move(_pendingAttributes);
	global.TemplateParams = std::move(_pendingTemplates);
	clearPending();

	_out->Globals.push_back(std::move(global));
}

void xenon::Assembler::parseLocal(const std::vector<Token>& tokens)
{
	if (_currentFunction == nullptr && _currentMethod == nullptr)
	{
		fail("'.local' outside of a function or method body");
	}
	if (tokens.size() != 3)
	{
		fail("expected '.local <type> <name>'");
	}
	if (_currentFunction != nullptr)
	{
		++_currentFunction->Locals;
	}
	else
	{
		++_currentMethod->Locals;
	}
}

void xenon::Assembler::parseLocals(const std::vector<Token>& tokens)
{
	if (_currentFunction == nullptr && _currentMethod == nullptr)
	{
		fail("'.locals' outside of a function or method body");
	}
	if (tokens.size() != 2 || !tokens[1].IsNumber)
	{
		fail("expected '.locals <count>'");
	}

	std::size_t count = 0;
	const auto result = std::from_chars(tokens[1].Text.data(),
										tokens[1].Text.data() + tokens[1].Text.size(), count);
	if (result.ec != std::errc() || result.ptr != tokens[1].Text.data() + tokens[1].Text.size())
	{
		fail("invalid local count: " + tokens[1].Text);
	}

	if (_currentFunction != nullptr)
	{
		_currentFunction->Locals = count;
	}
	else
	{
		_currentMethod->Locals = count;
	}
}

void xenon::Assembler::parseAttribute(const std::vector<Token>& tokens)
{
	if (tokens.size() < 2)
	{
		fail("expected '.attribute <key> [<value>]'");
	}

	const std::string key = tokens[1].Text;
	std::unique_ptr<Attribute> attribute;

	if (tokens.size() == 2)
	{
		auto base = std::make_unique<Attribute>();
		base->Key = key;
		attribute = std::move(base);
	}
	else if (tokens.size() == 3)
	{
		const Token& value = tokens[2];
		if (value.IsNumber)
		{
			if (value.Text.find('.') != std::string::npos)
			{
				auto number = std::make_unique<AttributeFloat>();
				number->Key = key;
				number->Value = std::strtod(value.Text.c_str(), nullptr);
				attribute = std::move(number);
			}
			else
			{
				auto number = std::make_unique<AttributeInteger>();
				number->Key = key;
				number->Value = std::strtoll(value.Text.c_str(), nullptr, 10);
				attribute = std::move(number);
			}
		}
		else if (value.IsString)
		{
			auto text = std::make_unique<AttributeString>();
			text->Key = key;
			text->Value = value.Text;
			attribute = std::move(text);
		}
		else
		{
			fail("expected a numeric or string attribute value");
		}
	}
	else if (tokens.size() == 4)
	{
		// ссылка на объект: .attribute <key> .class <name>
		if (!tokens[2].IsIdentifier || tokens[2].Text.empty() || tokens[2].Text[0] != '.')
		{
			fail("expected object reference like '.class <name>'");
		}
		auto object = std::make_unique<AttributeObject>();
		object->Key = key;
		object->Object = tokens[3].Text;
		attribute = std::move(object);
	}
	else
	{
		fail("too many tokens in '.attribute'");
	}

	if (_section == Section::Meta)
	{
		_out->Attributes.push_back(std::move(attribute));
	}
	else
	{
		_pendingAttributes.push_back(std::move(attribute));
	}
}

void xenon::Assembler::parseTemplate(const std::vector<Token>& tokens)
{
	if (tokens.size() < 3 || tokens[1].Text != "<")
	{
		fail("expected '.template <...>'");
	}

	std::size_t i = 2;
	while (i < tokens.size() && tokens[i].Text != ">")
	{
		std::vector<Token> group;
		while (i < tokens.size() && tokens[i].Text != "," && tokens[i].Text != ">")
		{
			group.push_back(tokens[i]);
			++i;
		}

		if (group.empty())
		{
			fail("empty template parameter");
		}

		std::vector<Token> left;
		std::string defaultValue;
		bool hasDefault = false;
		for (const auto& token : group)
		{
			if (token.Text == "=")
			{
				if (hasDefault)
				{
					fail("duplicate '=' in template parameter");
				}
				hasDefault = true;
				continue;
			}
			if (hasDefault)
			{
				defaultValue = token.Text;
			}
			else
			{
				left.push_back(token);
			}
		}

		TemplateParam param;
		if (left.size() == 1)
		{
			param.TypeName = left[0].Text;
			param.DefaultType = defaultValue;
		}
		else if (left.size() == 2)
		{
			param.ValueType = left[0].Text;
			param.TypeName = left[1].Text;
			param.DefaultValue = defaultValue;
			param.IsValue = true;
		}
		else
		{
			fail("invalid template parameter");
		}
		_pendingTemplates.push_back(std::move(param));

		if (i < tokens.size() && tokens[i].Text == ",")
		{
			++i;
		}
	}

	if (i >= tokens.size() || tokens[i].Text != ">")
	{
		fail("expected '>' in template");
	}
}

void xenon::Assembler::parseParent(const std::vector<Token>& tokens)
{
	if (tokens.size() != 3)
	{
		fail("expected '.parent <access> <type>'");
	}

	Parent parent;
	if (!parseAccess(tokens[1].Text, parent.AccessModifier))
	{
		fail("expected access modifier in '.parent'");
	}
	parent.TypeName = tokens[2].Text;
	_pendingParents.push_back(std::move(parent));
}

void xenon::Assembler::parseInstruction(const std::vector<Token>& tokens)
{
	if (_currentFunction == nullptr && _currentMethod == nullptr)
	{
		fail("instruction outside of a function or method body");
	}

	Instruction op;
	if (!instructionFromMnemonic(tokens[0].Text, op))
	{
		fail("unknown instruction: " + tokens[0].Text);
	}

	XenonBinary& body = (_currentFunction != nullptr) ? _currentFunction->Body : _currentMethod->Body;
	body.write(op, static_cast<int>(_line));

	const OperandKind kind = operandKind(op);
	const std::size_t size = operandSize(op);
	if (size == 0)
	{
		if (tokens.size() > 1)
		{
			fail("instruction '" + tokens[0].Text + "' takes no operand");
		}
		return;
	}

	if (tokens.size() < 2)
	{
		fail("instruction '" + tokens[0].Text + "' requires an operand");
	}

	std::int64_t value = 0;
	if (op == Instruction::Const)
	{
		const std::size_t index = body.addConstant(parseValue(tokens[1]));
		if (index > XenonBinary::MaxOperand)
		{
			fail("constant pool is full");
		}
		value = static_cast<std::int64_t>(index);
	}
	else if (kind == OperandKind::String && !tokens[1].IsNumber)
	{
		const std::size_t index = body.addString(tokens[1].Text);
		if (index > XenonBinary::MaxOperand)
		{
			fail("string pool is full");
		}
		value = static_cast<std::int64_t>(index);
	}
	else
	{
		if (!tokens[1].IsNumber)
		{
			fail("expected a numeric operand for '" + tokens[0].Text + "'");
		}
		const auto result = std::from_chars(tokens[1].Text.data(),
											tokens[1].Text.data() + tokens[1].Text.size(), value);
		if (result.ec != std::errc() || result.ptr != tokens[1].Text.data() + tokens[1].Text.size())
		{
			fail("invalid operand: " + tokens[1].Text);
		}
	}

	for (std::size_t k = 0; k < size; ++k)
	{
		body.write(static_cast<std::uint8_t>((value >> (8 * k)) & 0xff), static_cast<int>(_line));
	}
}
