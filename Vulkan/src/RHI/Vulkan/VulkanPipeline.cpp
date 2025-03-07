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
	if (pipeline)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_PIPELINE, pipeline});
		pipeline = nullptr;
	}

	if (pipeline_layout)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_PIPELINE_LAYOUT, pipeline_layout});
		pipeline_layout = nullptr;
	}
}

void VulkanPipeline::create(const PipelineDescription &description)
{
	PROFILE_CPU_FUNCTION();
	destroy();
	this->description = description;
	hash = description.getHash();

	std::vector<VulkanShader *> shaders;

	if (description.is_compute_pipeline)
	{
		shaders.push_back(static_cast<VulkanShader *>(description.compute_shader.getReference()));
	} else
	{
		shaders.push_back(static_cast<VulkanShader *>(description.vertex_shader.getReference()));
		shaders.push_back(static_cast<VulkanShader *>(description.fragment_shader.getReference()));
	}

	descriptors = VulkanShader::getDescriptors(shaders);
	descriptor_layout = VulkanUtils::getDescriptorLayout(descriptors);

	// Pipeline layout state (aka uniform values)
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
	descriptor_set_layouts.push_back(descriptor_layout.layout);

	// Bindless
	VulkanBindlessResources *native_bindless = (VulkanBindlessResources *)gDynamicRHI->getBindlessResources();
	descriptor_set_layouts.push_back(native_bindless->getDescriptorLayout());

	std::vector<VkPushConstantRange> push_constant_ranges = VulkanShader::getPushConstantRanges(descriptors);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descriptor_set_layouts.size();
	pipelineLayoutInfo.pSetLayouts = descriptor_set_layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = push_constant_ranges.size();
	pipelineLayoutInfo.pPushConstantRanges = push_constant_ranges.data();
	CHECK_ERROR(vkCreatePipelineLayout(VulkanUtils::getNativeRHI()->device->logicalHandle, &pipelineLayoutInfo, nullptr, &pipeline_layout));

	if (description.is_compute_pipeline)
	{
		VkPipelineShaderStageCreateInfo compShaderStageInfo{};
		compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = static_cast<VulkanShader *>(description.compute_shader.getReference())->handle;
		compShaderStageInfo.pName = "CSMain";

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = pipeline_layout;
		pipelineInfo.stage = compShaderStageInfo;
		CHECK_ERROR(vkCreateComputePipelines(VulkanUtils::getNativeRHI()->device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
	}
	else
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
		std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
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
		std::vector<VkFormat> color_attachments;
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
		pipelineInfo.layout = pipeline_layout;
		pipelineInfo.renderPass = nullptr;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		CHECK_ERROR(vkCreateGraphicsPipelines(VulkanUtils::getNativeRHI()->device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
	}
}