#include "xenon/compiler/module_parser.hpp"

#include <cctype>
#include <cstddef>
#include <string>

namespace xenon
{
namespace
{

std::string_view trim(std::string_view view)
{
    const auto isSpace = [](char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };

    while (!view.empty() && isSpace(view.front()))
    {
        view.remove_prefix(1);
    }
    while (!view.empty() && isSpace(view.back()))
    {
        view.remove_suffix(1);
    }
    return view;
}

bool startsWith(std::string_view view, std::string_view prefix)
{
    return view.size() >= prefix.size() && view.substr(0, prefix.size()) == prefix;
}

/// True when `line` starts with `keyword` followed by whitespace or end of line,
/// e.g. `import X` but not `importX`.
bool keywordStarts(std::string_view line, std::string_view keyword)
{
    if (!startsWith(line, keyword))
    {
        return false;
    }
    if (line.size() == keyword.size())
    {
        return true;
    }
    return std::isspace(static_cast<unsigned char>(line[keyword.size()])) != 0;
}

std::string_view stripTrailingSemicolon(std::string_view view)
{
    view = trim(view);
    if (!view.empty() && view.back() == ';')
    {
        view.remove_suffix(1);
    }
    return trim(view);
}

} // namespace

ModuleParseResult ModuleParser::parse(std::string_view source, ModuleHeader& out)
{
    _error.clear();
    out = ModuleHeader{};
    out.BodyOffset = source.size();

    bool seenDeclaration = false;
    std::size_t pos = 0;

    while (pos < source.size())
    {
        const std::size_t lineStart = pos;
        const std::size_t newline = source.find('\n', pos);
        const std::size_t end = (newline == std::string_view::npos) ? source.size() : newline;
        std::string_view line = source.substr(pos, end - pos);
        pos = (newline == std::string_view::npos) ? source.size() : newline + 1;

        // Drop `//` line comments, then trim surrounding whitespace.
        const std::size_t comment = line.find("//");
        if (comment != std::string_view::npos)
        {
            line = line.substr(0, comment);
        }
        line = trim(line);

        if (line.empty())
        {
            continue;
        }

        if (keywordStarts(line, "export module"))
        {
            if (seenDeclaration)
            {
                _error = "duplicate 'export module' declaration";
                return ModuleParseResult::Error;
            }

            const auto name =
                stripTrailingSemicolon(line.substr(std::string_view("export module").size()));
            if (name.empty())
            {
                _error = "expected a module name after 'export module'";
                return ModuleParseResult::Error;
            }

            out.Name = std::string(name);
            seenDeclaration = true;
            continue;
        }

        if (!seenDeclaration)
        {
            _error = "the file must start with 'export module Name;'";
            return ModuleParseResult::Error;
        }

        if (keywordStarts(line, "import"))
        {
            const auto name =
                stripTrailingSemicolon(line.substr(std::string_view("import").size()));
            if (name.empty())
            {
                _error = "expected a module name after 'import'";
                return ModuleParseResult::Error;
            }

            out.Imports.emplace_back(name);
            continue;
        }

        if (startsWith(line, "#include"))
        {
            out.Includes.emplace_back(line);
            continue;
        }

        // The first statement that is neither an import nor an include begins the body.
        out.BodyOffset = lineStart;
        break;
    }

    if (!seenDeclaration)
    {
        _error = "missing 'export module Name;' declaration";
        return ModuleParseResult::Error;
    }

    return ModuleParseResult::Ok;
}

} // namespace xenon
