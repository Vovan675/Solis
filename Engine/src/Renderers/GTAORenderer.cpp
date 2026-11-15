#include "pch.h"
#include "GTAORenderer.h"
#include "AmbientOcclusionRenderer.h"
#include "Core/Variables.h"
#include "Assets/AssetManager.h"
#include "Rendering/GlobalPipeline.h"

GTAORenderer::GTAORenderer()
{
	sampling_noise = AssetManager::getTextureAsset("assets/textures/noise/vector2_uniform_box5x5.png");
	temporal_sampling_noise = AssetManager::getTextureAsset("assets/textures/noise/vector2_uniform_box5x5_exp0101_product.png");
}

void GTAORenderer::addPasses(FrameGraph &fg, const AmbientOcclusionGuides &guides, GraphicsResourceName output)
{
	bool temporal = GFXOPTIONS(ambient_occlusion).gtao.temporal;

	fg.addCallbackPass("GTAO Main Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(output, guides.resolution.x, guides.resolution.y, Format::R8_UNORM);
		builder.writeUAVTexture(output);
		builder.readTexture(guides.view_depth);
		builder.readTexture(guides.normal);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		const AmbientOcclusionSettings &settings = GFXOPTIONS(ambient_occlusion);
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ao/gtao.hlsl", ShaderType::COMPUTE, "CSMain", {
			{"TEMPORAL", temporal ? "1" : "0"},
			{"SLICES", std::to_string(settings.gtao.slices).c_str()},
			{"STEPS", std::to_string(settings.gtao.steps).c_str()}
		}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t view_depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t output_tex_id;
			uint32_t sampling_noise_tex_id;
			float radius;
			float falloff;
			float final_power;
			int resolution_scale;
		} constants;
		constants.view_depth_tex_id = resources.getReadTexture(guides.view_depth);
		constants.normal_tex_id = resources.getReadTexture(guides.normal);
		constants.output_tex_id = resources.getReadWriteTexture(output);
		constants.sampling_noise_tex_id = (temporal ? temporal_sampling_noise : sampling_noise)->getShaderResourceView()->getBindlessIndex();
		constants.radius = settings.radius;
		constants.falloff = settings.falloff;
		constants.final_power = settings.gtao.final_power;
		constants.resolution_scale = guides.resolution_scale;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(guides.resolution.x, 16), Math::divideRoundUp(guides.resolution.y, 16), 1);
	});
}
