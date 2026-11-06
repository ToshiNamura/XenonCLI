#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <variant>

#include "xenon/compiler/compiler.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/env.hpp"

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

xenon::Module compile_or_fail(const std::string& source, const std::string& what)
{
    using namespace xenon;

    Module module;
    Compiler compiler;
    check(compiler.compile(source, module), what + " (compile failed): " + compiler.last_error());
    return module;
}

void test_function_call()
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

    const auto module = compile_or_fail(source, "function call");

    Env env;
    env.load(module);
    check(env.run("main") == InterpretResult::Ok, "run('main') failed");

    const auto* number = std::get_if<double>(&env.last_value());
    check(number != nullptr && std::abs(*number - 4.0) < 1e-12, "main() result wrong");
}

void test_locals_and_control_flow()
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

    const auto module = compile_or_fail(source, "control flow");

    Env env;
    env.load(module);

    env.push(10.0);
    check(env.run("sum_to") == InterpretResult::Ok, "run('sum_to') failed");
    const auto* total = std::get_if<double>(&env.last_value());
    check(total != nullptr && std::abs(*total - 55.0) < 1e-12, "sum_to(10) wrong");

    env.push(-5.0);
    check(env.run("classify") == InterpretResult::Ok, "run('classify') failed");
    const auto* cls = std::get_if<double>(&env.last_value());
    check(cls != nullptr && std::abs(*cls + 1.0) < 1e-12, "classify(-5) wrong");
}

void test_boolean_logic()
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

    const auto module = compile_or_fail(source, "boolean logic");

    Env env;
    env.load(module);

    env.push(5.0);
    check(env.run("test") == InterpretResult::Ok, "run('test') failed");
    check(std::get_if<bool>(&env.last_value()) != nullptr && std::get<bool>(env.last_value()),
          "test(5) should be true");

    env.push(50.0);
    check(env.run("test") == InterpretResult::Ok, "run('test') failed");
    check(std::get_if<bool>(&env.last_value()) != nullptr && !std::get<bool>(env.last_value()),
          "test(50) should be false");
}

void test_parse_error()
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
        test_function_call();
        test_locals_and_control_flow();
        test_boolean_logic();
        test_parse_error();
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
