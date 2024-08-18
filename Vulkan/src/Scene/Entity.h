#pragma once
#include "Mesh.h"
#include <entt/entt.hpp>
#include "Material.h"
#include <Scene/Scene.h>

// Entity is just a interface for working with components
class Entity
{
public:
	Entity() = default;
	Entity(entt::entity entity_id, Scene *scene) : entity_id(entity_id), scene(scene) {}
	void updateTransform();

	Scene* getScene() const { return scene; }

	TransformComponent &getTransform() const { return getComponent<TransformComponent>(); }

	glm::mat4 getWorldTransformMatrix() const
	{
		auto &transform_component = getTransform();
		glm::mat4 world_transform = transform_component.getTransformMatrix();
		if (transform_component.parent != entt::null)
		{
			Entity parent(transform_component.parent, scene);
			world_transform = parent.getWorldTransformMatrix() * world_transform;
		}

		return world_transform;
	}

	glm::vec3 getLocalDirection(glm::vec3 direction)
	{
		glm::vec3 scale, position, skew;
		glm::vec4 persp;
		glm::quat rotation;
		glm::decompose(getWorldTransformMatrix(), scale, rotation, position, skew, persp);
		return normalize(rotation * direction);
	}

	Entity getParent() const { return Entity(getTransform().parent, scene); }

	template<typename T, typename ...Args>
	T &addComponent(Args &&...args) const
	{
		return scene->registry.emplace_or_replace<T>(entity_id, args...);
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

private:
	entt::entity entity_id = entt::null;
	Scene* scene = nullptr;
};

