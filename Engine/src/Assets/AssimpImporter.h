#pragma once
#include <map>
#include "Rendering/MeshletBuilder.h"

struct ModelImportSettings;
class Model;
struct MeshNode;
struct aiNode;
struct aiScene;

namespace Engine { class Mesh; }

class AssimpImporter
{
public:
	static void import(const char *path, Model *model, ModelImportSettings &settings, const std::filesystem::path &runtime_path);

private:
	static void processNode(MeshNode *mesh_node, aiNode *node, const aiScene *scene,
		const ModelImportSettings &settings, const eastl::string &source_path,
		Model *model, eastl::map<int, int> &meshes_seen,
		eastl::unordered_map<Engine::Mesh *, MeshletBuildData> &build_data_map);
};
