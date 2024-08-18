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
		ar(cereal::make_nvp("x", vector.x), cereal::make_nvp("y", vector.y));
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::vec3 &vector)
	{
		ar(cereal::make_nvp("x", vector.x), cereal::make_nvp("y", vector.y), cereal::make_nvp("z", vector.z));
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::vec4 &vector)
	{
		ar(cereal::make_nvp("x", vector.x), cereal::make_nvp("y", vector.y), cereal::make_nvp("z", vector.z), cereal::make_nvp("w", vector.w));
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::mat4x4 &mat)
	{
		ar(mat[0], mat[1], mat[2], mat[3]);
	}

	template<class Archive>
	static void serialize(Archive &ar, glm::quat &quat)
	{
		ar(cereal::make_nvp("x", quat.x), cereal::make_nvp("y", quat.y), cereal::make_nvp("z", quat.z), cereal::make_nvp("w", quat.w));
	}
}