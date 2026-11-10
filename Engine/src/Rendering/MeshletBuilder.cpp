#include "pch.h"
#include "MeshletBuilder.h"
#include "Assets/ModelImportSettings.h"
#include "Assets/MeshFormat.h"
#include "meshoptimizer.h"

#define CLUSTERLOD_IMPLEMENTATION
#include "clusterlod.h"

namespace MeshletBuilder
{

// Usually positions extent inside one meshlet are not very big
// So we can calculate enough precision (per mesh for consistent edges) and use less bits for positions
// And then use position = bound_min + (offset * scale) in shader
// where bound_min is exact float (from 24bit int) start position per meshlet
// and offset is just small (1-48bit float3) position

static float calc_position_scale(const eastl::vector<Meshlet> &meshlets)
{
	// Mental model: +1 bigger exponent = twice bigger size

	const int TARGET_MESHLET_POSITION_BITS = 7;
	const int MAX_POSITION_EXPONENT = -11;
	const int MAX_POSITION_BITS = 16;

	// Calculate minimum precision enough for LOD0 of all meshlets in mesh (heuristic for starting minimum exponent)
	double lod0_size_exponent_sum = 0.0;
	uint32_t lod0_meshlet_count = 0;
	for (const Meshlet &meshlet : meshlets)
	{
		float size = glm::length(meshlet.bound_extent);
		if (meshlet.isMostDetailed() && size > 0.0f)
		{
			lod0_size_exponent_sum += std::log2(size);
			lod0_meshlet_count++;
		}
	}

	int average_size_exponent = (int)std::lround(lod0_size_exponent_sum / std::max(lod0_meshlet_count, 1u));
	int exponent = std::min(average_size_exponent - TARGET_MESHLET_POSITION_BITS, MAX_POSITION_EXPONENT);

	// Calculate maximum good exponent that can be used for all meshlets
	for (const Meshlet &meshlet : meshlets)
	{
		glm::highp_vec3 bound_max = meshlet.bound_min + meshlet.bound_extent * 2.0f;
		exponent = meshopt_computePositionExponent(&meshlet.bound_min.x, &bound_max.x, exponent, MAX_POSITION_BITS);
	}
	return std::ldexp(1.0f, exponent);
}

static void write_positions(uint8_t *dst, const glm::ivec3 *quantized_positions, glm::ivec3 quantized_min, int vertex_count, glm::uvec3 bits)
{
	uint32_t pending = 0;
	uint32_t pending_bits = 0;
	// Like in meshoptimizer/demo/main.cpp
	auto write_bits = [&](uint32_t value, uint32_t bit_count)
	{
		pending |= value << pending_bits;
		pending_bits += bit_count;
		while (pending_bits >= 8)
		{
			*dst++ = (uint8_t)pending;
			pending >>= 8;
			pending_bits -= 8;
		}
	};

	for (int v = 0; v < vertex_count; v++)
	{
		glm::ivec3 offset = quantized_positions[v] - quantized_min;
		write_bits(offset.x, bits.x);
		write_bits(offset.y, bits.y);
		write_bits(offset.z, bits.z);
	}

	if (pending_bits > 0)
		*dst = (uint8_t)pending;
}

// Writes meshlet groups into pages with sections [meshlets][vertices][triangles], triangles are meshopt encoded
class MeshletPageWriter
{
public:
	MeshletPageWriter(Engine::MeshletGeometry &geometry, eastl::vector<Meshlet> &meshlets, eastl::vector<uint8_t> &pages_data)
		: geometry(geometry), meshlets(meshlets), pages_data(pages_data)
	{
	}

	void beginMeshletGroup(LODGroup &group, uint32_t first_meshlet)
	{
		uint32_t meshlets_bytes = group.meshlet_count * sizeof(Meshlet);
		group_meshlets_page = ensure_page_space(meshlets_bytes);

		// Meshlets section
		group.meshlets_location = packPageLocation(group_meshlets_page, page.meshlets_bytes);
		page.meshlets_bytes += meshlets_bytes;
		pending_meshlets.push_back({group.meshlets_location, first_meshlet, group.meshlet_count});
	}

