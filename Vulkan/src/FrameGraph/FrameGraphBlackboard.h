#pragma once
#include <typeindex>
#include <any>

class FrameGraphBlackboard
{
public:
	FrameGraphBlackboard() = default;
	FrameGraphBlackboard(const FrameGraphBlackboard &) = default;
	FrameGraphBlackboard(FrameGraphBlackboard &&) noexcept = default;
	~FrameGraphBlackboard() = default;

	FrameGraphBlackboard &operator=(const FrameGraphBlackboard &) = default;
	FrameGraphBlackboard &operator=(FrameGraphBlackboard &&) noexcept = default;

	template <typename T, typename... Args>
	T &add(Args &&...args)
	{
		return objects[typeid(T)].emplace<T>(std::forward<Args>(args)...);
	}

	template <typename T>
	T &get()
	{
		return std::any_cast<T &>(objects.at(typeid(T)));
	}

	template <typename T>
	T *tryGet()
	{
		auto it = objects.find(typeid(T));
		if (it == objects.end())
			return nullptr;

		auto &ref = std::any_cast<T &>(it->second);
		return &ref;
	}

private:
	std::unordered_map<std::type_index, std::any> objects;
};