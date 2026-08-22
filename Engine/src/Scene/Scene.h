#pragma once
#include "entt/entt.hpp"
#include "Physics/PhysicsScene.h"

class Entity;

enum DirtyFlags : uint32_t
{
	DIRTY_TRANSFORM = 1 << 0,
	DIRTY_RENDER_STATE = 1 << 1,
	DIRTY_LIGHT = 1 << 2,
	DIRTY_MATERIAL = 1 << 3,
};

class Scene : public RefCounted
{
public:
	Scene();
	~Scene();

	Entity createEntity(eastl::string name);
	Entity createEntity(eastl::string name, entt::entity id);
	Entity findEntityByName(eastl::string name);

	void destroyEntity(entt::entity id);

	void markDirty(entt::entity entity, uint32_t channels);
	uint32_t getDirtyFlags(entt::entity entity) const;
	const eastl::vector<entt::entity> &getDirtyList() const { return dirty_list; }
	void clearDirty();

	Ref<Scene> copy();

	template<typename ...T>
	auto getEntitiesWith()
	{
		return registry.view<T...>();
	}

	void saveFile(const eastl::string &filename);
	void loadFile(const eastl::string &filename);

	static Ref<Scene> getCurrentScene() { return current_scene; }
	static Ref<Scene> loadScene(const eastl::string &filename);
	static void setCurrentScene(Ref<Scene> scene) { current_scene = scene; }
	static void closeScene();

	void updateRuntime();
private:
	friend class SceneRenderer;

	static void propagate_world_transforms_update(entt::entity entity_id);
	static void propagate_local_transforms_update(entt::entity entity_id);

	friend class Entity;
	friend class TransformComponent;
	entt::registry registry;

	eastl::hash_map<entt::entity, uint32_t> dirty_flags;
	eastl::vector<entt::entity> dirty_list;
public:
	Ref<PhysicsScene> physics_scene;

	static Ref<Scene> current_scene;
};
