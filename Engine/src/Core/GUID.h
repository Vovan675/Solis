#pragma once
#include <memory>
#include <xhash>

namespace Engine
{
	class GUID
	{
	public:
		GUID();
		GUID(uint64_t guid);
		GUID(const Engine::GUID &other);

		operator uint64_t() { return guid; }
		operator const uint64_t() const { return guid; }

		bool operator ==(const Engine::GUID &other) const { return other.guid == guid; }

		bool isValid() const { return guid != 0; }

	private:
		uint64_t guid = 0;
	};
}

namespace eastl
{
	template <>
	struct hash<Engine::GUID>
	{
		std::size_t operator()(const Engine::GUID& guid) const
		{
			return guid;
		}
	};
}