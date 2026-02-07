#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

GBufferPass::GBufferPass()
{
	gbuffer_vertex_shader = gDynamicRHI->createShader(L"shaders/opaque.hlsl", VERTEX_SHADER);
	gbuffer_fragment_shader = gDynamicRHI->createShader(L"shaders/opaque.hlsl", FRAGMENT_SHADER);
}

void GBufferPass::addPass(FrameGraph &fg, uint32_t max_draw_calls_count)
{
	// Render previously visible objects & frustum gpu_pass_cull them
	gpu_pass_cull(fg, false, max_draw_calls_count);
	render_pass(fg, max_draw_calls_count);

	fg.addCallbackPass("HiZ Pass",
	[&](RenderPassBuilder &builder)
	{
		glm::ivec2 gbuffer_size = Renderer::getViewportSize();

		TextureDescription desc;
		desc.format = FORMAT_R32_SFLOAT;

		glm::ivec2 mips = glm::max(glm::ceil(glm::log2(glm::vec2(gbuffer_size))), glm::vec2(1.0f));
		desc.mip_levels = std::max(mips.x, mips.y);
		desc.width = 1 << (mips.x - 1);
		desc.height = 1 << (mips.y - 1);

		builder.createTexture(GFXRID(HiZ), desc);
		builder.writeUAVTexture(GFXRID(HiZ));

		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		RHITexture *depth = resources.getTexture(GFXRID(GBufferDepth));
		RHITexture *hiz = resources.getTexture(GFXRID(HiZ));

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/depth_hiz.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t output_tex_id;
			uint32_t depth_tex_id;
			glm::ivec2 texture_size;
		} constants;

		for (int mip = 0; mip < hiz->getMipLevels(); mip++)
		{
			if (mip == 0)
				constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
			else
			{
				constants.depth_tex_id = hiz->getShaderResourceView(mip - 1)->getBindlessIndex();
			}
			constants.output_tex_id = hiz->getUnorderedAccessView(mip)->getBindlessIndex();
			constants.texture_size = hiz->getSize(mip);
			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			glm::ivec2 num_groups = glm::max(glm::vec2(1, 1), glm::ceil(glm::vec2(hiz->getSize(mip)) / 32.0f));
			cmd_list->dispatch(num_groups.x, num_groups.y, 1);
			hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
		}
		hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
	});

	// Render previously not visible objects & frustum, hiz gpu_pass_cull them
	if (!render_freeze_culling)
	{
		gpu_pass_cull(fg, true, max_draw_calls_count);
		render_pass(fg, max_draw_calls_count);
	}
}

