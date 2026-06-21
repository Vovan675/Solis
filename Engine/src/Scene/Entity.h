#pragma once
#include "Rendering/Mesh.h"
#include <entt/entt.hpp>
#include "Rendering/Material.h"
#include <Scene/Scene.h>
#include "Scene/Components.h"

// Entity is just a interface for working with components
class Entity
{
public:
	Entity() = default;
	Entity(entt::entity entity_id) : entity_id(entity_id), scene(Scene::current_scene) {}

	// TODO: return UUID
	uint32_t getID() const { return (uint32_t)entity_id; }

	Scene *getScene() const { return scene; }

	TransformComponent &getTransform() const { return getComponent<TransformComponent>(); }

	const glm::mat4 &getWorldTransformMatrix() const
	{
		return getTransform().getWorldTransform();
	}

	glm::mat4 getWorldInverseTransformMatrix() const
	{
		return glm::inverse(getWorldTransformMatrix());
	}

	glm::vec3 getLocalDirection(glm::vec3 direction)
	{
		return getTransform().getLocalDirection(direction);
	}

	Entity getParent() const { return Entity(getTransform().parent); }
	eastl::vector<entt::entity> getChildren() const { return getTransform().children; }

	void removeChild(entt::entity child)
	{
		auto &childs = getTransform().children;
		auto found = eastl::find(childs.begin(), childs.end(), child);
		if (found != childs.end())
			childs.erase(found);
	}

	template<typename T, typename ...Args>
	T &addComponent(Args &&...args) const
	{
		if (hasComponent<T>()) return getComponent<T>();
		return scene->registry.emplace_or_replace<T>(entity_id, args...);
	}

	void markDirty(uint32_t flags) const
	{
		scene->markDirty(entity_id, flags);
	}

	template<typename T>
	void removeComponent() const
	{
		scene->registry.remove<T>(entity_id);
	}

	template<typename T>
	T &getComponent() const
	{
		return scene->registry.get<T>(entity_id);
	}

	template<typename T>
	bool hasComponent() const
	{
		return scene->registry.all_of<T>(entity_id);
	}

	operator bool() const;
	operator entt::entity() const { return entity_id; };

	bool operator==(const Entity& other) const
	{
		return other.entity_id == entity_id && other.scene == scene;
	}

	bool operator==(const entt::entity& other) const
	{
		return other == entity_id;
	}

private:
	entt::entity entity_id = entt::null;
	Scene *scene;
};

