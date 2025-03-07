#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include "RHI/RHIBuffer.h"
#include "RHI/Vulkan/VulkanUtils.h"

class BottomLevelAccelerationStructures
{
public:
	~BottomLevelAccelerationStructures()
	{
		for (auto &blas : blases)
			VulkanUtils::vkDestroyAccelerationStructureKHR(blas.handle, nullptr);
	}

	void buildBLAS(std::vector<VkAccelerationStructureGeometryKHR> geometries, std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_range_infos, std::vector<uint32_t> max_primitives_counts)
	{
		// TODO: fix
		/*
		auto device = VkWrapper::device->logicalHandle;

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = geometries.size();
		accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

		VulkanUtils::vkGetAccelerationStructureBuildSizesKHR(
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			max_primitives_counts.data(),
			&accelerationStructureBuildSizesInfo);

		// Create Acceleration structure buffer
		BufferDescription accDesc;
		accDesc.size = VkWrapper::roundUp(accelerationStructureBuildSizesInfo.accelerationStructureSize, 256);
		accDesc.useStagingBuffer = true;
		accDesc.bufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

		auto &bottomAS = blases.emplace_back();
		bottomAS.buffer = Buffer::create(accDesc);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = bottomAS.buffer->buffer_handle;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		VulkanUtils::vkCreateAccelerationStructureKHR(&accelerationStructureCreateInfo, nullptr, &bottomAS.handle);

		BufferDescription scratchDesc;
		scratchDesc.size = VkWrapper::roundUp(accelerationStructureBuildSizesInfo.buildScratchSize, VkWrapper::device->physicalAccelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);
		scratchDesc.alignment = VkWrapper::device->physicalAccelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
		scratchDesc.useStagingBuffer = true;
		scratchDesc.bufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		bottomAS.scratch_buffer = Buffer::create(scratchDesc);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = bottomAS.handle;
		accelerationBuildGeometryInfo.geometryCount = geometries.size();
		accelerationBuildGeometryInfo.pGeometries = geometries.data();
		accelerationBuildGeometryInfo.scratchData.deviceAddress = VkWrapper::getBufferDeviceAddress(bottomAS.scratch_buffer->buffer_handle);

		std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos{};
		for (auto &build_range : build_range_infos)
			accelerationBuildStructureRangeInfos.push_back(&build_range);

		CommandBuffer command_buffer;
		command_buffer.init(true);
		command_buffer.open();

		VulkanUtils::vkCmdBuildAccelerationStructuresKHR(command_buffer.get_buffer(), 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());

		command_buffer.close();
		command_buffer.waitFence();

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = bottomAS.handle;
		bottomAS.deviceAddress = VulkanUtils::vkGetAccelerationStructureDeviceAddressKHR(&accelerationDeviceAddressInfo);
		*/
	}

	struct AccelerationStructure
	{
		VkAccelerationStructureKHR handle;
		uint64_t deviceAddress = 0;
		VkDeviceMemory memory;
		RHIBufferRef buffer;
		RHIBufferRef scratch_buffer;
	};

	std::vector<AccelerationStructure> blases;
};

class TopLevelAccelerationStructure
{
public:
	~TopLevelAccelerationStructure()
	{
		if (handle)
			VulkanUtils::vkDestroyAccelerationStructureKHR(handle, nullptr);
	}

	void buildTLAS(bool update)
	{
		// TODO: fix
		/*
		update = false;
		auto device = VkWrapper::device->logicalHandle;


		// Buffer for instance data
		BufferDescription instanceDesc;
		instanceDesc.size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
		instanceDesc.useStagingBuffer = true;
		instanceDesc.bufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		instanceDesc.alignment = 16;

		auto instanceBuffer = Buffer::create(instanceDesc);
		instanceBuffer->fill(instances.data());

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = VkWrapper::getBufferDeviceAddress(instanceBuffer->buffer_handle);

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
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = instances.size();

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		VulkanUtils::vkGetAccelerationStructureBuildSizesKHR(
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		if (update)
		{
		
		} else
		{
			if (handle)
			{
				VulkanUtils::vkDestroyAccelerationStructureKHR(handle, nullptr);
				handle = nullptr;
			}

			// Create acceleration structure buffer
			BufferDescription accDesc;
			accDesc.size = VkWrapper::roundUp(accelerationStructureBuildSizesInfo.accelerationStructureSize, 256);
			accDesc.useStagingBuffer = true;
			accDesc.bufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

			acc_buffer = Buffer::create(accDesc);
			buffer = acc_buffer->buffer_handle;

			VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
			accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			accelerationStructureCreateInfo.buffer = buffer;
			accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
			accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			VulkanUtils::vkCreateAccelerationStructureKHR(&accelerationStructureCreateInfo, nullptr, &handle);
		}

		// Create a small scratch buffer used during build of the top level acceleration structure
		BufferDescription scratchDesc;
		scratchDesc.size = VkWrapper::roundUp(accelerationStructureBuildSizesInfo.buildScratchSize, VkWrapper::device->physicalAccelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);
		scratchDesc.alignment = VkWrapper::device->physicalAccelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
		scratchDesc.useStagingBuffer = true;
		scratchDesc.bufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		scratch_buffer = Buffer::create(scratchDesc);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		if (update)
		{
			accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
			accelerationBuildGeometryInfo.srcAccelerationStructure = handle;
		} else
		{
			accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		}
		accelerationBuildGeometryInfo.dstAccelerationStructure = handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = VkWrapper::getBufferDeviceAddress(scratch_buffer->buffer_handle);

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = {&accelerationStructureBuildRangeInfo};

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		CommandBuffer command_buffer;
		command_buffer.init(true);
		command_buffer.open();

		VulkanUtils::vkCmdBuildAccelerationStructuresKHR(command_buffer.get_buffer(), 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());

		command_buffer.close();
		command_buffer.waitFence();


		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = handle;
		deviceAddress = VulkanUtils::vkGetAccelerationStructureDeviceAddressKHR(&accelerationDeviceAddressInfo);

		instances.clear();
		*/
	}

	void addInstance(VkDeviceAddress bottomLevelAccelerationStructure, glm::mat4 transform, uint32_t instance_index)
	{
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};
		transform = glm::transpose(transform);
		memcpy(transformMatrix.matrix, &transform[0], sizeof(VkTransformMatrixKHR));

		VkAccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = instance_index;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = bottomLevelAccelerationStructure;
		instances.push_back(instance);

		// TODO: set in object description array at index `instance_id`, index to another buffers that contains vertices, indices, materials
	}

	std::vector<VkAccelerationStructureInstanceKHR> instances;

	VkAccelerationStructureKHR handle = 0;
	uint64_t deviceAddress = 0;
	VkDeviceMemory memory;
	VkBuffer buffer;
	RHIBufferRef acc_buffer;
	RHIBufferRef scratch_buffer;
};