#include "pch.h"
#include "Renderer.h"
#include "RHI/BindlessResources.h"


RendererDebugInfo Renderer::prev_debug_info = {};
RendererDebugInfo Renderer::debug_info = {};

Renderer::DefaultUniforms Renderer::default_uniforms;
Camera *Renderer::camera;

glm::ivec2 Renderer::viewport_size;

void Renderer::init()
{
	viewport_size = gDynamicRHI->getSwapchainTexture(0)->getSize();
}

void Renderer::shutdown()
{
}

void Renderer::setViewportSize(glm::ivec2 size)
{
	if (viewport_size == size)
		return;
	viewport_size = size;
}

void Renderer::beginFrame()
{
	// Update debug info
	prev_debug_info = debug_info;
	debug_info = RendererDebugInfo{};
}

void Renderer::endFrame(unsigned int image_index)
{
}

void Renderer::updateDefaultUniforms(float delta_time)
{
	default_uniforms.view = camera->getView();
	default_uniforms.iview = glm::inverse(camera->getView());
	default_uniforms.projection = camera->getProj();
	default_uniforms.iprojection = glm::inverse(camera->getProj());
	default_uniforms.camera_position = glm::vec4(camera->getPosition(), 1.0);
	default_uniforms.swapchain_size = glm::vec4(Renderer::getViewportSize(), 1.0f / glm::vec2(Renderer::getViewportSize()));
	default_uniforms.time += delta_time;
	default_uniforms.frame = (uint32_t)gDynamicRHI->getFrame();
}

const Renderer::DefaultUniforms Renderer::getDefaultUniforms()
{
	return default_uniforms;
}
