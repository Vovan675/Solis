#pragma once
#include "entt/entt.hpp"
#include "Components.h"

class Entity;

class Scene
{
public:
	Scene() = default;

	Entity createEntity(std::string name);
	Entity createEntity(std::string name, entt::entity id);

	void destroyEntity(entt::entity id);

	template<typename ...T>
	auto getEntitiesWith()
	{
		return registry.view<T...>();
	}

	void saveFile(const std::string &filename);
	void loadFile(const std::string &filename);

private:
	friend class Entity;
	entt::registry registry;

	template<class Archive>
	void save(Archive &archive);

	template<class Archive>
	void load(Archive &archive);
};
