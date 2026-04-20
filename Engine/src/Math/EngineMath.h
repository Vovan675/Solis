#pragma once
#include <unordered_map>
#include "glm/glm.hpp"

namespace Engine::Math
{
	// Oct-encode a unit vector
	inline glm::vec2 octEncode(glm::vec3 n)
	{
		float len = fabsf(n.x) + fabsf(n.y) + fabsf(n.z);
		float ox = n.x / len, oy = n.y / len;
		if (n.z < 0.0f)
		{
			float tx = ox, ty = oy;
			ox = (1.0f - fabsf(ty)) * (tx >= 0.0f ? 1.0f : -1.0f);
			oy = (1.0f - fabsf(tx)) * (ty >= 0.0f ? 1.0f : -1.0f);
		}
		return {ox, oy};
	}

	// Oct-decode two snorm values to a unit vector
	inline glm::vec3 octDecode(float ox, float oy)
	{
		glm::vec3 n(ox, oy, 1.0f - fabsf(ox) - fabsf(oy));
		if (n.z < 0.0f)
		{
			float tx = n.x, ty = n.y;
			n.x = (1.0f - fabsf(ty)) * (tx >= 0.0f ? 1.0f : -1.0f);
			n.y = (1.0f - fabsf(tx)) * (ty >= 0.0f ? 1.0f : -1.0f);
		}
		return glm::normalize(n);
	}


	inline float lerp(float a, float b, float f)
	{
		return a + (b - a) * f;
	}

	template <class T>
	inline void hashCombine(size_t &hash, const T &value)
	{
		eastl::hash<T> h;
		hash ^= h(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	}

	inline size_t fnv1aHash(const uint32_t* code, size_t size) {
		size /= sizeof(uint32_t);

		const size_t fnvPrime = 0x01000193; // 16777619
		const size_t offsetBasis = 0x811C9DC5; // 2166136261
		size_t hash = offsetBasis;

		const uint8_t* data = reinterpret_cast<const uint8_t*>(code);
		size_t byteSize = size * sizeof(uint32_t);

		for (size_t i = 0; i < byteSize; ++i) {
			hash ^= data[i];
			hash *= fnvPrime;
		}

		return hash;
	}
}