#pragma once
#include <vector>
#include "RHI/RHITexture.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIBuffer.h"
#include "ShaderStructs.h"
#include "Rendering/Material.h"
#include "Rendering/Mesh.h"
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

struct TransformComponent;
struct RenderObject
{
	TransformComponent *transform;
	Engine::Mesh *mesh;
	Material *material;
	Engine::MeshletFileView file_view;
};

class Renderer
{
public:
	struct DefaultUniforms
	{
		glm::mat4 view;
		glm::mat4 iview;
		glm::mat4 projection;
		glm::mat4 iprojection;
		glm::mat4 view_projection;
		glm::mat4 old_view_projection;
		glm::mat4 view_projection_unjittered;
		glm::vec4 camera_position;
		glm::vec4 render_resolution; // (w, h, 1/w, 1/h)
		glm::vec4 output_resolution; // (w, h, 1/w, 1/h)
		float z_near = 0;
		float z_far = 0;
		float time = 0;
		float upscale_factor = 1.0f;
		glm::vec2 jitter = glm::vec2(0.0f);
		uint32_t frame = 0;
		float sky_intensity = 1.0f;
		float camera_exposure = 1.0f;
		uint32_t global_meshlets_geometry_buffer_id;
		uint32_t global_meshlets_lod_groups_buffer_id;
		uint32_t materials_buffer_id;
		uint32_t instances_buffer_id;
		uint32_t meshes_buffer_id;
		uint32_t global_lod_nodes_buffer_id = 0;
		uint32_t tlas_id = 0;
		uint32_t ddgi_volume_buffer_id = 0;
		uint32_t lines_gpu_buffer_id = 0;
	};

	Renderer() = delete;

	static void init();
	static void shutdown();

	// Scene rendering resolution
	static void setRenderResolution(glm::ivec2 size);
	static glm::ivec2 getRenderResolution() { return render_resolution; }
	static int getRenderWidth() { return render_resolution.x; }
	static int getRenderHeight() { return render_resolution.y; }

	// Output viewport resolution (can be other than rendering for example when upscaling)
	static void setOutputResolution(glm::ivec2 size);
	static glm::ivec2 getOutputResolution() { return output_resolution; }
	static int getOutputWidth() { return output_resolution.x; }
	static int getOutputHeight() { return output_resolution.y; }

	static glm::vec2 getJitter() { return jitter; }
	static const glm::mat4 &getOldViewProjection() { return old_view_projection; }
	static void beginFrame();
	static void endFrame(unsigned int image_index);

	static RendererDebugInfo getDebugInfo() { return prev_debug_info; };

	static void setCamera(Camera *camera) { Renderer::camera = camera; }
	static Camera *getCamera() { return camera; }
	static void updateDefaultUniforms(float delta_time);
	static const DefaultUniforms getDefaultUniforms();

	static void addDrawCalls(size_t count) { debug_info.drawcalls += count; }

	static bool isUpscalerActive();
	static bool isFXAAEnabled();
	static bool isRayTracedShadowsEnabled();

private:
	friend class VulkanDynamicRHI;
	friend class DX12DynamicRHI;

	static RendererDebugInfo prev_debug_info;
	static RendererDebugInfo debug_info;
	static DefaultUniforms default_uniforms;
	static Camera *camera;
	static glm::ivec2 render_resolution;
	static glm::ivec2 output_resolution;
	static glm::vec2 jitter;
	static glm::mat4 old_view_projection;
	static glm::mat4 view_projection_unjittered;
};