uint32_t max_clusters_per_frame = 1 << 22;
void GBufferPass::gpu_pass_cull(FrameGraph &fg, bool is_late, uint32_t max_draw_calls_count)
{
	fg.addCallbackPass("Init DrawCalls Pass",
	[&](RenderPassBuilder &builder)
	{
		if (!is_late)
		{
			builder.createBuffer(GFXRID(VisibleInstancesCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(VisibleMeshletsCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(DrawIndexedCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(VisibleInstancesCount));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DrawIndexedCount));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct Constants
		{
			uint32_t visible_instances_count_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_calls_count_buffer_id;
		} constants;
		constants.visible_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleInstancesCount));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.draw_calls_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/init_draw_calls.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});

	fg.addCallbackPass("Cull Instances Pass",
	[&](RenderPassBuilder &builder)
	{
		struct MeshletCandidate
		{
			uint32_t instance_id;
			uint32_t meshlet_id;
		};

		if (!is_late)
		{
			builder.createBuffer(GFXRID(VisibleInstances), sizeof(MeshletCandidate), max_clusters_per_frame, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(IndirectVisibility), sizeof(uint32_t), max_draw_calls_count, BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(VisibleInstances));
		builder.writeBuffer(GFXRID(VisibleInstancesCount));
		builder.writeBuffer(GFXRID(IndirectVisibility));
		builder.readBuffer(GFXRID(InstancesPassMask));

		if (is_late)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/create_draw_calls.hlsl", COMPUTE_SHADER, "CSMain", 
											  {{"IS_LATE", is_late ? "1" : "0"},
											  {"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
											  {"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			glm::mat4 frustum_view_projection;
			uint32_t visible_instances_buffer_id;
			uint32_t visible_instances_count_buffer_id;
			uint32_t draw_indexed_args_buffer_id;
			uint32_t draw_indexed_count_buffer_id;
			uint32_t draw_calls_indirect_instances_buffer_id;
			uint32_t instances_visibility_buffer_id;
			uint32_t instances_pass_mask_buffer_id;
			uint32_t instances_count;
			uint32_t hiz_tex_id;
			uint32_t hiz_width;
			uint32_t hiz_height;
			uint32_t hiz_mips;
			uint32_t current_pass_mask;
			uint32_t pad;
		} constants;
		resources.getBuffer(GFXRID(VisibleInstances))->transitState(ResourceState::UAV);
		resources.getBuffer(GFXRID(VisibleInstancesCount))->transitState(ResourceState::UAV);
		constants.frustum_view_projection = Renderer::getCamera()->getProj() * Renderer::getCamera()->getView();
		constants.visible_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleInstances));
		constants.visible_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleInstancesCount));
		constants.instances_visibility_buffer_id = resources.getReadWriteBuffer(GFXRID(IndirectVisibility));
		constants.instances_pass_mask_buffer_id = resources.getReadWriteBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = max_draw_calls_count;
		if (is_late)
		{
			RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
			constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
			constants.hiz_width = hiz->getWidth();
			constants.hiz_height = hiz->getHeight();
			constants.hiz_mips = hiz->getMipLevels();
		}
		constants.current_pass_mask = PASS_MASK_GBUFFER;

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		int num_groups = ceil(constants.instances_count / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);

		resources.getBuffer(GFXRID(VisibleInstances))->transitState(ResourceState::UAV);
		resources.getBuffer(GFXRID(VisibleInstancesCount))->transitState(ResourceState::UAV);
	});

	fg.addCallbackPass("Build Meshlet Dispatch Args Pass",
	[&](RenderPassBuilder &builder)
	{
		if (!is_late)
		{
			builder.createBuffer(GFXRID(DispatchMeshletIndirectArgs), sizeof(DispatchIndirect), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(VisibleInstancesCount));
		builder.writeBuffer(GFXRID(DispatchMeshletIndirectArgs));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct Constants
		{
			uint32_t visible_instances_count_buffer_id;
			uint32_t dispatch_indirect_args_buffer_id;
			uint32_t group_size;
		} constants;
		constants.visible_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleInstancesCount));
		constants.dispatch_indirect_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DispatchMeshletIndirectArgs));
		constants.group_size = 32;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/build_indirect_args.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});

	fg.addCallbackPass("Cull Meshlets Pass",
	[&](RenderPassBuilder &builder)
	{
		struct MeshletCandidate
		{
			uint32_t instance_id;
			uint32_t meshlet_id;
		};

		if (!is_late)
		{
			builder.createBuffer(GFXRID(VisibleMeshlets), sizeof(MeshletCandidate), max_clusters_per_frame, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(DrawIndexedArgs), sizeof(DrawIndexedIndirect), max_clusters_per_frame, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(DrawCallsInstances), sizeof(uint32_t), max_clusters_per_frame, BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(MeshletVisibility), sizeof(uint32_t), max_clusters_per_frame, BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(VisibleInstances));
		builder.writeBuffer(GFXRID(VisibleInstancesCount));
		builder.writeBuffer(GFXRID(VisibleMeshlets));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DrawIndexedArgs));
		builder.writeBuffer(GFXRID(DrawIndexedCount));
		builder.writeBuffer(GFXRID(DrawCallsInstances));
		builder.writeBuffer(GFXRID(MeshletVisibility));
		builder.readBuffer(GFXRID(InstancesPassMask));
		builder.readIndirectArgsBuffer(GFXRID(DispatchMeshletIndirectArgs));

		if (is_late)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_meshlets.hlsl", COMPUTE_SHADER, "CSMain", 
											  {{"IS_LATE", is_late ? "1" : "0"},
											  {"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
											  {"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		resources.getBuffer(GFXRID(VisibleInstances))->transitState(ResourceState::UAV);
		resources.getBuffer(GFXRID(VisibleInstancesCount))->transitState(ResourceState::UAV);

		struct Constants
		{
			glm::mat4 frustum_view_projection;
			uint32_t visible_instances_buffer_id;
			uint32_t visible_instances_count_buffer_id;
			uint32_t visible_meshlets_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_indexed_args_buffer_id;
			uint32_t draw_indexed_count_buffer_id;
			uint32_t draw_calls_indirect_instances_buffer_id;
			uint32_t instances_visibility_buffer_id;
			uint32_t instances_pass_mask_buffer_id;
			uint32_t instances_count;
			uint32_t hiz_tex_id;
			uint32_t hiz_width;
			uint32_t hiz_height;
			uint32_t hiz_mips;
			uint32_t current_pass_mask;
			uint32_t pad;
		} constants;

		constants.frustum_view_projection = Renderer::getCamera()->getProj() * Renderer::getCamera()->getView();
		constants.visible_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleInstances));
		constants.visible_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleInstancesCount));
		constants.visible_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshlets));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedArgs));
		constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));
		constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawCallsInstances));
		constants.instances_visibility_buffer_id = resources.getReadWriteBuffer(GFXRID(MeshletVisibility));
		constants.instances_pass_mask_buffer_id = resources.getReadWriteBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = max_draw_calls_count;
		if (is_late)
		{
			RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
			constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
			constants.hiz_width = hiz->getWidth();
			constants.hiz_height = hiz->getHeight();
			constants.hiz_mips = hiz->getMipLevels();
		}
		constants.current_pass_mask = PASS_MASK_GBUFFER;

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		resources.getBuffer(GFXRID(DispatchMeshletIndirectArgs))->transitState(ResourceState::INDIRECT_ARGS);
		cmd_list->dispatchIndirect(resources.getBuffer(GFXRID(DispatchMeshletIndirectArgs)), 1);
	});

	fg.addCallbackPass("Create Dispatch Args Pass",
	[&](RenderPassBuilder &builder)
	{
		if (!is_late)
		{
			builder.createBuffer(GFXRID(DispatchMeshIndirectArgs), sizeof(DispatchIndirect), max_clusters_per_frame, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DispatchMeshIndirectArgs));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct Constants
		{
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t dispatch_indirect_args_buffer_id;
			uint32_t group_size;
		} constants;
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.dispatch_indirect_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DispatchMeshIndirectArgs));
		constants.group_size = 1;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/build_indirect_args.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}

