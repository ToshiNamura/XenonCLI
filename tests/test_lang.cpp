#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <variant>

#include <xenon/asm/model.hpp>
#include <xenon/compiler/compiler.hpp>
#include <xenon/runtime/error.hpp>
#include <xenon/runtime/runtime.hpp>

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
	xenon::Module   module;
	xenon::Compiler compiler;
	const bool      compiled = compiler.compile(source, module);
	check(compiled, what + " (compile failed): " + compiler.lastError());
	return module;
}

void testFunctionCall()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 4.0) < 1e-12, "main() result wrong");
}

void testLocalsAndControlFlow()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);

	runtime.push(10.0);
	check(runtime.run("sum_to") == xenon::InterpretResult::Ok, "run('sum_to') failed");
	const auto* total = std::get_if<double>(&runtime.lastValue());
	check(total != nullptr && std::abs(*total - 55.0) < 1e-12, "sum_to(10) wrong");

	runtime.push(-5.0);
	check(runtime.run("classify") == xenon::InterpretResult::Ok, "run('classify') failed");
	const auto* cls = std::get_if<double>(&runtime.lastValue());
	check(cls != nullptr && std::abs(*cls + 1.0) < 1e-12, "classify(-5) wrong");
}

void testControlFlow()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 107.0) < 1e-12, "main() result wrong");
}

void testBooleanLogic()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);

	runtime.push(5.0);
	check(runtime.run("test") == xenon::InterpretResult::Ok, "run('test') failed");
	check(std::get_if<bool>(&runtime.lastValue()) != nullptr && std::get<bool>(runtime.lastValue()),
		  "test(5) should be true");

	runtime.push(50.0);
	check(runtime.run("test") == xenon::InterpretResult::Ok, "run('test') failed");
	check(std::get_if<bool>(&runtime.lastValue()) != nullptr && !std::get<bool>(runtime.lastValue()),
		  "test(50) should be false");
}

void testStringLiteral()
{
	const std::string source = R"(
export module Strings;

string greet()
{
	string s = "hello";
	return s;
}
)";

	const auto module = compileOrFail(source, "string literal");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("greet") == xenon::InterpretResult::Ok, "run('greet') failed");

	const auto* ref = std::get_if<xenon::Ref>(&runtime.lastValue());
	check(ref != nullptr, "greet() result is not a reference");
	if (ref != nullptr)
	{
		check(runtime.stringValue(*ref) == "hello", "greet() string value wrong");
	}
}

void testClassesAndFields()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 17.0) < 1e-12, "main() result wrong");
}

void testTemplates()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 12.0) < 1e-12, "main() result wrong");
}

void testGlobals()
{
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

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 20.0) < 1e-12, "main() result wrong");
}

void testCrossModuleCall()
{
	const std::string library = R"(
export module Lib;

export double add(double a, double b)
{
	return a + b;
}
)";

	const auto libraryModule = compileOrFail(library, "library");

	const std::string hint = "double add(double a, double b);\n";
	xenon::Module     hintsModule;
	xenon::Compiler   hintCompiler;
	check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

	const std::string application = R"(
export module App;

double main()
{
	return add(2.0, 3.0);
}
)";

	xenon::Module   applicationModule;
	xenon::Compiler compiler;
	check(compiler.compile(application, applicationModule, {hintsModule}),
		  "compile application failed: " + compiler.lastError());

	xenon::Runtime runtime;
	runtime.addModule(libraryModule);
	runtime.addModule(applicationModule);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 5.0) < 1e-12, "cross-module call result wrong");
}

void testArithmeticOperators()
{
	const std::string source = R"(
export module Arithmetic;

double compound()
{
	double x = 10.0;
	x += 5.0;
	x -= 3.0;
	x *= 2.0;
	x /= 4.0;
	x %= 4.0;
	return x;
}

double modulo(double a, double b)
{
	return a % b;
}

double main()
{
	return compound() + modulo(7.0, 4.0);
}
)";

	const auto module = compileOrFail(source, "arithmetic operators");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 5.0) < 1e-12, "compound assignment/result wrong");
}

