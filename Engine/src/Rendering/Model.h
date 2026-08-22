#pragma once
#include "Rendering/Mesh.h"
#include "Utils/FileMemory.h"
#include "Scene/Entity.h"
#include "Rendering/Material.h"

struct MeshNode
{
	struct Primitive
	{
		Engine::Mesh *mesh = nullptr;
		Ref<Material> material;
	};

	~MeshNode()
	{
		for (size_t i = 0; i < children.size(); i++)
			delete children[i];
	}

	eastl::string name = "";
	eastl::vector<Primitive> primitives;
	MeshNode *parent = nullptr;
	eastl::vector<MeshNode *> children;

	glm::mat4 local_model_matrix = glm::mat4(1);
	glm::mat4 global_model_matrix = glm::mat4(1);

	void updateTransform()
	{
		if (parent)
			global_model_matrix = parent->global_model_matrix * local_model_matrix;
		else
			global_model_matrix = local_model_matrix;

		for (auto &child : children)
			child->updateTransform();
	}
};

// Asset representing one imported model file.
// Always loaded from .mesh binary format
class Model: public Asset
{
public:
	Model() = default;
	~Model();
	void cleanup();


	void load(const char *path);
	void reload() override;

	static Entity createEntity(Model *model);

	Material *getMaterial(size_t id)
	{
		auto it = materials_id.find(id);
		return it == materials_id.end() ? nullptr : it->second.getReference();
	}

	Engine::Mesh *getMesh(size_t id)
	{
		auto it = meshes_id.find(id);
		return it == meshes_id.end() ? nullptr : it->second;
	}

	void getMeshes(eastl::vector<Ref<Engine::Mesh>> &out) const
	{
		for (auto &[id, mesh] : meshes_id)
			out.push_back(mesh);
	}

	MeshNode *getRootNode() const { return root_node; }
	eastl::vector<MeshNode *> &getLinearNodes() { return linear_nodes; }
	eastl::string getPath() const { return path; }

	const Engine::MeshletFileView *getFileView(size_t mesh_id) const
	{
		auto it = file_views.find(mesh_id);
		return it != file_views.end() ? &it->second : nullptr;
	}

private:
	friend class MeshSerializer;
	friend class GltfImporter;
	friend class AssimpImporter;

	static void assign_mesh_id(eastl::unordered_map<size_t, Ref<Engine::Mesh>> &meshes_id,
								Engine::Mesh *mesh,
								const eastl::string &node_name,
								const eastl::string &prim_name);

	static Entity create_entity_node(Model *model, MeshNode *node, Scene *scene);

	MeshNode *root_node = nullptr;
	eastl::vector<MeshNode *> linear_nodes = {};
	eastl::string path;

	FileMemory mesh_file_memory;
	eastl::unordered_map<size_t, Engine::MeshletFileView> file_views;

	eastl::unordered_map<size_t, Ref<Engine::Mesh>> meshes_id;
	eastl::unordered_map<size_t, Ref<Material>> materials_id;
};
