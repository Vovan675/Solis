#pragma once
#include <vector>
#include "RHI/RHITexture.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIBuffer.h"
#include "Material.h"
#include "BindlessResources.h"
#include "Camera.h"
#include "Assets/Asset.h"
#include <unordered_set>

enum RENDER_TARGETS
{
	RENDER_TARGET_TEXTURES_COUNT
};

struct DebugTime
{
	uint32_t index = 0;
	std::string name;
};

struct RendererDebugInfo
{
	size_t descriptors_count = 0;
	size_t descriptor_bindings_count = 0;
	size_t descriptors_max_offset = 0;
	size_t drawcalls = 0;
	std::vector<DebugTime> times;
};

struct RenderBatch
{
	std::shared_ptr<Engine::Mesh> mesh;
	std::shared_ptr<Material> material;
	glm::mat4 world_transform;
	bool camera_visible;
};

class Renderer
{
private:
	struct DefaultUniforms;
public:
	Renderer() = delete;

	static void init();
	static void shutdown();
	static void recreateScreenResources();
	static void setViewportSize(glm::ivec2 size);
	static glm::ivec2 getViewportSize() { return viewport_size; }
	static void beginFrame(unsigned int current_frame_in_flight, unsigned int current_image_index);
	static void endFrame(unsigned int image_index);
	static void deleteResources(unsigned int frame_in_flight);
	
	static RendererDebugInfo getDebugInfo() { return prev_debug_info; };

	static uint32_t beginTimestamp();
	static void endTimestamp(uint32_t index);
	static float getTimestampTime(uint32_t index);

	static std::shared_ptr<RHITexture> getRenderTarget(RENDER_TARGETS rt) { return screen_resources[rt]; }
	static uint32_t getRenderTargetBindlessId(RENDER_TARGETS rt) { return gDynamicRHI->getBindlessResources()->getTextureIndex(screen_resources[rt].get()); }

	static void setShadersAccelerationStructure(std::vector<std::shared_ptr<RHIShader>> shaders,
												VkAccelerationStructureKHR *acceleration_structure, unsigned int binding);

	static void setShadersStorageBuffer(std::vector<std::shared_ptr<RHIShader>> shaders,
										unsigned int binding, void *params_struct, size_t params_size);

	static void setShadersStorageBuffer(std::vector<std::shared_ptr<RHIShader>> shaders, unsigned int binding, std::shared_ptr<RHIBuffer> buffer);

	static void setShadersUniformBuffer(std::vector<std::shared_ptr<RHIShader>> shaders,
										unsigned int binding, void* params_struct, size_t params_size);

	static void setShadersTexture(std::vector<std::shared_ptr<RHIShader>> shaders,
										unsigned int binding, std::shared_ptr<RHITexture> texture, int mip = -1, int face = -1, bool is_uav = false);

	static void bindShadersDescriptorSets(std::vector<std::shared_ptr<RHIShader>> shaders, RHICommandList *cmd_list, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);

	// Default Uniforms
	static void setCamera(std::shared_ptr<Camera> camera) { Renderer::camera = camera; }
	static void updateDefaultUniforms(float delta_time);
	static const DefaultUniforms getDefaultUniforms();

	static int getCurrentFrameInFlight() { return current_frame_in_flight; }
	static int getCurrentImageIndex() { return current_image_index; }
	static uint32_t getCurrentFrame() { return current_frame; }

	static void deleteResource(RESOURCE_TYPE type, void *resource) { deletion_queue[current_frame_in_flight].push_back(std::make_pair(type, resource)); }

	static void addDrawCalls(size_t count) { debug_info.drawcalls += count; }
	static void addDebugTime(uint32_t index, std::string name) { debug_info.times.push_back({index, name}); }
private:
	
	static RendererDebugInfo prev_debug_info;
	static RendererDebugInfo debug_info;

public:
	static std::array<std::shared_ptr<RHITexture>, RENDER_TARGET_TEXTURES_COUNT> screen_resources;
private:
	friend class VulkanDynamicRHI;
	friend class DX12DynamicRHI;

	struct DefaultUniforms
	{
		glm::mat4 view;
		glm::mat4 iview;
		glm::mat4 projection;
		glm::mat4 iprojection;
		glm::vec4 camera_position;
		glm::vec4 swapchain_size;
		float time = 0;
		int frame = 0;
	};
	static DefaultUniforms default_uniforms;
	static std::shared_ptr<Camera> camera;

	static std::array<std::vector<std::pair<RESOURCE_TYPE, void *>>, MAX_FRAMES_IN_FLIGHT> deletion_queue;

	static int current_frame_in_flight;
	static int current_image_index;
	static uint64_t current_frame;
	static uint32_t timestamp_index;
	static glm::ivec2 viewport_size;
};

#include "RHI/Vulkan/VulkanUtils.h"

struct GPUScope
{
	GPUScope(const char *name, RHICommandList *cmd_list = nullptr, glm::vec3 color = glm::vec3(0.7, 0.7, 0.7))
	{
		timestamp = Renderer::beginTimestamp();
		Renderer::addDebugTime(timestamp, name);
		// TODO: fix, integrate tracy
		if (cmd_list)
		{
			this->cmd_list = cmd_list;
			cmd_list->beginDebugLabel(name, color);
		}
	}

	~GPUScope()
	{
		// TODO: fix, integrate tracy
		Renderer::endTimestamp(timestamp + 1);
		if (cmd_list)
		{
			cmd_list->endDebugLabel();
		}
	}

	uint32_t timestamp = 0;
	RHICommandList *cmd_list = nullptr;
};

#define GPU_PROFILE_SCOPE(name) GPUScope gpu_scope__LINE__(name)
#define GPU_PROFILE_SCOPE_FUNCTION() GPUScope gpu_scope__LINE__(__FUNCTION__)

#define GPU_SCOPE_FUNCTION(cmd_list) GPUScope gpu_scope__LINE__(__FUNCTION__, cmd_list)
#define GPU_SCOPE(name, cmd_list) GPUScope gpu_scope__LINE__(name, cmd_list)
