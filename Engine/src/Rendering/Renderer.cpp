#include "pch.h"
#include "Renderer.h"
#include "RHI/BindlessResources.h"
#include "RHI/Upscaler.h"
#include "Core/Variables.h"

RendererDebugInfo Renderer::prev_debug_info = {};
RendererDebugInfo Renderer::debug_info = {};

Camera *Renderer::camera;
const PassConstants *Renderer::pass_constants = nullptr;

glm::ivec2 Renderer::render_resolution;
glm::ivec2 Renderer::output_resolution;
int Renderer::screenshot_frames_waited = 0;

void Renderer::init()
{
	output_resolution = gDynamicRHI->getSwapchainTexture(0)->getSize();
	render_resolution = output_resolution;
}

void Renderer::shutdown()
{
}

void Renderer::setRenderResolution(glm::ivec2 size)
{
	if (render_resolution == size)
		return;
	render_resolution = size;
}

void Renderer::setOutputResolution(glm::ivec2 size)
{
	if (output_resolution == size)
		return;
	output_resolution = size;
}

void Renderer::beginFrame()
{
	// Update debug info
	prev_debug_info = debug_info;
	debug_info = RendererDebugInfo{};

	screenshot_frames_waited = render_screenshot ? screenshot_frames_waited + 1 : 0;
}

void Renderer::endFrame(unsigned int image_index)
{
}

bool Renderer::isUpscalerActive()
{
	Upscaler *upscaler = gDynamicRHI->getUpscaler();
	return GFXOPTIONS(anti_aliasing).upscale == UPSCALE_MODE_DLSS && upscaler && upscaler->isAvailable();
}

bool Renderer::isFXAAEnabled()
{
	return GFXOPTIONS(anti_aliasing).fxaa && !isUpscalerActive();
}

bool Renderer::isShadowMapsEnabled()
{
	return GFXOPTIONS(shadows).enabled && !isRTXDIEnabled();
}

bool Renderer::isRayTracedShadowsEnabled()
{
	return engine_ray_tracing && isShadowMapsEnabled() && GFXOPTIONS(shadows).ray_traced;
}

bool Renderer::isRTXDIEnabled()
{
	return engine_ray_tracing && GFXOPTIONS(lighting).direct == DIRECT_LIGHTING_RTXDI;
}

bool Renderer::isDDGIEnabled()
{
	return engine_ray_tracing && GFXOPTIONS(ddgi).enabled;
}

bool Renderer::isScreenshotFrame()
{
	return render_screenshot && screenshot_frames_waited >= render_screenshot_frames;
}
