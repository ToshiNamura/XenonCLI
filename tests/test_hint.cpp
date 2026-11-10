#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include <xenon/compiler/compiler.hpp>
#include <xenon/module/module.hpp>
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

void testHintCompile()
{
	const std::string hint = R"(
double sqrt(double x);
double pow(double base, double exp);
struct Point { double x; double y; };
enum Color { Red, Green, Blue };
)";

	xenon::Module module;
	xenon::Compiler compiler;
	check(compiler.compileHints(hint, module), "compileHints failed");
	check(module.Symbols.size() == 4, "expected 4 hint symbols");

	const xenon::Symbol* sqrtSym = module.find("sqrt");
	check(sqrtSym != nullptr && sqrtSym->Kind == xenon::SymbolKind::Function && !sqrtSym->Defined,
		  "sqrt should be an undefined function");
	check(sqrtSym != nullptr && sqrtSym->Arity == 1, "sqrt arity should be 1");

	const xenon::Symbol* point = module.find("Point");
	check(point != nullptr && point->Kind == xenon::SymbolKind::Struct && !point->Defined,
		  "Point should be an undefined struct");
}

void testHintResolution()
{
	const std::string hint = "double sqrt(double x);\n";

	const std::string source = R"(
export module UseHint;

double main()
{
	return sqrt(9.0);
}
)";

	xenon::Module hintsModule;
	xenon::Compiler hintCompiler;
	check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

	xenon::Module module;
	xenon::Compiler compiler;
	check(compiler.compile(source, module, {hintsModule}), "compile with hints failed");

	xenon::Runtime runtime;
	runtime.addModule(module);
	runtime.registerNative("sqrt",
						   [](const std::vector<xenon::Value>& args) -> xenon::Value
						   {
							   return xenon::Value{std::sqrt(std::get<double>(args[0]))};
						   });

	check(runtime.run("main") == xenon::InterpretResult::Ok, "run('main') failed");
	const auto* number = std::get_if<double>(&runtime.lastValue());
	check(number != nullptr && std::abs(*number - 3.0) < 1e-12, "sqrt(9) result wrong");
}

void testUnboundExternalThrows()
{
	const std::string hint = "double missing(double x);\n";
	const std::string source = R"(
export module UseMissing;
double main() { return missing(1.0); }
)";

	xenon::Module hintsModule;
	xenon::Compiler hintCompiler;
	check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

	xenon::Module module;
	xenon::Compiler compiler;
	check(compiler.compile(source, module, {hintsModule}), "compile failed");

	xenon::Runtime runtime;
	runtime.addModule(module);

	bool threw = false;
	try
	{
		(void)runtime.run("main");
	}
	catch (const xenon::RuntimeError&)
	{
		threw = true;
	}
	check(threw, "unbound external should throw RuntimeError");
}

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
