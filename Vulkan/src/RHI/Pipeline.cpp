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
}

void Pipeline::create(const PipelineDescription &description)
{
	cleanup();
	hash = description.getHash();

	// Shaders state
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = description.vertex_shader->handle;
	vertShaderStageInfo.pName = "main";//Entry point

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = description.fragment_shader->handle;
	fragShaderStageInfo.pName = "main";//Entry point

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
	pipelineInfo.pDynamicState = &dynamicState;//TODO: enable
	pipelineInfo.layout = pipeline_layout;
	pipelineInfo.renderPass = nullptr;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	CHECK_ERROR(vkCreateGraphicsPipelines(VkWrapper::device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
}
