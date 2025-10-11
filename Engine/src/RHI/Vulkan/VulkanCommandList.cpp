#include "pch.h"
#include "VulkanCommandList.h"
#include "VulkanDynamicRHI.h"
#include "VulkanUtils.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHITexture.h"
#include "RHI/RHIPipeline.h"
#include "Utils/Math.h"

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

void VulkanCommandList::setRenderTargets(const eastl::vector<RHITexture *> &color_attachments, RHITexture *depth_attachment, int layer, int mip, bool clear)
{
	PROFILE_CPU_FUNCTION();
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

	eastl::vector<VkRenderingAttachmentInfo> color_attachments_info;
	for (const auto &attachment : color_attachments)
	{
		VulkanTexture *texture = (VulkanTexture *)attachment;

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
		VulkanTexture *texture = (VulkanTexture *)depth_attachment;

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

void VulkanCommandList::setPipeline(RHIPipeline *pipeline)
{
	VulkanPipeline *native_pipeline = static_cast<VulkanPipeline *>(pipeline);
	VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
	if (native_pipeline->description.is_ray_tracing_pipeline)
		bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
	else if (native_pipeline->description.is_compute_pipeline)
		bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
	vkCmdBindPipeline(cmd_buffer, bind_point, native_pipeline->resource->pipeline);
	current_pipeline = pipeline;
}

void VulkanCommandList::setVertexBuffer(RHIBuffer *buffer)
{
	VulkanBuffer *native_buffer = static_cast<VulkanBuffer *>(buffer);
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &native_buffer->buffer->resource, offsets);
}

void VulkanCommandList::setIndexBuffer(RHIBuffer *buffer)
{
	VulkanBuffer *native_buffer = static_cast<VulkanBuffer *>(buffer);
	vkCmdBindIndexBuffer(cmd_buffer, native_buffer->buffer->resource, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandList::dispatchRays(uint32_t width, uint32_t height, uint32_t depth)
{
	gDynamicRHI->prepareRenderCall();

	VulkanDynamicRHI *native_rhi = VulkanUtils::getNativeRHI();
	VulkanPipeline *native_pipeline = (VulkanPipeline *)current_pipeline;

	const uint32_t handleSizeAligned = Math::alignedSize(native_rhi->device->physicalRayTracingProperties.shaderGroupHandleSize, native_rhi->device->physicalRayTracingProperties.shaderGroupHandleAlignment);
	VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
	// TODO: fix
	raygenShaderSbtEntry.deviceAddress = native_pipeline->raygenShaderBindingTable->getGPUAddress();
	raygenShaderSbtEntry.stride = handleSizeAligned;
	raygenShaderSbtEntry.size = handleSizeAligned;

	VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
	// TODO: fix
	missShaderSbtEntry.deviceAddress = native_pipeline->missShaderBindingTable->getGPUAddress();
	missShaderSbtEntry.stride = handleSizeAligned;
	missShaderSbtEntry.size = handleSizeAligned;

	VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
	// TODO: fix
	hitShaderSbtEntry.deviceAddress = native_pipeline->hitShaderBindingTable->getGPUAddress();
	hitShaderSbtEntry.stride = handleSizeAligned;
	hitShaderSbtEntry.size = handleSizeAligned;

	VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

	VulkanUtils::vkCmdTraceRaysKHR(cmd_buffer, &raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry, width, height, depth);
}

void VulkanCommandList::copyBuffer(RHIBuffer *src, RHIBuffer *dest, uint64_t src_offset, uint64_t dest_offset, uint64_t size)
{
	VulkanBuffer *native_src_buffer = (VulkanBuffer *)src;
	VulkanBuffer *native_dst_buffer = (VulkanBuffer *)dest;

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = src_offset;
	copyRegion.dstOffset = dest_offset;
	copyRegion.size = size;
	vkCmdCopyBuffer(cmd_buffer, native_src_buffer->buffer->resource, native_dst_buffer->buffer->resource, 1, &copyRegion);
}

void VulkanCommandList::beginDebugLabel(const char *label, glm::vec3 color, uint32_t line, const char *source, size_t source_size, const char *function, size_t function_size)
{
	#ifdef TRACY_ENABLE
		auto tracy_scope = std::make_unique<tracy::VkCtxScope>(VulkanUtils::getNativeRHI()->tracy_ctx, line, source, source_size, function, function_size,label, strlen(label), cmd_buffer, true);
		tracy_debug_label_stack.emplace_back(eastl::move(tracy_scope));
	#endif
	VulkanUtils::cmdBeginDebugLabel(this, label, color);
}

void VulkanCommandList::endDebugLabel()
{
	#ifdef TRACY_ENABLE
		tracy_debug_label_stack.pop_back();
	#endif
	VulkanUtils::cmdEndDebugLabel(this);
}
