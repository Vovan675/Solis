#include "pch.h"
#include "AmbientOcclusionRenderer.h"
#include "Core/Variables.h"
#include "Rendering/Renderer.h"

void AmbientOcclusionRenderer::addPasses(FrameGraph &fg, const Camera &camera)
{
	const AmbientOcclusionSettings &settings = GFXOPTIONS(ambient_occlusion);

	auto make_guides = [](int resolution_scale)
	{
		AmbientOcclusionGuides guides;
		guides.resolution_scale = resolution_scale;
		guides.resolution = glm::ivec2(Math::divideRoundUp(Renderer::getRenderWidth(), resolution_scale), Math::divideRoundUp(Renderer::getRenderHeight(), resolution_scale));
		guides.view_depth = GFXRID(AmbientOcclusionViewDepth);
		guides.normal = resolution_scale != 1 ? GFXRID(AmbientOcclusionNormal) : GFXRID(GBufferNormal);
		return guides;
	};

	auto add_denoise_passes = [&](const AmbientOcclusionGuides &guides, GraphicsResourceName input, bool temporal, int max_history_length, bool spatial_filter)
	{
		bool upsample = guides.resolution_scale != 1;

		if (temporal)
		{
			uint64_t frame = gDynamicRHI->getFrame();
			GraphicsResourceName history = GFXRID_ID(AmbientOcclusionHistory, (frame + 1) % 2);
			GraphicsResourceName accumulated = GFXRID_ID(AmbientOcclusionHistory, frame % 2);
			GraphicsResourceName output = spatial_filter || upsample ? GFXRID_NONE : GFXRID(AmbientOcclusion);
			add_temporal_pass(fg, guides, input, history, accumulated, output, max_history_length);
			input = accumulated;
		}

		if (spatial_filter)
		{
			GraphicsResourceName output = upsample ? GFXRID(AmbientOcclusionFiltered) : GFXRID(AmbientOcclusion);
			add_spatial_filter_pass(fg, guides, input, output);
			input = output;
		}

		if (upsample)
			add_upsample_pass(fg, guides, input, GFXRID(AmbientOcclusion));
	};

	switch (settings.mode)
	{
	case AMBIENT_OCCLUSION_OFF:
		break;
	case AMBIENT_OCCLUSION_SSAO:
		ssao_renderer.addPasses(fg);
		break;
	case AMBIENT_OCCLUSION_GTAO:
	{
		AmbientOcclusionGuides guides = make_guides(settings.gtao.half_resolution ? 2 : 1);
		bool denoise = settings.gtao.temporal || settings.gtao.spatial_filter || guides.resolution_scale != 1;
		GraphicsResourceName noisy = denoise ? GFXRID(AmbientOcclusionNoisy) : GFXRID(AmbientOcclusion);

		add_guides_pass(fg, guides, true);
		gtao_renderer.addPasses(fg, guides, noisy);
		if (denoise)
			add_denoise_passes(guides, noisy, settings.gtao.temporal, settings.gtao.max_history_length, settings.gtao.spatial_filter);
		break;
	}
	case AMBIENT_OCCLUSION_RTAO:
		if (Renderer::isRTAOEnabled())
		{
			AmbientOcclusionGuides guides = make_guides(settings.rtao.half_resolution && !settings.rtao.accumulate ? 2 : 1);
			bool denoise = !settings.rtao.accumulate && (settings.rtao.temporal || settings.rtao.spatial_filter || guides.resolution_scale != 1);
			GraphicsResourceName noisy = denoise ? GFXRID(AmbientOcclusionNoisy) : GFXRID(AmbientOcclusion);

			add_guides_pass(fg, guides, false);
			rtao_renderer.addPasses(fg, camera, guides, noisy);
			if (denoise)
				add_denoise_passes(guides, noisy, settings.rtao.temporal, settings.rtao.max_history_length, settings.rtao.spatial_filter);
		}
		break;
	}
}

void AmbientOcclusionRenderer::add_guides_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, bool view_depth_mips)
{
	bool write_normal = guides.resolution_scale != 1;

	fg.addCallbackPass("Ambient Occlusion Guides",
	[&](RenderPassBuilder &builder)
	{
		TextureDescription view_depth_desc;
		view_depth_desc.width = guides.resolution.x;
		view_depth_desc.height = guides.resolution.y;
		view_depth_desc.mip_levels = view_depth_mips ? 5 : 1;
		view_depth_desc.format = Format::R32_SFLOAT;
		builder.createTexture(guides.view_depth, view_depth_desc);
		builder.writeUAVTexture(guides.view_depth);
		builder.readTexture(GFXRID(GBufferDepth));

		if (write_normal)
		{
			builder.createTexture(guides.normal, guides.resolution.x, guides.resolution.y, Format::R8G8B8A8_UNORM);
			builder.writeUAVTexture(guides.normal);
			builder.readTexture(GFXRID(GBufferNormal));
		}
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ao/ao_guides.hlsl", ShaderType::COMPUTE, "CSMain", {
			{"WRITE_NORMAL", write_normal ? "1" : "0"},
			{"WRITE_MIPS", view_depth_mips ? "1" : "0"}
		}));
		gGlobalPipeline->flushAndBind(cmd_list);

		RHITexture *view_depth = resources.getTexture(guides.view_depth);
		struct
		{
			glm::uvec4 view_depth_mip_tex_ids;
			uint32_t depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t view_depth_mip0_tex_id;
			uint32_t normal_guide_tex_id;
			int resolution_scale;
		} constants;
		constants.view_depth_mip_tex_ids = glm::uvec4(0);
		if (view_depth_mips)
		{
			constants.view_depth_mip_tex_ids.x = view_depth->getUnorderedAccessView(1)->getBindlessIndex();
			constants.view_depth_mip_tex_ids.y = view_depth->getUnorderedAccessView(2)->getBindlessIndex();
			constants.view_depth_mip_tex_ids.z = view_depth->getUnorderedAccessView(3)->getBindlessIndex();
			constants.view_depth_mip_tex_ids.w = view_depth->getUnorderedAccessView(4)->getBindlessIndex();
		}
		constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.normal_tex_id = write_normal ? resources.getReadTexture(GFXRID(GBufferNormal)) : 0;
		constants.view_depth_mip0_tex_id = view_depth->getUnorderedAccessView(0)->getBindlessIndex();
		constants.normal_guide_tex_id = write_normal ? resources.getReadWriteTexture(guides.normal) : 0;
		constants.resolution_scale = guides.resolution_scale;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(guides.resolution.x, 16), Math::divideRoundUp(guides.resolution.y, 16), 1);
	});
}

