#include "pch.h"
#include "MeshSerializer.h"
#include "Rendering/Model.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/ShaderStructs.h"
#include "Math/EngineMath.h"
#include "meshoptimizer.h"
#include <fstream>

static MeshFormat::Header make_header(uint32_t node_count, uint32_t mesh_count, uint32_t material_count)
{
	MeshFormat::Header h{};
	h.node_count = node_count;
	h.mesh_count = mesh_count;
	h.material_count = material_count;
	return h;
}

static MeshFormat::MeshEntry make_mesh_entry(const Engine::Mesh *mesh, uint64_t file_offset, const MeshletBuildData *build_data)
{
	MeshFormat::MeshEntry e{};
	e.file_offset = file_offset;
	e.mesh_id = mesh->id;
	e.vertex_count = mesh->indexed ? mesh->indexed->vertices.size() : 0;
	e.index_count = mesh->indexed ? mesh->indexed->indices.size() : 0;
	e.meshlet_vertex_count = build_data ? build_data->vertex_count : 0;
	e.meshlet_triangle_count = build_data ? build_data->triangles.size() : 0;
	e.meshlet_count = mesh->meshlet_data ? mesh->meshlet_data->meshlets.size() : 0;
	e.lod_group_count = mesh->meshlet_data ? mesh->meshlet_data->meshlet_lod_groups.size() : 0;
	e.lod_node_count = mesh->meshlet_data ? mesh->meshlet_data->lod_nodes.size() : 0;
	e.lod_level_count = mesh->meshlet_data ? mesh->meshlet_data->meshlet_lod_levels.size() : 0;
	e.meshlet_root_group_local_offset = mesh->meshlet_data ? mesh->meshlet_data->meshlet_root_group_local_offset : 0;
	e.attribute_flags = mesh->attribute_flags;
	memcpy(e.bbox_min, &mesh->bound_box.min, 12);
	memcpy(e.bbox_max, &mesh->bound_box.max, 12);
	uint32_t vertex_stride = MeshFormat::diskVertexStride(e.attribute_flags);
	e.meshlet_vertices_file_offset = file_offset
		+ Math::alignedSize(e.vertex_count * sizeof(Engine::Vertex), MeshFormat::ALIGNMENT)
		+ Math::alignedSize(e.index_count * sizeof(uint32_t), MeshFormat::ALIGNMENT);
	e.meshlet_triangles_file_offset = e.meshlet_vertices_file_offset
		+ Math::alignedSize((uint64_t)e.meshlet_vertex_count * vertex_stride, MeshFormat::ALIGNMENT);
	return e;
}

