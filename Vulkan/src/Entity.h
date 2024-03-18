#pragma once
#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

struct Transform
{
	// Local space
	glm::vec3 pos = {0.0f, 0.0f, 0.0f};
	glm::vec3 eulerRot = {0.0f, 0.0f, 0.0f};
	glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

	glm::mat4 local_model_matrix = glm::mat4(1);

	// Global space
	glm::mat4 model_matrix = glm::mat4(1);
};

// Contains tree of nodes (tree of meshes)
class Entity
{
public:
	Transform transform;
	std::vector<std::shared_ptr<Entity>> children;
	Entity* parent = nullptr;

	std::vector<std::shared_ptr<Engine::Mesh>> meshes;
	Entity() = default;
	Entity(const char* model_path) { load_model(model_path); }
	void updateTransform();
private:
	void load_model(const char* model_path);
	void process_node(aiNode *node, const aiScene *scene);
};

