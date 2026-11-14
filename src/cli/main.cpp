#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <xenon/asm/assembler.hpp>
#include <xenon/asm/disassembler.hpp>
#include <xenon/compiler/compiler.hpp>
#include <xenon/core/defines.hpp>
#include <xenon/env/env_module.hpp>
#include <xenon/module/module_file.hpp>
#include <xenon/packer/manifest.hpp>
#include <xenon/packer/packer.hpp>
#include <xenon/runtime/error.hpp>
#include <xenon/runtime/runtime.hpp>

namespace
{
	constexpr int kExitUsage    = 64; // EX_USAGE
	constexpr int kExitData     = 65; // EX_DATAERR
	constexpr int kExitNoInput  = 66; // EX_NOINPUT
	constexpr int kExitSoftware = 70; // EX_SOFTWARE
	constexpr int kExitIo       = 74; // EX_IOERR

	void printHelp()
	{
		std::cout
			<< "Xenon " << XENON_VERSION_STRING
			<< " - compile, assemble, pack and run Xenon byte-code\n\n"
			<< "Usage:\n"
			<< "  xenon                                          Show version and this help.\n"
			<< "  xenon help                                     Show this help.\n"
			<< "  xenon version                                  Show the program version.\n"
			<< "  xenon compile <source.ixx> [hints...] [-o <module>]\n"
			<< "                                                 Compile a .ixx module with optional .hint files.\n"
			<< "  xenon asm <source.asm> [-o <module>]           Assemble textual Xenon ASM into a .module.\n"
			<< "  xenon disasm <module> [-o <listing.asm>]       Disassemble a .module into textual Xenon ASM.\n"
			<< "  xenon run <package.xenon> [args...]            Run a .xenon package (entry from manifest).\n"
			<< "  xenon run <module> [function] [args...]        Run a function or method from a .module (default: main).\n"
			<< "  xenon pack <modules...> <package.xenon> [manifest]\n"
			<< "                                                 Pack modules into a .xenon archive.\n";
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

	bool writeTextFile(const std::string& path, const std::string& text)
	{
		std::ofstream output(path, std::ios::binary);
		if (!output)
		{
			return false;
		}
		output.write(text.data(), static_cast<std::streamsize>(text.size()));
		return static_cast<bool>(output);
	}

	std::string replaceExtension(const std::string& path, const std::string& extension)
	{
		const auto dot = path.rfind('.');
		const auto slash = path.find_last_of("/\\");
		if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		{
			return path + extension;
		}
		return path.substr(0, dot) + extension;
	}

	std::string ensureExtension(const std::string& path, const std::string& extension)
	{
		return path.ends_with(extension) ? path : path + extension;
	}

	void registerBuiltInNatives(xenon::Runtime& runtime)
	{
		runtime.registerNative("sqrt",
							   [](const std::vector<xenon::Value>& args) -> xenon::Value
							   {
								   if (args.size() != 1 || !xenon::Runtime::isNumber(args[0]))
								   {
									   throw xenon::RuntimeError("sqrt expects one number");
								   }
								   return xenon::Value{std::sqrt(std::get<double>(args[0]))};
							   });

		runtime.registerNative("pow",
							   [](const std::vector<xenon::Value>& args) -> xenon::Value
							   {
								   if (args.size() != 2 || !xenon::Runtime::isNumber(args[0]) ||
									   !xenon::Runtime::isNumber(args[1]))
								   {
									   throw xenon::RuntimeError("pow expects two numbers");
								   }
								   return xenon::Value{std::pow(std::get<double>(args[0]),
																std::get<double>(args[1]))};
							   });
	}

	int compileCommand(const std::vector<std::string>& arguments)
	{
		std::vector<std::string> hintPaths;
		std::vector<std::string> sourcePaths;
		std::string              outputPath;

		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			const std::string& argument = arguments[i];
			if (argument == "-o" || argument == "--output")
			{
				if (i + 1 >= arguments.size())
				{
					std::cerr << "error: " << argument << " requires a path\n";
					return kExitUsage;
				}
				outputPath = arguments[++i];
			}
			else if (argument.ends_with(XENON_HINT_EXTENSION))
			{
				hintPaths.push_back(argument);
			}
			else if (argument.ends_with(XENON_SOURCE_EXTENSION))
			{
				sourcePaths.push_back(argument);
			}
			else
			{
				if (!outputPath.empty())
				{
					std::cerr << "error: multiple output module names given\n";
					return kExitUsage;
				}
				outputPath = argument;
			}
		}

		if (sourcePaths.size() != 1)
		{
			std::cerr << "error: exactly one .ixx source is required (got " << sourcePaths.size()
					  << ")\n";
			return kExitUsage;
		}

		std::vector<xenon::Module> hints;
		for (const auto& path : hintPaths)
		{
			std::string text;
			if (!readTextFile(path, text))
			{
				std::cerr << "error: cannot open '" << path << "'\n";
				return kExitNoInput;
			}

			xenon::Compiler compiler;
			xenon::Module   module;
			if (!compiler.compileHints(text, module))
			{
				std::cerr << "error: " << compiler.lastError() << '\n';
				return kExitData;
			}
			hints.push_back(std::move(module));
			std::cout << "hint: " << path << " (" << hints.back().Functions.size() << " functions, "
					  << hints.back().Types.size() << " types)\n";
		}

		const std::string& sourcePath = sourcePaths.front();

		std::string text;
		if (!readTextFile(sourcePath, text))
		{
			std::cerr << "error: cannot open '" << sourcePath << "'\n";
			return kExitNoInput;
		}

		xenon::Compiler compiler;
		xenon::Module   module;
		if (!compiler.compile(text, module, hints))
		{
			std::cerr << "error: " << compiler.lastError() << '\n';
			return kExitData;
		}

		if (outputPath.empty())
		{
			outputPath = replaceExtension(sourcePath, XENON_MODULE_EXTENSION);
		}
		else
		{
			outputPath = ensureExtension(outputPath, XENON_MODULE_EXTENSION);
		}

		try
		{
			writeModuleFile(outputPath, module);
		}
		catch (const xenon::ModuleFileError& error)
		{
			std::cerr << "error: " << error.what() << '\n';
			return kExitIo;
		}

		std::cout << "compiled: " << sourcePath << " -> " << outputPath << " ("
				  << module.Functions.size() << " functions, " << module.Types.size()
				  << " types)\n";
		return EXIT_SUCCESS;
	}

