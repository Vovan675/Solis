#include "pch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "Utils/YamlExtensions.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"

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
static void write_components(YAML::Emitter &out, Entity entity)
{
	([&]()
	{
		if (!entity.hasComponent<Component>())
			return;

		const StructInfo &info = Reflected<Component>::getInfo();
		out << YAML::Key << info.name << YAML::Value << YAML::BeginMap;
		ReflectionYaml::writeFields(out, info, &entity.getComponent<Component>(), nullptr);
		out << YAML::EndMap;
	}(), ...);
}

template<typename... Component>
static void read_components(const YAML::Node &node, Entity entity)
{
	([&]()
	{
		const StructInfo &info = Reflected<Component>::getInfo();
		YAML::Node component_node = node[info.name];
		if (!component_node)
			return;

		Component &component = entity.addComponent<Component>();
		ReflectionYaml::readFields(component_node, info, &component);
	}(), ...);
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

	copy_component<ALL_COMPONENTS>(registry, scene->registry);
	return scene;
}

void Scene::saveFile(const eastl::string &filename)
{
	std::ofstream file(filename.c_str());
	YAML::Emitter out(file);

	out << YAML::BeginMap;
	out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	for (entt::entity entity_id : registry.view<entt::entity>())
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Entity" << YAML::Value << (uint32_t)entity_id;
		write_components<ALL_COMPONENTS>(out, Entity(entity_id));
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;

	if (Camera *camera = Renderer::getCamera())
	{
		out << YAML::Key << "Camera" << YAML::Value << YAML::BeginMap;
		ReflectionYaml::writeFields(out, Reflected<Camera>::getInfo(), camera, nullptr);
		out << YAML::EndMap;
	}

	const StructInfo &render_settings_info = Reflected<RenderSettings>::getInfo();
	out << YAML::Key << "Settings" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Render" << YAML::Value << YAML::BeginMap;
	ReflectionYaml::writeFields(out, render_settings_info, &gRenderSettings, render_settings_info.defaults);
	out << YAML::EndMap << YAML::EndMap;

	out << YAML::EndMap;
}

void Scene::loadFile(const eastl::string &filename)
{
	YAML::Node root = YAML::LoadFile(filename.c_str());

	for (auto entity : root["Entities"])
	{
		entt::entity entity_id = entt::null;
		if (YAML::Node id_node = entity["Entity"])
			entity_id = (entt::entity)id_node.as<uint32_t>();

		read_components<ALL_COMPONENTS>(entity, createEntity("", entity_id));
	}

	for (auto [entity_id, transform] : registry.view<TransformComponent>().each())
	{
		if (transform.parent == entt::null)
			propagate_local_transforms_update(entity_id);
	}

	if (Camera *camera = Renderer::getCamera())
	{
		ReflectionYaml::readFields(root["Camera"], Reflected<Camera>::getInfo(), camera);
		camera->updateMatrices();
	}

	gRenderSettings = RenderSettings();
	ReflectionYaml::readFields(root["Settings"]["Render"], Reflected<RenderSettings>::getInfo(), &gRenderSettings);
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
