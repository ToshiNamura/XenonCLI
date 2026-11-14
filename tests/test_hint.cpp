#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

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

void testHintCompile()
{
	const std::string hint = R"(
double sqrt(double x);
double pow(double base, double exp);
struct Point { double x; double y; };
enum Color { Red, Green, Blue };
)";

	xenon::Module   module;
	xenon::Compiler compiler;
	check(compiler.compileHints(hint, module), "compileHints failed");
	check(module.Functions.size() == 2, "expected 2 hint functions");
	check(module.Types.size() == 2, "expected 2 hint types");

	const xenon::Function* sqrtFunction = module.findFunction("sqrt");
	check(sqrtFunction != nullptr && !sqrtFunction->Defined, "sqrt should be an undefined function");
	check(sqrtFunction != nullptr && sqrtFunction->Arity == 1, "sqrt arity should be 1");

	const xenon::Type* point = module.findType("Point");
	check(point != nullptr && point->Kind == xenon::TypeKind::Struct && !point->Defined,
		  "Point should be an undefined struct");
	check(point != nullptr && point->Fields.size() == 2, "Point should have two fields");
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

	xenon::Module   hintsModule;
	xenon::Compiler hintCompiler;
	check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

	xenon::Module   module;
	xenon::Compiler compiler;
	check(compiler.compile(source, module, {hintsModule}), "compile with hints failed");
	check(module.findFunction("sqrt") != nullptr, "hint declaration was not merged into the module");

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
	const std::string hint   = "double missing(double x);\n";
	const std::string source = R"(
export module UseMissing;
double main() { return missing(1.0); }
)";

	xenon::Module   hintsModule;
	xenon::Compiler hintCompiler;
	check(hintCompiler.compileHints(hint, hintsModule), "compileHints failed");

	xenon::Module   module;
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
