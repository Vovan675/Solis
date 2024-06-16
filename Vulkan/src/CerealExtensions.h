#pragma once
#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/string.hpp"
#include "glm/glm.hpp"

namespace cereal
{
	template<class Archive>
	static void serialize(Archive &ar, glm::vec2 &vector)
	{
		ar(vector.x, vector.y);
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::vec3 &vector)
	{
		ar(vector.x, vector.y, vector.z);
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::vec4 &vector)
	{
		ar(vector.x, vector.y, vector.z, vector.w);
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::mat4x4 &mat)
	{
		ar(mat[0], mat[1], mat[2], mat[3]);
	}
}