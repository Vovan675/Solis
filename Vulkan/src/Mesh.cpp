#include "pch.h"
#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace Engine
{
	Mesh::Mesh(const std::string& path)
		: filePath(path)
	{
		LoadModel();

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

	Mesh::~Mesh()
	{

	}

	void Mesh::LoadModel()
	{
		std::string pFile = filePath;
		// Create an instance of the Importer class
		Assimp::Importer importer;

		// And have it read the given file with some example postprocessing
		// Usually - if speed is not the most important aspect for you - you'll
		// probably to request more postprocessing than we do in this example.
		const aiScene* scene = importer.ReadFile(pFile,
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType);

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
	}

}