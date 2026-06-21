#include "pch.h"
#include "Rendering/Model.h"
#include "Assets/MeshSerializer.h"
#include "Assets/AssetManager.h"
#include "Rendering/Material.h"
#include "Scene/Components.h"
#include "Math/EngineMath.h"

Model::~Model()
{
	cleanup();
}

void Model::cleanup()
{
	if (root_node)
		delete root_node;
	root_node = nullptr;
	linear_nodes.clear();
	meshes_id.clear();
	mesh_file_memory.close();
}

void Model::load(const char *path)
{
	PROFILE_CPU_FUNCTION();

	this->path = path;
	auto mesh_path = AssetManager::getRuntimeAssetPath(std::filesystem::path(path));
	MeshSerializer::load(this, mesh_path.string().c_str());
}

void Model::reload()
{
	std::filesystem::path current = AssetManager::getPathFromGUID(asset_handle);
	if (current.empty())
		current = path.c_str();

	cleanup();
	AssetManager::recreateRuntime(current);
	load(current.string().c_str());
}

void Model::assign_mesh_id(eastl::unordered_map<size_t, Ref<Engine::Mesh>> &meshes_id, Engine::Mesh *mesh, const eastl::string &node_name, const eastl::string &prim_name)
{
	int id = 0;
	size_t hash;
	do
	{
		++id;
		hash = 0;
		Engine::Math::hashCombine(hash, node_name);
		Engine::Math::hashCombine(hash, prim_name);
		Engine::Math::hashCombine(hash, id);
	} while (meshes_id.find(hash) != meshes_id.end());
	meshes_id[hash] = mesh;
	mesh->id = hash;
}

Entity Model::createEntity(Model *model)
{
	return create_entity_node(model, model->root_node, Scene::getCurrentScene());
}

Entity Model::create_entity_node(Model *model, MeshNode *node, Scene *scene)
{
	Entity entity = scene->createEntity(node->name);
	auto &transform_component = entity.getComponent<TransformComponent>();

	if (!node->primitives.empty())
	{
		auto &mesh_renderer = entity.addComponent<MeshRendererComponent>();
		for (const MeshNode::Primitive &prim : node->primitives)
		{
			MeshRendererComponent::MeshId mesh_id;
			mesh_id.model = model;
			mesh_id.mesh_id = prim.mesh->id;
			mesh_renderer.meshes.push_back(mesh_id);
			mesh_renderer.materials.push_back(prim.material);
		}
		entity.markDirty(DIRTY_RENDER_STATE);
	}

	transform_component.setLocalTransform(node->local_model_matrix);

	for (int i = 0; i < node->children.size(); i++)
	{
		Entity child = create_entity_node(model, node->children[i], scene);
		auto &child_transform_component = child.getComponent<TransformComponent>();
		child_transform_component.parent = entity;
		transform_component.children.push_back(child);
	}
	return entity;
}
