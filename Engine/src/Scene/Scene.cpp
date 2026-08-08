#include "pch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "Utils/YamlExtensions.h"
#include "Rendering/Model.h"

namespace YAML
{
	static YAML::Emitter &operator <<(YAML::Emitter &out, const MeshRendererComponent::MeshId mesh_id)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "ModelGuid" << YAML::Value << (uint64_t)mesh_id.model->asset_handle;
		out << YAML::Key << "ModelPath" << YAML::Value << mesh_id.model->getPath(); // For fallback
		out << YAML::Key << "MeshId" << YAML::Value << mesh_id.mesh_id;
		out << YAML::EndMap;
		return out;
	}

	template<>
	struct convert<MeshRendererComponent::MeshId>
	{
		static bool decode(const Node &node, MeshRendererComponent::MeshId &mesh_id)
		{
			if (!node.IsMap())
				return false;

			Ref<Model> model;
			if (node["ModelGuid"])
				model = AssetManager::getModelAssetByGuid(node["ModelGuid"].as<uint64_t>());
			if (!model && node["ModelPath"])
				model = AssetManager::getModelAsset(node["ModelPath"].as<eastl::string>());

			if (!model)
				return false;

			mesh_id.model = model;
			mesh_id.mesh_id = node["MeshId"].as<size_t>();

			return true;
		}
	};

	static YAML::Emitter &operator <<(YAML::Emitter &out, const Entity entity)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Entity" << YAML::Value << entity.getID();

		if (entity.hasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto &comp = entity.getComponent<TransformComponent>();
			out << YAML::Key << "Name" << YAML::Value << comp.name;
			out << YAML::Key << "Position" << YAML::Value << comp.getLocalPosition();
			out << YAML::Key << "Rotation" << YAML::Value << comp.getLocalRotation();
			out << YAML::Key << "RotationEuler" << YAML::Value << comp.getLocalRotationEuler();
			out << YAML::Key << "Scale" << YAML::Value << comp.getLocalScale();
			out << YAML::Key << "Parent" << YAML::Value << comp.parent;
			out << YAML::Key << "Children" << YAML::Value << comp.children;

			out << YAML::EndMap;
		}

		if (entity.hasComponent<MeshRendererComponent>())
		{
			out << YAML::Key << "MeshRendererComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto &comp = entity.getComponent<MeshRendererComponent>();
			out << YAML::Key << "Meshes" << YAML::Value << comp.meshes;
			out << YAML::Key << "Materials" << YAML::Value << comp.materials;

			out << YAML::EndMap;
		}

		if (entity.hasComponent<LightComponent>())
		{
			out << YAML::Key << "LightComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto &comp = entity.getComponent<LightComponent>();
			out << YAML::Key << "Type" << YAML::Value << comp.getType();
			out << YAML::Key << "Color" << YAML::Value << comp.color;
			out << YAML::Key << "Intensity" << YAML::Value << comp.intensity;
			out << YAML::Key << "AttenuationRadius" << YAML::Value << comp.attenuation_radius;

			out << YAML::EndMap;
		}

		if (entity.hasComponent<RigidBodyComponent>())
		{
			out << YAML::Key << "RigidBodyComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto &comp = entity.getComponent<RigidBodyComponent>();
			out << YAML::Key << "IsStatic" << YAML::Value << comp.is_static;
			out << YAML::Key << "Mass" << YAML::Value << comp.mass;
			out << YAML::Key << "LinearDamping" << YAML::Value << comp.linear_damping;
			out << YAML::Key << "AngularDamping" << YAML::Value << comp.angular_damping;
			out << YAML::Key << "Gravity" << YAML::Value << comp.gravity;
			out << YAML::Key << "IsKinematic" << YAML::Value << comp.is_kinematic;

			out << YAML::EndMap;
		}

		if (entity.hasComponent<BoxColliderComponent>())
		{
			out << YAML::Key << "BoxColliderComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto &comp = entity.getComponent<BoxColliderComponent>();
			out << YAML::Key << "HalfExtent" << YAML::Value << comp.half_extent;

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
		return out;
	}
}

Ref<Scene> Scene::current_scene;

using namespace physx;

Scene::Scene()
{
	physics_scene = new PhysicsScene(this);
}

Scene::~Scene()
{
}

Entity Scene::createEntity(eastl::string name)
{
	return createEntity(name, entt::null);
}

Entity Scene::createEntity(eastl::string name, entt::entity id)
{
	entt::entity entity_id;
	if (id != entt::null)
		entity_id = registry.create(id);
	else
		entity_id = registry.create();

	auto &transform_component = registry.emplace<TransformComponent>(entity_id);
	transform_component.name = name;
	transform_component.owner = entity_id;

	return Entity(entity_id);
}

void Scene::markDirty(entt::entity entity, uint32_t flags)
{
	uint32_t &f = dirty_flags[entity];
	if (f == 0)
		dirty_list.push_back(entity);
	f |= flags;
}

uint32_t Scene::getDirtyFlags(entt::entity entity) const
{
	auto it = dirty_flags.find(entity);
	return it != dirty_flags.end() ? it->second : 0;
}

void Scene::clearDirty()
{
	for (entt::entity entity_id : dirty_list)
	{
		TransformComponent &transform = registry.get<TransformComponent>(entity_id);
		transform.old_world_transform = transform.world_transform;
	}
	dirty_flags.clear();
	dirty_list.clear();
}

Entity Scene::findEntityByName(eastl::string name)
{
	auto view = registry.view<TransformComponent>();
	for (auto [e, t] : view.each())
	{
		if (t.name == name)
			return Entity(e);
	}
	return Entity();
}

