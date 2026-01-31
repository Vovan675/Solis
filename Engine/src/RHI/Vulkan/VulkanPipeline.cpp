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

	if (description.pipeline_type == PipelineType::Compute)
	{
		shaders.push_back(static_cast<VulkanShader *>(description.compute_shader.getReference()));
	} else if (description.pipeline_type == PipelineType::RayTracing)
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
	eastl::fixed_vector<VkDescriptorSetLayout, 2> descriptor_set_layouts;
	descriptor_set_layouts.push_back(descriptor_layout.layout);

	VulkanDynamicRHI *native_rhi = VulkanUtils::getNativeRHI();

	// Bindless
	VulkanBindlessResources *native_bindless = (VulkanBindlessResources *)gDynamicRHI->getBindlessResources();
	descriptor_set_layouts.push_back(native_bindless->getDescriptorLayout());

	eastl::vector<VkPushConstantRange> push_constant_ranges = VulkanShader::getPushConstantRanges(descriptors);

	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = descriptor_set_layouts.size();
	pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
	pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
	pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();
	CHECK_ERROR(vkCreatePipelineLayout(native_rhi->device->logicalHandle, &pipeline_layout_info, nullptr, &resource->pipeline_layout));

	if (description.pipeline_type == PipelineType::Compute)
	{
		VkPipelineShaderStageCreateInfo compShaderStageInfo{};
		compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = static_cast<VulkanShader *>(description.compute_shader.getReference())->handle;
		compShaderStageInfo.pName = description.compute_shader->getEntry().c_str();

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = resource->pipeline_layout;
		pipelineInfo.stage = compShaderStageInfo;
		CHECK_ERROR(vkCreateComputePipelines(native_rhi->device->logicalHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resource->pipeline));
	} else if (description.pipeline_type == PipelineType::RayTracing)
	{
		eastl::fixed_vector<VkPipelineShaderStageCreateInfo, 3> shader_stages;

		// Ray generation group
		{
			VkPipelineShaderStageCreateInfo ray_gen_shader_stage{};
			ray_gen_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			ray_gen_shader_stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			ray_gen_shader_stage.module = static_cast<VulkanShader *>(description.ray_generation_shader.getReference())->handle;
			ray_gen_shader_stage.pName = "RayGen";
			shader_stages.push_back(ray_gen_shader_stage);

			VkRayTracingShaderGroupCreateInfoKHR shader_group{};
			shader_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shader_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shader_group.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
			shader_group.closestHitShader = VK_SHADER_UNUSED_KHR;
			shader_group.anyHitShader = VK_SHADER_UNUSED_KHR;
			shader_group.intersectionShader = VK_SHADER_UNUSED_KHR;
			shader_groups.push_back(shader_group);
		}

		// Miss group
		{
			VkPipelineShaderStageCreateInfo miss_shader_stage{};
			miss_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			miss_shader_stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
			miss_shader_stage.module = static_cast<VulkanShader *>(description.miss_shader.getReference())->handle;
			miss_shader_stage.pName = "Miss";
			shader_stages.push_back(miss_shader_stage);

			VkRayTracingShaderGroupCreateInfoKHR shader_group{};
			shader_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shader_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shader_group.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
			shader_group.closestHitShader = VK_SHADER_UNUSED_KHR;
			shader_group.anyHitShader = VK_SHADER_UNUSED_KHR;
			shader_group.intersectionShader = VK_SHADER_UNUSED_KHR;
			shader_groups.push_back(shader_group);
		}

		// Closest hit group
		{
			VkPipelineShaderStageCreateInfo closest_hit_shader_stage{};
			closest_hit_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			closest_hit_shader_stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			closest_hit_shader_stage.module = static_cast<VulkanShader *>(description.closest_hit_shader.getReference())->handle;
			closest_hit_shader_stage.pName = "ClosestHit";
			shader_stages.push_back(closest_hit_shader_stage);

			VkRayTracingShaderGroupCreateInfoKHR shader_group{};
			shader_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shader_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shader_group.generalShader = VK_SHADER_UNUSED_KHR;
			shader_group.closestHitShader = static_cast<uint32_t>(shader_stages.size()) - 1;
			shader_group.anyHitShader = VK_SHADER_UNUSED_KHR;
			shader_group.intersectionShader = VK_SHADER_UNUSED_KHR;
			shader_groups.push_back(shader_group);
		}

		//Create the ray tracing pipeline
		VkRayTracingPipelineCreateInfoKHR ray_tracing_pipeline_info{};
		ray_tracing_pipeline_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		ray_tracing_pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
		ray_tracing_pipeline_info.pStages = shader_stages.data();
		ray_tracing_pipeline_info.groupCount = static_cast<uint32_t>(shader_groups.size());
		ray_tracing_pipeline_info.pGroups = shader_groups.data();
		ray_tracing_pipeline_info.maxPipelineRayRecursionDepth = 1;
		ray_tracing_pipeline_info.layout = resource->pipeline_layout;
		CHECK_ERROR(VulkanUtils::vkCreateRayTracingPipelinesKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ray_tracing_pipeline_info, nullptr, &resource->pipeline));

		// Create shader binding table
		const uint32_t handle_size = native_rhi->device->physicalRayTracingProperties.shaderGroupHandleSize;
		const uint32_t handle_size_aligned = Math::alignedSize(native_rhi->device->physicalRayTracingProperties.shaderGroupHandleSize, native_rhi->device->physicalRayTracingProperties.shaderGroupHandleAlignment);
		const uint32_t group_count = static_cast<uint32_t>(shader_groups.size());
		const uint32_t sbt_size = group_count * handle_size_aligned;

		eastl::fixed_vector<uint8_t, 1> shader_handle_storage(sbt_size);
		CHECK_ERROR(VulkanUtils::vkGetRayTracingShaderGroupHandlesKHR(resource->pipeline, 0, group_count, sbt_size, shader_handle_storage.data()));

		BufferDescription sbt_buffer_desc;
		sbt_buffer_desc.size = handle_size;
		sbt_buffer_desc.use_staging_buffer = true;
		sbt_buffer_desc.usage = BufferUsage::SHADER_BINGING_TABLE_BUFFER;
		sbt_buffer_desc.alignment = native_rhi->device->physicalRayTracingProperties.shaderGroupBaseAlignment;

		raygen_sbt = gDynamicRHI->createBuffer(sbt_buffer_desc);
		miss_sbt = gDynamicRHI->createBuffer(sbt_buffer_desc);
		hit_sbt = gDynamicRHI->createBuffer(sbt_buffer_desc);

		// Copy handles
		raygen_sbt->fill(shader_handle_storage.data());
		miss_sbt->fill(shader_handle_storage.data() + handle_size_aligned);
		hit_sbt->fill(shader_handle_storage.data() + handle_size_aligned * 2);
	} else
	{

		// Shaders state
		VkPipelineShaderStageCreateInfo vs_stage_info{};
		vs_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vs_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vs_stage_info.module = static_cast<VulkanShader *>(description.vertex_shader.getReference())->handle;
		vs_stage_info.pName = description.vertex_shader->getEntry().c_str();

		VkPipelineShaderStageCreateInfo fs_stage_info{};
		fs_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fs_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fs_stage_info.module = static_cast<VulkanShader *>(description.fragment_shader.getReference())->handle;
		fs_stage_info.pName = description.fragment_shader->getEntry().c_str();

		VkPipelineShaderStageCreateInfo stages_info[] = {vs_stage_info, fs_stage_info};

		// Vertex input state
		VkPipelineVertexInputStateCreateInfo vertex_input_info{};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		// Use vertices
		bool use_vertex_inputs = !description.vertex_inputs_descriptions.inputs.empty();
		eastl::fixed_vector<VkVertexInputBindingDescription, 16> vertex_input_binding_descriptions;
		eastl::fixed_vector<VkVertexInputAttributeDescription, 16> vertex_input_attribute_descriptions;
		if (use_vertex_inputs)
		{
			struct BindingState
			{
				uint32_t stride = 0;
				VkVertexInputRate input_rate;
			};


			eastl::fixed_map<uint32_t, BindingState, 16> binding_states;
			

			int location = 0;
			for (const auto &input : description.vertex_inputs_descriptions.inputs)
			{
				auto &state = binding_states[input.vertex_buffer_slot];

				auto &attribute = vertex_input_attribute_descriptions.emplace_back();
				attribute.binding = input.vertex_buffer_slot;
				attribute.location = location;
				attribute.format = VulkanUtils::getNativeFormat(input.format);
				attribute.offset = state.stride;

				uint32_t format_size = VulkanUtils::getFormatSize(input.format);
				state.stride += format_size;
				state.input_rate = input.per_instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
				location++;
			}

			eastl::fixed_vector<uint32_t, 16> bindings;
			for (const auto &[binding, state] : binding_states)
			{
				VkVertexInputBindingDescription &desc = vertex_input_binding_descriptions.emplace_back();
				desc.binding = binding;
				desc.stride = state.stride;
				desc.inputRate = state.input_rate;
			}

			vertex_input_info.vertexBindingDescriptionCount = vertex_input_binding_descriptions.size();
			vertex_input_info.pVertexBindingDescriptions = vertex_input_binding_descriptions.data();
			vertex_input_info.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
			vertex_input_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();
		} else
		{
			vertex_input_info.vertexBindingDescriptionCount = 0;
			vertex_input_info.vertexAttributeDescriptionCount = 0;
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
		VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
		input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_info.topology = topology;
		input_assembly_info.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewport_info{};
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.viewportCount = 1;
		viewport_info.pViewports = nullptr; // ignored because its dynamic state
		viewport_info.scissorCount = 1;
		viewport_info.pScissors = nullptr; // ignored because its dynamic state


		VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
		switch (description.cull_mode)
		{
			case CULL_MODE_NONE: cull_mode = VK_CULL_MODE_NONE; break;
			case CULL_MODE_BACK: cull_mode = VK_CULL_MODE_BACK_BIT; break;
			case CULL_MODE_FRONT: cull_mode = VK_CULL_MODE_FRONT_BIT; break;
		}

		// Rasterizer state
		VkPipelineRasterizationStateCreateInfo rasterizer_info{};
		rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer_info.depthClampEnable = VK_FALSE;
		rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
		rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer_info.lineWidth = 1;
		rasterizer_info.cullMode = cull_mode;
		rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer_info.depthBiasEnable = VK_FALSE;
		rasterizer_info.depthBiasConstantFactor = 0;
		rasterizer_info.depthBiasClamp = 0;
		rasterizer_info.depthBiasSlopeFactor = 0;

		// Multisample state
		VkPipelineMultisampleStateCreateInfo multisampling_info{};
		multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling_info.sampleShadingEnable = VK_FALSE;
		multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Color blend state
		auto get_blend = [](Blend blend)
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

		auto get_blend_op = [](BlendOp op)
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

		eastl::fixed_vector<VkPipelineColorBlendAttachmentState, 8> color_blend_attachments(description.color_formats.size());
		for (int i = 0; i < description.color_formats.size(); i++)
		{
			VkPipelineColorBlendAttachmentState attachment{};
			attachment.blendEnable = description.use_blending;
			attachment.srcColorBlendFactor = get_blend(description.src_color_blend);
			attachment.dstColorBlendFactor = get_blend(description.dst_color_blend);
			attachment.colorBlendOp = get_blend_op(description.color_blend_op);
			attachment.srcAlphaBlendFactor = get_blend(description.src_alpha_blend);
			attachment.dstAlphaBlendFactor = get_blend(description.dst_alpha_blend);
			attachment.alphaBlendOp = get_blend_op(description.alpha_blend_op);
			attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			color_blend_attachments[i] = attachment;
		}

		VkPipelineColorBlendStateCreateInfo color_blending_info{};
		color_blending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending_info.logicOpEnable = VK_FALSE;
		color_blending_info.logicOp = VK_LOGIC_OP_COPY;
		color_blending_info.attachmentCount = color_blend_attachments.size();;
		color_blending_info.pAttachments = color_blend_attachments.data();
		color_blending_info.blendConstants[0] = 0;
		color_blending_info.blendConstants[1] = 0;
		color_blending_info.blendConstants[2] = 0;
		color_blending_info.blendConstants[3] = 0;

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

		VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.depthTestEnable = description.use_depth_test;
		depth_stencil_info.depthWriteEnable = description.use_depth_write;
		depth_stencil_info.depthCompareOp = depth_compare_op;
		depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_info.minDepthBounds = 0.0f;
		depth_stencil_info.maxDepthBounds = 1.0f;
		depth_stencil_info.stencilTestEnable = VK_FALSE;
		depth_stencil_info.front = {};
		depth_stencil_info.back = {};

		// Dynamic states
		eastl::fixed_vector<VkDynamicState, MAX_COLOR_ATTACHMENTS> dynamic_states = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		if (use_vertex_inputs)
			dynamic_states.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);

		VkPipelineDynamicStateCreateInfo dynamic_state_info{};
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.dynamicStateCount = dynamic_states.size();
		dynamic_state_info.pDynamicStates = dynamic_states.data();

		// Needed for dynamic rendering
		eastl::fixed_vector<VkFormat, MAX_COLOR_ATTACHMENTS> color_attachments;
		for (auto &format : description.color_formats)
			color_attachments.push_back(VulkanUtils::getNativeFormat(format));

		VkPipelineRenderingCreateInfo pipeline_rendering_info{};
		pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipeline_rendering_info.colorAttachmentCount = color_attachments.size();
		pipeline_rendering_info.pColorAttachmentFormats = color_attachments.data();

		if (description.depth_format != FORMAT_UNDEFINED)
			pipeline_rendering_info.depthAttachmentFormat = VulkanUtils::getNativeFormat(description.depth_format);

		// Finally create graphics pipeline
		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.pNext = &pipeline_rendering_info;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = stages_info;
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly_info;
		pipeline_info.pViewportState = &viewport_info;
		pipeline_info.pRasterizationState = &rasterizer_info;
		pipeline_info.pMultisampleState = &multisampling_info;
		pipeline_info.pDepthStencilState = &depth_stencil_info;
		pipeline_info.pColorBlendState = &color_blending_info;
		pipeline_info.pDynamicState = &dynamic_state_info;
		pipeline_info.layout = resource->pipeline_layout;
		pipeline_info.renderPass = nullptr;
		pipeline_info.subpass = 0;
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_info.basePipelineIndex = -1;

		CHECK_ERROR(vkCreateGraphicsPipelines(native_rhi->device->logicalHandle, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &resource->pipeline));
	}
}