void testBitwiseOperators()
{
	const std::string source = R"(
export module Bitwise;

double binary()
{
	return (5.0 & 3.0) + (5.0 | 2.0) + (5.0 ^ 1.0) + ~5.0;
}

double shifts()
{
	return (1.0 << 4.0) + (256.0 >> 4.0);
}

double compound()
{
	double x = 6.0;
	x &= 3.0;
	x |= 8.0;
	x ^= 3.0;
	x <<= 2.0;
	x >>= 3.0;
	return x;
}

double main()
{
	return binary() + shifts() + compound();
}
)";

	const auto module = compileOrFail(source, "bitwise operators");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 42.0) < 1e-12, "bitwise operator result wrong");
}

void testIncrementDecrement()
{
	const std::string source = R"(
export module Increment;

int counter = 0;

struct Box
{
	double value;
};

double locals()
{
	double i = 0.0;
	double a = i++;
	double b = ++i;
	double c = i--;
	double d = --i;
	return a + b + c + d + i;
}

double globals()
{
	counter++;
	double a = counter;
	++counter;
	double b = counter;
	counter--;
	double c = counter;
	--counter;
	double d = counter;
	return a + b + c + d + counter;
}

double field()
{
	Box box = new Box();
	box.value = 5.0;
	double a = box.value++;
	double b = ++box.value;
	box.value += 3.0;
	return a + b + box.value;
}

double main()
{
	return locals() + globals() + field();
}
)";

	const auto module = compileOrFail(source, "increment/decrement");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 30.0) < 1e-12, "increment/decrement result wrong");
}

void testTernary()
{
	const std::string source = R"(
export module Ternary;

double sign(double x)
{
	return x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0);
}

double main()
{
	return sign(5.0) + sign(-5.0) + sign(0.0);
}
)";

	const auto module = compileOrFail(source, "ternary operator");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number) < 1e-12, "ternary result wrong");
}

void testConstantExpressions()
{
	const std::string source = R"(
export module ConstExpr;

int mask = 1 << 4;
const double half = 10.0 / 4.0;

double main()
{
	return mask + half;
}
)";

	const auto module = compileOrFail(source, "constant expressions");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 18.5) < 1e-12, "constant expression result wrong");
}

void testNestedTemplateArguments()
{
	const std::string source = R"(
export module Nested;

template <typename T>
struct Box
{
	T value;
};

template <typename T>
struct Pair
{
	T first;
	T second;
};

double main()
{
	Pair<Box<double>> pair = new Pair<Box<double>>();
	return 1.0;
}
)";

	xenon::Module   module;
	xenon::Compiler compiler;
	check(compiler.compile(source, module),
		  "nested template arguments failed: " + compiler.lastError());
}

void testIncrementRequiresTarget()
{
	const std::string source = R"(
export module BadIncrement;

double main()
{
	return (1.0 + 2.0)++;
}
)";

	xenon::Module   module;
	xenon::Compiler compiler;
	check(!compiler.compile(source, module), "incrementing an rvalue should fail to compile");
}

bool compileFails(const std::string& source, const std::string& expectedFragment)
{
	xenon::Module   module;
	xenon::Compiler compiler;
	if (compiler.compile(source, module))
	{
		return false;
	}
	return compiler.lastError().find(expectedFragment) != std::string::npos;
}

void testTypeChecking()
{
	check(compileFails(R"(
export module BadReturn;
double broken()
{
	return "text";
}
)", "returns double"),
		  "returning a string from a double function should fail");

	check(compileFails(R"(
export module BadInit;
double broken()
{
	double x = "text";
	return x;
}
)", "cannot initialize 'x'"),
		  "initializing a double with a string should fail");

	check(compileFails(R"(
export module BadAssign;
double broken()
{
	string s = "text";
	s = 1.0;
	return 0.0;
}
)", "cannot assign"),
		  "assigning a number to a string should fail");

	check(compileFails(R"(
export module BadArgument;

double scale(double value)
{
	return value * 2.0;
}

double main()
{
	return scale("text");
}
)", "argument 1 of 'scale'"),
		  "passing a string to a double parameter should fail");

	check(compileFails(R"(
export module BadArity;

double add(double a, double b)
{
	return a + b;
}

double main()
{
	return add(1.0);
}
)", "expects 2 argument(s)"),
		  "calling a function with too few arguments should fail");

	check(compileFails(R"(
export module BadTernary;

double main()
{
	return 1.0 > 0.0 ? "text" : 2.0;
}
)", "incompatible types"),
		  "ternary branches with incompatible types should fail");
}