static void serialize_mesh_block(BinaryArchive &ar, Engine::Mesh *mesh, const MeshFormat::MeshEntry &entry,
	Engine::MeshletFileView *out_file_view = nullptr, const MeshletBuildData *build_data = nullptr)
{
	if (ar.isLoading())
	{
		if (entry.vertex_count > 0)
		{
			mesh->indexed.emplace();
			ar.array(mesh->indexed->vertices, entry.vertex_count);
			ar.array(mesh->indexed->indices, entry.index_count);
		}

		uint32_t vertex_stride = MeshFormat::diskVertexStride(entry.attribute_flags);
		const uint8_t *vertices_ptr = ar.map<uint8_t>(entry.meshlet_vertex_count * vertex_stride);
		const uint8_t *triangles_ptr = ar.map<uint8_t>(entry.meshlet_triangle_count);
		if (out_file_view)
		{
			out_file_view->vertices_ptr = vertices_ptr;
			out_file_view->triangles_ptr = triangles_ptr;
			out_file_view->vertex_count = entry.meshlet_vertex_count;
			out_file_view->triangle_count = entry.meshlet_triangle_count;
		}

		if (entry.meshlet_count > 0)
		{
			mesh->meshlet_data.emplace();
			eastl::vector<MeshFormat::DiskMeshlet> disk_meshlets;
			ar.array(disk_meshlets, entry.meshlet_count);
			mesh->meshlet_data->meshlets.resize(entry.meshlet_count);
			for (size_t i = 0; i < entry.meshlet_count; i++)
				mesh->meshlet_data->meshlets[i] = MeshFormat::decodeMeshlet(disk_meshlets[i]);
			ar.array(mesh->meshlet_data->meshlet_lod_groups, entry.lod_group_count);
			ar.array(mesh->meshlet_data->lod_nodes, entry.lod_node_count);
			ar.array(mesh->meshlet_data->meshlet_lod_levels, entry.lod_level_count);
		}
	} else
	{
		static const Engine::IndexedGeometry empty_traditional_geom;
		static const Engine::MeshletGeometry empty_meshlet_geom;
		const Engine::IndexedGeometry &traditional_geom = mesh->indexed ? *mesh->indexed : empty_traditional_geom;
		const Engine::MeshletGeometry &meshlet_geom = mesh->meshlet_data ? *mesh->meshlet_data : empty_meshlet_geom;

		ar.array(const_cast<Engine::Vertex *>(traditional_geom.vertices.data()), traditional_geom.vertices.size());
		ar.array(const_cast<uint32_t *>(traditional_geom.indices.data()), traditional_geom.indices.size());

		if (build_data)
		{
			ar.array(const_cast<uint8_t *>(build_data->vertices.data()), build_data->vertices.size());
			ar.array(const_cast<uint8_t *>(build_data->triangles.data()), build_data->triangles.size());
		}

		eastl::vector<MeshFormat::DiskMeshlet> disk_meshlets(meshlet_geom.meshlets.size());
		for (size_t i = 0; i < disk_meshlets.size(); i++)
			disk_meshlets[i] = MeshFormat::encodeMeshlet(meshlet_geom.meshlets[i]);
		ar.array(disk_meshlets.data(), disk_meshlets.size());
		ar.array(const_cast<LODGroup *>(meshlet_geom.meshlet_lod_groups.data()), meshlet_geom.meshlet_lod_groups.size());
		ar.array(const_cast<LodNode *>(meshlet_geom.lod_nodes.data()), meshlet_geom.lod_nodes.size());
		ar.array(const_cast<LODLevel *>(meshlet_geom.meshlet_lod_levels.data()), meshlet_geom.meshlet_lod_levels.size());
	}
}

void MeshSerializer::write_scene_data(BinaryArchive &ar, const Model *model, const eastl::vector<const Material *> &unique_materials,
										const eastl::vector<MeshFormat::PrimitiveRef> &primitive_refs, const eastl::vector<MeshFormat::MeshEntry> &mesh_entries, MeshFormat::OffsetTable &table)
{
	table.materials_offset = ar.tell();
	for (const Material *mat : unique_materials)
	{
		auto d = MeshFormat::encodeMaterial(mat);
		ar << d;
	}

	eastl::unordered_map<const MeshNode *, uint32_t> node_idx;
	for (uint32_t i = 0; i < model->linear_nodes.size(); i++)
		node_idx[model->linear_nodes[i]] = i;

	table.nodes_offset = ar.tell();
	uint32_t cur_prim_ref = 0;
	for (const MeshNode *node : model->linear_nodes)
	{
		MeshFormat::NodeDescriptor d{};
		strncpy_s(d.name, sizeof(d.name), node->name.c_str(), _TRUNCATE);
		memcpy(d.local_transform, glm::value_ptr(node->local_model_matrix), 64);
		d.parent_index = node->parent ? node_idx[node->parent] : -1;
		d.primitive_count = node->primitives.size();
		d.first_primitive_ref = cur_prim_ref;
		cur_prim_ref += d.primitive_count;
		ar << d;
	}

	table.primitive_refs_offset = ar.tell();
	ar.array(primitive_refs.data(), primitive_refs.size());

	table.mesh_entries_offset = ar.tell();
	ar.array(mesh_entries.data(), mesh_entries.size());
}

