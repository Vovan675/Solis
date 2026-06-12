#include "pch.h"
#include "GltfImporter.h"
#include "Core/Platform.h"
#include "Rendering/Model.h"
#include "ModelImportSettings.h"
#include "MeshSerializer.h"
#include "Assets/AssetManager.h"
#include "Rendering/MeshletBuilder.h"
#include "Math/EngineMath.h"
#include "Core/Variables.h"
#include "Utils/FileMemory.h"
#include <Utils/Math.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include "meshoptimizer.h"
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_set>

// Debug: load only meshes [GLTF_IMPORT_MESH_START, GLTF_IMPORT_MESH_START + GLTF_IMPORT_MESH_LIMIT).
#define GLTF_IMPORT_MESH_START 0
#define GLTF_IMPORT_MESH_LIMIT 0

static std::mutex meshopt_compression_mutex;
static bool ensure_meshopt_decompressed(cgltf_buffer_view *bv)
{
	if (!bv || !bv->has_meshopt_compression)
		return true;

	std::lock_guard<std::mutex> lock(meshopt_compression_mutex);
	if (bv->data)
		return true;

	const cgltf_meshopt_compression *mc = &bv->meshopt_compression;
	if (!mc->buffer || !mc->buffer->data)
	{
		CORE_ERROR("GltfImporter: meshopt buffer has no data");
		return false;
	}

	const unsigned char *src = (const unsigned char *)mc->buffer->data + mc->offset;
	size_t out_size = mc->count * mc->stride;
	void *dst = malloc(out_size);
	if (!dst)
		return false;

	int result = -1;
	switch (mc->mode)
	{
	case cgltf_meshopt_compression_mode_attributes:
		result = meshopt_decodeVertexBuffer(dst, mc->count, mc->stride, src, mc->size);
		break;
	case cgltf_meshopt_compression_mode_triangles:
		result = meshopt_decodeIndexBuffer(dst, mc->count, mc->stride, src, mc->size);
		break;
	case cgltf_meshopt_compression_mode_indices:
		result = meshopt_decodeIndexSequence(dst, mc->count, mc->stride, src, mc->size);
		break;
	default:
		break;
	}
	if (result != 0)
	{
		free(dst);
		CORE_ERROR("GltfImporter: meshopt decode failed (mode={})", (int)mc->mode);
		return false;
	}

	switch (mc->filter)
	{
	case cgltf_meshopt_compression_filter_octahedral:
		meshopt_decodeFilterOct(dst, mc->count, mc->stride);
		break;
	case cgltf_meshopt_compression_filter_quaternion:
		meshopt_decodeFilterQuat(dst, mc->count, mc->stride);
		break;
	case cgltf_meshopt_compression_filter_exponential:
		meshopt_decodeFilterExp(dst, mc->count, mc->stride);
		break;
	default:
		break;
	}

	bv->data = dst;
	return true;
}

static void release_meshopt_buffer_view(cgltf_buffer_view *bv)
{
	if (!bv || !bv->has_meshopt_compression)
		return;
	std::lock_guard<std::mutex> lock(meshopt_compression_mutex);
	if (bv->data)
	{
		free(bv->data);
		bv->data = nullptr;
	}
}

static void ensure_accessor_decompressed(const cgltf_accessor *acc)
{
	if (acc)
		ensure_meshopt_decompressed(acc->buffer_view);
}

static void ensure_primitive_decompressed(const cgltf_primitive *prim)
{
	ensure_accessor_decompressed(prim->indices);
	for (cgltf_size i = 0; i < prim->attributes_count; i++)
		ensure_accessor_decompressed(prim->attributes[i].data);
}

// Discard memory mapped pages, for decreasing memory usage
static void discard_accessor_pages(const cgltf_accessor *acc)
{
	if (!acc || !acc->buffer_view || !acc->buffer_view->buffer)
		return;

	release_meshopt_buffer_view(acc->buffer_view);

	const cgltf_buffer *buf = acc->buffer_view->buffer;
	if (!buf->data || buf->data_free_method != cgltf_data_free_method_none)
		return;

	const uint8_t *region = (const uint8_t *)buf->data + acc->buffer_view->offset;
	size_t size = acc->buffer_view->size;
	size_t page_size = Platform::getPageSize();
	size_t aligned_address = Math::alignedSize((size_t)region, page_size);
	size_t aligned_size = Math::alignedSize((size_t)region + size - aligned_address, page_size);
	Platform::discardMemory((void *)aligned_address, aligned_size);
}