	// Places the meshlet into the current page, starts a new page when it does not fit
	void addMeshlet(uint32_t meshlet_id, eastl::span<const uint8_t> vertices, eastl::span<const uint8_t> encoded_triangles)
	{
		Meshlet &meshlet = meshlets[meshlet_id];
		uint32_t index_count = meshlet.triangle_count * 3;
		uint32_t page_id = ensure_page_space(vertices.size() + index_count); // used just index_count because we care about end GPU format
		page_meshlet_ids.push_back(meshlet_id);

		// Locations are from section start there. On page flush offset of section added, so it would be full offset in page
		// Vertices section
		meshlet.vertex_location = packPageLocation(page_id, page.vertices_bytes);
		page_vertices.insert(page_vertices.end(), vertices.begin(), vertices.end());
		page.vertices_bytes = page_vertices.size();

		// Triangles section
		meshlet.triangle_location = packPageLocation(page_id, page.index_count);
		page.index_count += index_count;

		uint16_t encoded_bytes = (uint16_t)encoded_triangles.size();
		page_triangles.push_back(meshlet.triangle_count);
		size_t triangles_start = page_triangles.size();
		page_triangles.resize(triangles_start + sizeof(encoded_bytes) + encoded_bytes);
		memcpy(page_triangles.data() + triangles_start, &encoded_bytes, sizeof(encoded_bytes));
		memcpy(page_triangles.data() + triangles_start + sizeof(encoded_bytes), encoded_triangles.data(), encoded_bytes);
		page.encoded_triangles_bytes = page_triangles.size();
	}

	void endMeshletGroup(LODGroup &group)
	{
		group.page_count = geometry.pages.size() - group_meshlets_page + 1;
	}

	void finish()
	{
		flush_page();

		// Meshlets sections
		for (const PendingMeshlets &pending : pending_meshlets)
		{
			const Engine::MeshletGeometry::Page &meshlets_page = geometry.pages[unpackLocationPage(pending.meshlets_location)];
			memcpy(pages_data.data() + meshlets_page.file_offset + unpackLocationOffset(pending.meshlets_location), &meshlets[pending.first_meshlet], pending.meshlet_count * sizeof(Meshlet));
		}
	}

private:

	// Meshlets of a group, written in finish when all their locations are known
	struct PendingMeshlets
	{
		uint32_t meshlets_location;
		uint32_t first_meshlet;
		uint32_t meshlet_count;
	};

	// Returns the page for the next GPU bytes
	uint32_t ensure_page_space(uint32_t gpu_bytes)
	{
		uint32_t capacity = geometry.pages.empty() ? ROOT_PAGE_SIZE : MESHLET_PAGE_SIZE;
		if (page.getGpuBytes() + gpu_bytes > capacity)
			flush_page();
		return (uint32_t)geometry.pages.size();
	}

	// End current page
	void flush_page()
	{
		page.file_offset = pages_data.size();
		ENGINE_ASSERT(page.getGpuBytes() <= MESHLET_PAGE_SIZE);

		for (uint32_t meshlet_id : page_meshlet_ids)
		{
			meshlets[meshlet_id].vertex_location += page.getVerticesOffset();
			meshlets[meshlet_id].triangle_location += page.getIndicesOffset();
		}

		// Meshlets section, written in finish
		pages_data.resize(pages_data.size() + page.meshlets_bytes);

		// Vertices section
		pages_data.insert(pages_data.end(), page_vertices.begin(), page_vertices.end());

		// Triangles section
		pages_data.insert(pages_data.end(), page_triangles.begin(), page_triangles.end());

		geometry.pages.push_back(page);

		page = {};
		page_meshlet_ids.clear();
		page_vertices.clear();
		page_triangles.clear();
	}

	Engine::MeshletGeometry &geometry;
	eastl::vector<Meshlet> &meshlets;
	eastl::vector<uint8_t> &pages_data;

	// Current page
	Engine::MeshletGeometry::Page page{};
	eastl::vector<uint32_t> page_meshlet_ids;

	// Current page sections
	eastl::vector<uint8_t> page_vertices;
	eastl::vector<uint8_t> page_triangles; // triangles of a meshlet: uint8 triangle count, uint16 size, meshopt_encodeMeshlet bytes

	// Current group
	uint32_t group_meshlets_page = 0;

