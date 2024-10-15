#include "pch.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "VkWrapper.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

Pipeline::~Pipeline()
{
	cleanup();
}

void Pipeline::cleanup()
{
	if (pipeline != nullptr)
	{
		vkDestroyPipeline(VkWrapper::device->logicalHandle, pipeline, nullptr);
		pipeline = nullptr;
	}
	if (pipeline_layout != nullptr)
	{
		vkDestroyPipelineLayout(VkWrapper::device->logicalHandle, pipeline_layout, nullptr);
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
	if (description.descriptor_layout.layout)
	{
		descriptor_set_layouts.push_back(description.descriptor_layout.layout); // One descriptor set should be enough
	}
	descriptor_set_layouts.push_back(*BindlessResources::getDescriptorLayout()); // Add bindless layout to every pipeline
	descriptor_set_layouts.push_back(Renderer::getDefaultDescriptorLayout()); // Add default resources layout to every pipeline

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descriptor_set_layouts.size();
	pipelineLayoutInfo.pSetLayouts = descriptor_set_layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = description.push_constant_ranges.size();
	pipelineLayoutInfo.pPushConstantRanges = description.push_constant_ranges.data();
	CHECK_ERROR(vkCreatePipelineLayout(VkWrapper::device->logicalHandle, &pipelineLayoutInfo, nullptr, &pipeline_layout));

	if (description.is_ray_tracing_pipeline)
	{
		auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkGetRayTracingShaderGroupHandlesKHR"));
		auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkCreateRayTracingPipelinesKHR"));

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
		CHECK_ERROR(vkCreateRayTracingPipelinesKHR(VkWrapper::device->logicalHandle, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));

		// Create shader binding table
		const uint32_t handleSize = VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = VkWrapper::alignedSize(VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleSize, VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleAlignment);
		const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
		const uint32_t sbtSize = groupCount * handleSizeAligned;

		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		CHECK_ERROR(vkGetRayTracingShaderGroupHandlesKHR(VkWrapper::device->logicalHandle, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

		const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		const VkMemoryPropertyFlags memoryUsageFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		BufferDescription accDesc;
		accDesc.size = handleSize;
		accDesc.useStagingBuffer = true;
		accDesc.bufferUsageFlags = bufferUsageFlags;
		accDesc.alignment = VkWrapper::device->physicalRayTracingProperties.shaderGroupBaseAlignment;

		raygenShaderBindingTable = std::make_shared<Buffer>(accDesc);
		missShaderBindingTable = std::make_shared<Buffer>(accDesc);
		hitShaderBindingTable = std::make_shared<Buffer>(accDesc);

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
		vertShaderStageInfo.module = description.vertex_shader->handle;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = description.fragment_shader->handle;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStagesInfo[] = {vertShaderStageInfo, fragShaderStageInfo};

		// Vertex input state
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		auto bindingDescription = Engine::Vertex::GetBindingDescription();
		auto attributeDescriptions = Engine::Vertex::GetAttributeDescription();
		if (description.use_vertices)
		{
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		} else
		{
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
		}

		// Input assembly state
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology = description.primitive_topology;
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr; // ignored because its dynamic state
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr; // ignored because its dynamic state

		// Rasterizer state
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL; //TODO: Test another
		rasterizer.lineWidth = 1;
		rasterizer.cullMode = description.cull_mode;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
		std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(description.color_formats.size());
		for (int i = 0; i < description.color_formats.size(); i++)
		{
			VkPipelineColorBlendAttachmentState attachment{};
			attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			attachment.blendEnable = description.use_blending;
			attachment.srcColorBlendFactor = description.srcColorBlendFactor;
			attachment.dstColorBlendFactor = description.dstColorBlendFactor;
			attachment.colorBlendOp = description.colorBlendOp;
			attachment.srcAlphaBlendFactor = description.srcAlphaBlendFactor;
			attachment.dstAlphaBlendFactor = description.dstAlphaBlendFactor;
			attachment.alphaBlendOp = description.alphaBlendOp;
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
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = description.use_depth_test;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
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
		VkPipelineRenderingCreateInfo pipeline_rendering_create_info{};
		pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipeline_rendering_create_info.colorAttachmentCount = description.color_formats.size();
		pipeline_rendering_create_info.pColorAttachmentFormats = description.color_formats.data();
		pipeline_rendering_create_info.depthAttachmentFormat = description.depth_format;

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
		pipelineInfo.layout = pipeline_layout;
		pipelineInfo.renderPass = nullptr;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		CHECK_ERROR(vkCreateGraphicsPipelines(VkWrapper::device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
	}
}
