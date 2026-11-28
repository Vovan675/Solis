#pragma once
#include "Reflection.h"

struct ReflectedField
{
	void *owner = nullptr;
	const FieldInfo *info = nullptr;

	// Comparison rule for UI stuff
	bool operator<(const ReflectedField &other) const { return info != other.info ? info < other.info : owner < other.owner; }
};

struct ReflectedObject
{
	void *object = nullptr;
	const StructInfo *info = nullptr;

	// Field path is like "items[1].weight" -> returns [ReflectedField(items), ReflectedField(Element_1), ReflectedField(weight)]
	eastl::vector<ReflectedField> findFieldSteps(const char *path) const
	{
		eastl::vector<ReflectedField> steps;
		const char *cursor = path;
		while (*cursor)
		{
			const FieldInfo *parent_field = steps.empty() ? nullptr : steps.back().info;
			void *parent_value = parent_field ? parent_field->getAddress(steps.back().owner) : object;

			if (*cursor == '[')
			{
				// Array element
				const ArrayInfo *array_info = parent_field ? parent_field->arrayInfo : nullptr;
				if (!array_info)
					return {};

				char *index_end = nullptr;
				long index = strtol(cursor + 1, &index_end, 10);
				bool is_valid_index = *index_end == ']' && index >= 0 && index < array_info->size(parent_value);
				if (!is_valid_index)
					return {};

				void *element = array_info->at(parent_value, index);
				steps.push_back({element, &array_info->element});
				cursor = index_end + 1;
			} else
			{
				// Member
				if (*cursor == '.')
					cursor++;

				size_t name_length = strcspn(cursor, ".[");
				eastl::string field_name(cursor, name_length);
				const StructInfo *struct_info = parent_field ? parent_field->structInfo : info;
				const FieldInfo *field = struct_info ? struct_info->findField(field_name.c_str()) : nullptr;
				if (!field || field->isCategory())
					return {};

				steps.push_back({parent_value, field});
				cursor += name_length;
			}
		}
		return steps;
	}

	ReflectedField findField(const char *path) const
	{
		eastl::vector<ReflectedField> steps = findFieldSteps(path);
		return steps.empty() ? ReflectedField{} : steps.back();
	}
};

inline eastl::string makeMemberPath(const eastl::string &path, const char *name)
{
	return path.empty() ? eastl::string(name) : path + "." + name;
}

inline eastl::string makeIndexPath(const eastl::string &path, int index)
{
	return path + "[" + eastl::to_string(index) + "]";
}