static void discard_primitive_pages(const cgltf_primitive *prim)
{
	discard_accessor_pages(prim->indices);
	for (cgltf_size i = 0; i < prim->attributes_count; i++)
		discard_accessor_pages(prim->attributes[i].data);
}

// Read a gltf accessor into a destination buffer.
template <class T, bool doBBox = false>
static void read_gltf_attrib(const cgltf_accessor *acc, uint8_t *dst, size_t dst_stride, T *bbox_min = nullptr, T *bbox_max = nullptr)
{
	constexpr cgltf_type expected_type =
		sizeof(T) == 8 ? cgltf_type_vec2 :
		sizeof(T) == 12 ? cgltf_type_vec3 :
		sizeof(T) == 16 ? cgltf_type_vec4 : cgltf_type_invalid;

	bool is_direct = !acc->is_sparse
		&& !acc->normalized
		&& acc->buffer_view
		&& acc->component_type == cgltf_component_type_r_32f
		&& acc->type == expected_type
		&& acc->stride == sizeof(T);

	if (is_direct)
	{
		// Fast path: direct memcpy when accessor r_32f.
		const T *src = (const T *)(cgltf_buffer_view_data(acc->buffer_view) + acc->offset);
		for (cgltf_size i = 0; i < acc->count; i++)
		{
			T v = src[i];
			if constexpr (doBBox)
			{
				*bbox_min = glm::min(*bbox_min, v);
				*bbox_max = glm::max(*bbox_max, v);
			}
			*(T *)(dst + i * dst_stride) = v;
		}
	} else
	{
		// Slow path
		for (cgltf_size i = 0; i < acc->count; i++)
		{
			T v{};
			cgltf_accessor_read_float(acc, i, &v.x, sizeof(T) / sizeof(float));
			if constexpr (doBBox)
			{
				*bbox_min = glm::min(*bbox_min, v);
				*bbox_max = glm::max(*bbox_max, v);
			}
			*(T *)(dst + i * dst_stride) = v;
		}
	}
}

static void extract_indices(const cgltf_accessor *acc, eastl::vector<uint32_t> &inds)
{
	cgltf_size count = acc->count;
	inds.resize(count);

	bool is_direct = !acc->is_sparse
		&& !acc->normalized
		&& acc->buffer_view
		&& acc->component_type == cgltf_component_type_r_32u
		&& acc->type == cgltf_type_scalar
		&& acc->stride == sizeof(uint32_t);

	if (is_direct)
	{
		memcpy(inds.data(), cgltf_buffer_view_data(acc->buffer_view) + acc->offset, count * sizeof(uint32_t));
	} else
	{
		for (cgltf_size i = 0; i < count; ++i)
			inds[i] = (uint32_t)cgltf_accessor_read_index(acc, i);
	}
}

