#include "pch.h"
#include "VulkanPipeline.h"
#include "VulkanDynamicRHI.h"
#include "Utils/Math.h"

VulkanPipeline::~VulkanPipeline()
{
	destroy();
}

void VulkanPipeline::destroy()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	native_rhi->releaseGPUResource(resource.release());
}

void VulkanPipeline::create(const PipelineDescription &description)
{
	PROFILE_CPU_FUNCTION();
	destroy();
	resource = std::make_unique<VkPipelineResource>();
	this->description = description;
	hash = description.getHash();

	eastl::vector<VulkanShader *> shaders;

	if (description.is_compute_pipeline)
	{
		shaders.push_back(static_cast<VulkanShader *>(description.compute_shader.getReference()));
	} else if (description.is_ray_tracing_pipeline)
	{
		shaders.push_back(static_cast<VulkanShader *>(description.ray_generation_shader.getReference()));
		shaders.push_back(static_cast<VulkanShader *>(description.miss_shader.getReference()));
		shaders.push_back(static_cast<VulkanShader *>(description.closest_hit_shader.getReference()));
	} else
	{
		shaders.push_back(static_cast<VulkanShader *>(description.vertex_shader.getReference()));
		shaders.push_back(static_cast<VulkanShader *>(description.fragment_shader.getReference()));
	}

	descriptors = VulkanShader::getDescriptors(shaders);
	descriptor_layout = VulkanUtils::getDescriptorLayout(descriptors);

	// Pipeline layout state (aka uniform values)
	eastl::vector<VkDescriptorSetLayout> descriptor_set_layouts;
	descriptor_set_layouts.push_back(descriptor_layout.layout);

	VulkanDynamicRHI *native_rhi = VulkanUtils::getNativeRHI();

	// Bindless
	VulkanBindlessResources *native_bindless = (VulkanBindlessResources *)gDynamicRHI->getBindlessResources();
	descriptor_set_layouts.push_back(native_bindless->getDescriptorLayout());

	eastl::vector<VkPushConstantRange> push_constant_ranges = VulkanShader::getPushConstantRanges(descriptors);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descriptor_set_layouts.size();
	pipelineLayoutInfo.pSetLayouts = descriptor_set_layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = push_constant_ranges.size();
	pipelineLayoutInfo.pPushConstantRanges = push_constant_ranges.data();
	CHECK_ERROR(vkCreatePipelineLayout(native_rhi->device->logicalHandle, &pipelineLayoutInfo, nullptr, &resource->pipeline_layout));

	if (description.is_compute_pipeline)
	{
		VkPipelineShaderStageCreateInfo compShaderStageInfo{};
		compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = static_cast<VulkanShader *>(description.compute_shader.getReference())->handle;
		compShaderStageInfo.pName = "CSMain";

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = resource->pipeline_layout;
		pipelineInfo.stage = compShaderStageInfo;
		CHECK_ERROR(vkCreateComputePipelines(native_rhi->device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resource->pipeline));
	} else if (description.is_ray_tracing_pipeline)
	{
		eastl::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		// Ray generation group
		{
			VkPipelineShaderStageCreateInfo ray_gen_shader_stage{};
			ray_gen_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			ray_gen_shader_stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			ray_gen_shader_stage.module = static_cast<VulkanShader *>(description.ray_generation_shader.getReference())->handle;
			ray_gen_shader_stage.pName = "RayGen";
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
			miss_shader_stage.module = static_cast<VulkanShader *>(description.miss_shader.getReference())->handle;
			miss_shader_stage.pName = "Miss";
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
			VkPipelineShaderStageCreateInfo closest_hit_shader_stage{};
			closest_hit_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			closest_hit_shader_stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			closest_hit_shader_stage.module = static_cast<VulkanShader *>(description.closest_hit_shader.getReference())->handle;
			closest_hit_shader_stage.pName = "ClosestHit";
			shaderStages.push_back(closest_hit_shader_stage);

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
		rayTracingPipelineCI.layout = resource->pipeline_layout;
		CHECK_ERROR(VulkanUtils::vkCreateRayTracingPipelinesKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &resource->pipeline));

		// Create shader binding table
		const uint32_t handleSize = native_rhi->device->physicalRayTracingProperties.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = Math::alignedSize(native_rhi->device->physicalRayTracingProperties.shaderGroupHandleSize, native_rhi->device->physicalRayTracingProperties.shaderGroupHandleAlignment);
		const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
		const uint32_t sbtSize = groupCount * handleSizeAligned;

		eastl::vector<uint8_t> shaderHandleStorage(sbtSize);
		CHECK_ERROR(VulkanUtils::vkGetRayTracingShaderGroupHandlesKHR(resource->pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

		const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		BufferDescription accDesc;
		accDesc.size = handleSize;
		accDesc.useStagingBuffer = true;
		accDesc.usage = SHADER_BINGING_TABLE_BUFFER;
		accDesc.alignment = native_rhi->device->physicalRayTracingProperties.shaderGroupBaseAlignment;

		raygenShaderBindingTable = gDynamicRHI->createBuffer(accDesc);
		missShaderBindingTable = gDynamicRHI->createBuffer(accDesc);
		hitShaderBindingTable = gDynamicRHI->createBuffer(accDesc);

		// Copy handles
		raygenShaderBindingTable->fill(shaderHandleStorage.data());
		missShaderBindingTable->fill(shaderHandleStorage.data() + handleSizeAligned);
		hitShaderBindingTable->fill(shaderHandleStorage.data() + handleSizeAligned * 2);
	} else
	{

		// Shaders state
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = static_cast<VulkanShader *>(description.vertex_shader.getReference())->handle;
		vertShaderStageInfo.pName = "VSMain";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = static_cast<VulkanShader *>(description.fragment_shader.getReference())->handle;
		fragShaderStageInfo.pName = "PSMain";

		VkPipelineShaderStageCreateInfo shaderStagesInfo[] = {vertShaderStageInfo, fragShaderStageInfo};

		// Vertex input state
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		// Use vertices
		VkVertexInputBindingDescription vertex_input_binding_description{};
		eastl::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
		if (!description.vertex_inputs_descriptions.inputs.empty())
		{
			uint32_t offset = 0;
			int location = 0;
			for (auto &input : description.vertex_inputs_descriptions.inputs)
			{
				auto &attribute = vertex_input_attribute_descriptions.emplace_back();
				attribute.binding = 0;
				attribute.location = location;
				attribute.format = VulkanUtils::getNativeFormat(input.format);
				attribute.offset = offset;

				offset += VulkanUtils::getFormatSize(input.format);
				offset = Math::alignedSize(offset, 16);
				location++;
			}


			vertex_input_binding_description.binding = 0;
			vertex_input_binding_description.stride = offset;
			vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &vertex_input_binding_description;
			vertexInputInfo.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
			vertexInputInfo.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();
		} else
		{
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
		}

		// Input assembly state
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		switch (description.primitive_topology)
		{
			case TOPOLOGY_POINT_LIST: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
			case TOPOLOGY_LINE_LIST: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
			case TOPOLOGY_TRIANGLE_LIST: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
			case TOPOLOGY_TRIANGLE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		}
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology = topology;
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr; // ignored because its dynamic state
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr; // ignored because its dynamic state


		VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
		switch (description.cull_mode)
		{
			case CULL_MODE_NONE: cull_mode = VK_CULL_MODE_NONE; break;
			case CULL_MODE_BACK: cull_mode = VK_CULL_MODE_BACK_BIT; break;
			case CULL_MODE_FRONT: cull_mode = VK_CULL_MODE_FRONT_BIT; break;
		}

		// Rasterizer state
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1;
		rasterizer.cullMode = cull_mode;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0;
		rasterizer.depthBiasClamp = 0;
		rasterizer.depthBiasSlopeFactor = 0;

		// Multisample state
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Color blend state
		auto getBlend = [](Blend blend)
		{
			switch (blend)
			{
				case BLEND_ZERO: return VK_BLEND_FACTOR_ZERO;
				case BLEND_ONE: return VK_BLEND_FACTOR_ONE;
				case BLEND_SRC_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
				case BLEND_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				case BLEND_DST_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
				case BLEND_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				case BLEND_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
				case BLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				case BLEND_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
				case BLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				case BLEND_SRC_ALPHA_SATURATE: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				case BLEND_SRC1_COLOR: return VK_BLEND_FACTOR_SRC1_COLOR;
				case BLEND_ONE_MINUS_SRC1_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
				case BLEND_SRC1_ALPHA: return VK_BLEND_FACTOR_SRC1_ALPHA;
				case BLEND_ONE_MINUS_SRC1_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
			}
			return VK_BLEND_FACTOR_ZERO;
		};

		auto getBlendOp = [](BlendOp op)
		{
			switch (op)
			{
				case BLEND_OP_ADD: return VK_BLEND_OP_ADD;
				case BLEND_OP_SUBTRACT: return VK_BLEND_OP_SUBTRACT;
				case BLEND_OP_REV_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
				case BLEND_OP_MIN: return VK_BLEND_OP_MIN;
				case BLEND_OP_MAX: return VK_BLEND_OP_MAX;
			}
			return VK_BLEND_OP_ADD;
		};

		eastl::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(description.color_formats.size());
		for (int i = 0; i < description.color_formats.size(); i++)
		{
			VkPipelineColorBlendAttachmentState attachment{};
			attachment.blendEnable = description.use_blending;
			attachment.srcColorBlendFactor = getBlend(description.src_color_blend);
			attachment.dstColorBlendFactor = getBlend(description.dst_color_blend);
			attachment.colorBlendOp = getBlendOp(description.color_blend_op);
			attachment.srcAlphaBlendFactor = getBlend(description.src_alpha_blend);
			attachment.dstAlphaBlendFactor = getBlend(description.dst_alpha_blend);
			attachment.alphaBlendOp = getBlendOp(description.alpha_blend_op);
			attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			color_blend_attachments[i] = attachment;
		}

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = color_blend_attachments.size();;
		colorBlending.pAttachments = color_blend_attachments.data();
		colorBlending.blendConstants[0] = 0;
		colorBlending.blendConstants[1] = 0;
		colorBlending.blendConstants[2] = 0;
		colorBlending.blendConstants[3] = 0;

		// Depth Stencil state
		VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;
		switch (description.depth_compare_func)
		{
			case COMPARE_FUNC_NEVER: depth_compare_op = VK_COMPARE_OP_NEVER; break;
			case COMPARE_FUNC_LESS: depth_compare_op = VK_COMPARE_OP_LESS; break;
			case COMPARE_FUNC_EQUAL: depth_compare_op = VK_COMPARE_OP_EQUAL; break;
			case COMPARE_FUNC_LESS_EQUAL: depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL; break;
			case COMPARE_FUNC_GREATER: depth_compare_op = VK_COMPARE_OP_GREATER; break;
			case COMPARE_FUNC_NOT_EQUAL: depth_compare_op = VK_COMPARE_OP_NOT_EQUAL; break;
			case COMPARE_FUNC_GREATER_EQUAL: depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
			case COMPARE_FUNC_ALWAYS: depth_compare_op = VK_COMPARE_OP_ALWAYS; break;
		}

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = description.use_depth_test;
		depthStencil.depthWriteEnable = description.use_depth_write;
		depthStencil.depthCompareOp = depth_compare_op;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};

		// Dynamic states
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		// Needed for dynamic rendering
		eastl::vector<VkFormat> color_attachments;
		for (auto &format : description.color_formats)
			color_attachments.push_back(VulkanUtils::getNativeFormat(format));

		VkPipelineRenderingCreateInfo pipeline_rendering_create_info{};
		pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipeline_rendering_create_info.colorAttachmentCount = color_attachments.size();
		pipeline_rendering_create_info.pColorAttachmentFormats = color_attachments.data();

		if (description.depth_format != FORMAT_UNDEFINED)
			pipeline_rendering_create_info.depthAttachmentFormat = VulkanUtils::getNativeFormat(description.depth_format);

		// Finally create graphics pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = &pipeline_rendering_create_info;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStagesInfo;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = resource->pipeline_layout;
		pipelineInfo.renderPass = nullptr;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		CHECK_ERROR(vkCreateGraphicsPipelines(native_rhi->device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resource->pipeline));
	}
}
