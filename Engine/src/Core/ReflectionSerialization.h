#pragma once
#include "Reflection.h"
#include "Utils/YamlExtensions.h"
#include "Utils/StringUtils.h"

class ReflectionYaml
{
public:
	static void writeFields(YAML::Emitter &out, const StructInfo &info, const void *object, const void *default_object)
	{
		for (int i = 0; i < info.fieldsCount; i++)
		{
			const FieldInfo &field = info.fields[i];
			if (field.isCategory() || !field.isSerialized)
				continue;

			const void *value = field.getAddress(object);
			const void *default_value = default_object ? field.getAddress(default_object) : nullptr;
			if (default_value && field.isDefault(value, default_value))
				continue;

			out << YAML::Key << prettifyName(field.name, "").c_str() << YAML::Value;
			if (field.valueInfo)
			{
				write_value(out, *field.valueInfo, value);
			} else if (field.arrayInfo)
			{
				write_array(out, *field.arrayInfo, value);
			} else
			{
				if (is_flow(*field.structInfo))
					out << YAML::Flow;
				out << YAML::BeginMap;
				writeFields(out, *field.structInfo, value, default_value);
				out << YAML::EndMap;
			}
		}
	}

	static void readFields(const YAML::Node &node, const StructInfo &info, void *object, const void *default_object)
	{
		if (!node.IsMap())
			return;

		for (int i = 0; i < info.fieldsCount; i++)
		{
			const FieldInfo &field = info.fields[i];
			if (field.isCategory() || !field.isSerialized)
				continue;

			void *value = field.getAddress(object);
			const void *default_value = field.getAddress(default_object);

			YAML::Node child = node[prettifyName(field.name, "").c_str()];
			if (field.valueInfo)
				read_value(child, *field.valueInfo, value, default_value);
			else if (child && field.structInfo)
				readFields(child, *field.structInfo, value, default_value);
			else if (child)
				read_array(child, *field.arrayInfo, value);
		}
	}

	static void readFields(const YAML::Node &node, const StructInfo &info, void *object)
	{
		readFields(node, info, object, info.defaults);
	}

	static bool saveToFile(const StructInfo &info, const void *object, const std::filesystem::path &path)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << info.name;
		writeFields(out, info, object, nullptr);
		out << YAML::EndMap;

		std::ofstream file(path);
		if (!file)
			return false;
		file << out.c_str();
		return true;
	}

	static bool loadFromFile(const StructInfo &info, void *object, const std::filesystem::path &path)
	{
		if (!std::filesystem::exists(path))
			return false;

		YAML::Node root = YAML::LoadFile(path.string());
		if (root["Type"].as<std::string>("") != info.name)
			return false;

		readFields(root, info, object);
		return true;
	}

private:
	static void write_value(YAML::Emitter &out, const ValueInfo &info, const void *value)
	{
		switch (info.type)
		{
			case VALUE_TYPE_BOOL: out << *(const bool *)value; break;
			case VALUE_TYPE_INT32: out << *(const int32_t *)value; break;
			case VALUE_TYPE_UINT32: out << *(const uint32_t *)value; break;
			case VALUE_TYPE_UINT64: out << *(const uint64_t *)value; break;
			case VALUE_TYPE_FLOAT: out << *(const float *)value; break;
			case VALUE_TYPE_STRING: out << *(const eastl::string *)value; break;
		}
	}

	static void read_value(const YAML::Node &node, const ValueInfo &info, void *value, const void *default_value)
	{
		switch (info.type)
		{
			case VALUE_TYPE_BOOL: *(bool *)value = node.as<bool>(*(const bool *)default_value); break;
			case VALUE_TYPE_INT32: *(int32_t *)value = node.as<int32_t>(*(const int32_t *)default_value); break;
			case VALUE_TYPE_UINT32: *(uint32_t *)value = node.as<uint32_t>(*(const uint32_t *)default_value); break;
			case VALUE_TYPE_UINT64: *(uint64_t *)value = node.as<uint64_t>(*(const uint64_t *)default_value); break;
			case VALUE_TYPE_FLOAT: *(float *)value = node.as<float>(*(const float *)default_value); break;
			case VALUE_TYPE_STRING: *(eastl::string *)value = node.as<eastl::string>(*(const eastl::string *)default_value); break;
		}
	}

	static void write_array(YAML::Emitter &out, const ArrayInfo &info, const void *value)
	{
		out << YAML::BeginSeq;
		for (int i = 0; i < info.size(value); i++)
		{
			const void *element = info.at(value, i);
			if (!info.element.structInfo)
			{
				write_value(out, *info.element.valueInfo, element);
				continue;
			}

			if (is_flow(*info.element.structInfo))
				out << YAML::Flow;
			out << YAML::BeginMap;
			writeFields(out, *info.element.structInfo, element, nullptr);
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	static void read_array(const YAML::Node &node, const ArrayInfo &info, void *value)
	{
		if (!node.IsSequence())
			return;

		info.resize(value, node.size());
		for (int i = 0; i < node.size(); i++)
		{
			void *element = info.at(value, i);
			if (info.element.structInfo)
				readFields(node[i], *info.element.structInfo, element);
			else
				read_value(node[i], *info.element.valueInfo, element, element);
		}
	}

	static bool is_flow(const StructInfo &info)
	{
		for (int i = 0; i < info.fieldsCount; i++)
		{
			if (!info.fields[i].valueInfo)
				return false;
		}
		return true;
	}
};
