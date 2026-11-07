#include "xenon/compiler/compiler.hpp"

#include <string>

#include "xenon/compiler/codegen.hpp"
#include "xenon/compiler/lexer.hpp"
#include "xenon/compiler/module_parser.hpp"
#include "xenon/compiler/parser.hpp"

namespace xenon
{

bool Compiler::compile(std::string_view source, Module& out, const std::vector<Module>& hints)
{
    _error.clear();
    _header = ModuleHeader{};
    out = Module{};

    ModuleParser headerParser;
    if (headerParser.parse(source, _header) != ModuleParseResult::Ok)
    {
        _error = headerParser.lastError();
        return false;
    }

    out.Name = _header.Name;
    out.Imports = _header.Imports;

    Lexer lexer(source.substr(_header.BodyOffset));
    Parser parser(lexer);

    Program program;
    if (!parser.parse(program))
    {
        _error = parser.lastError();
        return false;
    }

    Module mergedHints;
    for (const auto& hint : hints)
    {
        mergedHints.Symbols.insert(mergedHints.Symbols.end(), hint.Symbols.begin(),
                                   hint.Symbols.end());
    }

    CodeGenerator codegen(program, mergedHints);
    if (!codegen.generate(out))
    {
        _error = codegen.lastError();
        return false;
    }

    return true;
}

bool Compiler::compileHints(std::string_view source, Module& out)
{
    _error.clear();
    _header = ModuleHeader{};
    out = Module{};

    Lexer lexer(source);
    Parser parser(lexer);

    Program program;
    if (!parser.parse(program))
    {
        _error = parser.lastError();
        return false;
    }

    CodeGenerator codegen(program, Module{}, true);
    if (!codegen.generate(out))
    {
        _error = codegen.lastError();
        return false;
    }

    return true;
}

} // namespace xenon
