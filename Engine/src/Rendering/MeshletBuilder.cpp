#include "pch.h"
#include "MeshletBuilder.h"
#include "Assets/ModelImportSettings.h"
#include "Assets/MeshFormat.h"
#include "meshoptimizer.h"

#define CLUSTERLOD_IMPLEMENTATION
#include "clusterlod.h"

namespace MeshletBuilder
{

MeshletBuildData build(Engine::Mesh *mesh, const char *debug_name, const eastl::vector<Engine::Vertex> &vertices, const eastl::vector<uint32_t> &indices, const ModelImportSettings &settings)
{
	if (vertices.empty() || indices.empty())
		return {};

	clodConfig config = clodDefaultConfig(settings.meshlet_max_triangles);
	config.max_vertices = settings.meshlet_max_vertices;
	config.partition_spatial = true;
	config.partition_sort = true;
	config.partition_size = 16;

	float attr_weights[9] = {
		settings.normal_weight, settings.normal_weight, settings.normal_weight,
		settings.tangent_weight, settings.tangent_weight, settings.tangent_weight,
		settings.tangent_weight,
		settings.uv_weight, settings.uv_weight,
	};

	clodMesh input_mesh{};
	input_mesh.indices = const_cast<uint32_t *>(indices.data());
	input_mesh.index_count = indices.size();
	input_mesh.vertex_count = vertices.size();
	input_mesh.vertex_positions = (const float *)const_cast<Engine::Vertex *>(vertices.data());
	input_mesh.vertex_positions_stride = sizeof(Engine::Vertex);
	input_mesh.vertex_attributes = (const float *)&const_cast<Engine::Vertex *>(vertices.data())[0].normal;
	input_mesh.vertex_attributes_stride = sizeof(Engine::Vertex);
	input_mesh.attribute_weights = attr_weights;
	input_mesh.attribute_count = _countof(attr_weights);
	input_mesh.attribute_protect_mask = 1 << 7 | 1 << 8;

	mesh->meshlet_data.emplace();
	Engine::MeshletGeometry &geometry = *mesh->meshlet_data;
	MeshletBuildData build_data;
	const uint32_t attribute_flags = mesh->attribute_flags;
	const uint32_t disk_stride = MeshFormat::diskVertexStride(attribute_flags);

	// Reserve memory conservatively to avoid possible reallocations
	{
		size_t base_triangle_count = indices.size() / 3;
		size_t estimated_triangle_count = base_triangle_count * 2; // about 2x of triangles in DAG
		size_t estimated_meshlet_count = Math::divideRoundUp(estimated_triangle_count, settings.meshlet_max_triangles) + 64;
		size_t estimated_vertex_count = estimated_meshlet_count * settings.meshlet_max_vertices;
		geometry.meshlets.reserve(estimated_meshlet_count);
		geometry.meshlet_lod_groups.reserve(estimated_meshlet_count / 4 + 16);
		geometry.meshlet_lod_group_data_info.reserve(estimated_meshlet_count / 4 + 16);
		build_data.vertices.reserve(estimated_vertex_count * disk_stride);
		build_data.triangles.reserve(estimated_triangle_count * 3);
	}

	uint32_t total_vertex_count = 0;
	uint32_t total_triangle_index_count = 0;

	eastl::vector<uint32_t> local_vertex_indices(settings.meshlet_max_vertices);
	eastl::vector<uint8_t> local_triangle_indices(settings.meshlet_max_triangles * 3);

	clodBuild(config, input_mesh,
		[&](const clodGroup &group, const clodCluster *clusters, size_t cluster_count)
	{
		uint32_t group_id = geometry.meshlet_lod_groups.size();
		uint32_t first_meshlet_in_group = geometry.meshlets.size();

		LODGroup &lod_group = geometry.meshlet_lod_groups.emplace_back();
		lod_group.center = glm::vec3(group.simplified.center[0], group.simplified.center[1], group.simplified.center[2]);
		lod_group.radius = group.simplified.radius;
		lod_group.error = group.simplified.error;
		lod_group.depth = group.depth;
		lod_group.first_meshlet = first_meshlet_in_group;
		lod_group.meshlet_count = cluster_count;

		Engine::MeshletGeometry::LODGroupDataInfo &group_info = geometry.meshlet_lod_group_data_info.emplace_back();
		group_info.cpu_vertex_offset = total_vertex_count;
		group_info.cpu_triangle_offset = total_triangle_index_count;

		uint32_t group_vertex_count = 0;
		uint32_t group_triangle_index_count = 0;

		for (size_t i = 0; i < cluster_count; i++)
		{
			const clodCluster *cluster = &clusters[i];

			Meshlet &meshlet = geometry.meshlets.emplace_back();
			meshlet.group_id = group_id;
			meshlet.refined_group_id = cluster->refined == -1 ? 0xFFFFFFFFu : cluster->refined;

			size_t unique_vertex_count = clodLocalIndices(local_vertex_indices.data(), local_triangle_indices.data(), cluster->indices, cluster->index_count);

			size_t packed_vertex_offset = build_data.vertices.size();
			build_data.vertices.resize(packed_vertex_offset + unique_vertex_count * disk_stride);
			uint8_t *packed_vertices = build_data.vertices.data() + packed_vertex_offset;

			glm::vec3 bbox_min(FLT_MAX);
			glm::vec3 bbox_max(-FLT_MAX);
			for (size_t v = 0; v < unique_vertex_count; v++)
			{
				const Engine::Vertex &source = vertices[local_vertex_indices[v]];
				MeshFormat::encodeVertex(packed_vertices + v * disk_stride, source, attribute_flags);
				bbox_min = glm::min(glm::highp_vec3(bbox_min), source.pos);
				bbox_max = glm::max(glm::highp_vec3(bbox_max), source.pos);
			}
			build_data.vertex_count += unique_vertex_count;
			build_data.triangles.insert(build_data.triangles.end(), local_triangle_indices.data(), local_triangle_indices.data() + cluster->index_count);

			meshlet.vertex_offset = group_vertex_count;
			meshlet.vertex_count = unique_vertex_count;
			meshlet.triangle_offset = group_triangle_index_count;
			meshlet.triangle_count = (uint8_t)(cluster->index_count / 3);
			meshlet.center = (bbox_min + bbox_max) * 0.5f;
			meshlet.extent = (bbox_max - bbox_min) * 0.5f;

			group_vertex_count += unique_vertex_count;
			group_triangle_index_count += cluster->index_count;
		}

		total_vertex_count += group_vertex_count;
		total_triangle_index_count += group_triangle_index_count;

		group_info.cpu_vertex_count = group_vertex_count;
		group_info.cpu_triangle_count = group_triangle_index_count;

		// Next level started
		if (group.depth >= geometry.meshlet_lod_levels.size())
		{
			LODLevel &level = geometry.meshlet_lod_levels.emplace_back();
			level.group_offset = group_id;
		}
		geometry.meshlet_lod_levels.back().group_count++;

		return group_id;
	});

	const LODLevel &last = geometry.meshlet_lod_levels.back();
	if (last.group_count != 1)
		CORE_CRITICAL("MeshletBuilder::build: {} root groups for '{}', expected 1.", last.group_count, debug_name);

	// BVH build
	static constexpr uint32_t NODE_WIDTH = 8;
	uint32_t groups_count = geometry.meshlet_lod_groups.size();

	geometry.lod_nodes.resize(groups_count);
	for (uint32_t g = 0; g < groups_count; g++)
	{
		const LODGroup &group = geometry.meshlet_lod_groups[g];
		LodNode &node = geometry.lod_nodes[g];
		node = {};
		node.center = group.center;
		node.radius = group.radius;
		node.error = group.error;
		node.group_index = g;
		node.meshlet_count = group.meshlet_count;
		node.child_count = 0; // leaf
	}

	uint32_t level_begin = 0;
	uint32_t level_count = groups_count;

	eastl::vector<uint32_t> remap;
	eastl::vector<LodNode> lod_nodes;

	while (level_count > 1)
	{
		remap.resize(level_count);
		meshopt_spatialClusterPoints(remap.data(), &geometry.lod_nodes[level_begin].center.x, level_count, sizeof(LodNode), NODE_WIDTH);

		lod_nodes.assign(geometry.lod_nodes.begin() + level_begin, geometry.lod_nodes.begin() + level_begin + level_count);
		for (uint32_t i = 0; i < level_count; i++)
			geometry.lod_nodes[level_begin + i] = lod_nodes[remap[i]];

		const uint32_t parent_begin = (uint32_t)geometry.lod_nodes.size();
		const uint32_t parent_count = (level_count + NODE_WIDTH - 1) / NODE_WIDTH;
		geometry.lod_nodes.resize(parent_begin + parent_count);

		for (uint32_t p = 0; p < parent_count; p++)
		{
			uint32_t first_child = level_begin + p * NODE_WIDTH;
			uint32_t child_count = std::min(first_child + NODE_WIDTH, level_begin + level_count) - first_child;

			LodNode &parent = geometry.lod_nodes[parent_begin + p];
			parent = {};
			parent.first_child = first_child;
			parent.child_count = child_count;

			// Merge bounding spheres
			meshopt_Bounds merged = meshopt_computeSphereBounds(
				&geometry.lod_nodes[first_child].center.x, child_count, sizeof(LodNode),
				&geometry.lod_nodes[first_child].radius, sizeof(LodNode));
			parent.center = glm::vec3(merged.center[0], merged.center[1], merged.center[2]);
			parent.radius = merged.radius;

			// Error is conservative
			for (uint32_t c = 0; c < child_count; c++)
				parent.error = std::max(parent.error, geometry.lod_nodes[first_child + c].error);
		}

		level_begin = parent_begin;
		level_count = parent_count;
	}

	// Root is the single node
	geometry.meshlet_root_group_local_offset = level_begin;
	return build_data;
}

}
