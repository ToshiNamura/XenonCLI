#include "xenon/compiler/module_parser.hpp"

#include <cctype>

namespace xenon
{
namespace
{

std::string_view trim(std::string_view view)
{
    const auto is_space = [](char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };

    while (!view.empty() && is_space(view.front()))
    {
        view.remove_prefix(1);
    }
    while (!view.empty() && is_space(view.back()))
    {
        view.remove_suffix(1);
    }
    return view;
}

bool starts_with(std::string_view view, std::string_view prefix)
{
    return view.size() >= prefix.size() && view.substr(0, prefix.size()) == prefix;
}

/// True when `line` starts with `keyword` followed by whitespace or end of line,
/// e.g. `import X` but not `importX`.
bool keyword_starts(std::string_view line, std::string_view keyword)
{
    if (!starts_with(line, keyword))
    {
        return false;
    }
    if (line.size() == keyword.size())
    {
        return true;
    }
    return std::isspace(static_cast<unsigned char>(line[keyword.size()])) != 0;
}

std::string_view strip_trailing_semicolon(std::string_view view)
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
    error_.clear();
    out = ModuleHeader{};
    out.body_offset = source.size();

    bool seen_declaration = false;
    std::size_t pos = 0;

    while (pos < source.size())
    {
        const std::size_t line_start = pos;
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

        if (keyword_starts(line, "export module"))
        {
            if (seen_declaration)
            {
                error_ = "duplicate 'export module' declaration";
                return ModuleParseResult::Error;
            }

            const auto name =
                strip_trailing_semicolon(line.substr(std::string_view("export module").size()));
            if (name.empty())
            {
                error_ = "expected a module name after 'export module'";
                return ModuleParseResult::Error;
            }

            out.name = std::string(name);
            seen_declaration = true;
            continue;
        }

        if (!seen_declaration)
        {
            error_ = "the file must start with 'export module Name;'";
            return ModuleParseResult::Error;
        }

        if (keyword_starts(line, "import"))
        {
            const auto name =
                strip_trailing_semicolon(line.substr(std::string_view("import").size()));
            if (name.empty())
            {
                error_ = "expected a module name after 'import'";
                return ModuleParseResult::Error;
            }

            out.imports.emplace_back(name);
            continue;
        }

        if (starts_with(line, "#include"))
        {
            out.includes.emplace_back(line);
            continue;
        }

        // The first statement that is neither an import nor an include begins the body.
        out.body_offset = line_start;
        break;
    }

    if (!seen_declaration)
    {
        error_ = "missing 'export module Name;' declaration";
        return ModuleParseResult::Error;
    }

    return ModuleParseResult::Ok;
}

} // namespace xenon
