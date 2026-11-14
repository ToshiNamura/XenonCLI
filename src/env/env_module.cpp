#include <xenon/env/env_module.hpp>

#include <utility>

xenon::Module xenon::environmentModule()
{
	xenon::Module module;
	module.Name = "xenon.env";

	xenon::Type object;
	object.Name    = "Object";
	object.Kind    = xenon::TypeKind::Class;
	object.Defined = false;
	module.Types.push_back(std::move(object));

	xenon::Type string;
	string.Name    = "String";
	string.Kind    = xenon::TypeKind::Class;
	string.Defined = false;
	string.Parents.push_back(xenon::Parent{xenon::Access::Public, "Object"});
	module.Types.push_back(std::move(string));

	xenon::Type vector;
	vector.Name    = "Vector";
	vector.Kind    = xenon::TypeKind::Class;
	vector.Defined = false;
	vector.Parents.push_back(xenon::Parent{xenon::Access::Public, "Object"});
	xenon::TemplateParam parameter;
	parameter.TypeName = "Type";
	vector.TemplateParams.push_back(std::move(parameter));
	module.Types.push_back(std::move(vector));

	xenon::Type environment;
	environment.Name    = "Environment";
	environment.Kind    = xenon::TypeKind::Class;
	environment.Defined = false;
	environment.Parents.push_back(xenon::Parent{xenon::Access::Public, "Object"});
	module.Types.push_back(std::move(environment));

	return module;
}
