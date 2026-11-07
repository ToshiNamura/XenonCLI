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

void testControlFlow()
{
    using namespace xenon;

    const std::string source = R"(
export module Flow;

double for_sum()
{
    double total = 0.0;
    for (double i = 1.0; i <= 10.0; i = i + 1.0)
    {
        total = total + i;
    }
    return total;
}

double do_while()
{
    double n = 0.0;
    do
    {
        n = n + 1.0;
    }
    while (n < 5.0);
    return n;
}

double with_continue()
{
    double total = 0.0;
    for (double i = 1.0; i <= 5.0; i = i + 1.0)
    {
        if (i == 3.0)
        {
            continue;
        }
        total = total + i;
    }
    return total;
}

double with_break()
{
    double total = 0.0;
    for (double i = 1.0; true; i = i + 1.0)
    {
        if (i > 5.0)
        {
            break;
        }
        total = total + i;
    }
    return total;
}

double switch_test(double x)
{
    switch (x)
    {
        case 1.0:
            return 10.0;
        case 2.0:
            return 20.0;
        default:
            return 99.0;
    }
}

double main()
{
    return for_sum() + do_while() + with_continue() + with_break() + switch_test(2.0);
}
)";

    const auto module = compileOrFail(source, "control flow statements");

    Runtime runtime;
    runtime.addModule(module);
    check(runtime.run("main") == InterpretResult::Ok, "run('main') failed");

    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 107.0) < 1e-12, "main() result wrong");
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

void testClassesAndFields()
{
    using namespace xenon;

    const std::string source = R"(
export module Shapes;

struct Point
{
    double x;
    double y;
};

struct Rect
{
    double w;
    double h;

    double area(Rect r)
    {
        return r.w * r.h;
    }
};

double main()
{
    Point p = new Point();
    p.x = 3.0;
    p.y = 4.0;

    Rect r = new Rect();
    r.w = 2.0;
    r.h = 5.0;

    return p.x + p.y + Rect::area(r);
}
)";

    const auto module = compileOrFail(source, "classes and fields");

    Runtime runtime;
    runtime.addModule(module);
    check(runtime.run("main") == InterpretResult::Ok, "run('main') failed");

    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 17.0) < 1e-12, "main() result wrong");
}

void testTemplates()
{
    using namespace xenon;

    const std::string source = R"(
export module Templates;

template <typename T>
struct Box
{
    T value;
};

template <typename T>
T id(T x)
{
    return x;
}

template <typename T>
struct Pair
{
    T first;
    T second;

    T sum(Pair p)
    {
        return p.first + p.second;
    }
};

double main()
{
    Box<double> b = new Box<double>();
    b.value = id<double>(3.0);

    Pair<double> p = new Pair<double>();
    p.first = 4.0;
    p.second = 5.0;

    return b.value + Pair<double>::sum(p);
}
)";

    const auto module = compileOrFail(source, "templates");

    Runtime runtime;
    runtime.addModule(module);
    check(runtime.run("main") == InterpretResult::Ok, "run('main') failed");

    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 12.0) < 1e-12, "main() result wrong");
}

void testGlobals()
{
    using namespace xenon;

    const std::string source = R"(
export module Globals;

int counter = 0;
const double pi = 3.0;

double apply(double x)
{
    counter = counter + 1;
    return x * pi;
}

double main()
{
    double a = apply(2.0);
    double b = apply(4.0);
    return a + b + counter;
}
)";

    const auto module = compileOrFail(source, "globals");

    Runtime runtime;
    runtime.addModule(module);
    check(runtime.run("main") == InterpretResult::Ok, "run('main') failed");

    const auto* number = std::get_if<double>(&runtime.lastValue());
    check(number != nullptr && std::abs(*number - 20.0) < 1e-12, "main() result wrong");
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
        testControlFlow();
        testBooleanLogic();
        testStringLiteral();
        testClassesAndFields();
        testTemplates();
        testGlobals();
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
