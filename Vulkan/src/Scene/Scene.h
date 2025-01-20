#pragma once
#include "entt/entt.hpp"
#include "Components.h"
#include "Physics/PhysicsScene.h"

class Entity;

class Scene
{
public:
	Scene();
	~Scene();

	Entity createEntity(std::string name);
	Entity createEntity(std::string name, entt::entity id);
	Entity findEntityByName(std::string name);

	void destroyEntity(entt::entity id);

	std::shared_ptr<Scene> copy();

	template<typename ...T>
	auto getEntitiesWith()
	{
		return registry.view<T...>();
	}

	void saveFile(const std::string &filename);
	void loadFile(const std::string &filename);

	static std::shared_ptr<Scene> getCurrentScene() { return current_scene; }
	static std::shared_ptr<Scene> loadScene(const std::string &filename);
	static void setCurrentScene(std::shared_ptr<Scene> scene) { current_scene = scene; }
	static void closeScene();

	void updateRuntime();
private:
	friend class Entity;
	entt::registry registry;
public:
	std::shared_ptr<PhysicsScene> physics_scene;

	static std::shared_ptr<Scene> current_scene;
};
