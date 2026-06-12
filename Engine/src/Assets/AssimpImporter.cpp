#include "pch.h"
#include "AssimpImporter.h"
#include "Rendering/Model.h"
#include "ModelImportSettings.h"
#include "MeshSerializer.h"
#include "Assets/AssetManager.h"
#include "Rendering/MeshletBuilder.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/material.h"
#include "assimp/pbrmaterial.h"
#include <filesystem>
#include <map>

static glm::mat4 convertAssimpMat4(const aiMatrix4x4 &m)
{
	glm::mat4 o;
	o[0][0] = m.a1; o[0][1] = m.b1; o[0][2] = m.c1; o[0][3] = m.d1;
	o[1][0] = m.a2; o[1][1] = m.b2; o[1][2] = m.c2; o[1][3] = m.d2;
	o[2][0] = m.a3; o[2][1] = m.b3; o[2][2] = m.c3; o[2][3] = m.d3;
	o[3][0] = m.a4; o[3][1] = m.b4; o[3][2] = m.c4; o[3][3] = m.d4;
	return o;
}

void AssimpImporter::processNode(MeshNode *mesh_node, aiNode *node, const aiScene *scene,
	const ModelImportSettings &settings, const eastl::string &source_path,
	Model *model, eastl::map<int, int> &meshes_seen,
	eastl::unordered_map<Engine::Mesh *, MeshletBuildData> &build_data_map)
{
	if (node->mName.length != 0)
		mesh_node->name = node->mName.C_Str();
	else if (node->mNumMeshes > 0)
		mesh_node->name = scene->mMeshes[node->mMeshes[0]]->mName.C_Str();

	mesh_node->local_model_matrix = convertAssimpMat4(node->mTransformation);

	eastl::vector<Engine::Vertex> vertices;
	eastl::vector<uint32_t> indices;

	for (int m = 0; m < (int)node->mNumMeshes; m++)
	{
		vertices.clear();
		indices.clear();

		int mesh_index = node->mMeshes[m];
		meshes_seen[mesh_index]++;
		aiMesh *mesh = scene->mMeshes[mesh_index];

		for (int v = 0; v < (int)mesh->mNumVertices; v++)
		{
			aiVector3D vertex = mesh->mVertices[v];

			glm::vec3 normal(0, 0, 0);
			glm::vec3 tangent(0, 0, 0);
			float tangent_sign = 1.0f;
			glm::vec2 uv(0, 0);
			glm::vec3 color(1, 1, 1);

			if (mesh->HasNormals())
			{
				aiVector3D aiNormal = mesh->mNormals[v];
				normal = glm::vec3(aiNormal.x, aiNormal.y, aiNormal.z);
			}
			if (mesh->HasTangentsAndBitangents())
			{
				aiVector3D aiTangent = mesh->mTangents[v];
				aiVector3D aiBitangent = mesh->mBitangents[v];
				tangent = glm::vec3(aiTangent.x, aiTangent.y, aiTangent.z);
				glm::vec3 bitangent = glm::vec3(aiBitangent.x, aiBitangent.y, aiBitangent.z);
				tangent_sign = glm::dot(glm::cross(normal, tangent), bitangent) >= 0.0f ? 1.0f : -1.0f;
			}
			if (mesh->mTextureCoords[0] != nullptr)
			{
				aiVector3D aiUV = mesh->mTextureCoords[0][v];
				uv = glm::vec2(aiUV.x, aiUV.y);
			}
			if (mesh->HasVertexColors(v))
			{
				aiColor4D *aiColor = mesh->mColors[v];
				color = glm::vec3(aiColor->r, aiColor->g, aiColor->b);
			}
			vertices.emplace_back(Engine::Vertex{{vertex.x, vertex.y, vertex.z}, normal, tangent, tangent_sign, uv});
		}

		for (int f = 0; f < (int)mesh->mNumFaces; f++)
		{
			aiFace face = mesh->mFaces[f];
			for (int i = 0; i < (int)face.mNumIndices; i++)
				indices.emplace_back(face.mIndices[i]);
		}

		Ref<Engine::Mesh> engine_mesh = new Engine::Mesh();
		if (mesh->HasTangentsAndBitangents())
			engine_mesh->attribute_flags |= MeshFormat::MESH_ATTR_TANGENT;
		if (settings.generate_meshlets)
			build_data_map[engine_mesh.getReference()] = MeshletBuilder::build(engine_mesh, mesh->mName.C_Str(), vertices, indices, settings);

		if (!engine_mesh->useMeshlets())
		{
			engine_mesh->indexed.emplace();
			engine_mesh->indexed->vertices = std::move(vertices);
			engine_mesh->indexed->indices = std::move(indices);
		}

		engine_mesh->bound_box = BoundBox(
			glm::vec3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z),
			glm::vec3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z));

		Model::assign_mesh_id(model->meshes_id, engine_mesh,
							mesh_node->name, eastl::string(mesh->mName.C_Str()));

		// Materials
		aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];

		for (int p = 0; p < (int)mat->mNumProperties; p++)
			aiString name = mat->mProperties[p]->mKey;

		Ref<Material> engine_material = new Material();
		mesh_node->primitives.push_back({engine_mesh, engine_material});

		// Textures
		unsigned int diffuse_count = mat->GetTextureCount(aiTextureType_DIFFUSE);
		if (diffuse_count > 0)
		{
			aiString texture_path;
			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == aiReturn_SUCCESS)
			{
				std::filesystem::path result_path(source_path.c_str());
				result_path = result_path.remove_filename().concat(texture_path.C_Str());
				engine_material->albedo_tex.asset_handle = AssetManager::getGUIDFromPath(result_path.string());
			}
		}

		unsigned int metalness_count = mat->GetTextureCount(aiTextureType_METALNESS);
		if (metalness_count > 0 && false) ////////////////////////////
		{
			aiString texture_path;
			if (mat->GetTexture(aiTextureType_METALNESS, 0, &texture_path) == aiReturn_SUCCESS)
			{
				std::filesystem::path result_path(source_path.c_str());
				result_path = result_path.remove_filename().concat(texture_path.C_Str());
				engine_material->metalness_tex.asset_handle = AssetManager::getGUIDFromPath(result_path.string());
			}
		}

		unsigned int roughness_count = mat->GetTextureCount(aiTextureType_SHININESS);
		if (roughness_count > 0)
		{
			aiString texture_path;
			if (mat->GetTexture(aiTextureType_SHININESS, 0, &texture_path) == aiReturn_SUCCESS)
			{
				std::filesystem::path result_path(source_path.c_str());
				result_path = result_path.remove_filename().concat(texture_path.C_Str());
				engine_material->roughness_tex.asset_handle = AssetManager::getGUIDFromPath(result_path.string());
			}
		}

		unsigned int specular_count = mat->GetTextureCount(aiTextureType_SPECULAR);
		if (specular_count > 0)
		{
			aiString texture_path;
			if (mat->GetTexture(aiTextureType_SPECULAR, 0, &texture_path) == aiReturn_SUCCESS)
			{
				std::filesystem::path result_path(source_path.c_str());
				result_path = result_path.remove_filename().concat(texture_path.C_Str());
				engine_material->specular_tex.asset_handle = AssetManager::getGUIDFromPath(result_path.string());
			}
		}

		unsigned int normals_count = mat->GetTextureCount(aiTextureType_NORMALS);
		if (normals_count > 0)
		{
			aiString texture_path;
			if (mat->GetTexture(aiTextureType_NORMALS, 0, &texture_path) == aiReturn_SUCCESS)
			{
				std::filesystem::path result_path(source_path.c_str());
				result_path = result_path.remove_filename().concat(texture_path.C_Str());
				engine_material->normal_tex.asset_handle = AssetManager::getGUIDFromPath(result_path.string());
			}
		}

		// Parameters
		aiColor3D aiColor;
		if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == aiReturn_SUCCESS)
			engine_material->albedo = {aiColor.r, aiColor.g, aiColor.b, 1.0};
		mat->Get(AI_MATKEY_METALLIC_FACTOR, engine_material->metalness);
		mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, engine_material->roughness);
		mat->Get(AI_MATKEY_SPECULAR_FACTOR, engine_material->specular);
	}

	for (int c = 0; c < (int)node->mNumChildren; c++)
	{
		aiNode *child = node->mChildren[c];
		MeshNode *child_node = new MeshNode();
		model->linear_nodes.push_back(child_node);
		child_node->parent = mesh_node;
		processNode(child_node, child, scene, settings, source_path, model, meshes_seen, build_data_map);
		mesh_node->children.push_back(child_node);
	}
}

