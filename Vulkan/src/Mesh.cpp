#include "pch.h"
#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace Engine
{
	static glm::mat4 convertAssimpMat4(const aiMatrix4x4 &m)
	{
		glm::mat4 o;
		o[0][0] = m.a1; o[0][1] = m.b1; o[0][2] = m.c1; o[0][3] = m.d1;
		o[1][0] = m.a2; o[1][1] = m.b2; o[1][2] = m.c2; o[1][3] = m.d2;
		o[2][0] = m.a3; o[2][1] = m.b3; o[2][2] = m.c3; o[2][3] = m.d3;
		o[3][0] = m.a4; o[3][1] = m.b4; o[3][2] = m.c4; o[3][3] = m.d4;
		return o;
	}

	Mesh::Mesh(const std::string& path)
		: filePath(path)
	{
		loadFromPath();

		
	}

	Mesh::~Mesh()
	{

	}

	void Mesh::loadFromPath()
	{
		std::string pFile = filePath;
		Assimp::Importer importer;

		const aiScene* scene = importer.ReadFile(pFile,
			aiProcess_CalcTangentSpace |
			aiProcess_GenSmoothNormals |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType);

		root_transform = convertAssimpMat4(scene->mRootNode->mChildren[0]->mTransformation);

		vertices.clear();
		indices.clear();
		for (int m = 0; m < scene->mNumMeshes; m++)
		{
			aiMesh* mesh = scene->mMeshes[m];
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
					aiColor4D* aiColor = mesh->mColors[v];
					color = glm::vec3(aiColor->r, aiColor->g, aiColor->b);
				}
				vertices.emplace_back(Vertex{ {vertex.x, vertex.y, vertex.z}, normal, uv, color });
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
		}

		// If the import failed, report it
		if (nullptr == scene) {
			CORE_ERROR(importer.GetErrorString());
		}

		create_buffers();
	}

	void Mesh::setData(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
	{
		this->vertices = vertices;
		this->indices = indices;
		create_buffers();
	}

	static struct MeshFileHeader
	{
		uint32_t magicValue;
		uint32_t verticesCount;
		uint32_t indicesCount;
	};

	void Mesh::create_buffers()
	{
		// Create Vertex buffer
		BufferDescription vertexDesc;
		vertexDesc.size = sizeof(vertices[0]) * vertices.size();
		vertexDesc.useStagingBuffer = true;
		vertexDesc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		vertexBuffer = std::make_shared<Buffer>(vertexDesc);
		vertexBuffer->fill(vertices.data());

		// Create Index buffer
		BufferDescription indexDesc;
		indexDesc.size = sizeof(indices[0]) * indices.size();
		indexDesc.useStagingBuffer = true;
		indexDesc.bufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		indexBuffer = std::make_shared<Buffer>(indexDesc);
		indexBuffer->fill(indices.data());
	}

}