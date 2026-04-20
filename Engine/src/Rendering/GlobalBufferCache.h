#pragma once
#include "Mesh.h"
#include "RHI/RHICommandList.h"

class GlobalBufferCache
{
public:
	struct MeshGlobalOffsets
	{
		uint64_t lod_groups_offset;
		uint64_t lod_nodes_offset;
	};

	static void shutdown();

	static uint64_t addMeshletLodGroupData(LODGroup *data, uint32_t count, RHICommandList *cmd_list);
	static uint64_t addLodNodeData(LodNode *data, uint32_t count, RHICommandList *cmd_list);

	static void registerMeshOffsets(size_t mesh_id, uint64_t lod_groups_offset, uint64_t lod_nodes_offset);
	static MeshGlobalOffsets getMeshOffsets(size_t mesh_id);

	static uint64_t addMeshletGeometryData(RHIBuffer *source, uint64_t source_offset, uint32_t size, RHICommandList *cmd_list);
	static void removeMeshletGeometryData(uint64_t offset, uint64_t size);

	static RHIBuffer *getGlobalMeshletGeometryBuffer() { return geometry.get(); }
	static RHIBuffer *getGlobalMeshletLodGroupsBuffer() { return lod_groups.get(); }
	static RHIBufferRef getGlobalGroupChildrenBuffer() { return global_meshlet_group_children_buffer; }
	static RHIBuffer *getGlobalLodNodesBuffer() { return lod_nodes.get(); }

	static uint64_t getMeshletGeometryBufferMaxSize() { return geometry.getMaxSize(); }
	static uint64_t getMeshletGeometryBufferUsedSize() { return geometry.getUsedSize(); }

private:
	// One big buffer with free-list allocator
	struct GlobalBuffer
	{
		void init(uint64_t initial_max_size, BufferUsage buffer_usage, const char *name, bool growable = true);

		uint64_t add(const void *cpu_data, uint64_t size, RHICommandList *cmd_list);
		uint64_t add(RHIBuffer *src_buffer, uint64_t buffer_offset, uint64_t buffer_size, RHICommandList *cmd_list);

		void remove(uint64_t offset, uint64_t size);

		bool isInitialized() const { return buffer; }
		RHIBuffer *get() const { return buffer; }
		uint64_t getUsedSize() const;
		uint64_t getMaxSize() const { return max_size; }

	private:
		struct FreeRange { uint64_t offset; uint64_t size; };

		void ensure_created(uint64_t minimum_size);
		uint64_t allocate_range(uint64_t needed_size, RHICommandList *cmd_list);

		uint64_t initial_max_size = 0;
		BufferUsage buffer_usage = BufferUsage::NONE;
		const char *debug_name = nullptr;
		bool can_grow = true;
		eastl::vector<FreeRange> free_ranges; // sorted by offset

		uint64_t max_size = 0;
		RHIBufferRef buffer;
	};

	static inline eastl::unordered_map<size_t, MeshGlobalOffsets> mesh_offsets;

	static inline GlobalBuffer lod_groups;
	static inline GlobalBuffer lod_nodes;
	static inline GlobalBuffer geometry;

	static inline RHIBufferRef global_meshlet_group_children_buffer;
};
