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

void GBufferPass::addPass(FrameGraph &fg, uint32_t max_draw_calls_count, RHIBufferRef instances_pass_masks_gpu)
{
	auto fill_buffer = [](RHIBufferRef &buffer, void *data, size_t count, size_t stride, const char *name, BufferUsage usage = BufferUsage::SHADER_READ_BUFFER, bool use_staging = false)
	{
		uint32_t buffer_size = count * stride;
		if (!buffer || buffer->getSize() < buffer_size)
		{
			BufferDescription desc;
			desc.size = buffer_size;
			desc.usage = usage;
			desc.use_staging_buffer = use_staging;
			desc.storage_stride = stride;
			buffer = gDynamicRHI->createBuffer(desc);
			buffer->setDebugName(name);
		}

		if (data)
			buffer->fill(data);
	};

	fill_buffer(draw_indexed_args_gpu, nullptr, max_draw_calls_count, sizeof(DrawIndexedIndirect), "Draw Indexed Args Buffer", BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER, true);
	fill_buffer(draw_indexed_count_gpu, nullptr, 1, sizeof(uint32_t), "Draw Indexed Count Buffer", BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER, true);
	fill_buffer(draw_calls_instances_gpu, nullptr, max_draw_calls_count, sizeof(uint32_t), "Indirect Instances Buffer", BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER, true);

	fill_buffer(indirect_visibility_gpu, nullptr, max_draw_calls_count,  sizeof(uint32_t), "Indirect Visibility Buffer", BufferUsage::SHADER_WRITE_BUFFER, true);

	// Render previously visible objects & frustum gpu_pass_cull them
	gpu_pass_cull(fg, false, max_draw_calls_count, instances_pass_masks_gpu);
	render_pass(fg, max_draw_calls_count);

	fg.addCallbackPass<EmptyData>("HiZ Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		glm::ivec2 gbuffer_size = Renderer::getViewportSize();

		TextureDescription desc;
		desc.format = FORMAT_R32_SFLOAT;

		glm::ivec2 mips = glm::max(glm::ceil(glm::log2(glm::vec2(gbuffer_size))), glm::vec2(1.0f));
		desc.mip_levels = std::max(mips.x, mips.y);
		desc.width = 1 << (mips.x - 1);
		desc.height = 1 << (mips.y - 1);

		builder.createTexture(GFXRID(HiZ), desc);
		builder.writeUAVTexture(GFXRID(HiZ), TEXTURE_RESOURCE_ACCESS_GENERAL);

		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
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
				constants.depth_tex_id = resources.getBindlessId(GFXRID(GBufferDepth));
			else
			{
				constants.depth_tex_id = hiz->getShaderResourceView(mip - 1)->getBindlessIndex();
			}
			constants.output_tex_id = hiz->getUnorderedAccessView(mip)->getBindlessIndex();
			constants.texture_size = hiz->getSize(mip);
			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			glm::ivec2 num_groups = glm::ceil(glm::vec2(hiz->getSize(mip)) / 32.0f);
			cmd_list->dispatch(num_groups.x, num_groups.y, 1);
			hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
		}
		hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
	});

	// Render previously not visible objects & frustum, hiz gpu_pass_cull them
	if (!render_freeze_culling)
	{
		gpu_pass_cull(fg, true, max_draw_calls_count, instances_pass_masks_gpu);
		render_pass(fg, max_draw_calls_count);
	}
}

void GBufferPass::gpu_pass_cull(FrameGraph &fg, bool is_late, uint32_t max_draw_calls_count, RHIBufferRef instances_pass_masks_gpu)
{
	fg.addCallbackPass<EmptyData>("Create DrawCalls Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.setSideEffect(true);

		if (is_late)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		draw_indexed_args_gpu->transitState(ResourceState::UAV);
		draw_indexed_count_gpu->transitState(ResourceState::UAV);
		draw_calls_instances_gpu->transitState(ResourceState::UAV);
		indirect_visibility_gpu->transitState(ResourceState::UAV);

		{
			struct Constants
			{
				uint32_t draw_calls_count_buffer_id;
			} constants;
			constants.draw_calls_count_buffer_id = draw_indexed_count_gpu->getUnorderedAccessView()->getBindlessIndex();

			gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/init_draw_calls.hlsl", COMPUTE_SHADER));
			gGlobalPipeline->flushAndBind(cmd_list);

			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
			cmd_list->dispatch(1, 1, 1);
		}

		draw_indexed_args_gpu->transitState(ResourceState::UAV);
		draw_indexed_count_gpu->transitState(ResourceState::UAV);
		draw_calls_instances_gpu->transitState(ResourceState::UAV);
		indirect_visibility_gpu->transitState(ResourceState::UAV);

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/create_draw_calls.hlsl", COMPUTE_SHADER, "CSMain", 
											  {{"IS_LATE", is_late ? "1" : "0"},
											  {"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
											  {"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			glm::mat4 frustum_view_projection;
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
		constants.draw_indexed_args_buffer_id = draw_indexed_args_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.draw_indexed_count_buffer_id = draw_indexed_count_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.draw_calls_indirect_instances_buffer_id = draw_calls_instances_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.instances_visibility_buffer_id = indirect_visibility_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.instances_pass_mask_buffer_id = instances_pass_masks_gpu->getUnorderedAccessView()->getBindlessIndex();
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
		instances_pass_masks_gpu->transitState(ResourceState::UAV);
		indirect_visibility_gpu->transitState(ResourceState::UAV);
	});
}

void GBufferPass::render_pass(FrameGraph &fg, uint32_t max_draw_calls_count)
{
	struct GBufferData
	{
		FrameGraphTextureId albedo;
		FrameGraphTextureId normal;
		FrameGraphTextureId depth;
		FrameGraphTextureId shading;
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

		data.albedo = builder.writeTexture(GFXRID(GBufferAlbedo));
		data.normal = builder.writeTexture(GFXRID(GBufferNormal));
		data.shading = builder.writeTexture(GFXRID(GBufferShading));
		data.depth = builder.writeTexture(GFXRID(GBufferDepth));
	},
	[=](const GBufferData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto albedo = resources.getTexture(data.albedo);
		auto normal = resources.getTexture(data.normal);
		auto depth = resources.getTexture(data.depth);
		auto shading = resources.getTexture(data.shading);

		// Render meshes into gbuffer
		VertexInputsDescription inputs_desc;
		inputs_desc.inputs.push_back({"INSTANCE_ID", 1, FORMAT_R32_UINT, true});

		cmd_list->setVertexBuffer(GlobalBufferCache::getGlobalVertexBuffer(), 0, sizeof(Engine::Vertex), 0);
		cmd_list->setVertexBuffer(draw_calls_instances_gpu, 0, sizeof(uint32_t), 1);
		cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalIndexBuffer(), 0, IndexFormat::UINT32);
		
		draw_indexed_args_gpu->transitState(ResourceState::INDIRECT_ARGS);
		draw_indexed_count_gpu->transitState(ResourceState::INDIRECT_ARGS);

		cmd_list->setRenderTargets({albedo, normal, shading}, {depth}, 0, 0, data.is_first_pass);
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, gbuffer_vertex_shader, gbuffer_fragment_shader, inputs_desc, false, true, CULL_MODE_BACK);
		gGlobalPipeline->flushAndBind(cmd_list);

		cmd_list->drawIndexedIndirect(draw_indexed_args_gpu, max_draw_calls_count, draw_indexed_count_gpu);

		cmd_list->resetRenderTargets();
	});
}
