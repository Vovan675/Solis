#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "FrameGraph/FrameGraphData.h"
#include "Core/Variables.h"
#include "Rendering/GlobalPipeline.h"

namespace
{
constexpr uint32_t MAX_MESHLETS_PER_FRAME = 1 << 23;
constexpr uint32_t HIZ_THREADGROUP_SIZE = 32;
}

void GBufferPass::addPass(FrameGraph &fg, uint32_t max_draw_calls)
{
	MeshletCullDesc desc;
	desc.pass_mask = PASS_MASK_GBUFFER;
	desc.view_projection = Renderer::getCamera()->getViewProj();
	desc.instance_count = max_draw_calls;

	meshlet_pass.addEarlyCullingPasses(fg, desc);
	addGeometryPass(fg, max_draw_calls);
	addHiZPass(fg);

	if (!render_freeze_culling)
	{
		meshlet_pass.addLateCullingPasses(fg, desc);
		addGeometryPass(fg, max_draw_calls);
	}
}

void GBufferPass::addGeometryPass(FrameGraph &fg, uint32_t max_draw_calls)
{
	struct PassData { bool clear_targets; };

	fg.addCallbackPass<PassData>("Geometry",
	[&](RenderPassBuilder &builder, PassData &data)
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
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));

		if (render_meshlets_use_mesh_shaders)
			builder.readIndirectArgsBuffer(GFXRID(DispatchMeshIndirectArgs));
		else
		{
			builder.readIndirectArgsBuffer(GFXRID(DrawIndexedArgs));
			builder.readIndirectArgsBuffer(GFXRID(DrawIndexedCount));
			builder.readVertexBuffer(GFXRID(DrawCallsInstances));
		}
	},
	[this, max_draw_calls](const PassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		RHITexture *albedo = resources.getTexture(GFXRID(GBufferAlbedo));
		RHITexture *normal = resources.getTexture(GFXRID(GBufferNormal));
		RHITexture *shading = resources.getTexture(GFXRID(GBufferShading));
		RHITexture *depth = resources.getTexture(GFXRID(GBufferDepth));

		if (!render_meshlets_use_mesh_shaders)
		{
			cmd_list->setVertexBuffer(resources.getBuffer(GFXRID(DrawCallsInstances)), 0, sizeof(uint32_t), 0);
			cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalMeshletGeometryBuffer(), 0);
		}

		cmd_list->setRenderTargets({albedo, normal, shading}, {depth}, 0, 0, data.clear_targets);

		RHIShader *pixel_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", FRAGMENT_SHADER);
		if (render_meshlets_use_mesh_shaders)
		{
			RHIShader *mesh_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", MESH_SHADER);
			gGlobalPipeline->setupMeshPipeline(cmd_list, mesh_shader, pixel_shader, false, true, CULL_MODE_NONE);
		} else
		{
			RHIShader *vertex_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", VERTEX_SHADER);
			VertexInputsDescription inputs;
			inputs.inputs.push_back({"INSTANCE_ID", 0, FORMAT_R32_UINT, true});
			gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader, pixel_shader, inputs, false, true, CULL_MODE_NONE);
		}

		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t visible_meshlets_buffer_id;
			uint32_t group_residency_buffer_id;
		} constants;
		constants.visible_meshlets_buffer_id = resources.getReadBuffer(GFXRID(VisibleMeshlets));
		constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		if (render_meshlets_use_mesh_shaders)
			cmd_list->dispatchMeshIndirect(resources.getBuffer(GFXRID(DispatchMeshIndirectArgs)), 1);
		else
			cmd_list->drawIndexedIndirect(resources.getBuffer(GFXRID(DrawIndexedArgs)), MAX_MESHLETS_PER_FRAME, resources.getBuffer(GFXRID(DrawIndexedCount)));

		cmd_list->resetRenderTargets();
	});
}

void GBufferPass::addHiZPass(FrameGraph &fg)
{
	fg.addCallbackPass("HiZ Generation",
	[&](RenderPassBuilder &builder)
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
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		RHITexture *depth = resources.getTexture(GFXRID(GBufferDepth));
		RHITexture *hiz = resources.getTexture(GFXRID(HiZ));

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
