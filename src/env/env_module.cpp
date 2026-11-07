#include "xenon/env/env_module.hpp"

#include <utility>

namespace xenon
{

Module environmentModule()
{
    Module module;
    module.Name = "xenon.env";

    Symbol object;
    object.Name = "Object";
    object.Kind = SymbolKind::Class;
    object.Signature = "class Object";
    module.Symbols.push_back(std::move(object));

    Symbol string;
    string.Name = "String";
    string.Kind = SymbolKind::Class;
    string.Signature = "class String";
    module.Symbols.push_back(std::move(string));

    Symbol vector;
    vector.Name = "Vector";
    vector.Kind = SymbolKind::Class;
    vector.Signature = "template class Vector<Type>";
    module.Symbols.push_back(std::move(vector));

    Symbol environment;
    environment.Name = "Environment";
    environment.Kind = SymbolKind::Class;
    environment.Signature = "class Environment";
    module.Symbols.push_back(std::move(environment));

    return module;
}

} // namespace xenon
