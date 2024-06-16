#pragma once
#include "Entity.h"

class Scene
{
public:
	Scene() = delete;

	static void addEntity(std::shared_ptr<Entity> entity) { root_entities.push_back(entity); }
	static std::vector<std::shared_ptr<Entity>>& getRootEntites() { return root_entities; }
private:
	static std::vector<std::shared_ptr<Entity>> root_entities;
};