void testConstChecking()
{
	check(compileFails(R"(
export module ConstLocal;

double main()
{
	const double pi = 3.0;
	pi = 4.0;
	return pi;
}
)", "const"),
		  "assigning to a const local should fail");

	check(compileFails(R"(
export module ConstGlobal;

const double pi = 3.0;

double main()
{
	pi = 4.0;
	return pi;
}
)", "const"),
		  "assigning to a const global should fail");

	check(compileFails(R"(
export module ConstField;

struct Circle
{
	const double radius;
};

double main()
{
	Circle c = new Circle();
	c.radius = 2.0;
	return c.radius;
}
)", "const"),
		  "assigning to a const field should fail");

	check(compileFails(R"(
export module ConstParam;

double scale(const double value)
{
	value = 2.0;
	return value;
}
)", "const"),
		  "assigning to a const parameter should fail");

	check(compileFails(R"(
export module ConstIncrement;

double main()
{
	const double value = 1.0;
	value++;
	return value;
}
)", "const"),
		  "incrementing a const local should fail");

	check(compileFails(R"(
export module ConstNoInit;

double main()
{
	const double value;
	return 0.0;
}
)", "initializer"),
		  "a const local without an initializer should fail");

	const std::string valid = R"(
export module ConstOk;

const double pi = 3.0;

struct Circle
{
	const double radius;
};

double area(const double radius)
{
	Circle c = new Circle();
	double scale = const_scale(radius);
	return pi * scale * scale;
}

double const_scale(const double value)
{
	return value;
}

double main()
{
	return area(2.0) + pi;
}
)";

	xenon::Module   module;
	xenon::Compiler compiler;
	check(compiler.compile(valid, module), "valid const usage failed: " + compiler.lastError());

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");
	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 15.0) < 1e-12, "const usage result wrong");
}

void testEnumValues()
{
	const std::string source = R"(
export module Enums;

enum Color
{
	Red,
	Green,
	Blue,
};

Color current = Color::Green;

double pick(Color color)
{
	switch (color)
	{
		case Color::Red:
			return 10.0;
		case Color::Green:
			return 20.0;
		default:
			return 30.0;
	}
}

double main()
{
	Color favorite = Color::Blue;
	return pick(current) + pick(favorite) + Red;
}
)";

	const auto module = compileOrFail(source, "enum values");

	xenon::Runtime runtime;
	runtime.addModule(module);
	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");

	// Green = 1 -> 20, Blue = 2 -> 30, Red = 0
	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 50.0) < 1e-12, "enum value result wrong");

	check(compileFails(R"(
export module BadEnum;

enum Color
{
	Red,
	Green,
};

double main()
{
	return Color::Purple;
}
)", "unknown enumerator"),
		  "an unknown enumerator should fail");

	check(compileFails(R"(
export module AmbiguousEnum;

enum Left
{
	Shared,
	OnlyLeft,
};

enum Right
{
	Shared,
	OnlyRight,
};

double main()
{
	return Shared;
}
)", "ambiguous enumerator"),
		  "an unqualified name shared by two enums should fail");
}