static uint32_t extract_vertices(const cgltf_primitive *prim, eastl::vector<Engine::Vertex> &vertices, eastl::vector<uint32_t> &indices, BoundBox &out_bbox)
{
	const cgltf_accessor *pos_acc = nullptr;
	const cgltf_accessor *norm_acc = nullptr;
	const cgltf_accessor *tan_acc = nullptr;
	const cgltf_accessor *uv_acc = nullptr;

	for (cgltf_size i = 0; i < prim->attributes_count; i++)
	{
		const cgltf_attribute &a = prim->attributes[i];
		switch (a.type)
		{
			case cgltf_attribute_type_position: pos_acc = a.data; break;
			case cgltf_attribute_type_normal: norm_acc = a.data; break;
			case cgltf_attribute_type_tangent: tan_acc = a.data; break;
			case cgltf_attribute_type_texcoord:
				if (a.index == 0) uv_acc = a.data;
				break;
		}
	}
	if (!pos_acc)
		return 0;

	uint32_t count = (uint32_t)pos_acc->count;
	vertices.resize(count);

	constexpr size_t stride = sizeof(Engine::Vertex);
	uint8_t *base = (uint8_t *)vertices.data();

	glm::highp_vec3 bbox_min(FLT_MAX), bbox_max(-FLT_MAX);
	read_gltf_attrib<glm::highp_vec3, true>(pos_acc, base + offsetof(Engine::Vertex, pos), stride, &bbox_min, &bbox_max);
	out_bbox = BoundBox(bbox_min, bbox_max);

	if (norm_acc) read_gltf_attrib<glm::highp_vec3>(norm_acc, base + offsetof(Engine::Vertex, normal), stride);
	if (tan_acc) read_gltf_attrib<glm::highp_vec4>(tan_acc, base + offsetof(Engine::Vertex, tangent), stride);
	if (uv_acc) read_gltf_attrib<glm::highp_vec2>(uv_acc, base + offsetof(Engine::Vertex, uv), stride);

	if (prim->indices)
	{
		extract_indices(prim->indices, indices);
	} else
	{
		indices.resize(count);
		for (uint32_t i = 0; i < count; ++i) indices[i] = i;
	}

	if (!norm_acc)
	{
		for (Engine::Vertex &v : vertices)
			v.normal = glm::vec3(0.0f);
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			uint32_t i0 = indices[i + 0];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];
			glm::vec3 face_normal = glm::cross(vertices[i1].pos - vertices[i0].pos, vertices[i2].pos - vertices[i0].pos);
			vertices[i0].normal += glm::highp_vec3(face_normal);
			vertices[i1].normal += glm::highp_vec3(face_normal);
			vertices[i2].normal += glm::highp_vec3(face_normal);
		}
		for (Engine::Vertex &v : vertices)
		{
			float len = glm::length(v.normal);
			v.normal = len > 1e-6f ? v.normal / len : glm::vec3(0.0f, 1.0f, 0.0f);
		}
		tan_acc = nullptr;
	}

	return tan_acc ? MeshFormat::MESH_ATTR_TANGENT : 0;
}

static uint64_t resolve_texture_guid(const cgltf_texture_view &view, const char *gltf_path)
{
	if (!view.texture || !view.texture->image)
		return 0;
	const char *uri = view.texture->image->uri;
	if (!uri || strncmp(uri, "data:", 5) == 0)
		return 0;
	auto p = std::filesystem::path(gltf_path).parent_path() / uri;
	return AssetManager::getGUIDFromPath(p.string());
}

static Ref<Material> extract_material(const cgltf_material *mat, const char *gltf_path)
{
	Ref<Material> m = new Material();
	if (!mat || !mat->has_pbr_metallic_roughness)
		return m;

	const auto &pbr = mat->pbr_metallic_roughness;
	m->albedo = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2], pbr.base_color_factor[3]);
	m->metalness = pbr.metallic_factor;
	m->roughness = pbr.roughness_factor;
	m->albedo_tex.asset_handle = resolve_texture_guid(pbr.base_color_texture, gltf_path);
	m->metalness_tex.asset_handle = resolve_texture_guid(pbr.metallic_roughness_texture, gltf_path);
	m->roughness_tex.asset_handle = resolve_texture_guid(pbr.metallic_roughness_texture, gltf_path);
	m->normal_tex.asset_handle = resolve_texture_guid(mat->normal_texture, gltf_path);
	return m;
}

// Deduplication key for a cgltf_mesh
using GltfMeshKey = size_t;

static GltfMeshKey calculate_mesh_key(const cgltf_mesh *mesh)
{
	size_t hash = 0;

	for (cgltf_size p = 0; p < mesh->primitives_count; p++)
	{
		const cgltf_primitive *prim = &mesh->primitives[p];
		const void *pos = nullptr;
		const void *norm = nullptr;
		const void *uv = nullptr;
		for (cgltf_size i = 0; i < prim->attributes_count; i++)
		{
			const cgltf_attribute &a = prim->attributes[i];
			if (a.type == cgltf_attribute_type_position) pos = a.data;
			else if (a.type == cgltf_attribute_type_normal) norm = a.data;
			else if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uv = a.data;
		}

		hashCombine(hash, pos);
		hashCombine(hash, norm);
		hashCombine(hash, prim->indices);
		hashCombine(hash, uv);
	}

	return hash;
}

