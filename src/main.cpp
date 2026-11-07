#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "xenon/core/defines.hpp"
#include "xenon/compiler/compiler.hpp"
#include "xenon/env/env_module.hpp"
#include "xenon/module/module_file.hpp"
#include "xenon/packer/manifest.hpp"
#include "xenon/packer/packer.hpp"
#include "xenon/runtime/error.hpp"
#include "xenon/runtime/runtime.hpp"

namespace
{

constexpr int kExitUsage = 64;    // EX_USAGE
constexpr int kExitData = 65;     // EX_DATAERR
constexpr int kExitNoInput = 66;  // EX_NOINPUT
constexpr int kExitSoftware = 70; // EX_SOFTWARE
constexpr int kExitIo = 74;       // EX_IOERR

void printHelp()
{
    std::cout
        << "Xenon " << XENON_VERSION_STRING
        << " - compile, pack and run Xenon byte-code\n\n"
        << "Usage:\n"
        << "  xenon                       Show version and this help.\n"
        << "  xenon help                  Show this help.\n"
        << "  xenon version               Show the program version.\n"
        << "  xenon compile <sources>... [module]\n"
        << "                              Compile one .ixx and N .hint files into one .module.\n"
        << "  xenon run <xenon> <args>... Run a .xenon package (entry from manifest).\n"
        << "  xenon run <module> <function/method> <args>...\n"
        << "                              Run a function/method from a .module file.\n"
        << "  xenon pack <modules>... <xenon> [manifest]\n"
        << "                              Pack modules into a .xenon archive.\n";
}

void printVersion()
{
    std::cout << "Xenon " << XENON_VERSION_STRING << '\n';
}

bool readTextFile(const std::string& path, std::string& out)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

void registerBuiltInNatives(xenon::Runtime& runtime)
{
    using namespace xenon;

    runtime.registerNative("sqrt",
                           [](const std::vector<Value>& args) -> Value
                           {
                               if (args.size() != 1 || !Runtime::isNumber(args[0]))
                               {
                                   throw RuntimeError("sqrt expects one number");
                               }
                               return Value{std::sqrt(std::get<double>(args[0]))};
                           });

    runtime.registerNative("pow",
                           [](const std::vector<Value>& args) -> Value
                           {
                               if (args.size() != 2 || !Runtime::isNumber(args[0]) ||
                                   !Runtime::isNumber(args[1]))
                               {
                                   throw RuntimeError("pow expects two numbers");
                               }
                               return Value{std::pow(std::get<double>(args[0]),
                                                     std::get<double>(args[1]))};
                           });
}

int compileCommand(const std::vector<std::string>& sources)
{
    using namespace xenon;

    std::vector<std::string> hintPaths;
    std::vector<std::string> sourcePaths;
    std::string outputPath;

    for (const auto& arg : sources)
    {
        if (arg.ends_with(XENON_HINT_EXTENSION))
        {
            hintPaths.push_back(arg);
        }
        else if (arg.ends_with(XENON_SOURCE_EXTENSION))
        {
            sourcePaths.push_back(arg);
        }
        else
        {
            if (!outputPath.empty())
            {
                std::cerr << "error: multiple output module names given\n";
                return kExitUsage;
            }
            outputPath = arg;
        }
    }

    if (sourcePaths.size() != 1)
    {
        std::cerr << "error: exactly one .ixx source is required (got " << sourcePaths.size()
                  << ")\n";
        return kExitUsage;
    }

    std::vector<Module> hints;
    for (const auto& path : hintPaths)
    {
        std::string text;
        if (!readTextFile(path, text))
        {
            std::cerr << "error: cannot open '" << path << "'\n";
            return kExitNoInput;
        }

        Compiler compiler;
        Module module;
        if (!compiler.compileHints(text, module))
        {
            std::cerr << "error: " << compiler.lastError() << '\n';
            return kExitData;
        }
        hints.push_back(std::move(module));
        std::cout << "hint: " << path << " (" << hints.back().Symbols.size() << " symbols)\n";
    }

    const std::string& sourcePath = sourcePaths.front();

    std::string text;
    if (!readTextFile(sourcePath, text))
    {
        std::cerr << "error: cannot open '" << sourcePath << "'\n";
        return kExitNoInput;
    }

    Compiler compiler;
    Module module;
    if (!compiler.compile(text, module, hints))
    {
        std::cerr << "error: " << compiler.lastError() << '\n';
        return kExitData;
    }

    if (outputPath.empty())
    {
        outputPath = sourcePath.substr(0, sourcePath.rfind('.')) + XENON_MODULE_EXTENSION;
    }
    else if (!outputPath.ends_with(XENON_MODULE_EXTENSION))
    {
        outputPath += XENON_MODULE_EXTENSION;
    }

    try
    {
        writeModuleFile(outputPath, module);
    }
    catch (const ModuleFileError& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return kExitIo;
    }

    std::cout << "compiled: " << sourcePath << " -> " << outputPath << " ("
              << module.Symbols.size() << " symbols)\n";
    return EXIT_SUCCESS;
}

int runPackage(const std::string& archivePath, const std::vector<std::string>& arguments)
{
    using namespace xenon;

    std::vector<PackedEntry> entries;
    std::string manifestText;
    try
    {
        entries = Packer::readArchive(archivePath);
        manifestText = Packer::readManifest(archivePath);
    }
    catch (const PackError& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return kExitData;
    }

    const Manifest manifest = Manifest::parse(manifestText);
    if (!manifest.hasEntryPoint())
    {
        std::cerr << "error: the manifest does not designate an entry function\n";
        return kExitData;
    }

    Runtime runtime;
    runtime.addModule(environmentModule());

    std::string moduleName = manifest.MainModule;
    if (moduleName.ends_with(XENON_MODULE_EXTENSION))
    {
        moduleName = moduleName.substr(0, moduleName.size() - std::string(XENON_MODULE_EXTENSION).size());
    }

    bool loaded = false;
    for (const auto& entry : entries)
    {
        std::stringstream buffer(std::string(entry.Data.begin(), entry.Data.end()));
        Module module;
        try
        {
            readModuleFile(buffer, module);
        }
        catch (const ModuleFileError&)
        {
            continue; // not a module file
        }

        if (module.Name == manifest.MainModule || module.Name == moduleName)
        {
            runtime.addModule(module);
            loaded = true;
            break;
        }
    }

    if (!loaded)
    {
        std::cerr << "error: module '" << manifest.MainModule << "' not found in package\n";
        return kExitData;
    }

    registerBuiltInNatives(runtime);
    return runtime.runMain(manifest.MainFunction, arguments, {});
}

int runFunction(const std::string& modulePath, const std::string& function,
                const std::vector<std::string>& arguments)
{
    using namespace xenon;

    Module module;
    try
    {
        readModuleFile(modulePath, module);
    }
    catch (const ModuleFileError&)
    {
        readModuleFile(modulePath + XENON_MODULE_EXTENSION, module);
    }

    Runtime runtime;
    runtime.addModule(environmentModule());
    runtime.addModule(module);
    registerBuiltInNatives(runtime);

    std::string target = function;
    const auto separator = target.rfind("::");
    if (separator != std::string::npos)
    {
        target = target.substr(separator + 2);
    }

    return runtime.runMain(target, arguments, {});
}

int packCommand(const std::vector<std::string>& args)
{
    using namespace xenon;

    std::vector<std::string> modules;
    std::string archivePath;
    std::string manifestPath;

    for (const auto& arg : args)
    {
        if (arg.ends_with(XENON_PACKAGE_EXTENSION))
        {
            if (!archivePath.empty())
            {
                std::cerr << "error: multiple .xenon archive names given\n";
                return kExitUsage;
            }
            archivePath = arg;
        }
        else if (arg.ends_with(XENON_MODULE_EXTENSION) || arg.ends_with(XENON_HINT_EXTENSION))
        {
            modules.push_back(arg);
        }
        else
        {
            if (!manifestPath.empty())
            {
                std::cerr << "error: multiple manifest files given\n";
                return kExitUsage;
            }
            manifestPath = arg;
        }
    }

    if (archivePath.empty())
    {
        std::cerr << "error: no .xenon archive name given\n";
        return kExitUsage;
    }
    if (modules.empty())
    {
        std::cerr << "error: no modules to pack\n";
        return kExitUsage;
    }

    try
    {
        Packer packer;
        packer.pack(modules, archivePath, manifestPath);
    }
    catch (const PackError& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return kExitIo;
    }

    std::cout << "packed: " << archivePath << " (" << modules.size() << " file(s))\n";
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        if (argc == 1)
        {
            printVersion();
            std::cout << '\n';
            printHelp();
            return EXIT_SUCCESS;
        }

        const std::string command = argv[1];

        if (command == "help")
        {
            printHelp();
            return EXIT_SUCCESS;
        }

        if (command == "version")
        {
            printVersion();
            return EXIT_SUCCESS;
        }

        if (command == "compile")
        {
            if (argc < 3)
            {
                std::cerr << "usage: xenon compile <sources>... [module]\n";
                return kExitUsage;
            }
            std::vector<std::string> args;
            for (int i = 2; i < argc; ++i)
            {
                args.emplace_back(argv[i]);
            }
            return compileCommand(args);
        }

        if (command == "run")
        {
            if (argc < 3)
            {
                std::cerr << "usage: xenon run <xenon> <arguments>...\n";
                std::cerr << "       xenon run <module> <function/method> <arguments>...\n";
                return kExitUsage;
            }

            const std::string first = argv[2];

            if (first.ends_with(XENON_PACKAGE_EXTENSION))
            {
                std::vector<std::string> args;
                for (int i = 3; i < argc; ++i)
                {
                    args.emplace_back(argv[i]);
                }
                return runPackage(first, args);
            }

            if (argc < 4)
            {
                std::cerr << "usage: xenon run <module> <function/method> <arguments>...\n";
                return kExitUsage;
            }

            std::vector<std::string> args;
            for (int i = 4; i < argc; ++i)
            {
                args.emplace_back(argv[i]);
            }
            return runFunction(first, argv[3], args);
        }

        if (command == "pack")
        {
            if (argc < 4)
            {
                std::cerr << "usage: xenon pack <modules>... <xenon> [manifest]\n";
                return kExitUsage;
            }
            std::vector<std::string> args;
            for (int i = 2; i < argc; ++i)
            {
                args.emplace_back(argv[i]);
            }
            return packCommand(args);
        }

        std::cerr << "unknown command: " << command << '\n';
        printHelp();
        return kExitUsage;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
