#pragma once
#include <yaml-cpp/yaml.h>

namespace YAML
{
	template<>
	struct convert<eastl::string>
	{
		static Node encode(const eastl::string &value)
		{
			Node node;
			node.push_back(std::string(value.c_str()));
			return node;
		}

		static bool decode(const Node &node, eastl::string &value)
		{
			std::string text;
			if (!convert<std::string>::decode(node, text))
				return false;

			value = text.c_str();
			return true;
		}
	};

	inline Emitter &operator <<(Emitter &out, const eastl::string &value)
	{
		return out << std::string(value.c_str());
	}
}