#include "pch.h"
#include "Entity.h"

static glm::mat4 convertAssimpMat4(const aiMatrix4x4 &m)
{
	glm::mat4 o;
	o[0][0] = m.a1; o[0][1] = m.b1; o[0][2] = m.c1; o[0][3] = m.d1;
	o[1][0] = m.a2; o[1][1] = m.b2; o[1][2] = m.c2; o[1][3] = m.d2;
	o[2][0] = m.a3; o[2][1] = m.b3; o[2][2] = m.c3; o[2][3] = m.d3;
	o[3][0] = m.a4; o[3][1] = m.b4; o[3][2] = m.c4; o[3][3] = m.d4;
	return o;
}

void Entity::updateTransform()
{
	if (parent != nullptr)
	{
		transform.model_matrix = transform.local_model_matrix * parent->transform.model_matrix;
	} else
	{
		transform.model_matrix = transform.local_model_matrix;
	}

	for (auto &child : children)
	{
		child->updateTransform();
	}
}

void Entity::load_model(const char *model_path)
{
	Assimp::Importer importer;

	const aiScene *scene = importer.ReadFile(model_path,
											 aiProcess_CalcTangentSpace |
											 aiProcess_GenSmoothNormals |
											 aiProcess_Triangulate |
											 aiProcess_JoinIdenticalVertices |
											 aiProcess_SortByPType);

	process_node(scene->mRootNode, scene);
}

void Entity::process_node(aiNode *node, const aiScene *scene)
{
	std::vector<Engine::Vertex> vertices;
	std::vector<uint32_t> indices;

	transform.local_model_matrix = convertAssimpMat4(node->mTransformation);
	updateTransform();
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
		meshes.push_back(engine_mesh);
	}

	for (int c = 0; c < node->mNumChildren; c++)
	{
		aiNode *child = node->mChildren[c];
		std::shared_ptr<Entity> entity = std::make_shared<Entity>();
		entity->parent = this;
		entity->process_node(child, scene);
		children.push_back(entity);
	}

}
