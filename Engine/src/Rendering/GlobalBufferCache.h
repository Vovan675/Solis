#pragma once
#include "Mesh.h"

class GlobalBufferCache
{
public:
	static void shutdown();

	static uint64_t addVertexBufferData(Engine::Vertex *vertices, uint32_t count);
	static uint64_t addIndexBufferData(uint32_t *indices, uint32_t count);
	static uint64_t addMeshletVertexData(uint32_t *data, uint32_t count);
	static uint64_t addMeshletTriangleData(uint32_t *data, uint32_t count);

	static RHIBufferRef getGlobalVertexBuffer() { return global_vertex_buffer; }
	static RHIBufferRef getGlobalIndexBuffer() { return global_index_buffer; }
	static RHIBufferRef getGlobalMeshletVertexBuffer() { return global_meshlet_vertex_buffer; }
	static RHIBufferRef getGlobalMeshletTriangleBuffer() { return global_meshlet_triangle_buffer; }

private:
	static inline RHIBufferRef global_vertex_buffer;
	static inline RHIBufferRef global_index_buffer;
	static inline RHIBufferRef global_meshlet_vertex_buffer;
	static inline RHIBufferRef global_meshlet_triangle_buffer;

	static inline uint64_t current_vertex_buffer_max_size = 0;
	static inline uint64_t current_vertex_buffer_size = 0;

	static inline uint64_t current_index_buffer_max_size = 0;
	static inline uint64_t current_index_buffer_size = 0;

	static inline uint64_t current_meshlet_vertex_buffer_max_size = 0;
	static inline uint64_t current_meshlet_vertex_buffer_size = 0;

	static inline uint64_t current_meshlet_triangle_buffer_max_size = 0;
	static inline uint64_t current_meshlet_triangle_buffer_size = 0;
};