using GltfMeshCache = eastl::unordered_map<GltfMeshKey, eastl::vector<Ref<Engine::Mesh>>>;

// Builds the scene node tree, assigning pre-built meshes from cache.
static void process_node(const cgltf_node *gltf_node, MeshNode *parent,
							eastl::vector<MeshNode *> &linear_nodes,
							const GltfMeshCache &mesh_cache,
							const char *path)
{
	MeshNode *node = new MeshNode();
	linear_nodes.push_back(node);
	node->parent = parent;
	if (parent)
		parent->children.push_back(node);

	if (gltf_node->name)
		node->name = gltf_node->name;

	float transform_data[16];
	cgltf_node_transform_local(gltf_node, transform_data);
	memcpy(glm::value_ptr(node->local_model_matrix), transform_data, 64);

	if (gltf_node->mesh)
	{
		auto it = mesh_cache.find(calculate_mesh_key(gltf_node->mesh));
		if (it != mesh_cache.end())
		{
			const cgltf_mesh *gltf_mesh = gltf_node->mesh;
			for (cgltf_size p = 0; p < gltf_mesh->primitives_count; p++)
			{
				if (gltf_mesh->primitives[p].type != cgltf_primitive_type_triangles)
					continue;
				if (p < it->second.size() && it->second[p])
				{
					MeshNode::Primitive &prim = node->primitives.emplace_back();
					prim.mesh = it->second[p];
					prim.material = extract_material(gltf_mesh->primitives[p].material, path);
				}
			}
		}
	}

	for (cgltf_size c = 0; c < gltf_node->children_count; c++)
		process_node(gltf_node->children[c], node, linear_nodes, mesh_cache, path);
}

static constexpr uint64_t AUTO_MESHLET_VERTEX_COUNT = 1'000'000;

