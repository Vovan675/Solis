#include "pch.h"
#include "VulkanAccelerationStructure.h"
#include "RHI/Vulkan/VulkanDynamicRHI.h"

static void build_acceleration_structure(const VkAccelerationStructureBuildGeometryInfoKHR &build_info, const VkAccelerationStructureBuildRangeInfoKHR *const *range_infos)
{
	VkCommandBuffer cmd_buffer = ((VulkanCommandList *)gDynamicRHI->getUploadCmdList())->cmd_buffer;

	// Wait while last frame done using AS
	VulkanUtils::cmdMemoryBarrier(cmd_buffer,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT);

	VulkanUtils::vkCmdBuildAccelerationStructuresKHR(cmd_buffer, 1, &build_info, range_infos);

	// Wait for build to read acceleration structure
	VulkanUtils::cmdMemoryBarrier(cmd_buffer,
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
}

VulkanBottomLevelAccelerationStructure::~VulkanBottomLevelAccelerationStructure()
{
	gDynamicRHI->releaseGPUResource(handle.release());
}

void VulkanBottomLevelAccelerationStructure::build(const eastl::vector<RayTracingGeometry> &geometries)
{
	if (geometries.empty()) return;

	auto device = VulkanUtils::getNativeRHI()->device->logicalHandle;

	eastl::vector<VkAccelerationStructureGeometryKHR> geometries_desc;
	geometries_desc.resize(geometries.size());

	eastl::vector<uint32_t> max_primitives_counts;
	max_primitives_counts.resize(geometries.size());

	eastl::vector<VkAccelerationStructureBuildRangeInfoKHR> build_range_infos;
	build_range_infos.resize(geometries.size());

	for (int i = 0; i < geometries.size(); i++)
	{
		const RayTracingGeometry &geometry = geometries[i];

		VkAccelerationStructureGeometryKHR &desc = geometries_desc[i];
		desc.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		desc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		desc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
		triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		triangles.vertexFormat = VulkanUtils::getNativeFormat(geometry.vertex_format);
		triangles.vertexData.deviceAddress = geometry.vertex_buffer->getGPUAddress();
		triangles.vertexStride = geometry.vertex_buffer_stride;
		triangles.maxVertex = geometry.vertex_count;

		triangles.indexType = geometry.index_format == FORMAT_R32_UINT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR;
		triangles.indexData.deviceAddress = geometry.index_buffer->getGPUAddress();
		triangles.transformData.deviceAddress = 0;
		desc.geometry.triangles = triangles;

		max_primitives_counts[i] = geometry.index_count / 3;

		VkAccelerationStructureBuildRangeInfoKHR &build_range_info = build_range_infos[i];
		build_range_info.primitiveCount = max_primitives_counts[i];
		build_range_info.primitiveOffset = 0;
		build_range_info.firstVertex = 0;
		build_range_info.transformOffset = 0;
	}


	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = geometries_desc.size();
	accelerationStructureBuildGeometryInfo.pGeometries = geometries_desc.data();

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	VulkanUtils::vkGetAccelerationStructureBuildSizesKHR(
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		max_primitives_counts.data(),
		&accelerationStructureBuildSizesInfo);

	// Create Acceleration structure buffer
	BufferDescription accDesc;
	accDesc.size = Math::roundUp(accelerationStructureBuildSizesInfo.accelerationStructureSize, 256);
	accDesc.use_staging_buffer = true;
	accDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER;

	buffer = gDynamicRHI->createBuffer(accDesc);

	VulkanBuffer *native_buffer = (VulkanBuffer *)buffer.getReference();

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
	accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreateInfo.buffer = native_buffer->getBuffer();
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	handle = std::make_unique<VkAccelerationStructureResource>();
	VulkanUtils::vkCreateAccelerationStructureKHR(&accelerationStructureCreateInfo, nullptr, &handle->resource);

	uint32_t min_scratch_alignment = VulkanUtils::getNativeRHI()->device->physicalAccelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
	BufferDescription scratchDesc;
	scratchDesc.size = Math::roundUp(accelerationStructureBuildSizesInfo.buildScratchSize, min_scratch_alignment);
	scratchDesc.alignment = min_scratch_alignment;
	scratchDesc.use_staging_buffer = true;
	scratchDesc.usage = BufferUsage::SCRATCH_BUFFER;

	RHIBufferRef scratch_buffer = gDynamicRHI->createBuffer(scratchDesc);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = handle->resource;
	accelerationBuildGeometryInfo.geometryCount = geometries_desc.size();
	accelerationBuildGeometryInfo.pGeometries = geometries_desc.data();
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratch_buffer->getGPUAddress();

	eastl::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos{};
	for (auto &build_range : build_range_infos)
		accelerationBuildStructureRangeInfos.push_back(&build_range);

	build_acceleration_structure(accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());


	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = handle->resource;
	deviceAddress = VulkanUtils::vkGetAccelerationStructureDeviceAddressKHR(&accelerationDeviceAddressInfo);
}

VulkanTopLevelAccelerationStructure::~VulkanTopLevelAccelerationStructure()
{
	gDynamicRHI->releaseGPUResource(handle.release());
}

void VulkanTopLevelAccelerationStructure::build(const eastl::vector<RayTracingInstance> &instances)
{
	eastl::vector<VkAccelerationStructureInstanceKHR> instances_desc;
	instances_desc.resize(instances.size());

	for (int i = 0; i < instances.size(); i++)
	{
		const RayTracingInstance &instance = instances[i];

		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};
		auto transform = glm::transpose(instance.transform);
		memcpy(transformMatrix.matrix, &transform[0], sizeof(VkTransformMatrixKHR));

		VkAccelerationStructureInstanceKHR &desc = instances_desc[i];
		desc.transform = transformMatrix;
		desc.instanceCustomIndex = instance.instance_id;
		desc.mask = instance.instance_mask;
		desc.instanceShaderBindingTableRecordOffset = instance.instance_contribution_to_hit_group_index;
		desc.flags = 0;

		VulkanBottomLevelAccelerationStructure *native_blas = (VulkanBottomLevelAccelerationStructure *)instance.blas.getReference();
		desc.accelerationStructureReference = native_blas->deviceAddress;

		// TODO: set in object description array at index `instance_id`, index to another buffers that contains vertices, indices, materials
	}

	// Buffer for instance data
	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
	if (!instances_desc.empty())
	{
		BufferDescription instanceDesc;
		instanceDesc.size = instances_desc.size() * sizeof(VkAccelerationStructureInstanceKHR);
		instanceDesc.use_staging_buffer = false;
		instanceDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;
		instanceDesc.alignment = 16;

		auto instanceBuffer = gDynamicRHI->createBuffer(instanceDesc);
		instanceBuffer->fill(instances_desc.data());

		instanceDataDeviceAddress.deviceAddress = instanceBuffer->getGPUAddress();
	}

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	uint32_t primitive_count = instances_desc.size();

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	VulkanUtils::vkGetAccelerationStructureBuildSizesKHR(
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&primitive_count,
		&accelerationStructureBuildSizesInfo);

	// Reallocate when more size needed or less than half is used
	const uint64_t allocation_granularity = 1024 * 1024;
	auto needs_reallocation = [](const RHIBufferRef &buffer, uint64_t size)
	{
		return !buffer || size > buffer->getSize() || size < buffer->getSize() / 2;
	};

	uint64_t acceleration_structure_size = Math::roundUp(accelerationStructureBuildSizesInfo.accelerationStructureSize, allocation_granularity);
	if (needs_reallocation(acc_buffer, acceleration_structure_size))
	{
		if (handle)
		{
			gDynamicRHI->getBindlessResources()->removeAccelerationStructure(this);
			gDynamicRHI->releaseGPUResource(handle.release());
		}

		// Create acceleration structure buffer
		BufferDescription accDesc;
		accDesc.size = acceleration_structure_size;
		accDesc.use_staging_buffer = true;
		accDesc.usage = BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER;

		acc_buffer = gDynamicRHI->createBuffer(accDesc);
		VulkanBuffer *native_buffer = (VulkanBuffer *)acc_buffer.getReference();

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = native_buffer->getBuffer();
		accelerationStructureCreateInfo.size = acceleration_structure_size;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		handle = std::make_unique<VkAccelerationStructureResource>();
		VulkanUtils::vkCreateAccelerationStructureKHR(&accelerationStructureCreateInfo, nullptr, &handle->resource);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = handle->resource;
		deviceAddress = VulkanUtils::vkGetAccelerationStructureDeviceAddressKHR(&accelerationDeviceAddressInfo);

		bindless_id = gDynamicRHI->getBindlessResources()->addAccelerationStructure(this);
	}

	// Create a small scratch buffer used during build of the top level acceleration structure
	uint32_t min_scratch_alignment = VulkanUtils::getNativeRHI()->device->physicalAccelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
	uint64_t scratch_size = Math::roundUp(accelerationStructureBuildSizesInfo.buildScratchSize, allocation_granularity);
	if (needs_reallocation(scratch_buffer, scratch_size))
	{
		BufferDescription scratchDesc;
		scratchDesc.size = scratch_size;
		scratchDesc.alignment = min_scratch_alignment;
		scratchDesc.use_staging_buffer = true;
		scratchDesc.usage = BufferUsage::SCRATCH_BUFFER;

		scratch_buffer = gDynamicRHI->createBuffer(scratchDesc);
	}

	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = handle->resource;
	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratch_buffer->getGPUAddress();

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	VkAccelerationStructureBuildRangeInfoKHR *accelerationBuildStructureRangeInfo = &accelerationStructureBuildRangeInfo;

	build_acceleration_structure(accelerationStructureBuildGeometryInfo, &accelerationBuildStructureRangeInfo);
}
