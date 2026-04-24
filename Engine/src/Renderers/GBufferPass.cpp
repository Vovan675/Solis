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

	import_or_create_hiz(fg);

	// Main cull using prev HiZ to cull almost everything ideally
	meshlet_pass.addMainCullingPasses(fg, desc);
	add_geometry_pass(fg, max_draw_calls);
	add_hiz_pass(fg);

	// TODO: not meshleted geometries render here, it would benefit from depth/HiZ if needed

	if (!render_freeze_culling)
	{
		// Fix culling rescue what is not occluded against new "Real" HiZ
		meshlet_pass.addFixCullingPasses(fg, desc);
		add_geometry_pass(fg, max_draw_calls);
		// Rebuild HiZ for next frame
		add_hiz_pass(fg);
	}
}

void GBufferPass::import_or_create_hiz(FrameGraph &fg)
{
	glm::ivec2 viewport_size = Renderer::getViewportSize();
	glm::ivec2 mip_dimensions = glm::max(glm::ceil(glm::log2(glm::vec2(viewport_size))), glm::vec2(1.0f));
	uint32_t mip_levels = std::max(mip_dimensions.x, mip_dimensions.y);
	uint32_t width = 1u << (mip_dimensions.x - 1);
	uint32_t height = 1u << (mip_dimensions.y - 1);

	bool need_realloc = !persistent_hiz
		|| persistent_hiz->getWidth() != width
		|| persistent_hiz->getHeight() != height
		|| persistent_hiz->getMipLevels() != mip_levels;

	if (need_realloc)
	{
		TextureDescription desc;
		desc.format = FORMAT_R32_SFLOAT;
		desc.width = width;
		desc.height = height;
		desc.mip_levels = mip_levels;
		desc.usage_flags = TEXTURE_USAGE_STORAGE;
		desc.filtering = FILTER_NEAREST;
		persistent_hiz = gDynamicRHI->createTexture(desc);
		persistent_hiz->fill();
		persistent_hiz->setDebugName("Persistent HiZ");
	}

	fg.importTexture(GFXRID(HiZ), persistent_hiz);
}

void GBufferPass::add_geometry_pass(FrameGraph &fg, uint32_t max_draw_calls)
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

void GBufferPass::add_hiz_pass(FrameGraph &fg)
{
	fg.addCallbackPass("HiZ Generation",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(HiZ));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
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
