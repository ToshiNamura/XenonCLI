#include "xenon/compiler/compiler.hpp"

#include "xenon/compiler/codegen.hpp"
#include "xenon/compiler/lexer.hpp"
#include "xenon/compiler/module_parser.hpp"
#include "xenon/compiler/parser.hpp"

namespace xenon
{

bool Compiler::compile(std::string_view source, Module& out)
{
    error_.clear();
    header_ = ModuleHeader{};
    out = Module{};

    ModuleParser header_parser;
    if (header_parser.parse(source, header_) != ModuleParseResult::Ok)
    {
        error_ = header_parser.last_error();
        return false;
    }

    out.name = header_.name;
    out.imports = header_.imports;

    Lexer lexer(source.substr(header_.body_offset));
    Parser parser(lexer);

    Program program;
    if (!parser.parse(program))
    {
        error_ = parser.last_error();
        return false;
    }

    CodeGenerator codegen(program);
    if (!codegen.generate(out))
    {
        error_ = codegen.last_error();
        return false;
    }

    return true;
}

} // namespace xenon
