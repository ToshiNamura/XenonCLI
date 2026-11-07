#include "xenon/packer/manifest.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace xenon
{
namespace
{

std::string_view trim(std::string_view view)
{
    const auto isSpace = [](char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
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

} // namespace

std::string Manifest::toString() const
{
    std::ostringstream os;
    os << "Manifest-Version: 1.0\n";
    if (!MainModule.empty())
    {
        os << "Main-Module: " << MainModule << '\n';
    }
    if (!MainFunction.empty())
    {
        os << "Main-Function: " << MainFunction << '\n';
    }
    return os.str();
}

Manifest Manifest::parse(std::string_view text)
{
    Manifest manifest;

    std::istringstream input{std::string(text)};
    std::string line;
    while (std::getline(input, line))
    {
        const auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        const std::string key = std::string(trim(std::string_view(line).substr(0, colon)));
        const std::string value = std::string(trim(std::string_view(line).substr(colon + 1)));

        if (key == "Main-Module")
        {
            manifest.MainModule = value;
        }
        else if (key == "Main-Function")
        {
            manifest.MainFunction = value;
        }
    }

    return manifest;
}

Manifest Manifest::fromModule(const Module& module)
{
    Manifest manifest;
    manifest.MainModule = module.Name;
    if (module.find("main") != nullptr)
    {
        manifest.MainFunction = "main";
    }
    return manifest;
}

} // namespace xenon
