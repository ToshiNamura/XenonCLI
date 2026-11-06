#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "xenon/bytecode/opcode.hpp"
#include "xenon/compiler/compiler.hpp"
#include "xenon/compiler/module.hpp"
#include "xenon/compiler/module_parser.hpp"
#include "xenon/module/module.hpp"
#include "xenon/module/module_file.hpp"
#include "xenon/runtime/env.hpp"

namespace
{

/// Builds a demo module with a single `add` function computing `1.2 + 3.4`.
xenon::Module make_demo_module()
{
    using namespace xenon;

    Module module;
    module.name = "Demo";

    Symbol add;
    add.name = "add";
    add.kind = SymbolKind::Function;
    add.signature = "double add(double, double)";

    const auto a = add.body.add_constant(1.2);
    const auto b = add.body.add_constant(3.4);
    add.body.write(OpCode::Constant, 1);
    add.body.write(static_cast<std::uint8_t>(a), 1);
    add.body.write(OpCode::Constant, 1);
    add.body.write(static_cast<std::uint8_t>(b), 1);
    add.body.write(OpCode::Add, 1);
    add.body.write(OpCode::Return, 1);

    module.symbols.push_back(std::move(add));
    return module;
}

/// Prints the symbols exported by a module.
void print_symbols(const xenon::Module& module)
{
    for (const auto& symbol : module.symbols)
    {
        std::cout << "  " << symbol.name << " : " << symbol.signature << '\n';
    }
}

/// Loads `module` into the VM, lists its symbols and runs the entry function.
int run_module(const xenon::Module& module)
{
    using namespace xenon;

    std::cout << "module: " << module.name << '\n';
    for (const auto& import : module.imports)
    {
        std::cout << "  import: " << import << '\n';
    }
    print_symbols(module);

    Env env;
    env.load(module);

    std::string entry;
    if (module.find("main") != nullptr)
    {
        entry = "main";
    }
    else if (!module.symbols.empty() && module.symbols.front().kind == SymbolKind::Function)
    {
        entry = module.symbols.front().name;
    }

    if (entry.empty())
    {
        std::cout << "no runnable function\n";
        return EXIT_SUCCESS;
    }

    if (env.run(entry) != InterpretResult::Ok)
    {
        return 70; // EX_SOFTWARE
    }
    std::cout << "result: " << env.last_value() << '\n';
    return EXIT_SUCCESS;
}

/// Demonstrates the runtime environment API (heap, objects, operand stack).
int run_env_demo()
{
    using namespace xenon;

    Env env;

    const auto point = env.new_object("Point");
    env.set_field(point, "x", 1.5);
    env.set_field(point, "y", 2.5);

    const auto numbers = env.new_array(3);
    env.set_element(numbers, 0, 10.0);
    env.set_element(numbers, 1, 20.0);
    env.set_element(numbers, 2, 30.0);

    const auto greeting = env.new_string("hello");

    std::cout << "heap: " << env.heap().size() << " object(s)\n";
    std::cout << "  " << env.class_name(point) << " x=" << env.get_field(point, "x")
              << " y=" << env.get_field(point, "y") << '\n';
    std::cout << "  array length=" << env.array_length(numbers)
              << " [0]=" << env.get_element(numbers, 0) << '\n';
    std::cout << "  string=\"" << env.string_value(greeting) << "\"\n";

    env.push(1.2);
    env.push(env.get_field(point, "x"));
    env.push(env.get_element(numbers, 2));
    std::cout << "stack size=" << env.stack_size() << " top=" << env.peek() << '\n';
    std::cout << "pop=" << env.pop() << ", pop=" << env.pop() << ", pop=" << env.pop() << '\n';

    return EXIT_SUCCESS;
}

/// Runs the built-in demo: a packed module round-tripped through the `.module` format.
int run_demo()
{
    using namespace xenon;

    const auto module = make_demo_module();

    std::stringstream buffer;
    write_module_file(buffer, module);

    Module loaded;
    read_module_file(buffer, loaded);

    std::cout << "== packed into .module bytes and reloaded ==\n";
    const int rc = run_module(loaded);

    std::cout << "== environment API demo ==\n";
    run_env_demo();

    return rc;
}

/// Loads and runs a compiled `.module` file.
int run_module_file(const std::string& path)
{
    using namespace xenon;

    Module module;
    try
    {
        read_module_file(path, module);
    }
    catch (const ModuleFileError& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 65; // EX_DATAERR
    }
    return run_module(module);
}

/// Compiles a `.ixx` source file into a module and runs it.
int run_source_file(const std::string& path)
{
    using namespace xenon;

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        std::cerr << "error: cannot open '" << path << "'\n";
        return 66; // EX_NOINPUT
    }

    const std::string source{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};

    ModuleHeader header;
    ModuleParser parser;
    if (parser.parse(source, header) != ModuleParseResult::Ok)
    {
        std::cerr << "error: " << parser.last_error() << '\n';
        return 65; // EX_DATAERR
    }

    std::cout << "module: " << header.name << '\n';
    for (const auto& dependency : header.imports)
    {
        std::cout << "  import: " << dependency << '\n';
    }
    for (const auto& include : header.includes)
    {
        std::cout << "  include: " << include << '\n';
    }

    Module module;
    Compiler compiler;
    if (!compiler.compile(source, module))
    {
        std::cerr << "error: " << compiler.last_error() << '\n';
        return 65; // EX_DATAERR
    }

    return run_module(module);
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        if (argc == 1)
        {
            return run_demo();
        }
        if (argc == 2)
        {
            const std::string path = argv[1];
            if (path.ends_with(".module"))
            {
                return run_module_file(path);
            }
            return run_source_file(path);
        }

        std::cerr << "usage: xenon [script.ixx | file.module]\n";
        return 64; // EX_USAGE
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
