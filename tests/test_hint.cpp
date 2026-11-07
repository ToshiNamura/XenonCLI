#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "xenon/compiler/compiler.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/error.hpp"
#include "xenon/runtime/runtime.hpp"

namespace
{

int g_failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++g_failures;
    }
}

void testHintCompile()
{
    using namespace xenon;

    const std::string hint = R"(
double sqrt(double x);
double pow(double base, double exp);
struct Point { double x; double y; };
enum Color { Red, Green, Blue };
)";

    Module module;
    Compiler compiler;
    check(compiler.compileHints(hint, module), "compileHints failed");
    check(module.Symbols.size() == 4, "expected 4 hint symbols");

    const Symbol* sqrtSym = module.find("sqrt");
    check(sqrtSym != nullptr && sqrtSym->Kind == SymbolKind::Function && !sqrtSym->Defined,
          "sqrt should be an undefined function");
    check(sqrtSym != nullptr && sqrtSym->Arity == 1, "sqrt arity should be 1");

    const Symbol* point = module.find("Point");
    check(point != nullptr && point->Kind == SymbolKind::Struct && !point->Defined,
          "Point should be an undefined struct");
}

void testHintResolution()
{
    using namespace xenon;

    const std::string hint = "double sqrt(double x);\n";

    const std::string source = R"(
export module UseHint;

double main()
{
    return sqrt(9.0);
}
)";

    Module hintsModule;
    Compiler hintCompiler;
    check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

    Module module;
    Compiler compiler;
    check(compiler.compile(source, module, {hintsModule}), "compile with hints failed");

    Runtime runtime;
    runtime.addModule(module);
    runtime.registerNative("sqrt",
                           [](const std::vector<Value>& args) -> Value
                           {
                               return Value{std::sqrt(std::get<double>(args[0]))};
                           });

    check(runtime.run("main") == InterpretResult::Ok, "run('main') failed");
    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 3.0) < 1e-12, "sqrt(9) result wrong");
}

void testUnboundExternalThrows()
{
    using namespace xenon;

    const std::string hint = "double missing(double x);\n";
    const std::string source = R"(
export module UseMissing;
double main() { return missing(1.0); }
)";

    Module hintsModule;
    Compiler hintCompiler;
    check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

    Module module;
    Compiler compiler;
    check(compiler.compile(source, module, {hintsModule}), "compile failed");

    Runtime runtime;
    runtime.addModule(module);

    bool threw = false;
    try
    {
        (void)runtime.run("main");
    }
    catch (const RuntimeError&)
    {
        threw = true;
    }
    check(threw, "unbound external should throw RuntimeError");
}

} // namespace

int main()
{
    try
    {
        testHintCompile();
        testHintResolution();
        testUnboundExternalThrows();
    }
    catch (const std::exception& error)
    {
        std::cerr << "EXCEPTION: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    if (g_failures == 0)
    {
        std::cout << "All tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
