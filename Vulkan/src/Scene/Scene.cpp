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

		out << YAML::EndMap;
		return out;
	}
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

	return Entity(entity_id, this);
}

void Scene::destroyEntity(entt::entity id)
{
	Entity entity(id, this);
	
	auto children = entity.getChildren();
	for(auto &child : children)
		destroyEntity(child);

	registry.destroy(id);
}

void Scene::saveFile(const std::string &filename)
{
	std::ofstream file(filename);
	YAML::Emitter out(file);
	
	out << YAML::BeginSeq;
	auto entities_id = registry.group<entt::entity>();
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id, this);
		out << entity;
	}
	out << YAML::EndSeq;
}

void Scene::loadFile(const std::string &filename)
{
	std::ifstream file(filename);
	YAML::Node node = YAML::LoadFile(filename);;
	
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
	}
}