void testExportedVisibility()
{
	const std::string library = R"(
export module Secret;

double hidden(double a)
{
	return a;
}

export double visible(double a)
{
	return a;
}
)";

	const auto libraryModule = compileOrFail(library, "visibility library");
	check(!libraryModule.findFunction("hidden")->Exported, "hidden must not be exported");
	check(libraryModule.findFunction("visible")->Exported, "visible must be exported");

	const std::string hint = "double hidden(double a);\ndouble visible(double a);\n";

	xenon::Module   hintsModule;
	xenon::Compiler hintCompiler;
	check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

	const auto caller = [](const std::string& callee)
	{
		return R"(
export module Caller;
double main()
{
	return )" + callee + R"((1.0);
}
)";
	};

	xenon::Module   hiddenModule;
	xenon::Compiler hiddenCompiler;
	check(hiddenCompiler.compile(caller("hidden"), hiddenModule, {hintsModule}),
		  "compile hidden caller failed: " + hiddenCompiler.lastError());

	xenon::Runtime hiddenRuntime;
	hiddenRuntime.addModule(libraryModule);
	hiddenRuntime.addModule(hiddenModule);
	bool threw = false;
	try
	{
		(void)hiddenRuntime.run("main");
	}
	catch (const xenon::RuntimeError&)
	{
		threw = true;
	}
	check(threw, "calling a non-exported function from another module should fail");

	xenon::Module   visibleModule;
	xenon::Compiler visibleCompiler;
	check(visibleCompiler.compile(caller("visible"), visibleModule, {hintsModule}),
		  "compile visible caller failed: " + visibleCompiler.lastError());

	xenon::Runtime visibleRuntime;
	visibleRuntime.addModule(libraryModule);
	visibleRuntime.addModule(visibleModule);
	check(visibleRuntime.run("main") == xenon::InterpretResult::Ok, "exported call failed");
	const auto* number = std::get_if<double>(&visibleRuntime.lastValue());
	check(number != nullptr && std::abs(*number - 1.0) < 1e-12, "exported call result wrong");
}

void testInitializers()
{
	const std::string source = R"(
export module Init;

string greeting = "hello";
double limit = 2.5;

struct Config
{
	const double radius = 2.0;
	double width = 3.0;
	string label = "default";
	bool enabled = true;
	double scaled = 2.0 * 1.5;
};

Config make()
{
	return new Config();
}

string greet()
{
	return greeting;
}

double main()
{
	Config c = make();
	return c.radius + c.width + c.scaled + limit;
}
)";

	const auto module = compileOrFail(source, "initializers");

	xenon::Runtime runtime;
	runtime.addModule(module);

	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");
	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 10.5) < 1e-12, "initializer result wrong");

	check(runtime.run("greet") == xenon::InterpretResult::Ok, "run('greet') failed");
	const auto* text = std::get_if<xenon::Ref>(&runtime.lastValue());
	check(text != nullptr && runtime.stringValue(*text) == "hello", "string global initializer wrong");

	check(runtime.run("make") == xenon::InterpretResult::Ok, "run('make') failed");
	const auto* config = std::get_if<xenon::Ref>(&runtime.lastValue());
	check(config != nullptr, "make() did not return an object");
	if (config != nullptr)
	{
		const xenon::Value widthValue = runtime.getField(*config, "width");
		const auto*       width      = std::get_if<double>(&widthValue);
		check(width != nullptr && std::abs(*width - 3.0) < 1e-12, "numeric field initializer wrong");

		const xenon::Value labelValue = runtime.getField(*config, "label");
		const auto*       label      = std::get_if<xenon::Ref>(&labelValue);
		check(label != nullptr && runtime.stringValue(*label) == "default",
			  "string field initializer wrong");

		const xenon::Value enabledValue = runtime.getField(*config, "enabled");
		const auto*       enabled      = std::get_if<bool>(&enabledValue);
		check(enabled != nullptr && *enabled, "bool field initializer wrong");
	}

	check(compileFails(R"(
export module BadFieldInit;

struct Config
{
	double width = "text";
};

double main()
{
	Config c = new Config();
	return c.width;
}
)", "cannot initialize the field"),
		  "a mismatched field initializer should fail");
}

void testParseError()
{
	const std::string source = R"(
export module Broken;

double broken()
{
	return 1 + ;
}
)";

	xenon::Module module;
	xenon::Compiler compiler;
	check(!compiler.compile(source, module), "broken syntax should fail to compile");
}

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
		testCrossModuleCall();
		testArithmeticOperators();
		testBitwiseOperators();
		testIncrementDecrement();
		testTernary();
		testConstantExpressions();
		testNestedTemplateArguments();
		testIncrementRequiresTarget();
		testTypeChecking();
		testConstChecking();
		testEnumValues();
		testExportedVisibility();
		testInitializers();
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
