#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <variant>

#include "xenon/compiler/compiler.hpp"
#include "xenon/module/module.hpp"
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

xenon::Module compileOrFail(const std::string& source, const std::string& what)
{
    using namespace xenon;

    Module module;
    Compiler compiler;
    check(compiler.compile(source, module), what + " (compile failed): " + compiler.lastError());
    return module;
}

void testFunctionCall()
{
    using namespace xenon;

    const std::string source = R"(
export module Math;

double add(double a, double b)
{
    return a + b;
}

double main()
{
    return add(1.5, 2.5);
}
)";

    const auto module = compileOrFail(source, "function call");

    Runtime runtime;
    runtime.addModule(module);
    check(runtime.run("main") == InterpretResult::Ok, "run('main') failed");

    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 4.0) < 1e-12, "main() result wrong");
}

void testLocalsAndControlFlow()
{
    using namespace xenon;

    const std::string source = R"(
export module Control;

double sum_to(double n)
{
    double total = 0.0;
    double i = 0.0;
    while (i < n)
    {
        i = i + 1.0;
        total = total + i;
    }
    return total;
}

double classify(double x)
{
    if (x < 0.0)
    {
        return -1.0;
    }
    else
    {
        return 1.0;
    }
}
)";

    const auto module = compileOrFail(source, "control flow");

    Runtime runtime;
    runtime.addModule(module);

    runtime.push(10.0);
    check(runtime.run("sum_to") == InterpretResult::Ok, "run('sum_to') failed");
    const auto* total = std::get_if<double>(&runtime.lastValue());
    check(total != nullptr && std::abs(*total - 55.0) < 1e-12, "sum_to(10) wrong");

    runtime.push(-5.0);
    check(runtime.run("classify") == InterpretResult::Ok, "run('classify') failed");
    const auto* cls = std::get_if<double>(&runtime.lastValue());
    check(cls != nullptr && std::abs(*cls + 1.0) < 1e-12, "classify(-5) wrong");
}

void testBooleanLogic()
{
    using namespace xenon;

    const std::string source = R"(
export module Logic;

bool test(double x)
{
    if (x > 0.0 && x < 10.0)
    {
        return true;
    }
    return false;
}
)";

    const auto module = compileOrFail(source, "boolean logic");

    Runtime runtime;
    runtime.addModule(module);

    runtime.push(5.0);
    check(runtime.run("test") == InterpretResult::Ok, "run('test') failed");
    check(std::get_if<bool>(&runtime.lastValue()) != nullptr && std::get<bool>(runtime.lastValue()),
          "test(5) should be true");

    runtime.push(50.0);
    check(runtime.run("test") == InterpretResult::Ok, "run('test') failed");
    check(std::get_if<bool>(&runtime.lastValue()) != nullptr && !std::get<bool>(runtime.lastValue()),
          "test(50) should be false");
}

void testStringLiteral()
{
    using namespace xenon;

    const std::string source = R"(
export module Strings;

string greet()
{
    string s = "hello";
    return s;
}
)";

    const auto module = compileOrFail(source, "string literal");

    Runtime runtime;
    runtime.addModule(module);
    check(runtime.run("greet") == InterpretResult::Ok, "run('greet') failed");

    const auto* ref = std::get_if<Ref>(&runtime.lastValue());
    check(ref != nullptr, "greet() result is not a reference");
    if (ref != nullptr)
    {
        check(runtime.stringValue(*ref) == "hello", "greet() string value wrong");
    }
}

void testParseError()
{
    using namespace xenon;

    const std::string source = R"(
export module Broken;

double broken()
{
    return 1 + ;
}
)";

    Module module;
    Compiler compiler;
    check(!compiler.compile(source, module), "broken syntax should fail to compile");
}

} // namespace

int main()
{
    try
    {
        testFunctionCall();
        testLocalsAndControlFlow();
        testBooleanLogic();
        testStringLiteral();
        testParseError();
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
