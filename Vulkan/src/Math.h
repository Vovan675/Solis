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
}