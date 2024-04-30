#pragma once
#include <unordered_map>

template <class T>
inline void hash_combine(std::size_t &hash, const T &value)
{
	std::hash<T> h;
	hash ^= h(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}