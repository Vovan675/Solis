#include "pch.h"
#include "Scene.h"
#include "Entity.h"
#include "CerealExtensions.h"
#include <cereal/archives/json.hpp>
#include "Model.h"

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
	registry.destroy(id);
}

void Scene::saveFile(const std::string &filename)
{
	std::ofstream file(filename);
	cereal::JSONOutputArchive archive(file);
	save(archive);
}

void Scene::loadFile(const std::string &filename)
{
	std::ifstream file(filename);
	cereal::JSONInputArchive archive(file);
	load(archive);
}

template<class Archive>
static void save(Archive &archive, const MeshRendererComponent::MeshId &mesh_id)
{
	archive(cereal::make_nvp("model_path", mesh_id.model->getPath()));
	archive(cereal::make_nvp("mesh_id", mesh_id.mesh_id));
}

template<class Archive>
static void load(Archive &archive, MeshRendererComponent::MeshId &mesh_id)
{
	std::string model_path;
	archive(cereal::make_nvp("model_path", model_path));
	// TODO: fix LEAK, maybe create manager
	Model *model = new Model();
	model->load(model_path.c_str());
	mesh_id.model = model;
	archive(cereal::make_nvp("mesh_id", mesh_id.mesh_id));
}

template<class Archive>
static void save(Archive &archive, const LightComponent &light)
{
	archive(cereal::make_nvp("color", light.color));
	archive(cereal::make_nvp("intensity", light.intensity));
	archive(cereal::make_nvp("radius", light.radius));
}

template<class Archive>
static void load(Archive &archive, LightComponent &light)
{
	archive(cereal::make_nvp("color", light.color));
	archive(cereal::make_nvp("intensity", light.intensity));
	archive(cereal::make_nvp("radius", light.radius));
}

template<class Archive>
static void save(Archive &archive, const Entity &entity)
{
	auto &transform = entity.getTransform();
	archive(cereal::make_nvp("name", transform.name));
	archive(cereal::make_nvp("parent", transform.parent));
	archive(cereal::make_nvp("children", transform.children));
	archive(cereal::make_nvp("position", transform.position));
	archive(cereal::make_nvp("scale", transform.scale));
	archive(cereal::make_nvp("rotation_euler", transform.rotation_euler));
	archive(cereal::make_nvp("rotation", transform.rotation));

	// TODO: make more friendly
	size_t components_count = 0;
	if (entity.hasComponent<MeshRendererComponent>()) components_count++;
	if (entity.hasComponent<LightComponent>()) components_count++;

	archive(cereal::make_nvp("components_count", components_count));

	if (entity.hasComponent<MeshRendererComponent>())
	{
		std::string type = "MeshRendererComponent";
		archive(type);
		auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
		archive(mesh_renderer.meshes);
	}

	if (entity.hasComponent<LightComponent>())
	{
		std::string type = "LightComponent";
		archive(type);
		auto &component = entity.getComponent<LightComponent>();
		archive(component);
	}
}

template<class Archive>
static void load(Archive &archive, Entity &entity)
{
	auto &transform = entity.getTransform();
	archive(cereal::make_nvp("name", transform.name));
	archive(cereal::make_nvp("parent", transform.parent));
	archive(cereal::make_nvp("children", transform.children));
	archive(cereal::make_nvp("position", transform.position));
	archive(cereal::make_nvp("scale", transform.scale));
	archive(cereal::make_nvp("rotation_euler", transform.rotation_euler));
	archive(cereal::make_nvp("rotation", transform.rotation));

	size_t components_count = 0;
	archive(cereal::make_nvp("components_count", components_count));

	for (size_t i = 0; i < components_count; i++)
	{
		std::string type;
		archive(type);

		if (type == "MeshRendererComponent")
		{
			auto &mesh_renderer = entity.addComponent<MeshRendererComponent>();
			archive(mesh_renderer.meshes);
		} else if(type == "LightComponent")
		{
			auto &component = entity.addComponent<LightComponent>();
			archive(component);
		}
	}
}


static struct EntityItem
{
	entt::entity entity_id = entt::null;
	Entity entity;
	Scene &scene;

	EntityItem(Scene &scene): scene(scene) {}
	EntityItem(entt::entity entity_id, Entity entity, Scene &scene): entity_id(entity_id), entity(entity), scene(scene) {}

	template <class Archive>
	inline void save(Archive &archive) const
	{
		archive(cereal::make_nvp<Archive>("entity_id", entity_id),
				cereal::make_nvp<Archive>("entity", entity));
	}

	template <class Archive>
	inline void load(Archive &archive)
	{
		archive(cereal::make_nvp<Archive>("entity_id", entity_id));
		entity = scene.createEntity("", entity_id);
		archive(cereal::make_nvp<Archive>("entity", entity));
	}
};

template<class Archive>
void Scene::save(Archive &archive)
{
	auto entities_id = registry.group<entt::entity>();
	archive(cereal::make_size_tag(entities_id.size()));
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id, this);
		archive(EntityItem(entity_id, entity, *this));
	}
}

template<class Archive>
void Scene::load(Archive &archive)
{
	size_t entities_count;
	archive(cereal::make_size_tag(entities_count));
	for(size_t i = 0; i < entities_count; i++)
	{
		EntityItem entity_item(*this);
		archive(entity_item);
	}
}
