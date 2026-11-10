#pragma once
#include "Mesh.h"

struct ModelImportSettings;

struct MeshletBuildData
{
	eastl::vector<uint8_t> pages_data; // pages in sequence. Page sections:[meshlets][vertices][triangles_encoded], where [triangles_encoded]: uint8 triangle count, uint16 size, meshopt_encodeMeshlet bytes
	uint32_t meshlet_count = 0;
	uint32_t vertex_count = 0;
	uint32_t index_count = 0;
};

namespace MeshletBuilder
{
MeshletBuildData build(Engine::Mesh *mesh, const char *debug_name, const eastl::vector<Engine::Vertex> &vertices, const eastl::vector<uint32_t> &indices, const ModelImportSettings &settings);
}
