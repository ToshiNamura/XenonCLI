#include <xenon/compiler/compiler.hpp>

#include <string>

#include <xenon/compiler/codegen.hpp>
#include <xenon/compiler/lexer.hpp>
#include <xenon/compiler/module_parser.hpp>
#include <xenon/compiler/parser.hpp>

bool xenon::Compiler::compile(std::string_view source, xenon::Module& out, const std::vector<xenon::Module>& hints)
{
	_error.clear();
	_header = xenon::ModuleHeader{};
	out     = xenon::Module{};

	xenon::ModuleParser headerParser;
	if (headerParser.parse(source, _header) != xenon::ModuleParseResult::Ok)
	{
		_error = headerParser.lastError();
		return false;
	}

	out.Name    = _header.Name;
	out.Imports = _header.Imports;

	xenon::Lexer  lexer(source.substr(_header.BodyOffset));
	xenon::Parser parser(lexer);

	xenon::Program program;
	if (!parser.parse(program))
	{
		_error = parser.lastError();
		return false;
	}

	xenon::Module mergedHints;
	for (const auto& hint : hints)
	{
		mergedHints.Functions.insert(mergedHints.Functions.end(), hint.Functions.begin(),
									 hint.Functions.end());
		mergedHints.Types.insert(mergedHints.Types.end(), hint.Types.begin(), hint.Types.end());
		mergedHints.Globals.insert(mergedHints.Globals.end(), hint.Globals.begin(), hint.Globals.end());
	}

	xenon::CodeGenerator codegen(program, mergedHints);
	if (!codegen.generate(out))
	{
		_error = codegen.lastError();
		return false;
	}

	return true;
}

bool xenon::Compiler::compileHints(std::string_view source, xenon::Module& out)
{
	_error.clear();
	_header = xenon::ModuleHeader{};
	out     = xenon::Module{};

	xenon::Lexer  lexer(source);
	xenon::Parser parser(lexer);

	xenon::Program program;
	if (!parser.parse(program))
	{
		_error = parser.lastError();
		return false;
	}

	xenon::CodeGenerator codegen(program, xenon::Module{}, true);
	if (!codegen.generate(out))
	{
		_error = codegen.lastError();
		return false;
	}

	return true;
}
