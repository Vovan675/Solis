#pragma once

class RHIBuffer;
class RHITexture;
class RHIPipeline;

class RHICommandList
{
public:
	virtual ~RHICommandList();

	virtual void open() = 0;
	virtual void close() = 0;

	virtual void setRenderTargets(const std::vector<std::shared_ptr<RHITexture>> &color_attachments, std::shared_ptr<RHITexture> depth_attachment, int layer, int mip, bool clear) = 0;
	virtual void resetRenderTargets() = 0;
	virtual std::vector<std::shared_ptr<RHITexture>> &getCurrentRenderTargets() = 0;

	virtual void setPipeline(std::shared_ptr<RHIPipeline> pipeline) = 0;

	virtual void setVertexBuffer(std::shared_ptr<RHIBuffer> buffer) = 0;
	virtual void setIndexBuffer(std::shared_ptr<RHIBuffer> buffer) = 0;
	virtual void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;
	virtual void drawInstanced(uint32_t vertex_count_per_instance, uint32_t instance_count, uint32_t firstVertex, uint32_t firstInstance) = 0;

	virtual void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) = 0;

	virtual void beginDebugLabel(const char *label, glm::vec3 color) = 0;
	virtual void endDebugLabel() = 0;
};
