#pragma once
#include "Core/ReflectionSerialization.h"

enum MeshletMode
{
	MESHLET_MODE_AUTO = 0,
	MESHLET_MODE_ENABLED,
	MESHLET_MODE_DISABLED,
};
inline const char *const meshlet_mode_items[] = {"Auto", "Enabled", "Disabled"};

struct ModelImportSettings
{
	// Meshlet generation
	MeshletMode meshlet_mode = MESHLET_MODE_AUTO;
	uint32_t meshlet_max_vertices = 128;
	uint32_t meshlet_max_triangles = 128;

	// Weights for Meshlet LOD simplification
	float position_weight = 1.0f;
	float normal_weight = 0.5f;
	float tangent_weight = 0.01f;
	float uv_weight = 1.0f;
	float color_weight = 0.0f;
};

REFLECT_BEGIN(ModelImportSettings)
	REFLECT_FIELD(meshlet_mode).label("Meshlet (Nanite) Geometry").items(meshlet_mode_items),
	REFLECT_CATEGORY("Meshlets"),
	REFLECT_FIELD(meshlet_max_vertices).range(32.0f, 256.0f).EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
	REFLECT_FIELD(meshlet_max_triangles).range(32.0f, 256.0f).EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
	REFLECT_CATEGORY("Meshlets - Simplification"),
	REFLECT_FIELD(position_weight).range(0.0f, 2.0f).format("%.2f").EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
	REFLECT_FIELD(normal_weight).range(0.0f, 2.0f).format("%.2f").EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
	REFLECT_FIELD(tangent_weight).range(0.0f, 2.0f).format("%.2f").EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
	REFLECT_FIELD(uv_weight).label("UV Weight").range(0.0f, 2.0f).format("%.2f").EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
	REFLECT_FIELD(color_weight).range(0.0f, 2.0f).format("%.2f").EDIT_IF(owner.meshlet_mode != MESHLET_MODE_DISABLED),
REFLECT_END()