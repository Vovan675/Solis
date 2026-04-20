#pragma once
#include "Mesh.h"

struct ModelImportSettings;

struct MeshletBuildData
{
	eastl::vector<uint8_t> vertices;
	uint32_t vertex_count = 0;
	eastl::vector<uint8_t> triangles;
};

namespace MeshletBuilder
{
MeshletBuildData build(Engine::Mesh *mesh, const char *debug_name,
	const eastl::vector<uint32_t> &indices,
	const ModelImportSettings &settings);
}