static void log_mesh_entry_stats(const MeshFormat::MeshEntry &e)
{
	uint64_t total =
		e.vertex_count * sizeof(Engine::Vertex) +
		e.index_count * sizeof(uint32_t) +
		e.meshlet_vertex_count * MeshFormat::diskVertexStride(e.attribute_flags) +
		e.meshlet_triangle_count +
		e.meshlet_count * sizeof(MeshFormat::DiskMeshlet) +
		e.lod_group_count * sizeof(LODGroup) +
		e.lod_node_count * sizeof(LodNode) +
		e.lod_level_count * sizeof(LODLevel);
	uint32_t tris = e.index_count > 0 ? e.index_count / 3 : e.meshlet_triangle_count / 3;
	uint32_t verts = e.vertex_count > 0 ? e.vertex_count : e.meshlet_vertex_count;
	CORE_INFO("  Mesh {:016X}: {} verts  {} tris  {:.3f} MB  ({:.1f} B/tri)", e.mesh_id, verts, tris, total / (1024.0f * 1024.0), tris > 0 ? (double)total / tris : 0.0);
}

MeshSerializer::StreamingWriter MeshSerializer::beginStream(const char *path)
{
	StreamingWriter w;
	w.file.open(path, std::ios::binary);
	if (!w.file.is_open())
	{
		CORE_ERROR("MeshSerializer::beginStream failed: {}", path);
		return w;
	}

	BinaryArchive ar = BinaryArchive::createForSaving(w.file, MeshFormat::ALIGNMENT);
	ar.reserve(sizeof(MeshFormat::Header));
	ar.reserve(sizeof(MeshFormat::OffsetTable));

	w.is_begin = true;
	return w;
}

bool MeshSerializer::writeMeshBlock(StreamingWriter &w, Engine::Mesh *mesh, const MeshletBuildData *build_data)
{
	if (!w.is_begin)
		return false;

	BinaryArchive ar = BinaryArchive::createForSaving(w.file, MeshFormat::ALIGNMENT);
	MeshFormat::MeshEntry entry = make_mesh_entry(mesh, ar.tell(), build_data);
	serialize_mesh_block(ar, mesh, entry, nullptr, build_data);
	log_mesh_entry_stats(entry);

	w.mesh_to_id[mesh] = w.mesh_entries.size();
	w.mesh_entries.push_back(entry);
	return true;
}

bool MeshSerializer::finalizeStream(StreamingWriter &w, const Model *model)
{
	if (!w.is_begin || !model)
		return false;

	eastl::vector<const Material *> unique_materials;
	eastl::vector<MeshFormat::PrimitiveRef> primitive_refs;
	eastl::unordered_map<const Material *, uint32_t> mat_to_idx;

	for (const MeshNode *node : model->linear_nodes)
	{
		for (const auto &prim : node->primitives)
		{
			Engine::Mesh *mesh = prim.mesh;
			auto it = w.mesh_to_id.find(mesh);
			uint32_t mesh_idx = (it != w.mesh_to_id.end()) ? it->second : 0;
			auto [ai, a_new] = mat_to_idx.emplace(prim.material.getReference(), unique_materials.size());
			if (a_new)
				unique_materials.push_back(prim.material.getReference());
			primitive_refs.push_back({mesh_idx, ai->second});
		}
	}

	for (auto &[mesh, idx] : w.mesh_to_id)
		w.mesh_entries[idx].mesh_id = mesh->id;

	BinaryArchive ar = BinaryArchive::createForSaving(w.file, MeshFormat::ALIGNMENT);

	MeshFormat::OffsetTable table{};
	write_scene_data(ar, model, unique_materials, primitive_refs, w.mesh_entries, table);

	uint64_t file_size = ar.tell();

	MeshFormat::Header header = make_header(model->linear_nodes.size(), w.mesh_entries.size(), unique_materials.size());
	ar.patchAt(0, &header, sizeof(header));
	ar.patchAt(sizeof(MeshFormat::Header), &table, sizeof(table));
	w.file.close();

	CORE_INFO("Saved .mesh: {} nodes, {} meshes, {} materials  {:.3f} MB total", header.node_count, header.mesh_count, header.material_count, file_size / (1024.0 * 1024.0));
	w.is_begin = false;
	return true;
}

