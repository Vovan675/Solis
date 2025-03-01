#include "pch.h"
#include "VulkanCommandList.h"
#include "VulkanDynamicRHI.h"
#include "VulkanUtils.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHITexture.h"
#include "RHI/RHIPipeline.h"

VulkanCommandList::VulkanCommandList()
{
	device = VulkanUtils::getNativeRHI()->device->logicalHandle;
	cmd_pool = VulkanUtils::getNativeRHI()->command_pool;

	// Create buffer
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = cmd_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer);
}

void VulkanCommandList::setRenderTargets(const std::vector<std::shared_ptr<RHITexture>> &color_attachments, std::shared_ptr<RHITexture> depth_attachment, int layer, int mip, bool clear)
{
	VkExtent2D extent;
	if (color_attachments.size() > 0)
	{
		extent.width = color_attachments[0]->getWidth(mip);
		extent.height = color_attachments[0]->getHeight(mip);
	} else
	{
		extent.width = depth_attachment->getWidth(0);
		extent.height = depth_attachment->getHeight(0);
	}

	VkRenderingInfo rendering_info{};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.renderArea.offset = {0, 0};
	rendering_info.renderArea.extent = extent;
	rendering_info.layerCount = 1;

	std::vector<VkRenderingAttachmentInfo> color_attachments_info;
	for (const auto &attachment : color_attachments)
	{
		VulkanTexture *texture = (VulkanTexture *)attachment.get();

		VkRenderingAttachmentInfo info{};
		info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		info.imageView = texture->getImageView(mip, layer);
		info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		info.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
		color_attachments_info.push_back(info);
	}
	rendering_info.colorAttachmentCount = color_attachments_info.size();
	rendering_info.pColorAttachments = color_attachments_info.data();

	VkRenderingAttachmentInfo depth_stencil_attachment_info{};
	if (depth_attachment != nullptr)
	{
		VulkanTexture *texture = (VulkanTexture *)depth_attachment.get();

		depth_stencil_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_stencil_attachment_info.imageView = texture->getImageView(mip, layer);
		depth_stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_stencil_attachment_info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		depth_stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_stencil_attachment_info.clearValue.depthStencil.depth = 1.0f;
		depth_stencil_attachment_info.clearValue.depthStencil.stencil = 0.0f;
		rendering_info.pDepthAttachment = &depth_stencil_attachment_info;
	}

	vkCmdBeginRendering(cmd_buffer, &rendering_info);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = extent.height;
	viewport.width = extent.width;
	viewport.height = extent.height; // Use negative height for Y-axis flipping
	viewport.height *= -1;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = extent;
	vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

	current_render_targets = color_attachments;
	if (depth_attachment != nullptr)
		current_render_targets.push_back(depth_attachment);
}

void VulkanCommandList::setPipeline(std::shared_ptr<RHIPipeline> pipeline)
{
	VulkanPipeline *native_pipeline = static_cast<VulkanPipeline *>(pipeline.get());
	VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
	if (native_pipeline->description.is_ray_tracing_pipeline)
		bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
	else if (native_pipeline->description.is_compute_pipeline)
		bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
	vkCmdBindPipeline(cmd_buffer, bind_point, native_pipeline->pipeline);
	current_pipeline = pipeline;
}

void VulkanCommandList::setVertexBuffer(std::shared_ptr<RHIBuffer> buffer)
{
	VulkanBuffer *native_buffer = static_cast<VulkanBuffer *>(buffer.get());
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &native_buffer->bufferHandle, offsets);
}

void VulkanCommandList::setIndexBuffer(std::shared_ptr<RHIBuffer> buffer)
{
	VulkanBuffer *native_buffer = static_cast<VulkanBuffer *>(buffer.get());
	vkCmdBindIndexBuffer(cmd_buffer, native_buffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32); // TODO 16 bit?
}

void VulkanCommandList::beginDebugLabel(const char *label, glm::vec3 color)
{
	VulkanUtils::cmdBeginDebugLabel(this, label, color);
}

void VulkanCommandList::endDebugLabel()
{
	VulkanUtils::cmdEndDebugLabel(this);
}
