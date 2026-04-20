#include "pch.h"
#include "Renderer.h"
#include "GlobalBufferCache.h"
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
	default_uniforms.view_projection = camera->getProj() * camera->getView();
	default_uniforms.camera_position = glm::vec4(camera->getPosition(), 1.0);
	default_uniforms.swapchain_size = glm::vec4(Renderer::getViewportSize(), 1.0f / glm::vec2(Renderer::getViewportSize()));
	default_uniforms.z_near = camera->getNear();
	default_uniforms.z_far = camera->getFar();
	default_uniforms.time += delta_time;
	default_uniforms.frame = (uint32_t)gDynamicRHI->getFrame();
	if (GlobalBufferCache::getGlobalMeshletGeometryBuffer())
		default_uniforms.global_meshlets_geometry_buffer_id = GlobalBufferCache::getGlobalMeshletGeometryBuffer()->getShaderResourceView()->getBindlessIndex();
	if (GlobalBufferCache::getGlobalMeshletLodGroupsBuffer())
		default_uniforms.global_meshlets_lod_groups_buffer_id = GlobalBufferCache::getGlobalMeshletLodGroupsBuffer()->getShaderResourceView()->getBindlessIndex();
	if (GlobalBufferCache::getGlobalLodNodesBuffer())
		default_uniforms.global_lod_nodes_buffer_id = GlobalBufferCache::getGlobalLodNodesBuffer()->getShaderResourceView()->getBindlessIndex();
}

const Renderer::DefaultUniforms Renderer::getDefaultUniforms()
{
	return default_uniforms;
}
