#include "pch.h"
#include "Model.h"
#include <filesystem>
#include "EngineMath.h"
#include "Assets/AssetManager.h"

static glm::mat4 convertAssimpMat4(const aiMatrix4x4 &m)
{
	glm::mat4 o;
	o[0][0] = m.a1; o[0][1] = m.b1; o[0][2] = m.c1; o[0][3] = m.d1;
	o[1][0] = m.a2; o[1][1] = m.b2; o[1][2] = m.c2; o[1][3] = m.d2;
	o[2][0] = m.a3; o[2][1] = m.b3; o[2][2] = m.c3; o[2][3] = m.d3;
	o[3][0] = m.a4; o[3][1] = m.b4; o[3][2] = m.c4; o[3][3] = m.d4;
	return o;
}

Model::~Model()
{
	if (root_node)
		delete root_node;
}

void Model::load(const char *path)
{
	this->path = path;
	Assimp::Importer importer;

	const aiScene *scene = importer.ReadFile(path,
											 aiProcess_CalcTangentSpace |
											 aiProcess_GenSmoothNormals |
											 aiProcess_Triangulate |
											 aiProcess_JoinIdenticalVertices |
											 aiProcess_SortByPType);
	process_node(nullptr, scene->mRootNode, scene);
	root_node->updateTransform();
}

void Model::process_node(MeshNode *mesh_node, aiNode *node, const aiScene *scene)
{
	if (!mesh_node)
	{
		root_node = new MeshNode();
		mesh_node = root_node;
	}

	if (node->mName.length != 0)
		mesh_node->name = node->mName.C_Str();
	else if (node->mNumMeshes > 0)
		mesh_node->name = scene->mMeshes[node->mMeshes[0]]->mName.C_Str();

	std::vector<Engine::Vertex> vertices;
	std::vector<uint32_t> indices;

	mesh_node->local_model_matrix = convertAssimpMat4(node->mTransformation);
	//materials.resize(node->mNumMeshes);
	for (int m = 0; m < node->mNumMeshes; m++)
	{
		vertices.clear();
		indices.clear();

		int mesh_index = node->mMeshes[m];
		aiMesh *mesh = scene->mMeshes[mesh_index];

		for (int v = 0; v < mesh->mNumVertices; v++)
		{
			aiVector3D vertex = mesh->mVertices[v];

			glm::vec3 normal(0, 0, 0);
			glm::vec2 uv(0, 0);
			glm::vec3 color(1, 1, 1);

			if (mesh->HasNormals())
			{
				aiVector3D aiNormal = mesh->mNormals[v];
				normal = glm::vec3(aiNormal.x, aiNormal.y, aiNormal.z);
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
			vertices.emplace_back(Engine::Vertex{{vertex.x, vertex.y, vertex.z}, normal, uv, color});
		}

		for (int f = 0; f < mesh->mNumFaces; f++)
		{
			aiFace face = mesh->mFaces[f];

			for (int i = 0; i < face.mNumIndices; i++)
			{
				int index = face.mIndices[i];
				indices.emplace_back(index);
			}
		}

		std::shared_ptr<Engine::Mesh> engine_mesh = std::make_shared<Engine::Mesh>();
		engine_mesh->setData(vertices, indices);
		mesh_node->meshes.push_back(engine_mesh);

		{
			size_t mesh_hash = 0;
			int id = 0;
			std::string name = mesh_node->name;
			Engine::Math::hash_combine(mesh_hash, name);
			name = mesh->mName.C_Str();
			Engine::Math::hash_combine(mesh_hash, name);
			Engine::Math::hash_combine(mesh_hash, id);

			while (getMesh(mesh_hash) != nullptr)
			{
				id++;
				name = mesh_node->name;
				Engine::Math::hash_combine(mesh_hash, name);
				name = mesh->mName.C_Str();
				Engine::Math::hash_combine(mesh_hash, name);
				Engine::Math::hash_combine(mesh_hash, id);
			}
			meshes_id[mesh_hash] = engine_mesh;
			engine_mesh->id = mesh_hash;
		}

		aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];

		for (int p = 0; p < mat->mNumProperties; p++)
			aiString name = mat->mProperties[p]->mKey;

		std::shared_ptr<Material> engine_material = std::make_shared<Material>();
		mesh_node->materials.push_back(engine_material);
		unsigned int diffuse_count = mat->GetTextureCount(aiTextureType_DIFFUSE);
		if (diffuse_count > 0)
		{
			aiString texture_path;
			aiReturn res = mat->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path);
			if (res == aiReturn_SUCCESS)
			{
				TextureDescription tex_description{};
				tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
				tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

				std::filesystem::path result_path(path);
				result_path = result_path.remove_filename();
				result_path = result_path.concat(texture_path.C_Str());
				auto tex = AssetManager::getAsset<Texture>(result_path.string());
				if (tex->imageHandle == nullptr)
					continue;
				engine_material->albedo_tex_id = BindlessResources::addTexture(tex.get());
			}
		}

		unsigned int metalness_count = mat->GetTextureCount(aiTextureType_METALNESS);
		if (metalness_count > 0)
		{
			aiString texture_path;
			aiReturn res = mat->GetTexture(aiTextureType_METALNESS, 0, &texture_path);
			if (res == aiReturn_SUCCESS)
			{
				TextureDescription tex_description{};
				tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
				tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

				std::filesystem::path result_path(path);
				result_path = result_path.remove_filename();
				result_path = result_path.concat(texture_path.C_Str());
				auto tex = AssetManager::getAsset<Texture>(result_path.string());
				if (tex->imageHandle == nullptr)
					continue;
				engine_material->metalness_tex_id = BindlessResources::addTexture(tex.get());
			}
		}
	}

	for (int c = 0; c < node->mNumChildren; c++)
	{
		aiNode *child = node->mChildren[c];
		MeshNode *child_node = new MeshNode();
		child_node->parent = mesh_node;
		process_node(child_node, child, scene);
		mesh_node->children.push_back(child_node);
	}

}

Entity Model::createEntity(std::shared_ptr<Model> model, Scene *scene)
{
	return create_entity_node(model, model->root_node, scene);
}

Entity Model::create_entity_node(std::shared_ptr<Model> model, MeshNode *node, Scene *scene)
{
	Entity entity = scene->createEntity(node->name);
	auto &transform_component = entity.getComponent<TransformComponent>();

	auto &mesh_renderer = entity.addComponent<MeshRendererComponent>();


	for (auto &mesh : node->meshes)
	{
		MeshRendererComponent::MeshId mesh_id;
		mesh_id.model = model;
		mesh_id.mesh_id = mesh->id;
		mesh_renderer.meshes.push_back(mesh_id);
	}

	mesh_renderer.materials = node->materials;

	transform_component.setTransform(node->local_model_matrix);

	for (int i = 0; i < node->children.size(); i++)
	{
		Entity child = create_entity_node(model, node->children[i], scene);
		auto &child_transform_component = child.getComponent<TransformComponent>();
		child_transform_component.parent = entity;
		transform_component.children.push_back(child);
	}
	return entity;
}