void AmbientOcclusionRenderer::add_temporal_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName input, GraphicsResourceName history, GraphicsResourceName accumulated, GraphicsResourceName output, int max_history_length)
{
	uint64_t frame = gDynamicRHI->getFrame();
	bool reset_history = frame != last_temporal_frame + 1 || guides.resolution != last_temporal_resolution;
	last_temporal_frame = frame;
	last_temporal_resolution = guides.resolution;

	bool write_output = output.isValid();

	fg.addCallbackPass("Ambient Occlusion Temporal Accumulation",
	[&](RenderPassBuilder &builder)
	{
		TextureDescription history_desc;
		history_desc.width = guides.resolution.x;
		history_desc.height = guides.resolution.y;
		history_desc.format = Format::R16G16B16A16_SFLOAT;
		history_desc.usage_flags = TextureUsage::UNORDERED_ACCESS;
		builder.createPersistentTexture(history, history_desc);
		builder.createPersistentTexture(accumulated, history_desc);
		builder.writeUAVTexture(accumulated);
		builder.readTexture(history);
		builder.readTexture(input);
		builder.readTexture(guides.view_depth);
		builder.readTexture(GFXRID(MotionVectors));

		if (write_output)
		{
			builder.createTexture(output, guides.resolution.x, guides.resolution.y, Format::R8_UNORM);
			builder.writeUAVTexture(output);
		}
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ao/ao_temporal.hlsl", ShaderType::COMPUTE, "CSMain", {{"WRITE_OUTPUT", write_output ? "1" : "0"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t noisy_tex_id;
			uint32_t history_tex_id;
			uint32_t view_depth_tex_id;
			uint32_t motion_vectors_tex_id;
			uint32_t accumulated_tex_id;
			uint32_t reset_history;
			float max_history_length;
			int resolution_scale;
			uint32_t output_tex_id;
		} constants;
		constants.noisy_tex_id = resources.getReadTexture(input);
		constants.history_tex_id = resources.getReadTexture(history);
		constants.view_depth_tex_id = resources.getReadTexture(guides.view_depth);
		constants.motion_vectors_tex_id = resources.getReadTexture(GFXRID(MotionVectors));
		constants.accumulated_tex_id = resources.getReadWriteTexture(accumulated);
		constants.reset_history = reset_history;
		constants.max_history_length = max_history_length;
		constants.resolution_scale = guides.resolution_scale;
		constants.output_tex_id = write_output ? resources.getReadWriteTexture(output) : 0;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(guides.resolution.x, 8), Math::divideRoundUp(guides.resolution.y, 8), 1);
	});
}

void AmbientOcclusionRenderer::add_spatial_filter_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName input, GraphicsResourceName output)
{
	fg.addCallbackPass("Ambient Occlusion Spatial Filter",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(output, guides.resolution.x, guides.resolution.y, Format::R8_UNORM);
		builder.writeUAVTexture(output);
		builder.readTexture(input);
		builder.readTexture(guides.view_depth);
		builder.readTexture(guides.normal);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ao/ao_spatial_filter.hlsl", ShaderType::COMPUTE, "CSMain"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t input_tex_id;
			uint32_t view_depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t output_tex_id;
			int resolution_scale;
		} constants;
		constants.input_tex_id = resources.getReadTexture(input);
		constants.view_depth_tex_id = resources.getReadTexture(guides.view_depth);
		constants.normal_tex_id = resources.getReadTexture(guides.normal);
		constants.output_tex_id = resources.getReadWriteTexture(output);
		constants.resolution_scale = guides.resolution_scale;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(guides.resolution.x, 16), Math::divideRoundUp(guides.resolution.y, 16), 1);
	});
}

void AmbientOcclusionRenderer::add_upsample_pass(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName input, GraphicsResourceName output)
{
	fg.addCallbackPass("Ambient Occlusion Upsample",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(output, Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R8_UNORM);
		builder.writeUAVTexture(output);
		builder.readTexture(input);
		builder.readTexture(guides.view_depth);
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferNormal));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ao/ao_upsample.hlsl", ShaderType::COMPUTE, "CSMain"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t input_tex_id;
			uint32_t input_view_depth_tex_id;
			uint32_t depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t output_tex_id;
			int resolution_scale;
		} constants;
		constants.input_tex_id = resources.getReadTexture(input);
		constants.input_view_depth_tex_id = resources.getReadTexture(guides.view_depth);
		constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		constants.output_tex_id = resources.getReadWriteTexture(output);
		constants.resolution_scale = guides.resolution_scale;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(Renderer::getRenderWidth(), 8), Math::divideRoundUp(Renderer::getRenderHeight(), 8), 1);
	});
}