	int asmCommand(const std::vector<std::string>& arguments)
	{
		std::string sourcePath;
		std::string outputPath;

		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			const std::string& argument = arguments[i];
			if (argument == "-o" || argument == "--output")
			{
				if (i + 1 >= arguments.size())
				{
					std::cerr << "error: " << argument << " requires a path\n";
					return kExitUsage;
				}
				outputPath = arguments[++i];
			}
			else if (sourcePath.empty())
			{
				sourcePath = argument;
			}
			else
			{
				std::cerr << "error: unexpected argument '" << argument << "'\n";
				return kExitUsage;
			}
		}

		if (sourcePath.empty())
		{
			std::cerr << "usage: xenon asm <source.asm> [-o <module>]\n";
			return kExitUsage;
		}

		std::string text;
		if (!readTextFile(sourcePath, text))
		{
			std::cerr << "error: cannot open '" << sourcePath << "'\n";
			return kExitNoInput;
		}

		xenon::Assembler assembler;
		xenon::Module    module;
		if (!assembler.assemble(text, module))
		{
			std::cerr << "error: " << assembler.lastError() << '\n';
			return kExitData;
		}

		if (module.Name.empty())
		{
			std::cerr << "error: the assembly has no '.module <name>' declaration\n";
			return kExitData;
		}

		if (outputPath.empty())
		{
			outputPath = replaceExtension(sourcePath, XENON_MODULE_EXTENSION);
		}
		else
		{
			outputPath = ensureExtension(outputPath, XENON_MODULE_EXTENSION);
		}

		try
		{
			writeModuleFile(outputPath, module);
		}
		catch (const xenon::ModuleFileError& error)
		{
			std::cerr << "error: " << error.what() << '\n';
			return kExitIo;
		}

