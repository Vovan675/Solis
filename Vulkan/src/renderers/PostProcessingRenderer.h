#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"


class PostProcessingRenderer: public RendererBase
{
public:
	struct UBO
	{
		uint32_t composite_final_tex_id = 0;
		float use_vignette = 1;
		float vignette_radius = 0.7;
		float vignette_smoothness = 0.2;
	} ubo;

	PostProcessingRenderer();
	virtual ~PostProcessingRenderer();

	void recreatePipeline();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void updateUniformBuffer(uint32_t image_index) override;
	void renderImgui();
private:
	std::shared_ptr<Pipeline> pipeline;

	VkDescriptorSetLayout descriptor_set_layout;
	std::vector<VkDescriptorSet> descriptor_sets;

	std::vector<std::shared_ptr<Buffer>> uniform_buffers;
	std::vector<void *> uniform_buffers_mapped;

	uint32_t preview_index = 0;
};

