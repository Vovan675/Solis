#include "pch.h"
#include "GUID.h"
#include <random>

namespace Engine
{
	static std::random_device random_device;
	static std::mt19937_64 random_engine(random_device());
	static std::uniform_int_distribution<uint64_t> random_distribution;

	GUID::GUID(uint64_t guid): guid(guid)
	{}

	GUID::GUID(const GUID &other): guid(other)
	{}

	GUID GUID::generate()
	{
		return GUID(random_distribution(random_engine));
	}
}