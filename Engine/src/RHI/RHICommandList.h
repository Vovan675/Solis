#pragma once
#include "RHIDefinitions.h"

class RHIBuffer;
class RHITexture;
class RHIPipeline;

class RHICommandList
{
public:
	virtual ~RHICommandList();

	virtual void open() = 0;
	virtual void close() = 0;
	bool isOpen() const { return is_open; }

	virtual void setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear, float depth_clear_value = 0.0f) = 0;
	virtual void resetRenderTargets() = 0;
	eastl::vector<RHITexture *> &getCurrentRenderTargets() { return current_render_targets; }

	virtual void setPipeline(RHIPipeline *pipeline) = 0;
	RHIPipeline *getCurrentPipeline() const { return current_pipeline; }

	virtual void setVertexBuffer(RHIBuffer *buffer, uint32_t offset, uint32_t stride, uint32_t slot = 0) = 0;
	virtual void setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format = IndexFormat::UINT32) = 0;
	virtual void drawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) = 0;
	virtual void drawInstanced(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) = 0;

	virtual void drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t draw_count) = 0;
	virtual void drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer) = 0;
	virtual void drawIndirect(RHIBuffer *args_buffer, uint32_t draw_count) = 0;
	virtual void drawIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer) = 0;

	virtual void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) = 0;
	virtual void dispatchIndirect(RHIBuffer *args_buffer, uint32_t dispatch_count) = 0;
	virtual void dispatchRays(uint32_t width, uint32_t height, uint32_t depth) = 0;
	virtual void dispatchMesh(uint32_t group_x, uint32_t group_y, uint32_t group_z) = 0;
	virtual void dispatchMeshIndirect(RHIBuffer *args_buffer, uint32_t draw_count) = 0;

	virtual void copyBuffer(RHIBuffer *src, RHIBuffer *dst, uint64_t src_offset, uint64_t dst_offset, uint64_t size) = 0;
	virtual void copyTexture(RHITexture *src, RHITexture *dst) = 0;
	virtual void copyTextureToBuffer(RHITexture *src, RHIBuffer *dst) = 0;
	virtual void clearBuffer(RHIBuffer *buffer, uint32_t value) = 0;
	virtual void clearTexture(RHITexture *texture, const glm::vec4 &color) = 0;

	virtual void beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size) = 0;
	virtual void endDebugLabel() = 0;

protected:
	bool is_open = false;
	RHIPipeline *current_pipeline = nullptr;
	eastl::vector<RHITexture *> current_render_targets;
};