static constexpr uintmax_t AUTO_MESHLET_SOURCE_SIZE = 1024ull * 1024 * 1024;

void AssimpImporter::import(const char *path, Model *model, ModelImportSettings &settings)
{
	PROFILE_CPU_FUNCTION();

	if (!settings.generate_meshlets_explicitly_set)
		settings.generate_meshlets = std::filesystem::file_size(path) > AUTO_MESHLET_SOURCE_SIZE;

	Assimp::Importer importer;
	const aiScene *scene = importer.ReadFile(path,
		aiProcess_CalcTangentSpace |
		aiProcess_GenSmoothNormals |
		aiProcess_GenBoundingBoxes |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_OptimizeMeshes |
		aiProcess_FlipUVs);

	if (!scene || !scene->mRootNode)
	{
		CORE_ERROR("AssimpImporter: failed to load '{}'", path);
		return;
	}

	model->root_node = new MeshNode();
	model->linear_nodes.push_back(model->root_node);

	eastl::map<int, int> meshes_seen;
	eastl::unordered_map<Engine::Mesh *, MeshletBuildData> build_data_map;
	processNode(model->root_node, scene->mRootNode, scene, settings, path, model, meshes_seen, build_data_map);
	model->root_node->updateTransform();

	auto mesh_path = AssetManager::getRuntimeAssetPath(std::filesystem::path(path));
	std::filesystem::create_directories(mesh_path.parent_path());
	MeshSerializer::save(model, mesh_path.string().c_str(), &build_data_map);
}
