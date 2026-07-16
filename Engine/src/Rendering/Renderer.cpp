#include "pch.h"
#include "Renderer.h"
#include "GlobalBufferCache.h"
#include "RHI/BindlessResources.h"
#include "Core/Variables.h"
#include "Utils/Math.h"

RendererDebugInfo Renderer::prev_debug_info = {};
RendererDebugInfo Renderer::debug_info = {};

Renderer::DefaultUniforms Renderer::default_uniforms;
Camera *Renderer::camera;

glm::ivec2 Renderer::render_resolution;
glm::ivec2 Renderer::output_resolution;
glm::vec2 Renderer::jitter;
glm::mat4 Renderer::old_view_projection = glm::mat4(1.0f);
glm::mat4 Renderer::view_projection_unjittered = glm::mat4(1.0f);

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
}

void Renderer::endFrame(unsigned int image_index)
{
}

void Renderer::updateDefaultUniforms(float delta_time)
{
	old_view_projection = view_projection_unjittered;

	glm::mat4 view = camera->getView();
	glm::mat4 projection = camera->getProj();
	view_projection_unjittered = projection * view;

	static bool old_view_projection_set = false;
	if (!old_view_projection_set)
	{
		old_view_projection = view_projection_unjittered;
		old_view_projection_set = true;
	}

	float upscale_factor = (float)getOutputWidth() / getRenderWidth();

	jitter = glm::vec2(0.0f);
	if (render_upscale_mode == UPSCALE_MODE_DLSS)
	{
		// From DLSS guide section 3.7.1.1
		int phase_count = glm::max(int(8.0f * upscale_factor * upscale_factor), 1);

		uint32_t index = gDynamicRHI->getFrame() % phase_count + 1;
		jitter = glm::vec2(Math::halton2D(index) - 0.5f);

		projection[2][0] += 2.0f * jitter.x / getRenderWidth();
		projection[2][1] -= 2.0f * jitter.y / getRenderHeight();
	}

	default_uniforms.view = view;
	default_uniforms.iview = glm::inverse(view);
	default_uniforms.projection = projection;
	default_uniforms.iprojection = glm::inverse(projection);
	default_uniforms.view_projection = projection * view;
	default_uniforms.old_view_projection = old_view_projection;
	default_uniforms.view_projection_unjittered = view_projection_unjittered;
	default_uniforms.camera_position = glm::vec4(camera->getPosition(), 1.0);
	default_uniforms.render_resolution = glm::vec4(getRenderResolution(), 1.0f / glm::vec2(getRenderResolution()));
	default_uniforms.output_resolution = glm::vec4(getOutputResolution(), 1.0f / glm::vec2(getOutputResolution()));
	default_uniforms.z_near = camera->getNear();
	default_uniforms.z_far = camera->getFar();
	default_uniforms.time += delta_time;
	default_uniforms.upscale_factor = upscale_factor;
	default_uniforms.jitter = jitter;
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
