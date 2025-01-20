#include "pch.h"
#include "Scene.h"
#include "Entity.h"
#include "YamlExtensions.h"
#include "Model.h"

namespace YAML
{
	static YAML::Emitter &operator <<(YAML::Emitter &out, const MeshRendererComponent::MeshId mesh_id)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "ModelPath" << YAML::Value << mesh_id.model->getPath();
		out << YAML::Key << "MeshId" << YAML::Value << mesh_id.mesh_id;
		out << YAML::EndMap;
		return out;
	}

	template<>
	struct convert<MeshRendererComponent::MeshId>
	{
		static bool decode(const Node &node, MeshRendererComponent::MeshId &mesh_id)
		{
			if (!node.IsMap() || node.size() != 2)
				return false;

			std::string model_path = node["ModelPath"].as<std::string>();
			auto model = AssetManager::getModelAsset(model_path);
			mesh_id.model = model;
			size_t model_mesh_id = node["MeshId"].as<size_t>();
			mesh_id.mesh_id = model_mesh_id;

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
			out << YAML::Key << "Position" << YAML::Value << comp.position;
			out << YAML::Key << "Rotation" << YAML::Value << comp.rotation;
			out << YAML::Key << "RotationEuler" << YAML::Value << comp.rotation_euler;
			out << YAML::Key << "Scale" << YAML::Value << comp.scale;
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
			out << YAML::Key << "Radius" << YAML::Value << comp.radius;

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

std::shared_ptr<Scene> Scene::current_scene;

using namespace physx;

Scene::Scene()
{
	physics_scene = std::make_shared<PhysicsScene>(this);
}

Scene::~Scene()
{
}

Entity Scene::createEntity(std::string name)
{
	return createEntity(name, entt::null);
}

Entity Scene::createEntity(std::string name, entt::entity id)
{
	entt::entity entity_id;
	if (id != entt::null)
		entity_id = registry.create(id);
	else
		entity_id = registry.create();

	auto &transform_component = registry.emplace<TransformComponent>(entity_id);
	transform_component.name = name;

	return Entity(entity_id);
}

Entity Scene::findEntityByName(std::string name)
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

std::shared_ptr<Scene> Scene::copy()
{
	auto scene = std::make_shared<Scene>();

	// Copy all entities & components
	auto view = registry.view<TransformComponent>();

	for (auto [e, transform] : view.each())
	{
		scene->createEntity(transform.name, e);
	}

	copy_component<TransformComponent, MeshRendererComponent, LightComponent, RigidBodyComponent, BoxColliderComponent>(registry, scene->registry);
	return scene;
}

void Scene::saveFile(const std::string &filename)
{
	std::ofstream file(filename);
	YAML::Emitter out(file);
	
	out << YAML::BeginSeq;
	auto entities_id = registry.group<entt::entity>();
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id);
		out << entity;
	}
	out << YAML::EndSeq;
}

void Scene::loadFile(const std::string &filename)
{
	std::ifstream file(filename);
	YAML::Node node = YAML::LoadFile(filename);
	
	for (auto entity : node)
	{
		entt::entity entity_id = entity["Entity"].as<entt::entity>();
		Entity engine_entity = createEntity("", entity_id);
		
		auto comp = entity["TransformComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<TransformComponent>();
			c.name = comp["Name"].as<std::string>();
			c.position = comp["Position"].as<glm::vec3>();
			c.rotation = comp["Rotation"].as<glm::quat>();
			c.rotation_euler = comp["RotationEuler"].as<glm::vec3>();
			c.scale = comp["Scale"].as<glm::vec3>();
			c.parent = comp["Parent"].as<entt::entity>();
			c.children = comp["Children"].as<std::vector<entt::entity>>();
		}

		comp = entity["MeshRendererComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<MeshRendererComponent>();
			c.meshes = comp["Meshes"].as<std::vector<MeshRendererComponent::MeshId>>();
			c.materials = comp["Materials"].as<std::vector<std::shared_ptr<Material>>>();
		}

		comp = entity["LightComponent"];
		if (comp)
		{
			auto &c = engine_entity.addComponent<LightComponent>();
			c.setType((LIGHT_TYPE)comp["Type"].as<int>());
			c.color = comp["Color"].as<glm::vec3>();
			c.intensity = comp["Intensity"].as<float>();
			c.radius = comp["Radius"].as<float>();
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

std::shared_ptr<Scene> Scene::loadScene(const std::string &filename)
{
	current_scene = std::make_shared<Scene>();
	current_scene->loadFile(filename);
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
