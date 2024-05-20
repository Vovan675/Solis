#pragma once
#include <vector>
#include "RHI/Texture.h"
#include "RHI/Shader.h"
#include "RHI/Buffer.h"
#include "RHI/VkWrapper.h"
#include "BindlessResources.h"
#include "Camera.h"

enum RENDER_TARGETS
{
	RENDER_TARGET_GBUFFER_ALBEDO,
	RENDER_TARGET_GBUFFER_NORMAL,
	RENDER_TARGET_GBUFFER_DEPTH_STENCIL,
	RENDER_TARGET_GBUFFER_POSITION, // TODO: Remove (get from depth in shaders)
	RENDER_TARGET_GBUFFER_SHADING, // R - metalness, G - roughness, B - specular, A - ?

	RENDER_TARGET_LIGHTING_DIFFUSE,
	RENDER_TARGET_LIGHTING_SPECULAR,

	RENDER_TARGET_SSAO_RAW,
	RENDER_TARGET_SSAO,

	RENDER_TARGET_COMPOSITE,

	RENDER_TARGET_TEXTURES_COUNT
};

struct RendererDebugInfo
{
	size_t descriptors_count;
	size_t descriptor_bindings_count;
	size_t descriptors_max_offset;
};

class Renderer
{
public:
	Renderer() = delete;

	static void init();
	static void recreateScreenResources();
	static void endFrame(unsigned int image_index);
	
	static RendererDebugInfo getDebugInfo() { return debug_info; };

	static std::shared_ptr<Texture> getRenderTarget(RENDER_TARGETS rt) { return screen_resources[rt]; }
	static uint32_t getRenderTargetBindlessId(RENDER_TARGETS rt) { return BindlessResources::getTextureIndex(screen_resources[rt].get()); }

	static void setShadersUniformBuffer(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader,
										unsigned int binding, void* params_struct, size_t params_size, unsigned int image_index);

	static void setShadersTexture(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader,
										unsigned int binding, std::shared_ptr<Texture> texture, unsigned int image_index, int mip = -1, int face = -1);

	static void bindShadersDescriptorSets(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader, CommandBuffer &command_buffer, VkPipelineLayout pipeline_layout, unsigned int image_index);

	// Default Uniforms
	static void setCamera(std::shared_ptr<Camera> camera) { Renderer::camera = camera; }
	static void updateDefaultUniforms(unsigned int image_index);

	static VkDescriptorSetLayout getDefaultDescriptorLayout() { return default_descriptor_layout.layout; }
private:
	static void ensureDescriptorsAllocated(DescriptorLayout descriptor_layout, size_t descriptor_hash, size_t offset);
	
	static RendererDebugInfo debug_info;

	static std::array<std::shared_ptr<Texture>, RENDER_TARGET_TEXTURES_COUNT> screen_resources;

	struct PerFrameDescriptor
	{
		std::vector<VkDescriptorSet> descriptor_per_offset;
	};

	struct PerFrameDescriptorBinding
	{
		std::vector<std::pair<std::shared_ptr<Buffer>, void *>> bindings_per_offset;
	};
	// Use descriptor layout hash
	static std::unordered_map<size_t, std::array<PerFrameDescriptor, MAX_FRAMES_IN_FLIGHT>> descriptors;
	static std::unordered_map<size_t, std::array<size_t, MAX_FRAMES_IN_FLIGHT>> descriptors_offset;

	// Use descriptor layout & binding hash
	static std::unordered_map<size_t, std::array<PerFrameDescriptorBinding, MAX_FRAMES_IN_FLIGHT>> descriptor_bindings;

	struct DefaultUniforms
	{
		glm::mat4 view;
		glm::mat4 iview;
		glm::mat4 projection;
		glm::mat4 iprojection;
		glm::vec4 camera_position;
		glm::vec4 swapchain_size;
	};
	static DefaultUniforms default_uniforms;
	static DescriptorLayout default_descriptor_layout;
	static std::shared_ptr<Camera> camera;
};

