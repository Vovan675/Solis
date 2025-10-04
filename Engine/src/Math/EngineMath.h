#pragma once
#include <unordered_map>

namespace Engine::Math
{
	inline float lerp(float a, float b, float f)
	{
		return a + (b - a) * f;
	}

	template <class T>
	inline void hash_combine(std::size_t &hash, const T &value)
	{
		std::hash<T> h;
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