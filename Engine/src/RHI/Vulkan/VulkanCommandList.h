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
		VkCommandBufferBeginInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Because we don't need to submit the same command buffer twice
		vkResetCommandBuffer(cmd_buffer, 0);
		vkBeginCommandBuffer(cmd_buffer, &info);
		is_open = true;
	}

	void close() override
	{
		vkEndCommandBuffer(cmd_buffer);
		is_open = false;
	}

	void setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear) override;

	void resetRenderTargets() override
	{
		vkCmdEndRendering(cmd_buffer);
		current_render_targets.clear();
	}

	eastl::vector<RHITexture *> &getCurrentRenderTargets()
	{
		return current_render_targets;
	}

	void setPipeline(RHIPipeline *pipeline) override;

	void setVertexBuffer(RHIBuffer *buffer, uint32_t offset, uint32_t stride) override;

	void setIndexBuffer(RHIBuffer *buffer, uint32_t offset, IndexFormat format = IndexFormat::UINT32) override;

	void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override
	{
		gDynamicRHI->prepareRenderCall();
		vkCmdDrawIndexed(cmd_buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void drawInstanced(uint32_t vertex_count_per_instance, uint32_t instance_count, uint32_t firstVertex, uint32_t firstInstance)
	{
		gDynamicRHI->prepareRenderCall();
		vkCmdDraw(cmd_buffer, vertex_count_per_instance, instance_count, firstVertex, firstInstance);
	}

	void dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) override
	{
		gDynamicRHI->prepareRenderCall();
		vkCmdDispatch(cmd_buffer, group_x, group_y, group_z);
	}
	
	void dispatchRays(uint32_t width, uint32_t height, uint32_t depth) override;

	void copyBuffer(RHIBuffer *src, RHIBuffer *dest, uint64_t src_offset, uint64_t dest_offset, uint64_t size) override;

	void beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char* source, size_t source_size, const char* function, size_t function_size);
	void endDebugLabel();

	bool is_open = false;

	VkDevice device;
	VkCommandPool cmd_pool;
	VkCommandBuffer cmd_buffer;
	RHIPipeline *current_pipeline;
	eastl::vector<RHITexture *> current_render_targets;

	eastl::vector<std::unique_ptr<tracy::VkCtxScope>> tracy_debug_label_stack;
};