void GBufferPass::render_pass(FrameGraph &fg, uint32_t max_draw_calls_count)
{
	struct GBufferData
	{
		bool is_first_pass;
	};

	fg.addCallbackPass<GBufferData>("GBuffer Pass",
	[&](RenderPassBuilder &builder, GBufferData &data)
	{
		glm::ivec2 gbuffer_size = Renderer::getViewportSize();

		data.is_first_pass = !builder.isTextureCreated(GFXRID(GBufferAlbedo));

		if (data.is_first_pass)
		{
			builder.createTexture(GFXRID(GBufferAlbedo), gbuffer_size.x, gbuffer_size.y, FORMAT_R8G8B8A8_UNORM);
			builder.createTexture(GFXRID(GBufferNormal), gbuffer_size.x, gbuffer_size.y, FORMAT_R8G8B8A8_UNORM);
			builder.createTexture(GFXRID(GBufferShading), gbuffer_size.x, gbuffer_size.y, FORMAT_R8G8B8A8_UNORM);
			builder.createTexture(GFXRID(GBufferDepth), gbuffer_size.x, gbuffer_size.y, FORMAT_D32S8);
		}

		builder.writeTexture(GFXRID(GBufferAlbedo));
		builder.writeTexture(GFXRID(GBufferNormal));
		builder.writeTexture(GFXRID(GBufferShading));
		builder.writeTexture(GFXRID(GBufferDepth));

		builder.readBuffer(GFXRID(VisibleMeshlets));
		builder.readIndirectArgsBuffer(GFXRID(DispatchMeshIndirectArgs));
		builder.readIndirectArgsBuffer(GFXRID(DrawIndexedArgs));
		builder.readIndirectArgsBuffer(GFXRID(DrawIndexedCount));
		builder.readVertexBuffer(GFXRID(DrawCallsInstances));
	},
	[=](const GBufferData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto albedo = resources.getTexture(GFXRID(GBufferAlbedo));
		auto normal = resources.getTexture(GFXRID(GBufferNormal));
		auto shading = resources.getTexture(GFXRID(GBufferShading));
		auto depth = resources.getTexture(GFXRID(GBufferDepth));

		resources.getBuffer(GFXRID(DrawIndexedArgs))->transitState(ResourceState::INDIRECT_ARGS);
		resources.getBuffer(GFXRID(DrawIndexedCount))->transitState(ResourceState::INDIRECT_ARGS);

		// per-draw meshlet_id for instance data
		VertexInputsDescription inputs_desc;
		inputs_desc.inputs.push_back({"INSTANCE_ID", 0, FORMAT_R32_UINT, true});

		cmd_list->setVertexBuffer(resources.getBuffer(GFXRID(DrawCallsInstances)), 0, sizeof(uint32_t), 0);
		cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalMeshletTriangleBuffer(), 0);

		cmd_list->setRenderTargets({albedo, normal, shading}, {depth}, 0, 0, data.is_first_pass);
		//gGlobalPipeline->setupGraphicsPipeline(cmd_list, gbuffer_vertex_shader, gbuffer_fragment_shader, inputs_desc, false, true, CULL_MODE_BACK);
		gGlobalPipeline->setupMeshPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/opaque.hlsl", MESH_SHADER), gbuffer_fragment_shader, false, true, CULL_MODE_BACK);
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t visible_meshlets_buffer_id;
		} constants;
		constants.visible_meshlets_buffer_id = resources.getReadBuffer(GFXRID(VisibleMeshlets));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		//cmd_list->drawIndexedIndirect(resources.getBuffer(GFXRID(DrawIndexedArgs)), max_draw_calls_count, resources.getBuffer(GFXRID(DrawIndexedCount)));
		cmd_list->dispatchMeshIndirect(resources.getBuffer(GFXRID(DispatchMeshIndirectArgs)), 1);

		cmd_list->resetRenderTargets();
	});
}
