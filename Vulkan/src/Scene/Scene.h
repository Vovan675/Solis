#pragma once
#include "entt/entt.hpp"
#include "Components.h"
#include "Physics/PhysicsScene.h"

class Entity;

class Scene : public RefCounted
{
public:
	Scene();
	~Scene();

	Entity createEntity(std::string name);
	Entity createEntity(std::string name, entt::entity id);
	Entity findEntityByName(std::string name);

	void destroyEntity(entt::entity id);

	Ref<Scene> copy();

	template<typename ...T>
	auto getEntitiesWith()
	{
		return registry.view<T...>();
	}

	void saveFile(const std::string &filename);
	void loadFile(const std::string &filename);

	static Ref<Scene> getCurrentScene() { return current_scene; }
	static Ref<Scene> loadScene(const std::string &filename);
	static void setCurrentScene(Ref<Scene> scene) { current_scene = scene; }
	static void closeScene();

	void updateRuntime();
private:
	friend class Entity;
	entt::registry registry;
public:
	Ref<PhysicsScene> physics_scene;

	static Ref<Scene> current_scene;
};