bool MeshSerializer::save(const Model *model, const char *filepath, const eastl::unordered_map<Engine::Mesh *, MeshletBuildData> *build_data_map)
{
	if (!model || !filepath || !model->getRootNode())
		return false;

	std::ofstream f(filepath, std::ios::binary);
	if (!f.is_open())
	{
		CORE_ERROR("MeshSerializer::save - failed to create: {}", filepath);
		return false;
	}

	eastl::vector<Engine::Mesh *> unique_meshes;
	eastl::vector<const Material *> unique_materials;
	eastl::vector<MeshFormat::PrimitiveRef> primitive_refs;
	eastl::unordered_map<Engine::Mesh *, uint32_t> mesh_to_id;
	eastl::unordered_map<const Material *, uint32_t> mat_to_idx;

	for (const MeshNode *node : model->linear_nodes)
	{
		for (const auto &prim : node->primitives)
		{
			auto [mi, m_new] = mesh_to_id.emplace(prim.mesh, unique_meshes.size());
			if (m_new)
				unique_meshes.push_back(prim.mesh);
			auto [ai, a_new] = mat_to_idx.emplace(prim.material.getReference(), unique_materials.size());
			if (a_new)
				unique_materials.push_back(prim.material.getReference());
			primitive_refs.push_back({mi->second, ai->second});
		}
	}

	BinaryArchive ar = BinaryArchive::createForSaving(f, MeshFormat::ALIGNMENT);
	ar.reserve(sizeof(MeshFormat::Header));
	ar.reserve(sizeof(MeshFormat::OffsetTable));

	eastl::vector<MeshFormat::MeshEntry> mesh_entries(unique_meshes.size());
	for (uint32_t i = 0; i < unique_meshes.size(); i++)
	{
		Engine::Mesh *mesh = unique_meshes[i];
		const MeshletBuildData *build_data = nullptr;
		if (build_data_map)
		{
			auto it = build_data_map->find(mesh);
			if (it != build_data_map->end())
				build_data = &it->second;
		}
		mesh_entries[i] = make_mesh_entry(mesh, ar.tell(), build_data);
		serialize_mesh_block(ar, mesh, mesh_entries[i], nullptr, build_data);
		log_mesh_entry_stats(mesh_entries[i]);
	}

	MeshFormat::OffsetTable table{};
	write_scene_data(ar, model, unique_materials, primitive_refs, mesh_entries, table);

	MeshFormat::Header header = make_header(model->linear_nodes.size(), unique_meshes.size(), unique_materials.size());

	uint64_t file_size = ar.tell();
	ar.patchAt(0, &header, sizeof(header));
	ar.patchAt(sizeof(MeshFormat::Header), &table, sizeof(table));

	CORE_INFO("Saved .mesh: {}  ({} nodes, {} meshes, {} materials)  {:.3f} MB total", filepath, header.node_count, header.mesh_count, header.material_count, file_size / (1024.0 * 1024.0));
	return true;
}

bool MeshSerializer::load(Model *model, const char *filepath)
{
	if (!model || !filepath)
		return false;
	model->cleanup();

	if (!model->mesh_file_memory.open(filepath, true, 0))
		return false;

	return load_from_memory(model, static_cast<const uint8_t *>(model->mesh_file_memory.getData()), static_cast<uint64_t>(model->mesh_file_memory.getSize()));
}

