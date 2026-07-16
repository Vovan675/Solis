#include "pch.h"
#include "UpscaleRenderer.h"
#include "Core/Variables.h"
#include "Rendering/Renderer.h"
#include "RHI/Upscaler.h"
#include "RHI/BindlessResources.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/RenderPassBuilder.h"
#include "FrameGraph/RenderPassResources.h"

glm::ivec2 UpscaleRenderer::getRenderResolution(glm::ivec2 output_resolution)
{
	Upscaler *upscaler = gDynamicRHI->getUpscaler();
	if (render_upscale_mode != UPSCALE_MODE_DLSS || !upscaler)
	{
		if (last_render_resolution != glm::ivec2(0))
		{
			gDynamicRHI->waitGPU();
			gDynamicRHI->getBindlessResources()->setSamplersMipBias(0.0f);
		}
		history_valid = false;
		last_render_resolution = glm::ivec2(0);
		return output_resolution;
	}

	glm::ivec2 render_resolution = upscaler->getRenderResolution(output_resolution);

	if (render_resolution != last_render_resolution)
	{
		gDynamicRHI->waitGPU();
		upscaler->freeResources();
		last_render_resolution = render_resolution;
		history_valid = false;

		float mip_bias = getMipBias((float)render_resolution.x / output_resolution.x);
		gDynamicRHI->getBindlessResources()->setSamplersMipBias(mip_bias);
	}

	return render_resolution;
}

void UpscaleRenderer::addPasses(FrameGraph &frame_graph)
{
	if (render_upscale_mode != UPSCALE_MODE_DLSS)
		return;

	bool reset_history = !history_valid;
	history_valid = true;

	GraphicsResourceName color_input = render_ssr ? GFXRID(SSR) : GFXRID(FinalNoPostTexture);

	frame_graph.addCallbackPass("DLSS Upscale",
	[&](RenderPassBuilder &builder)
	{
		auto &input_desc = builder.getTextureDescription(color_input);
		if (!builder.isTextureCreated(GFXRID(UpscaledColor)))
			builder.createTexture(GFXRID(UpscaledColor), Renderer::getOutputWidth(), Renderer::getOutputHeight(), input_desc.format);

		builder.readTexture(color_input);
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(MotionVectors));
		builder.writeUAVTexture(GFXRID(UpscaledColor));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		Upscaler *upscaler = gDynamicRHI->getUpscaler();
		if (!upscaler || !upscaler->isAvailable())
			return;

		UpscalerInputs inputs;
		inputs.color_input = resources.getTexture(color_input);
		inputs.color_output = resources.getTexture(GFXRID(UpscaledColor));
		inputs.depth = resources.getTexture(GFXRID(GBufferDepth));
		inputs.motion_vectors = resources.getTexture(GFXRID(MotionVectors));
		inputs.reset_history = reset_history;

		upscaler->evaluate(cmd_list, inputs);
	});
}
