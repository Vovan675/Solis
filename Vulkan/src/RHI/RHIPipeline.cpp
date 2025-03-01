#include "pch.h"
#include "RHIPipeline.h"

// TODO: remove when ray tracing done
/*
Pipeline::~Pipeline()
{
	cleanup();
}

void Pipeline::cleanup()
{
	if (pipeline != nullptr)
	{
		vkDestroyPipeline(VulkanUtils::getNativeRHI()->device->logicalHandle, pipeline, nullptr);
		pipeline = nullptr;
	}
	if (pipeline_layout != nullptr)
	{
		vkDestroyPipelineLayout(VulkanUtils::getNativeRHI()->device->logicalHandle, pipeline_layout, nullptr);
		pipeline_layout = nullptr;
	}
	description = PipelineDescription{};
	shaderGroups.clear();
}

void Pipeline::create(const PipelineDescription &description)
{
	cleanup();
	this->description = description;
	hash = description.getHash();

	// Pipeline layout state (aka uniform values)
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
	//if (description.descriptor_layout.layout)
	{
		//descriptor_set_layouts.push_back(description.descriptor_layout.layout); // One descriptor set should be enough
	}
	///descriptor_set_layouts.push_back(*BindlessResources::getDescriptorLayout()); // Add bindless layout to every pipeline
	///descriptor_set_layouts.push_back(Renderer::getDefaultDescriptorLayout()); // Add default resources layout to every pipeline

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descriptor_set_layouts.size();
	pipelineLayoutInfo.pSetLayouts = descriptor_set_layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = description.push_constant_ranges.size();
	pipelineLayoutInfo.pPushConstantRanges = description.push_constant_ranges.data();
	CHECK_ERROR(vkCreatePipelineLayout(VulkanUtils::getNativeRHI()->device->logicalHandle, &pipelineLayoutInfo, nullptr, &pipeline_layout));

	/*
	if (description.is_ray_tracing_pipeline)
	{
		

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		// Ray generation group
		{
			VkPipelineShaderStageCreateInfo ray_gen_shader_stage{};
			ray_gen_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			ray_gen_shader_stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			ray_gen_shader_stage.module = description.ray_generation_shader->handle;
			ray_gen_shader_stage.pName = "main";
			shaderStages.push_back(ray_gen_shader_stage);

			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups.push_back(shaderGroup);
		}

		// Miss group
		{
			VkPipelineShaderStageCreateInfo miss_shader_stage{};
			miss_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			miss_shader_stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
			miss_shader_stage.module = description.miss_shader->handle;
			miss_shader_stage.pName = "main";
			shaderStages.push_back(miss_shader_stage);

			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups.push_back(shaderGroup);
		}

		// Closest hit group
		{
			VkPipelineShaderStageCreateInfo miss_shader_stage{};
			miss_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			miss_shader_stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			miss_shader_stage.module = description.closest_hit_shader->handle;
			miss_shader_stage.pName = "main";
			shaderStages.push_back(miss_shader_stage);

			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups.push_back(shaderGroup);
		}

		//Create the ray tracing pipeline
		VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
		rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		rayTracingPipelineCI.pStages = shaderStages.data();
		rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
		rayTracingPipelineCI.pGroups = shaderGroups.data();
		rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
		rayTracingPipelineCI.layout = pipeline_layout;
		CHECK_ERROR(VulkanUtils::vkCreateRayTracingPipelinesKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));

		// Create shader binding table
		const uint32_t handleSize = VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = VkWrapper::alignedSize(VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleSize, VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleAlignment);
		const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
		const uint32_t sbtSize = groupCount * handleSizeAligned;

		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		CHECK_ERROR(VulkanUtils::vkGetRayTracingShaderGroupHandlesKHR(pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

		const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		BufferDescription accDesc;
		accDesc.size = handleSize;
		accDesc.useStagingBuffer = true;
		accDesc.bufferUsageFlags = bufferUsageFlags;
		accDesc.alignment = VkWrapper::device->physicalRayTracingProperties.shaderGroupBaseAlignment;

		raygenShaderBindingTable = gDynamicRHI->createBuffer(accDesc);
		missShaderBindingTable = gDynamicRHI->createBuffer(accDesc);
		hitShaderBindingTable = gDynamicRHI->createBuffer(accDesc);

		// Copy handles
		raygenShaderBindingTable->fill(shaderHandleStorage.data());
		missShaderBindingTable->fill(shaderHandleStorage.data() + handleSizeAligned);
		hitShaderBindingTable->fill(shaderHandleStorage.data() + handleSizeAligned * 2);
	} else if (description.is_compute_pipeline)
	{
		VkPipelineShaderStageCreateInfo compShaderStageInfo{};
		compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = description.compute_shader->handle;
		compShaderStageInfo.pName = "main";

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = pipeline_layout;
		pipelineInfo.stage = compShaderStageInfo;
		CHECK_ERROR(vkCreateComputePipelines(VkWrapper::device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
	} else
	{
		/////
	}
	*/
//}
