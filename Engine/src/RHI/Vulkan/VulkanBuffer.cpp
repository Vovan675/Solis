#include "pch.h"
#include "vma/vk_mem_alloc.h"
#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanBuffer::VulkanBuffer(BufferDescription description) : RHIBuffer(description)
{
	VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	if (hasAnyFlags(description.usage, BufferUsage::VERTEX_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (hasAnyFlags(description.usage, BufferUsage::INDEX_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (hasAnyFlags(description.usage, BufferUsage::CONSTANT_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (hasAnyFlags(description.usage, (BufferUsage::SHADER_READ_BUFFER | BufferUsage::SHADER_WRITE_BUFFER | BufferUsage::SCRATCH_BUFFER)))
		usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (hasAnyFlags(description.usage, BufferUsage::INDIRECT_ARGS_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	if (hasAnyFlags(description.usage, BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	if (hasAnyFlags(description.usage, BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	if (hasAnyFlags(description.usage, BufferUsage::SHADER_BINGING_TABLE_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

	VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO;
	VmaAllocationCreateFlags flags = 0;
	if (description.use_staging_buffer && !hasAnyFlags(description.usage, BufferUsage::STAGING_BUFFER))
	{
		memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
	} else
	{
		memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	buffer = std::make_unique<VkBufferResource>();
	allocation = std::make_unique<VkAllocationResource>();
	VulkanUtils::createBuffer(description.size, usage_flags, memory_usage, flags, buffer->resource, allocation->resource, description.alignment);
}

VulkanBuffer::~VulkanBuffer()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	if (is_mapped)
		unmap();

	native_rhi->releaseGPUResource(buffer.release());
	native_rhi->releaseGPUResource(allocation.release());
}

void VulkanBuffer::fill(const void *sourceData)
{
	ENGINE_ASSERT(sourceData);
	PROFILE_CPU_FUNCTION();

	auto native_rhi = VulkanUtils::getNativeRHI();
	uint64_t buffer_size = description.size;

	if (description.use_staging_buffer && !hasAnyFlags(description.usage, BufferUsage::STAGING_BUFFER))
	{
		// Staging buffer
		BufferDescription staging_buffer_description;
		staging_buffer_description.size = description.size;
		staging_buffer_description.alignment = description.alignment;
		staging_buffer_description.use_staging_buffer = false;
		RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description);

		// Map buffer memory to CPU accessible memory
		void* data;
		staging_buffer->map(&data);
		memcpy(data, sourceData, buffer_size);
		staging_buffer->unmap();

		// Copy from staging to buffer
		RHICommandList *copy_cmd_list = gDynamicRHI->getCmdListCopy();
		copy_cmd_list->open();
		copy_cmd_list->copyBuffer(staging_buffer, this, 0, 0, buffer_size);
		copy_cmd_list->close();
		gDynamicRHI->getCmdQueueCopy()->execute(copy_cmd_list);
		gDynamicRHI->getCmdQueueCopy()->waitIdle();
	}
	else
	{
		// Map buffer memory to CPU accessible memory
		void* data;
		vmaMapMemory(native_rhi->allocator, allocation->resource, &data);
		memcpy(data, sourceData, buffer_size);
		vmaUnmapMemory(native_rhi->allocator, allocation->resource);
	}
}

void VulkanBuffer::map(void **data)
{
	ENGINE_ASSERT(data);
	if (is_mapped)
		return;
	// Map buffer memory to CPU accessible memory
	vmaMapMemory(VulkanUtils::getNativeRHI()->allocator, allocation->resource, data);
	is_mapped = true;
}

void VulkanBuffer::unmap()
{
	if (!is_mapped)
		return;
	vmaUnmapMemory(VulkanUtils::getNativeRHI()->allocator, allocation->resource);
	is_mapped = false;
}

void VulkanBuffer::setDebugName(const char *name)
{
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->resource, name);
}

uint64_t VulkanBuffer::getGPUAddress() const
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAI.buffer = buffer->resource;
	return VulkanUtils::vkGetBufferDeviceAddressKHR(&bufferDeviceAI);
}

struct VulkanAccessInfo
{
	VkPipelineStageFlags stage;
	VkAccessFlags access;
};

static VulkanAccessInfo toVulkanAccessInfo(ResourceState state)
{
	VulkanAccessInfo info{};
	info.stage = 0;
	info.access = 0;

	if (hasAnyFlags(state, ResourceState::SHADER_RESOURCE)) 
	{
		info.stage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		info.access |= VK_ACCESS_SHADER_READ_BIT;
	}
	if (hasAnyFlags(state, ResourceState::COPY_SRC))
	{
		info.stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		info.access |= VK_ACCESS_TRANSFER_READ_BIT;
	}
	if (hasAnyFlags(state, ResourceState::COPY_DST)) 
	{
		info.stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		info.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if (hasAnyFlags(state, ResourceState::UAV))
	{
		info.stage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		info.access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	if (hasAnyFlags(state, ResourceState::VERTEX_BUFFER))
	{
		info.stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if (hasAnyFlags(state, ResourceState::INDEX_BUFFER))
	{
		info.stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		info.access |= VK_ACCESS_INDEX_READ_BIT;
	}
	if (hasAnyFlags(state, ResourceState::INDIRECT_ARGS))
	{
		info.stage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		info.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	}
	return info;
}

void VulkanBuffer::transitState(ResourceState new_state)
{
	VulkanCommandList *native_cmd_list = (VulkanCommandList *)gDynamicRHI->getCmdList();

	auto writes = [](ResourceState state) { return hasAnyFlags(state, ResourceState::UAV); };

	if (!writes(current_state) && !writes(new_state))
	{
		current_state = new_state;
		return;
	}

	VulkanAccessInfo src = toVulkanAccessInfo(current_state);
	VulkanAccessInfo dst = toVulkanAccessInfo(new_state);

	VkBufferMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	barrier.srcStageMask = src.stage; // pipeline stage(s) that must be completed before the barrier is crossed
	barrier.srcAccessMask = src.access; // operations that must complete before the barrier is crossed - example: write
	barrier.dstStageMask = dst.stage; // pipeline stage(s) that must wait for the barrier to be crossed before beginning
	barrier.dstAccessMask = dst.access; // operations that must wait for the barrier to be is crossed - example: read
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer->resource;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	VkDependencyInfo dependency_info{};
	dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency_info.bufferMemoryBarrierCount = 1;
	dependency_info.pBufferMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(native_cmd_list->cmd_buffer, &dependency_info);	
	current_state = new_state;
}

RHIBufferView *VulkanBuffer::getShaderResourceView()
{
	if (!shader_resource_view)
		shader_resource_view = new VulkanBufferView(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE));
	return shader_resource_view;
}

RHIBufferView *VulkanBuffer::getUnorderedAccessView()
{
	if (!unordered_access_view)
		unordered_access_view = new VulkanBufferView(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE_STORAGE));
	return unordered_access_view;
}

VulkanBufferView::VulkanBufferView(BufferViewDescription description): RHIBufferView(description)
{
	ENGINE_ASSERT(description.buffer);
	
	if (description.view_type == BufferViewType::SHADER_RESOURCE)
		bindless_index = gDynamicRHI->getBindlessResources()->addBuffer(this);
	else if (description.view_type == BufferViewType::SHADER_RESOURCE_STORAGE)
		bindless_index = gDynamicRHI->getBindlessResources()->addBuffer(this);
}

VulkanBufferView::~VulkanBufferView()
{
	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeBuffer(this);
}