	eastl::vector<PendingMeshlets> pending_meshlets;
};

static void encode_pages(Engine::Mesh *mesh, const eastl::vector<Engine::Vertex> &vertices, eastl::vector<Meshlet> &meshlets, const eastl::vector<uint32_t> &meshlet_vertices, const eastl::vector<uint8_t> &meshlet_triangles, eastl::vector<uint8_t> &pages_data)
{
	Engine::MeshletGeometry &geometry = *mesh->meshlet_data;
	const uint32_t attributes_bytes = MeshFormat::vertexAttributesBytes(mesh->attribute_flags);
	const float inverse_position_scale = 1.0f / mesh->position_scale;

	glm::ivec3 quantized_positions[256];
	eastl::vector<uint8_t> packed_vertices;
	eastl::vector<uint8_t> encoded_triangles(meshopt_encodeMeshletBound(0, 256));

	// Worst case scenario allocation
	pages_data.reserve(
		meshlets.size() * sizeof(Meshlet) + // All meshlets size
		meshlet_vertices.size() * (3 * sizeof(uint16_t) + attributes_bytes) + // All meshlets vertices size
		meshlet_triangles.size() * sizeof(uint8_t)); // All triangles (1 byte per index)

	eastl::vector<LODGroup> &groups = geometry.meshlet_lod_groups;

	// Meshlets, vertices, triangles first indices in build order
	eastl::vector<uint32_t> group_first_meshlets(groups.size());
	for (int i = 1; i < groups.size(); i++)
		group_first_meshlets[i] = group_first_meshlets[i - 1] + groups[i - 1].meshlet_count;

	eastl::vector<uint32_t> meshlet_first_vertices(meshlets.size());
	eastl::vector<uint32_t> meshlet_first_triangles(meshlets.size());
	for (int i = 1; i < meshlets.size(); i++)
	{
		meshlet_first_vertices[i] = meshlet_first_vertices[i - 1] + meshlets[i - 1].vertex_count;
		meshlet_first_triangles[i] = meshlet_first_triangles[i - 1] + meshlets[i - 1].triangle_count * 3;
	}

	// Coarse -> Detailed order
	eastl::vector<uint32_t> groups_order;
	for (int level = geometry.meshlet_lod_levels.size() - 1; level >= 0; level--)
	{
		LODLevel &lod_level = geometry.meshlet_lod_levels[level];
		// clodConfig::partition_sort sorted it already
		for (int i = 0; i < lod_level.group_count; i++)
			groups_order.push_back(lod_level.group_offset + i);
	}

	MeshletPageWriter writer(geometry, meshlets, pages_data);
	for (uint32_t group_id : groups_order)
	{
		LODGroup &group = groups[group_id];
		writer.beginMeshletGroup(group, group_first_meshlets[group_id]);

		for (uint32_t i = 0; i < group.meshlet_count; i++)
		{
			uint32_t meshlet_id = group_first_meshlets[group_id] + i;
			Meshlet &meshlet = meshlets[meshlet_id];
			const uint32_t *vertex_ids = &meshlet_vertices[meshlet_first_vertices[meshlet_id]];
			const uint8_t *triangles = &meshlet_triangles[meshlet_first_triangles[meshlet_id]];

			// Quantize
			glm::ivec3 quantized_min(INT_MAX);
			glm::ivec3 quantized_max(INT_MIN);
			for (int v = 0; v < meshlet.vertex_count; v++)
			{
				quantized_positions[v] = glm::ivec3(glm::round(vertices[vertex_ids[v]].pos * inverse_position_scale));
				quantized_min = glm::min(quantized_min, quantized_positions[v]);
				quantized_max = glm::max(quantized_max, quantized_positions[v]);
			}

			// Bounds
			glm::ivec3 max_offset = quantized_max - quantized_min;
			meshlet.bound_min = glm::vec3(quantized_min) * mesh->position_scale;
			meshlet.bound_extent = glm::vec3(max_offset) * (mesh->position_scale * 0.5f);
			meshlet.position_bits = MeshFormat::packPositionBits(max_offset);

			const uint32_t meshlet_vertex_bytes = MeshFormat::meshletVertexBytes(meshlet.vertex_count, meshlet.position_bits, mesh->attribute_flags);

			// Write
			packed_vertices.assign(meshlet_vertex_bytes, 0);
			write_positions(packed_vertices.data(), quantized_positions, quantized_min, meshlet.vertex_count, MeshFormat::unpackPositionBits(meshlet.position_bits));

			uint8_t *packed_attributes = packed_vertices.data() + MeshFormat::meshletPositionBytes(meshlet.vertex_count, meshlet.position_bits);
			for (int v = 0; v < meshlet.vertex_count; v++)
				MeshFormat::encodeVertexAttributes(packed_attributes + v * attributes_bytes, vertices[vertex_ids[v]], mesh->attribute_flags);

			size_t encoded_bytes = meshopt_encodeMeshlet(encoded_triangles.data(), encoded_triangles.size(), nullptr, 0, triangles, meshlet.triangle_count);
			writer.addMeshlet(meshlet_id, packed_vertices, {encoded_triangles.data(), encoded_bytes});
		}

		writer.endMeshletGroup(group);
	}
	writer.finish();
}

MeshletBuildData build(Engine::Mesh *mesh, const char *debug_name, const eastl::vector<Engine::Vertex> &vertices, const eastl::vector<uint32_t> &indices, const ModelImportSettings &settings)
{
	if (vertices.empty() || indices.empty())
		return {};

	clodConfig config = clodDefaultConfig(settings.meshlet_max_triangles);
	config.max_vertices = settings.meshlet_max_vertices;
	config.partition_spatial = true;
	config.partition_sort = true;
	config.partition_size = 16;

	const uint32_t attribute_flags = mesh->attribute_flags;
	const bool has_tangent = attribute_flags & MeshFormat::MESH_ATTR_TANGENT;
	const bool has_uv = attribute_flags & MeshFormat::MESH_ATTR_UV;
	const float tangent_weight = has_tangent ? settings.tangent_weight : 0.0f;
	const float uv_weight = has_uv ? settings.uv_weight : 0.0f;
	float attr_weights[9] = {
		settings.normal_weight, settings.normal_weight, settings.normal_weight,
		tangent_weight, tangent_weight, tangent_weight,
		tangent_weight,
		uv_weight, uv_weight,
	};

	clodMesh input_mesh{};
	input_mesh.indices = indices.data();
	input_mesh.index_count = indices.size();
	input_mesh.vertex_count = vertices.size();
	input_mesh.vertex_positions = &vertices[0].pos.x;
	input_mesh.vertex_positions_stride = sizeof(Engine::Vertex);
	input_mesh.vertex_attributes = &vertices[0].normal.x;
	input_mesh.vertex_attributes_stride = sizeof(Engine::Vertex);
	input_mesh.attribute_weights = attr_weights;
	input_mesh.attribute_count = _countof(attr_weights);
	input_mesh.attribute_protect_mask = has_uv ? 1 << 7 | 1 << 8 : 0;

	mesh->meshlet_data.emplace();
	Engine::MeshletGeometry &geometry = *mesh->meshlet_data;
	MeshletBuildData build_data;
	eastl::vector<Meshlet> meshlets;
	eastl::vector<uint32_t> meshlet_vertices;
	eastl::vector<uint8_t> meshlet_triangles;

	// Reserve memory conservatively to avoid possible reallocations
	{
		size_t base_triangle_count = indices.size() / 3;
		size_t estimated_triangle_count = base_triangle_count * 2; // about 2x of triangles in DAG
		size_t estimated_meshlet_count = Math::divideRoundUp(estimated_triangle_count, settings.meshlet_max_triangles) + 64;
		size_t estimated_vertex_count = estimated_meshlet_count * settings.meshlet_max_vertices;
		meshlets.reserve(estimated_meshlet_count);
		geometry.meshlet_lod_groups.reserve(estimated_meshlet_count / 4 + 16);
		meshlet_vertices.reserve(estimated_vertex_count);
		meshlet_triangles.reserve(estimated_triangle_count * 3);
	}

	eastl::vector<uint32_t> local_vertex_indices(settings.meshlet_max_vertices);
	eastl::vector<uint8_t> local_triangle_indices(settings.meshlet_max_triangles * 3);

	clodBuild(config, input_mesh,
		[&](const clodGroup &group, const clodCluster *clusters, size_t cluster_count)
	{
		uint32_t group_id = geometry.meshlet_lod_groups.size();

		LODGroup &lod_group = geometry.meshlet_lod_groups.emplace_back();
		lod_group.center = glm::vec3(group.simplified.center[0], group.simplified.center[1], group.simplified.center[2]);
		lod_group.radius = group.simplified.radius;
		lod_group.error = group.simplified.error;
		lod_group.meshlet_count = cluster_count;

		for (size_t i = 0; i < cluster_count; i++)
		{
			const clodCluster *cluster = &clusters[i];

			Meshlet &meshlet = meshlets.emplace_back();
			meshlet.group_id = group_id;
			meshlet.refined_group_id = cluster->refined == -1 ? MOST_DETAILED_CLUSTER_GROUP_ID : cluster->refined;

			size_t unique_vertex_count = clodLocalIndices(local_vertex_indices.data(), local_triangle_indices.data(), cluster->indices, cluster->index_count);

			glm::vec3 bbox_min(FLT_MAX);
			glm::vec3 bbox_max(-FLT_MAX);
			for (size_t v = 0; v < unique_vertex_count; v++)
			{
				const Engine::Vertex &source = vertices[local_vertex_indices[v]];
				bbox_min = glm::min(glm::highp_vec3(bbox_min), source.pos);
				bbox_max = glm::max(glm::highp_vec3(bbox_max), source.pos);
			}
			meshlet_vertices.insert(meshlet_vertices.end(), local_vertex_indices.data(), local_vertex_indices.data() + unique_vertex_count);
			meshlet_triangles.insert(meshlet_triangles.end(), local_triangle_indices.data(), local_triangle_indices.data() + cluster->index_count);

			meshlet.vertex_count = unique_vertex_count;
			meshlet.triangle_count = (uint8_t)(cluster->index_count / 3);
			meshlet.bound_min = bbox_min;
			meshlet.bound_extent = (bbox_max - bbox_min) * 0.5f;
		}

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

	mesh->position_scale = calc_position_scale(meshlets);

	// Encode vertices there instead of inside clodBuild because we need full information to quantize vertices
	build_data.meshlet_count = meshlets.size();
	build_data.vertex_count = meshlet_vertices.size();
	build_data.index_count = meshlet_triangles.size();
	encode_pages(mesh, vertices, meshlets, meshlet_vertices, meshlet_triangles, build_data.pages_data);

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
