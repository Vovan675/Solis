#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"


class DebugRenderer : public RendererBase
{
public:
	struct PresentUBO
	{
		uint32_t present_mode = 0;
		uint32_t composite_final_tex_id = 0;
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t position_tex_id = 0;
		uint32_t brdf_lut_id = 0;
	} ubo;

	DebugRenderer();
	virtual ~DebugRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void updateUniformBuffer(uint32_t image_index) override;

private:
	DescriptorLayout descriptor_layout;

	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::vector<VkDescriptorSet> descriptor_sets;

	std::vector<std::shared_ptr<Buffer>> uniform_buffers;
	std::vector<void *> uniform_buffers_mapped;

	uint32_t preview_index = 0;
};

