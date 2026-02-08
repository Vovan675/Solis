#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Core/Variables.h"

namespace
{
	constexpr uint32_t MAX_MESHLETS_PER_FRAME = 1 << 23;
	constexpr uint32_t HIZ_THREADGROUP_SIZE = 32;
	constexpr uint32_t INSTANCE_CULLING_THREADGROUP_SIZE = 32;
	constexpr uint32_t MESHLET_CULLING_THREADGROUP_SIZE = 32;

	struct MeshletCandidate
	{
		uint32_t instance_id;
		uint32_t meshlet_id;
	};
}

void GBufferPass::addPass(FrameGraph& fg, uint32_t max_draw_calls)
{
	addCullingPasses(fg, false, max_draw_calls);
	addGeometryPass(fg, max_draw_calls);
	addHiZPass(fg);

	if (!render_freeze_culling)
	{
		addCullingPasses(fg, true, max_draw_calls);
		addGeometryPass(fg, max_draw_calls);
	}
}

void GBufferPass::addHiZPass(FrameGraph& fg)
{
	fg.addCallbackPass("HiZ Generation",
	[&](RenderPassBuilder& builder)
	{
		glm::ivec2 viewport_size = Renderer::getViewportSize();

		TextureDescription desc;
		desc.format = FORMAT_R32_SFLOAT;

		glm::ivec2 mip_dimensions = glm::max(glm::ceil(glm::log2(glm::vec2(viewport_size))), glm::vec2(1.0f));
		desc.mip_levels = std::max(mip_dimensions.x, mip_dimensions.y);
		desc.width = 1 << (mip_dimensions.x - 1);
		desc.height = 1 << (mip_dimensions.y - 1);

		builder.createTexture(GFXRID(HiZ), desc);
		builder.writeUAVTexture(GFXRID(HiZ));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		RHITexture* depth = resources.getTexture(GFXRID(GBufferDepth));
		RHITexture* hiz = resources.getTexture(GFXRID(HiZ));

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/depth_hiz.hlsl", COMPUTE_SHADER, "CSMain",
		{
			{"THREADGROUP_SIZE", std::to_string(HIZ_THREADGROUP_SIZE).c_str()}
		}));
		gGlobalPipeline->flushAndBind(cmd_list);

		for (int mip = 0; mip < hiz->getMipLevels(); mip++)
		{
			struct
			{
				uint32_t output_tex_id;
				uint32_t depth_tex_id;
				glm::ivec2 texture_size;
			} constants;
			constants.depth_tex_id = (mip == 0)
				? resources.getReadTexture(GFXRID(GBufferDepth))
				: hiz->getShaderResourceView(mip - 1)->getBindlessIndex();
			constants.output_tex_id = hiz->getUnorderedAccessView(mip)->getBindlessIndex();
			constants.texture_size = hiz->getSize(mip);

			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			glm::ivec2 dispatch_size = glm::max(glm::vec2(1, 1), glm::ceil(glm::vec2(hiz->getSize(mip)) / float(HIZ_THREADGROUP_SIZE)));
			cmd_list->dispatch(dispatch_size.x, dispatch_size.y, 1);
			hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
		}
		hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
	});
}

void GBufferPass::addCullingPasses(FrameGraph& fg, bool is_late_pass, uint32_t max_draw_calls)
{
	addCounterInitPass(fg, is_late_pass);
	addInstanceCullingPass(fg, is_late_pass, max_draw_calls);
	addMeshletDispatchArgsPass(fg, is_late_pass);
	addMeshletCullingPass(fg, is_late_pass, max_draw_calls);
	addGeometryDispatchArgsPass(fg, is_late_pass);
}

