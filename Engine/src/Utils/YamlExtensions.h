#pragma once
#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <entt/entt.hpp>

namespace YAML
{
	static YAML::Emitter &operator <<(YAML::Emitter &out, const entt::entity &entity)
	{
		return out << (uint32_t)entity;
	}

	static YAML::Emitter &operator <<(YAML::Emitter &out, const glm::vec2 &vector)
	{
		out << YAML::Flow;
		return out << YAML::BeginSeq << vector.x << vector.y << YAML::EndSeq;
	}

	static YAML::Emitter &operator <<(YAML::Emitter &out, const glm::vec3 &vector)
	{
		out << YAML::Flow;
		return out << YAML::BeginSeq << vector.x << vector.y << vector.z << YAML::EndSeq;
	}

	static YAML::Emitter &operator <<(YAML::Emitter &out, const glm::vec4 &vector)
	{
		out << YAML::Flow;
		return out << YAML::BeginSeq << vector.x << vector.y << vector.z << vector.w << YAML::EndSeq;
	}

	static YAML::Emitter &operator <<(YAML::Emitter &out, const glm::mat4x4 &mat)
	{
		out << YAML::Flow;
		return out << YAML::BeginSeq << mat[0] << mat[1] << mat[2] << mat[3] << YAML::EndSeq;
	}

	static YAML::Emitter &operator <<(YAML::Emitter &out, const glm::quat &quat)
	{
		out << YAML::Flow;
		return out << YAML::BeginSeq << quat.x << quat.y << quat.z << quat.w << YAML::EndSeq;
	}

	template<>
	struct convert<entt::entity>
	{
		static bool decode(const Node &node, entt::entity &e)
		{
			if (!node.IsScalar())
				return false;

			e = (entt::entity)node.as<uint32_t>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec2>
	{
		static bool decode(const Node &node, glm::vec2 &v)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static bool decode(const Node &node, glm::vec3 &v)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static bool decode(const Node &node, glm::vec4 &v)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();
			v.w = node[3].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::quat>
	{
		static bool decode(const Node &node, glm::quat &q)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			q.x = node[0].as<float>();
			q.y = node[1].as<float>();
			q.z = node[2].as<float>();
			q.w = node[3].as<float>();

			return true;
		}
	};

	template<>
	struct convert<eastl::string>
	{
		static Node encode(const eastl::string& rhs)
		{
			std::string str = rhs.c_str();
			Node node;
			node.push_back(str);
			return node;
		}

		static bool decode(const Node& node, eastl::string& rhs)
		{
			std::string s;
			bool success = convert<std::string>::decode(node, s);
			rhs = s.c_str();
			return success;
		}
	};

	static YAML::Emitter &operator <<(YAML::Emitter &out, const eastl::string &value)
	{
		std::string str = value.c_str();
		return out << str;
	}

	template <typename T, typename A>
	struct convert<eastl::vector<T, A>> {
		static Node encode(const eastl::vector<T, A>& rhs) {
			Node node(NodeType::Sequence);
			for (const auto& element : rhs)
				node.push_back(element);
			return node;
		}

		static bool decode(const Node& node, eastl::vector<T, A>& rhs) {
			if (!node.IsSequence())
				return false;

			rhs.clear();
			for (const auto& element : node)
				rhs.push_back(element.as<T>());
			return true;
		}
	};

	template <typename T>
	static YAML::Emitter &operator <<(YAML::Emitter &out, const eastl::vector<T> &value)
	{
		return EmitSeq(out, value);
	}
}