void GltfImporter::import(const char *path, Model *model, ModelImportSettings &settings)
{
	PROFILE_CPU_FUNCTION();

	cgltf_options options{};
	cgltf_data *data = nullptr;
	if (cgltf_parse_file(&options, path, &data) != cgltf_result_success)
	{
		CORE_ERROR("GltfImporter: failed to parse '{}'", path);
		return;
	}

	// Memory-map all .bin gltf buffers.
	auto base_dir = std::filesystem::path(path).parent_path();
	eastl::vector<FileMemory> mapped_buffers(data->buffers_count);
	for (cgltf_size i = 0; i < data->buffers_count; i++)
	{
		cgltf_buffer &buf = data->buffers[i];
		if (buf.data || !buf.uri || strncmp(buf.uri, "data:", 5) == 0)
			continue;
		eastl::string bin_path = (base_dir / buf.uri).string().c_str();
		if (mapped_buffers[i].open(bin_path, true, 0))
		{
			buf.data = mapped_buffers[i].getData();
			buf.data_free_method = cgltf_data_free_method_none;
		}
	}

	// load other, non .bin buffers data
	cgltf_load_buffers(&options, data, path);

	const cgltf_scene *scene = data->scene;
	if (!scene && data->scenes_count > 0)
		scene = &data->scenes[0];

	if (!scene || scene->nodes_count == 0)
	{
		CORE_ERROR("GltfImporter: no scene in '{}'", path);
		cgltf_free(data);
		return;
	}

	// Collect unique mesh jobs
	struct PrimBuildResult
	{
		Ref<Engine::Mesh> engine_mesh;
		bool success = false;
	};
	struct MeshBuildJob
	{
		const cgltf_mesh *gltf_mesh;
		GltfMeshKey key;
		eastl::vector<PrimBuildResult> prims;
	};

	eastl::vector<MeshBuildJob> jobs;
	{
		eastl::unordered_set<GltfMeshKey> seen;
		for (cgltf_size i = 0; i < data->meshes_count; i++)
		{
			const cgltf_mesh *m = &data->meshes[i];
			GltfMeshKey key = calculate_mesh_key(m);

			if (!seen.insert(key).second)
				continue;

			MeshBuildJob &job = jobs.emplace_back();
			job.gltf_mesh = m;
			job.key = key;
			job.prims.resize(m->primitives_count);
			for (cgltf_size p = 0; p < m->primitives_count; p++)
				if (m->primitives[p].type == cgltf_primitive_type_triangles)
					job.prims[p].engine_mesh = new Engine::Mesh();
		}
	}

	// Sort that biggest meshes go first, better shows peak memory consumption, better multithreaded
	auto get_total_vertices = [](const cgltf_mesh *m)
	{
		size_t total = 0;
		for (cgltf_size p = 0; p < m->primitives_count; p++)
		{
			const cgltf_primitive &prim = m->primitives[p];
			for (cgltf_size i = 0; i < prim.attributes_count; i++)
			{
				const cgltf_attribute &attribute = prim.attributes[i];
				if (attribute.type == cgltf_attribute_type_position && attribute.data)
				{
					total += attribute.data->count;
					break;
				}
			}
		}
		return total;
	};
	eastl::sort(jobs.begin(), jobs.end(), [&](const MeshBuildJob &a, const MeshBuildJob &b)
	{
		return get_total_vertices(a.gltf_mesh) > get_total_vertices(b.gltf_mesh);
	});

	if (!settings.generate_meshlets_explicitly_set)
	{
		uint64_t total_vertices = 0;
		for (const MeshBuildJob &job : jobs)
			total_vertices += get_total_vertices(job.gltf_mesh);
		settings.generate_meshlets = total_vertices > AUTO_MESHLET_VERTEX_COUNT;
	}

	// Remove some jobs for debugging
	#if GLTF_IMPORT_MESH_START > 0
	if ((int)jobs.size() > GLTF_IMPORT_MESH_START)
		jobs.erase(jobs.begin(), jobs.begin() + GLTF_IMPORT_MESH_START);
	else
		jobs.clear();
	#endif
	#if GLTF_IMPORT_MESH_LIMIT > 0
	if ((int)jobs.size() > GLTF_IMPORT_MESH_LIMIT)
		jobs.resize(GLTF_IMPORT_MESH_LIMIT);
	#endif

	const int total_jobs = (int)jobs.size();

	uint64_t total_buf_bytes = 0;
	for (cgltf_size i = 0; i < data->buffers_count; i++)
		total_buf_bytes += data->buffers[i].size;
	CORE_INFO("GltfImporter: '{}' — {} nodes, {} meshes ({} unique), {:.2f} GB",
		path, data->nodes_count, data->meshes_count, total_jobs,
		total_buf_bytes / (1024.0 * 1024.0 * 1024.0));

	// Parallel vertex data extraction, meshlet building, output file write.
	auto mesh_path = AssetManager::getRuntimeAssetPath(std::filesystem::path(path));
	std::filesystem::create_directories(mesh_path.parent_path());
	auto writer = MeshSerializer::beginStream(mesh_path.string().c_str());

	std::atomic<int> job_counter = 0;
	std::atomic<int> done_counter = 0;

	auto worker = [&]()
	{
		// Reusable memory
		eastl::vector<Engine::Vertex> vertices;
		eastl::vector<uint32_t> indices;
		eastl::vector<uint32_t> remap;

		while (true)
		{
			const int job_id = job_counter.fetch_add(1, std::memory_order_relaxed);
			if (job_id >= total_jobs)
				break;
			MeshBuildJob &job = jobs[job_id];
			const char *mesh_name = job.gltf_mesh->name ? job.gltf_mesh->name : "mesh";

			for (cgltf_size p = 0; p < job.prims.size(); p++)
			{
				PrimBuildResult &res = job.prims[p];
				if (!res.engine_mesh)
					continue;

				const cgltf_primitive *primitive = &job.gltf_mesh->primitives[p];

				vertices.clear();
				indices.clear();
				ensure_primitive_decompressed(primitive);
				res.engine_mesh->attribute_flags = extract_vertices(primitive, vertices, indices, res.engine_mesh->bound_box);

				if (vertices.empty() || indices.empty())
					continue;

				// Dedup some vertices (about 20%)
				size_t src_count = vertices.size();
				remap.resize(src_count);
				size_t unique = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), src_count, sizeof(Engine::Vertex));
				if (unique < src_count)
				{
					meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
					meshopt_remapVertexBuffer(vertices.data(), vertices.data(), src_count, sizeof(Engine::Vertex), remap.data());
					vertices.resize(unique);
				}

				discard_primitive_pages(primitive);

				MeshletBuildData build_data;
				if (settings.generate_meshlets)
				{
					build_data = MeshletBuilder::build(res.engine_mesh, mesh_name, vertices, indices, settings);
				} else
				{
					res.engine_mesh->indexed.emplace();
					res.engine_mesh->indexed->vertices = std::move(vertices);
					res.engine_mesh->indexed->indices = std::move(indices);
				}

				{
					std::unique_lock lock(*writer.mutex);
					MeshSerializer::writeMeshBlock(writer, res.engine_mesh.getReference(), res.engine_mesh->meshlet_data ? &build_data : nullptr);
				}

				// Release unneded data
				res.engine_mesh->meshlet_data.reset();
				res.engine_mesh->indexed.reset();

				// Release reusable vectors when they are too big
				if (vertices.capacity() > 1'000'000) vertices.set_capacity(0);
				if (indices.capacity() > 4'000'000) indices.set_capacity(0);
				if (remap.capacity() > 1'000'000) remap.set_capacity(0);

				res.success = true;
			}

			int done = done_counter.fetch_add(1, std::memory_order_relaxed) + 1;
			if (done % 10 == 0 || done == total_jobs)
			{
				double memory_usage = Platform::getProcessMemoryUsage() / (1024.0 * 1024.0);
				CORE_INFO("GltfImporter: [{}/{}] Memory Usage={:.0f}MB", done, total_jobs, memory_usage);
			}
		}
	};

	// Cap threads to avoid exhausting memory on large models.
	int threads_count;
	if (engine_gltf_import_threads > 0)
		threads_count = engine_gltf_import_threads;
	else
		threads_count = std::max(1u, std::thread::hardware_concurrency());

	eastl::vector<std::thread> threads(threads_count - 1);
	for (auto &t : threads)
		t = std::thread(worker);
	worker(); // main thread also executes

	for (auto &t : threads)
		t.join();

	// Release memory mapped files
	for (cgltf_size i = 0; i < data->buffers_count; i++)
	{
		if (mapped_buffers[i].getData())
		{
			data->buffers[i].data = nullptr;
			mapped_buffers[i].close();
		}
	}

	// Set meshes id
	GltfMeshCache mesh_cache;
	mesh_cache.reserve(total_jobs);
	for (MeshBuildJob &job : jobs)
	{
		auto &cache_entry = mesh_cache.emplace(job.key, eastl::vector<Ref<Engine::Mesh>>(job.prims.size())).first->second;
		const char *mesh_name = job.gltf_mesh->name ? job.gltf_mesh->name : "mesh";
		for (size_t p = 0; p < job.prims.size(); p++)
		{
			PrimBuildResult &res = job.prims[p];
			if (!res.engine_mesh || !res.success)
				continue;

			Model::assign_mesh_id(model->meshes_id, res.engine_mesh, eastl::string(mesh_name), eastl::string(mesh_name));
			cache_entry[p] = res.engine_mesh;
		}
	}

	// Build scene graph
	if (scene->nodes_count == 1)
	{
		process_node(scene->nodes[0], nullptr, model->linear_nodes, mesh_cache, path);
	} else
	{
		MeshNode *synthetic = new MeshNode();
		synthetic->name = "Scene";
		model->linear_nodes.push_back(synthetic);
		for (cgltf_size i = 0; i < scene->nodes_count; i++)
			process_node(scene->nodes[i], synthetic, model->linear_nodes, mesh_cache, path);
	}
	CORE_INFO("GltfImporter: done, {} unique meshes built, {} total nodes", total_jobs, model->linear_nodes.size());

	if (!model->linear_nodes.empty())
	{
		model->root_node = model->linear_nodes[0];
		model->root_node->updateTransform();
	}

	MeshSerializer::finalizeStream(writer, model);

	for (cgltf_size i = 0; i < data->buffer_views_count; i++)
		release_meshopt_buffer_view(&data->buffer_views[i]);
	cgltf_free(data);
}