void GBufferPass::addCounterInitPass(FrameGraph& fg, bool is_late_pass)
{
	fg.addCallbackPass("Init Counters",
	[&, is_late_pass](RenderPassBuilder& builder)
	{
		if (!is_late_pass)
		{
			builder.createBuffer(GFXRID(CandidateMeshletsCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(VisibleMeshletsCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(DrawIndexedCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(CandidateMeshletsCount));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DrawIndexedCount));
	},
	[=](const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		struct
		{
			uint32_t candidate_meshlets_count_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_calls_count_buffer_id;
		} constants;
		constants.candidate_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(CandidateMeshletsCount));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.draw_calls_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/init_draw_calls.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}

void GBufferPass::addInstanceCullingPass(FrameGraph& fg, bool is_late_pass, uint32_t max_draw_calls)
{
	fg.addCallbackPass("Cull Instances",
	[&, is_late_pass, max_draw_calls](RenderPassBuilder& builder)
	{
		if (!is_late_pass)
		{
			builder.createBuffer(GFXRID(CandidateMeshlets), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(IndirectVisibility), sizeof(uint32_t), max_draw_calls, BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(CandidateMeshlets));
		builder.writeBuffer(GFXRID(CandidateMeshletsCount));
		builder.writeBuffer(GFXRID(IndirectVisibility));
		builder.readBuffer(GFXRID(InstancesPassMask));

		if (is_late_pass)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_instances.hlsl", COMPUTE_SHADER, "CSMain",
		{
			{"IS_LATE", is_late_pass ? "1" : "0"},
			{"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
			{"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"},
			{"THREADGROUP_SIZE", std::to_string(INSTANCE_CULLING_THREADGROUP_SIZE).c_str()}
		}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t candidate_meshlets_buffer_id;
			uint32_t candidate_meshlets_count_buffer_id;
			uint32_t instances_visibility_buffer_id;
			uint32_t instances_pass_mask_buffer_id;
			uint32_t instances_count;
			uint32_t hiz_tex_id;
			uint32_t hiz_width;
			uint32_t hiz_height;
			uint32_t hiz_mips;
			uint32_t current_pass_mask;
		} constants = {};
		constants.frustum_view_projection = Renderer::getCamera()->getViewProj();
		constants.candidate_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(CandidateMeshlets));
		constants.candidate_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(CandidateMeshletsCount));
		constants.instances_visibility_buffer_id = resources.getReadWriteBuffer(GFXRID(IndirectVisibility));
		constants.instances_pass_mask_buffer_id = resources.getReadWriteBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = max_draw_calls;
		constants.current_pass_mask = PASS_MASK_GBUFFER;
		if (is_late_pass)
		{
			RHITexture* hiz = resources.getTexture(GFXRID(HiZ));
			constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
			constants.hiz_width = hiz->getWidth();
			constants.hiz_height = hiz->getHeight();
			constants.hiz_mips = hiz->getMipLevels();
		}
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		uint32_t num_groups = ceil(max_draw_calls / float(INSTANCE_CULLING_THREADGROUP_SIZE));
		cmd_list->dispatch(num_groups, 1, 1);
	});
}

void GBufferPass::addMeshletDispatchArgsPass(FrameGraph& fg, bool is_late_pass)
{
	fg.addCallbackPass("Build Meshlet Dispatch Args",
	[&, is_late_pass](RenderPassBuilder& builder)
	{
		if (!is_late_pass)
			builder.createBuffer(GFXRID(DispatchMeshletIndirectArgs), sizeof(DispatchIndirect), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);

		builder.writeBuffer(GFXRID(CandidateMeshletsCount));
		builder.writeBuffer(GFXRID(DispatchMeshletIndirectArgs));
	},
	[=](const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		struct
		{
			uint32_t candidate_meshlets_count_buffer_id;
			uint32_t dispatch_indirect_args_buffer_id;
			uint32_t group_size;
		} constants;
		constants.candidate_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(CandidateMeshletsCount));
		constants.dispatch_indirect_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DispatchMeshletIndirectArgs));
		constants.group_size = MESHLET_CULLING_THREADGROUP_SIZE;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/build_indirect_args.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}

void GBufferPass::addMeshletCullingPass(FrameGraph& fg, bool is_late_pass, uint32_t max_draw_calls)
{
	fg.addCallbackPass("Cull Meshlets",
	[&, is_late_pass, max_draw_calls](RenderPassBuilder& builder)
	{
		if (!is_late_pass)
		{
			builder.createBuffer(GFXRID(VisibleMeshlets), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(MeshletVisibility), sizeof(uint32_t), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);

			if (!render_meshlets_use_mesh_shaders)
			{
				builder.createBuffer(GFXRID(DrawIndexedArgs), sizeof(DrawIndexedIndirect), MAX_MESHLETS_PER_FRAME, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
				builder.createBuffer(GFXRID(DrawCallsInstances), sizeof(uint32_t), MAX_MESHLETS_PER_FRAME, BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			}
		}

		builder.writeBuffer(GFXRID(CandidateMeshlets));
		builder.writeBuffer(GFXRID(CandidateMeshletsCount));
		builder.writeBuffer(GFXRID(VisibleMeshlets));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(MeshletVisibility));
		builder.readIndirectArgsBuffer(GFXRID(DispatchMeshletIndirectArgs));

		if (!render_meshlets_use_mesh_shaders)
		{
			builder.writeBuffer(GFXRID(DrawIndexedArgs));
			builder.writeBuffer(GFXRID(DrawIndexedCount));
			builder.writeBuffer(GFXRID(DrawCallsInstances));
		}

		if (is_late_pass)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_meshlets.hlsl", COMPUTE_SHADER, "CSMain",
		{
			{"IS_LATE", is_late_pass ? "1" : "0"},
			{"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
			{"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"},
			{"USE_MESH_SHADERS", render_meshlets_use_mesh_shaders ? "1" : "0"},
			{"THREADGROUP_SIZE", std::to_string(MESHLET_CULLING_THREADGROUP_SIZE).c_str()}
		}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t candidate_meshlets_buffer_id;
			uint32_t candidate_meshlets_count_buffer_id;
			uint32_t visible_meshlets_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_indexed_args_buffer_id;
			uint32_t draw_indexed_count_buffer_id;
			uint32_t draw_calls_indirect_instances_buffer_id;
			uint32_t instances_visibility_buffer_id;
		} constants = {};
		constants.frustum_view_projection = Renderer::getCamera()->getViewProj();
		constants.candidate_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(CandidateMeshlets));
		constants.candidate_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(CandidateMeshletsCount));
		constants.visible_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshlets));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		if (!render_meshlets_use_mesh_shaders)
		{
			constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedArgs));
			constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));
			constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawCallsInstances));
		}
		constants.instances_visibility_buffer_id = resources.getReadWriteBuffer(GFXRID(MeshletVisibility));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatchIndirect(resources.getBuffer(GFXRID(DispatchMeshletIndirectArgs)), 1);
	});
}

