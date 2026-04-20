#pragma once
#include "MeshFormat.h"
#include "Utils/BinaryArchive.h"
#include "Rendering/MeshletBuilder.h"
#include <fstream>
#include <mutex>

class Model;
class Material;

namespace Engine
{
class Mesh;
}

class MeshSerializer
{
public:
	static bool save(const Model *model, const char *filepath,
		const eastl::unordered_map<Engine::Mesh *, MeshletBuildData> *build_data_map = nullptr);
	static bool load(Model *model, const char *filepath);

	struct StreamingWriter
	{
		std::ofstream file;
		eastl::vector<MeshFormat::MeshEntry> mesh_entries;
		eastl::unordered_map<Engine::Mesh *, uint32_t> mesh_to_id;
		std::shared_ptr<std::mutex> mutex = std::make_shared<std::mutex>();
		bool is_begin = false;
	};

	// Streaming writing, write parts of format while loading model
	static StreamingWriter beginStream(const char *path);
	static bool writeMeshBlock(StreamingWriter &w, Engine::Mesh *mesh, const MeshletBuildData *build_data = nullptr);
	static bool finalizeStream(StreamingWriter &w, const Model *model);

private:
	static bool load_from_memory(Model *model, const uint8_t *data, uint64_t file_size);

	static void write_scene_data(BinaryArchive &ar, const Model *model, const eastl::vector<const Material *> &unique_materials, const eastl::vector<MeshFormat::PrimitiveRef> &primitive_refs,
								 const eastl::vector<MeshFormat::MeshEntry> &mesh_entries, MeshFormat::OffsetTable &table);

	MeshSerializer() = delete;
};