		std::cout << "assembled: " << sourcePath << " -> " << outputPath << " ("
				  << module.Functions.size() << " functions, " << module.Types.size()
				  << " types)\n";
		return EXIT_SUCCESS;
	}

	int disasmCommand(const std::vector<std::string>& arguments)
	{
		std::string modulePath;
		std::string outputPath;

		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			const std::string& argument = arguments[i];
			if (argument == "-o" || argument == "--output")
			{
				if (i + 1 >= arguments.size())
				{
					std::cerr << "error: " << argument << " requires a path\n";
					return kExitUsage;
				}
				outputPath = arguments[++i];
			}
			else if (modulePath.empty())
			{
				modulePath = argument;
			}
			else
			{
				std::cerr << "error: unexpected argument '" << argument << "'\n";
				return kExitUsage;
			}
		}

		if (modulePath.empty())
		{
			std::cerr << "usage: xenon disasm <module> [-o <listing.asm>]\n";
			return kExitUsage;
		}

		xenon::Module module;
		try
		{
			readModuleFile(ensureExtension(modulePath, XENON_MODULE_EXTENSION), module);
		}
		catch (const xenon::ModuleFileError& error)
		{
			std::cerr << "error: " << error.what() << '\n';
			return kExitNoInput;
		}

		const std::string text = xenon::disassemble(module);
		if (outputPath.empty())
		{
			std::cout << text;
			return EXIT_SUCCESS;
		}

		if (!writeTextFile(outputPath, text))
		{
			std::cerr << "error: cannot write '" << outputPath << "'\n";
			return kExitIo;
		}

		std::cout << "disassembled: " << modulePath << " -> " << outputPath << '\n';
		return EXIT_SUCCESS;
	}

	int runPackage(const std::string& archivePath, const std::vector<std::string>& arguments)
	{
		std::vector<xenon::PackedEntry> entries;
		std::string                     manifestText;
		try
		{
			entries      = xenon::Packer::readArchive(archivePath);
			manifestText = xenon::Packer::readManifest(archivePath);
		}
		catch (const xenon::PackError& error)
		{
			std::cerr << "error: " << error.what() << '\n';
			return kExitData;
		}

		const xenon::Manifest manifest = xenon::Manifest::parse(manifestText);
		if (!manifest.hasEntryPoint())
		{
			std::cerr << "error: the manifest does not designate an entry function\n";
			return kExitData;
		}

		xenon::Runtime runtime;
		runtime.addModule(xenon::environmentModule());

		std::string moduleName = manifest.MainModule;
		if (moduleName.ends_with(XENON_MODULE_EXTENSION))
		{
			moduleName = moduleName.substr(0, moduleName.size() - std::string(XENON_MODULE_EXTENSION).size());
		}

		bool loaded = false;
		for (const auto& entry : entries)
		{
			std::stringstream buffer(std::string(entry.Data.begin(), entry.Data.end()));
			xenon::Module     module;
			try
			{
				readModuleFile(buffer, module);
			}
			catch (const xenon::ModuleFileError&)
			{
				continue; // not a module file
			}

			runtime.addModule(module);
			if (module.Name == manifest.MainModule || module.Name == moduleName)
			{
				loaded = true;
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

	int runFunction(const std::string& modulePath, const std::vector<std::string>& candidates)
	{
		xenon::Module module;
		try
		{
			readModuleFile(ensureExtension(modulePath, XENON_MODULE_EXTENSION), module);
		}
		catch (const xenon::ModuleFileError& error)
		{
			std::cerr << "error: " << error.what() << '\n';
			return kExitNoInput;
		}

		xenon::Runtime runtime;
		runtime.addModule(xenon::environmentModule());
		runtime.addModule(module);
		registerBuiltInNatives(runtime);

		std::string              function = "main";
		std::vector<std::string> arguments = candidates;
		if (!candidates.empty() && runtime.hasFunction(candidates.front()))
		{
			function = candidates.front();
			arguments.assign(candidates.begin() + 1, candidates.end());
		}

		return runtime.runMain(function, arguments, {});
	}

	int packCommand(const std::vector<std::string>& arguments)
	{
		std::vector<std::string> modules;
		std::string              archivePath;
		std::string              manifestPath;

		for (const auto& argument : arguments)
		{
			if (argument.ends_with(XENON_PACKAGE_EXTENSION))
			{
				if (!archivePath.empty())
				{
					std::cerr << "error: multiple .xenon archive names given\n";
					return kExitUsage;
				}
				archivePath = argument;
			}
			else if (argument.ends_with(XENON_MODULE_EXTENSION) || argument.ends_with(XENON_HINT_EXTENSION))
			{
				modules.push_back(argument);
			}
			else
			{
				if (!manifestPath.empty())
				{
					std::cerr << "error: multiple manifest files given\n";
					return kExitUsage;
				}
				manifestPath = argument;
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
			xenon::Packer packer;
			packer.pack(modules, archivePath, manifestPath);
		}
		catch (const xenon::PackError& error)
		{
			std::cerr << "error: " << error.what() << '\n';
			return kExitIo;
		}

		std::cout << "packed: " << archivePath << " (" << modules.size() << " file(s))\n";
		return EXIT_SUCCESS;
	}
}

int main(const int argc, const char* argv[], const char* envp[])
{
	(void)envp;

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

		if (command == "help" || command == "-h" || command == "--help")
		{
			printHelp();
			return EXIT_SUCCESS;
		}

		if (command == "version" || command == "--version")
		{
			printVersion();
			return EXIT_SUCCESS;
		}

		std::vector<std::string> arguments;
		for (int i = 2; i < argc; ++i)
		{
			arguments.emplace_back(argv[i]);
		}

		if (command == "compile")
		{
			if (arguments.empty())
			{
				std::cerr << "usage: xenon compile <source.ixx> [hints...] [-o <module>]\n";
				return kExitUsage;
			}
			return compileCommand(arguments);
		}

		if (command == "asm")
		{
			return asmCommand(arguments);
		}

		if (command == "disasm")
		{
			return disasmCommand(arguments);
		}

		if (command == "run")
		{
			if (arguments.empty())
			{
				std::cerr << "usage: xenon run <package.xenon> [args...]\n";
				std::cerr << "       xenon run <module> [function] [args...]\n";
				return kExitUsage;
			}

			const std::string& first = arguments.front();

			if (first.ends_with(XENON_PACKAGE_EXTENSION))
			{
				std::vector<std::string> runArguments(arguments.begin() + 1, arguments.end());
				return runPackage(first, runArguments);
			}

			std::vector<std::string> runArguments(arguments.begin() + 1, arguments.end());
			return runFunction(first, runArguments);
		}

		if (command == "pack")
		{
			if (arguments.size() < 2)
			{
				std::cerr << "usage: xenon pack <modules...> <package.xenon> [manifest]\n";
				return kExitUsage;
			}
			return packCommand(arguments);
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