void GBufferPass::addGeometryDispatchArgsPass(FrameGraph& fg, bool is_late_pass)
{
	if (!render_meshlets_use_mesh_shaders)
		return;

	fg.addCallbackPass("Build Geometry Dispatch Args",
	[&, is_late_pass](RenderPassBuilder& builder)
	{
		if (!is_late_pass)
			builder.createBuffer(GFXRID(DispatchMeshIndirectArgs), sizeof(DispatchIndirect), MAX_MESHLETS_PER_FRAME, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);

		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DispatchMeshIndirectArgs));
	},
	[=](const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		struct
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

void GBufferPass::addGeometryPass(FrameGraph& fg, uint32_t max_draw_calls)
{
	struct GeometryPassData
	{
		bool clear_targets;
	};

	fg.addCallbackPass<GeometryPassData>("Geometry",
	[&](RenderPassBuilder& builder, GeometryPassData& data)
	{
		glm::ivec2 viewport_size = Renderer::getViewportSize();

		data.clear_targets = !builder.isTextureCreated(GFXRID(GBufferAlbedo));

		if (data.clear_targets)
		{
			builder.createTexture(GFXRID(GBufferAlbedo), viewport_size.x, viewport_size.y, FORMAT_R8G8B8A8_UNORM);
			builder.createTexture(GFXRID(GBufferNormal), viewport_size.x, viewport_size.y, FORMAT_R8G8B8A8_UNORM);
			builder.createTexture(GFXRID(GBufferShading), viewport_size.x, viewport_size.y, FORMAT_R8G8B8A8_UNORM);
			builder.createTexture(GFXRID(GBufferDepth), viewport_size.x, viewport_size.y, FORMAT_D32S8);
		}

		builder.writeTexture(GFXRID(GBufferAlbedo));
		builder.writeTexture(GFXRID(GBufferNormal));
		builder.writeTexture(GFXRID(GBufferShading));
		builder.writeTexture(GFXRID(GBufferDepth));

		builder.readBuffer(GFXRID(VisibleMeshlets));

		if (render_meshlets_use_mesh_shaders)
		{
			builder.readIndirectArgsBuffer(GFXRID(DispatchMeshIndirectArgs));
		} else
		{
			builder.readIndirectArgsBuffer(GFXRID(DrawIndexedArgs));
			builder.readIndirectArgsBuffer(GFXRID(DrawIndexedCount));
			builder.readVertexBuffer(GFXRID(DrawCallsInstances));
		}
	},
	[this, max_draw_calls](const GeometryPassData& data, const RenderPassResources& resources, RHICommandList* cmd_list)
	{
		RHITexture* albedo = resources.getTexture(GFXRID(GBufferAlbedo));
		RHITexture* normal = resources.getTexture(GFXRID(GBufferNormal));
		RHITexture* shading = resources.getTexture(GFXRID(GBufferShading));
		RHITexture* depth = resources.getTexture(GFXRID(GBufferDepth));

		if (!render_meshlets_use_mesh_shaders)
		{
			cmd_list->setVertexBuffer(resources.getBuffer(GFXRID(DrawCallsInstances)), 0, sizeof(uint32_t), 0);
			cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalMeshletTriangleBuffer(), 0);
		}

		cmd_list->setRenderTargets({albedo, normal, shading}, {depth}, 0, 0, data.clear_targets);
		
		RHIShader *pixel_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", FRAGMENT_SHADER);
		if (render_meshlets_use_mesh_shaders)
		{
			RHIShader* mesh_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", MESH_SHADER);
			gGlobalPipeline->setupMeshPipeline(cmd_list, mesh_shader, pixel_shader, false, true, CULL_MODE_BACK);
		} else
		{
			RHIShader *vertex_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", VERTEX_SHADER);
			VertexInputsDescription inputs;
			inputs.inputs.push_back({"INSTANCE_ID", 0, FORMAT_R32_UINT, true});
			gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, pixel_shader, inputs, false, true, CULL_MODE_BACK);
		}

		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t visible_meshlets_buffer_id;
		} constants;
		constants.visible_meshlets_buffer_id = resources.getReadBuffer(GFXRID(VisibleMeshlets));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		if (render_meshlets_use_mesh_shaders)
			cmd_list->dispatchMeshIndirect(resources.getBuffer(GFXRID(DispatchMeshIndirectArgs)), 1);
		else
			cmd_list->drawIndexedIndirect(resources.getBuffer(GFXRID(DrawIndexedArgs)), MAX_MESHLETS_PER_FRAME, resources.getBuffer(GFXRID(DrawIndexedCount)));

		cmd_list->resetRenderTargets();
	});
}
