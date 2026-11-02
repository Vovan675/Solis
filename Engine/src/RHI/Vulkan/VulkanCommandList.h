#pragma once
#include "RHI/DynamicRHI.h"
#include "TracyVulkan.hpp"

class VulkanCommandList final: public RHICommandList
{
public:
	VulkanCommandList();

	~VulkanCommandList()
	{
		gDynamicRHI->waitGPU();
		vkFreeCommandBuffers(device, cmd_pool, 1, &cmd_buffer);
	}

	void open() override
	{
		if (is_open)
			return;
		VkCommandBufferBeginInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Because we don't need to submit the same command buffer twice
		vkResetCommandBuffer(cmd_buffer, 0);
		vkBeginCommandBuffer(cmd_buffer, &info);
		is_open = true;
	}

	void close() override
	{
		if (!is_open)
			return;
		vkEndCommandBuffer(cmd_buffer);
		is_open = false;
	}

	void setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear, float depth_clear_value = 0.0f) override;

	void resetRenderTargets() override
	{
		vkCmdEndRendering(cmd_buffer);
		current_render_targets.clear();
	}

	void setPipeline(RHIPipeline *pipeline) override;

	void setVertexBuffer(RHIBuffer *buffer, uint32_t offset, uint32_t stride, uint32_t slot) override;

	void setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format = IndexFormat::UINT32) override;

	void drawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) override
	{
		gDynamicRHI->prepareRenderCall();
		vkCmdDrawIndexed(cmd_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
	}

	void drawInstanced(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
	{
		gDynamicRHI->prepareRenderCall();
		vkCmdDraw(cmd_buffer, vertex_count, instance_count, first_vertex, first_instance);
	}

	void drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer) override;
	void drawIndexedIndirect(RHIBuffer *args_buffer, uint32_t draw_count) override;
	void drawIndirect(RHIBuffer *args_buffer, uint32_t max_draw_count, RHIBuffer *count_buffer) override;
	void drawIndirect(RHIBuffer *args_buffer, uint32_t draw_count) override;

	void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) override
	{
		gDynamicRHI->prepareRenderCall();
		vkCmdDispatch(cmd_buffer, group_x, group_y, group_z);
	}

	void dispatchIndirect(RHIBuffer *args_buffer, uint32_t dispatch_count) override;

	void dispatchRays(uint32_t width, uint32_t height, uint32_t depth) override;
	void dispatchMesh(uint32_t group_x, uint32_t group_y, uint32_t group_z) override;
	void dispatchMeshIndirect(RHIBuffer *args_buffer, uint32_t draw_count) override;

	void copyBuffer(RHIBuffer *src, RHIBuffer *dst, uint64_t src_offset, uint64_t dst_offset, uint64_t size) override;
	void copyTexture(RHITexture *src, RHITexture *dst) override;
	void copyTextureToBuffer(RHITexture *src, RHIBuffer *dst) override;
	void clearBuffer(RHIBuffer *buffer, uint32_t value) override;
	void clearTexture(RHITexture *texture, const glm::vec4 &color) override;

	void beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size);
	void endDebugLabel();

	VkDevice device;
	VkCommandPool cmd_pool;
	VkCommandBuffer cmd_buffer;

	eastl::vector<std::unique_ptr<tracy::VkCtxScope>> tracy_debug_label_stack;
};