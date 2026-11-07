#pragma once
#include "Mesh.h"

struct ModelImportSettings;

struct MeshletBuildData
{
	eastl::vector<uint8_t> groups_data; // all LOD groups data in sequence: [meshlets][vertices][per meshlet: uint16 size + meshopt_encodeMeshlet triangles], 4-byte align
	uint32_t meshlet_count = 0;
	uint32_t vertex_count = 0;
	uint32_t index_count = 0;
};

namespace MeshletBuilder
{
MeshletBuildData build(Engine::Mesh *mesh, const char *debug_name, const eastl::vector<Engine::Vertex> &vertices, const eastl::vector<uint32_t> &indices, const ModelImportSettings &settings);
}
