#include "pch.h"
#include "RTAORenderer.h"
#include "AmbientOcclusionRenderer.h"
#include "Core/Variables.h"
#include "Assets/AssetManager.h"
#include "Rendering/GlobalPipeline.h"

RTAORenderer::RTAORenderer()
{
	sampling_noise = AssetManager::getTextureAsset("assets/textures/noise/vector2_uniform_box5x5.png");
	temporal_sampling_noise = AssetManager::getTextureAsset("assets/textures/noise/vector2_uniform_box5x5_exp0101_product.png");
}

void RTAORenderer::addPasses(FrameGraph &fg, const Camera &camera, const AmbientOcclusionGuides &guides, GraphicsResourceName output)
{
	const AmbientOcclusionSettings &settings = GFXOPTIONS(ambient_occlusion);
	bool accumulate = settings.rtao.accumulate;
	bool temporal = !accumulate && settings.rtao.temporal;

	glm::mat4 view_projection = camera.getViewProj();
	bool reset = !last_accumulate || view_projection != last_view_projection || guides.resolution != last_resolution || settings.radius != last_radius || settings.falloff != last_falloff || settings.rtao.rays_per_pixel != last_rays_per_pixel;
	accumulated_frames = reset ? 0 : accumulated_frames + 1;
	last_accumulate = accumulate;
	last_view_projection = view_projection;
	last_resolution = guides.resolution;
	last_radius = settings.radius;
	last_falloff = settings.falloff;
	last_rays_per_pixel = settings.rtao.rays_per_pixel;

	fg.addCallbackPass("RTAO Trace Rays",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(output, guides.resolution.x, guides.resolution.y, Format::R8_UNORM);
		builder.writeUAVTexture(output);
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(guides.normal);

		if (accumulate)
		{
			builder.createPersistentTexture(GFXRID(RTAOAccumulation), guides.resolution.x, guides.resolution.y, Format::R32_SFLOAT);
			builder.writeUAVTexture(GFXRID(RTAOAccumulation));
		}
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ao/rtao.hlsl", ShaderType::COMPUTE, "CSMain", {{"ACCUMULATE", accumulate ? "1" : "0"}, {"TEMPORAL", temporal ? "1" : "0"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t output_tex_id;
			uint32_t accumulation_tex_id;
			uint32_t accumulated_frames;
			int rays_per_pixel;
			float radius;
			float falloff;
			uint32_t sampling_noise_tex_id;
			int resolution_scale;
		} constants;
		constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.normal_tex_id = resources.getReadTexture(guides.normal);
		constants.output_tex_id = resources.getReadWriteTexture(output);
		constants.accumulation_tex_id = accumulate ? resources.getReadWriteTexture(GFXRID(RTAOAccumulation)) : 0;
		constants.accumulated_frames = accumulated_frames;
		constants.rays_per_pixel = settings.rtao.rays_per_pixel;
		constants.radius = settings.radius;
		constants.falloff = settings.falloff;
		constants.sampling_noise_tex_id = (temporal ? temporal_sampling_noise : sampling_noise)->getShaderResourceView()->getBindlessIndex();
		constants.resolution_scale = guides.resolution_scale;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(guides.resolution.x, 8), Math::divideRoundUp(guides.resolution.y, 8), 1);
	});
}