bool MeshSerializer::load_from_memory(Model *model, const uint8_t *data, uint64_t file_size)
{
	if (file_size < sizeof(MeshFormat::Header) + sizeof(MeshFormat::OffsetTable))
	{
		CORE_ERROR("MeshSerializer::load - file too small");
		return false;
	}

	const auto *header = reinterpret_cast<const MeshFormat::Header *>(data);
	if (!header->isValid())
	{
		CORE_ERROR("MeshSerializer::load - invalid header");
		return false;
	}
	const auto *table = reinterpret_cast<const MeshFormat::OffsetTable *>(data + sizeof(MeshFormat::Header));
	const auto *mesh_entries = reinterpret_cast<const MeshFormat::MeshEntry *>(data + table->mesh_entries_offset);
	const auto *mat_descs = reinterpret_cast<const MeshFormat::MaterialDescriptor *>(data + table->materials_offset);
	const auto *prim_refs = reinterpret_cast<const MeshFormat::PrimitiveRef *>(data + table->primitive_refs_offset);
	const auto *node_descs = reinterpret_cast<const MeshFormat::NodeDescriptor *>(data + table->nodes_offset);

	eastl::vector<Ref<Engine::Mesh>> meshes;
	meshes.reserve(header->mesh_count);
	for (uint32_t i = 0; i < header->mesh_count; i++)
	{
		const MeshFormat::MeshEntry &entry = mesh_entries[i];

		Ref<Engine::Mesh> mesh = new Engine::Mesh();
		mesh->id = entry.mesh_id;
		mesh->bound_box.min = glm::vec3(entry.bbox_min[0], entry.bbox_min[1], entry.bbox_min[2]);
		mesh->bound_box.max = glm::vec3(entry.bbox_max[0], entry.bbox_max[1], entry.bbox_max[2]);
		mesh->attribute_flags = entry.attribute_flags;

		BinaryArchive ar = BinaryArchive::createForLoading(data, file_size, entry.file_offset, MeshFormat::ALIGNMENT);
		serialize_mesh_block(ar, mesh, entry, &model->file_views[mesh->id]);

		if (mesh->meshlet_data)
		{
			Engine::MeshletGeometry &meshlet_geom = *mesh->meshlet_data;
			meshlet_geom.meshlet_root_group_local_offset = entry.meshlet_root_group_local_offset;

			// Reconstruct group data info from meshlets
			meshlet_geom.meshlet_lod_group_data_info.resize(entry.lod_group_count);
			uint32_t current_vert = 0;
			uint32_t current_tri = 0;
			for (uint32_t j = 0; j < entry.lod_group_count; j++)
			{
				const LODGroup &lod = meshlet_geom.meshlet_lod_groups[j];
				Engine::MeshletGeometry::LODGroupDataInfo &gdi = meshlet_geom.meshlet_lod_group_data_info[j];
				gdi.cpu_vertex_offset = current_vert;
				gdi.cpu_triangle_offset = current_tri;
				uint32_t vert_count = 0;
				uint32_t tri_count = 0;
				for (uint32_t k = lod.first_meshlet; k < lod.first_meshlet + lod.meshlet_count; k++)
				{
					vert_count += meshlet_geom.meshlets[k].vertex_count;
					tri_count += meshlet_geom.meshlets[k].triangle_count * 3;
				}
				gdi.cpu_vertex_count = vert_count;
				gdi.cpu_triangle_count = tri_count;
				current_vert += vert_count;
				current_tri += tri_count;
			}
			mesh->initMeshleted();
		} else
		{
			mesh->initTraditional(std::move(mesh->indexed->vertices), std::move(mesh->indexed->indices));
		}
		meshes.push_back(mesh);
	}

	eastl::vector<Ref<Material>> materials;
	materials.reserve(header->material_count);
	for (uint32_t i = 0; i < header->material_count; i++)
		materials.push_back(MeshFormat::decodeMaterial(mat_descs[i]));

	eastl::vector<MeshNode *> nodes(header->node_count);
	for (auto &n : nodes)
		n = new MeshNode();

	for (uint32_t i = 0; i < header->node_count; i++)
	{
		const MeshFormat::NodeDescriptor &d = node_descs[i];
		MeshNode *node = nodes[i];
		node->name = d.name;
		memcpy(glm::value_ptr(node->local_model_matrix), d.local_transform, 64);

		if (d.parent_index >= 0)
		{
			node->parent = nodes[d.parent_index];
			nodes[d.parent_index]->children.push_back(node);
		}
		node->global_model_matrix = node->parent
			? node->parent->global_model_matrix * node->local_model_matrix
			: node->local_model_matrix;
		for (uint32_t p = 0; p < d.primitive_count; p++)
		{
			const MeshFormat::PrimitiveRef &ref = prim_refs[d.first_primitive_ref + p];
			Ref<Engine::Mesh> &mesh = meshes[ref.mesh_idx];
			node->primitives.push_back({mesh, materials[ref.material_idx]});
			model->meshes_id[mesh->id] = mesh;
			model->materials_id[mesh->id] = materials[ref.material_idx];
		}
	}

	model->root_node = nodes[0];
	model->linear_nodes = nodes;

	CORE_INFO("Loaded .mesh: {} nodes, {} meshes, {} materials", header->node_count, header->mesh_count, header->material_count);
	return true;
}