void Scene::destroyEntity(entt::entity id)
{
	Entity entity(id);
	
	if (entity.getParent())
		entity.getParent().removeChild(id);

	auto children = entity.getChildren();
	for(auto &child : children)
		destroyEntity(child);

	registry.destroy(id);
}

template<typename... Component>
static void copy_component(entt::registry &src, entt::registry &dst)
{
	([&]()
	{
		auto view = src.view<Component>();
		for (auto src_entity : view)
		{
			auto &src_component = src.get<Component>(src_entity);
			dst.emplace_or_replace<Component>(src_entity, src_component);
		}
	}(), ...);
}

Ref<Scene> Scene::copy()
{
	auto scene = new Scene();

	// Copy all entities & components
	auto view = registry.view<TransformComponent>();

	for (auto [e, transform] : view.each())
	{
		scene->createEntity(transform.name, e);
	}

	copy_component<TransformComponent, MeshRendererComponent, LightComponent, RigidBodyComponent, BoxColliderComponent>(registry, scene->registry);
	return scene;
}

void Scene::saveFile(const eastl::string &filename)
{
	std::ofstream file(filename.c_str());
	YAML::Emitter out(file);
	
	out << YAML::BeginSeq;
	for (entt::entity entity_id : registry.view<entt::entity>())
	{
		Entity entity(entity_id);
		out << entity;
	}
	out << YAML::EndSeq;
}

void Scene::loadFile(const eastl::string &filename)
{
	std::ifstream file(filename.c_str());
	YAML::Node node = YAML::LoadFile(filename.c_str());
	
	for (auto entity : node)
	{
		entt::entity entity_id = entity["Entity"].as<entt::entity>();
		Entity engine_entity = createEntity("", entity_id);
		
		auto comp = entity["TransformComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<TransformComponent>();
			c.name = comp["Name"].as<eastl::string>();
			c.setPosition(comp["Position"].as<glm::vec3>());
			c.setLocalRotation(comp["Rotation"].as<glm::quat>());
			c.setLocalRotationEuler(comp["RotationEuler"].as<glm::vec3>());
			c.setLocalScale(comp["Scale"].as<glm::vec3>());
			c.parent = comp["Parent"].as<entt::entity>();
			c.children = comp["Children"].as<eastl::vector<entt::entity>>();
		}

		comp = entity["MeshRendererComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<MeshRendererComponent>();
			c.meshes = comp["Meshes"].as<eastl::vector<MeshRendererComponent::MeshId>>();
			c.materials = comp["Materials"].as<eastl::vector<Ref<Material>>>();
		}

		comp = entity["LightComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<LightComponent>();
			c.setType((LIGHT_TYPE)comp["Type"].as<int>());
			c.color = comp["Color"].as<glm::vec3>();
			c.intensity = comp["Intensity"].as<float>();
			c.attenuation_radius = comp["AttenuationRadius"].as<float>();
		}

		comp = entity["RigidBodyComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<RigidBodyComponent>();
			c.is_static = comp["IsStatic"].as<bool>();
			c.mass = comp["Mass"].as<float>();
			c.linear_damping = comp["LinearDamping"].as<float>();
			c.angular_damping = comp["AngularDamping"].as<float>();
			c.gravity = comp["Gravity"].as<bool>();
			c.is_kinematic = comp["IsKinematic"].as<bool>();
		}

		comp = entity["BoxColliderComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<BoxColliderComponent>();
			c.half_extent = comp["HalfExtent"].as<glm::vec3>();
		}
	}
}

Ref<Scene> Scene::loadScene(const eastl::string &filename)
{
	current_scene = new Scene();
	current_scene->loadFile(filename.c_str());
	return current_scene;
}

void Scene::closeScene()
{
	current_scene = nullptr;
}

void Scene::updateRuntime()
{
	physics_scene->simulate();
}

void Scene::propagate_world_transforms_update(entt::entity entity_id)
{
	Entity entity(entity_id);
	TransformComponent &transform = entity.getComponent<TransformComponent>();

	// World transformation was changed
	if (transform.parent == entt::null)
	{
		transform.setLocalTransform(transform.world_transform);
	} else
	{
		Entity parent(transform.parent);
		const glm::mat4 &parent_inverse_transform = parent.getTransform().getInverseWorldTransform();
		transform.setLocalTransform(parent_inverse_transform * transform.world_transform);
	}
	transform.inverse_world_transform = glm::inverse(transform.world_transform);

	if (current_scene)
		current_scene->markDirty(entity_id, DIRTY_TRANSFORM);

	auto children = entity.getChildren();
	for (auto &child_id : children)
	{
		propagate_local_transforms_update(child_id);
	}
}

void Scene::propagate_local_transforms_update(entt::entity entity_id)
{
	Entity entity(entity_id);
	TransformComponent &transform = entity.getComponent<TransformComponent>();

	// Local transformation was changed
	if (transform.parent == entt::null)
	{
		transform.world_transform = transform.getLocalTransform();
	} else
	{
		Entity parent(transform.parent);
		const glm::mat4 &parent_transform = parent.getTransform().getWorldTransform();
		transform.world_transform = parent_transform * transform.getLocalTransform();
	}
	transform.inverse_world_transform = glm::inverse(transform.world_transform);

	// Init old transform first time
	if (!transform.old_world_transform_set)
	{
		transform.old_world_transform = transform.world_transform;
		transform.old_world_transform_set = true;
	}

	if (current_scene)
		current_scene->markDirty(entity_id, DIRTY_TRANSFORM);

	auto children = entity.getChildren();
	for (auto &child_id : children)
	{
		propagate_local_transforms_update(child_id);
	}
}
