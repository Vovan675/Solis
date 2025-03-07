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

	virtual void setRenderTargets(const std::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear) = 0;
	virtual void resetRenderTargets() = 0;
	virtual std::vector<RHITexture *> &getCurrentRenderTargets() = 0;

	virtual void setPipeline(RHIPipeline *pipeline) = 0;

	virtual void setVertexBuffer(RHIBuffer *buffer) = 0;
	virtual void setIndexBuffer(RHIBuffer *buffer) = 0;
	virtual void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;
	virtual void drawInstanced(uint32_t vertex_count_per_instance, uint32_t instance_count, uint32_t firstVertex, uint32_t firstInstance) = 0;

	virtual void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) = 0;

	virtual void beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size) = 0;
	virtual void endDebugLabel() = 0;
};
