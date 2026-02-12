#pragma once
#include <vector>
#include "RHI/RHITexture.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIBuffer.h"
#include "ShaderStructs.h"
#include "Rendering/Material.h"
#include "RHI/BindlessResources.h"
#include "Utils/Camera.h"
#include "Assets/Asset.h"
#include <unordered_set>

struct DebugTime
{
	uint32_t index = 0;
	eastl::string name;
};

struct RendererDebugInfo
{
	size_t descriptors_count = 0;
	size_t descriptor_bindings_count = 0;
	size_t descriptors_max_offset = 0;
	size_t drawcalls = 0;
};

class Renderer
{
private:
	struct DefaultUniforms;
public:
	Renderer() = delete;

	static void init();
	static void shutdown();
	static void setViewportSize(glm::ivec2 size);
	static glm::ivec2 getViewportSize() { return viewport_size; }
	static int getViewportWidth() { return viewport_size.x; }
	static int getViewportHeight() { return viewport_size.y; }
	static void beginFrame();
	static void endFrame(unsigned int image_index);
	
	static RendererDebugInfo getDebugInfo() { return prev_debug_info; };

	// Default Uniforms
	static void setCamera(Camera *camera) { Renderer::camera = camera; }
	static Camera *getCamera() { return camera; }
	static void updateDefaultUniforms(float delta_time);
	static const DefaultUniforms getDefaultUniforms();

	static void addDrawCalls(size_t count) { debug_info.drawcalls += count; }
private:
	
	static RendererDebugInfo prev_debug_info;
	static RendererDebugInfo debug_info;

private:
	friend class VulkanDynamicRHI;
	friend class DX12DynamicRHI;

	struct DefaultUniforms
	{
		glm::mat4 view;
		glm::mat4 iview;
		glm::mat4 projection;
		glm::mat4 iprojection;
		glm::mat4 view_projection;
		glm::vec4 camera_position;
		glm::vec4 swapchain_size;
		float z_near = 0;
		float z_far = 0;
		float time = 0;
		uint32_t frame = 0;
		uint32_t global_vertex_buffer_id;
		uint32_t global_meshlets_vertex_buffer_id;
		uint32_t global_meshlets_triangles_buffer_id;
		uint32_t global_meshlets_lod_groups_buffer_id;
		uint32_t materials_buffer_id;
		uint32_t instances_buffer_id;
		uint32_t meshes_buffer_id;
		uint32_t meshlets_buffer_id;
		uint32_t tlas_id = 0;
		uint32_t ddgi_volume_buffer_id = 0;
		uint32_t lines_gpu_buffer_id = 0;
	};
	static DefaultUniforms default_uniforms;
	static Camera *camera;

	static glm::ivec2 viewport_size;
};
