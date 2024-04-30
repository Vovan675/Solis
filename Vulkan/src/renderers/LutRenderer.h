#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class LutRenderer: public RendererBase
{
public:
	LutRenderer();
	virtual ~LutRenderer();

	void recreatePipeline();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	void updateUniformBuffer(uint32_t image_index) override;

private:
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_pool;

	std::shared_ptr<Pipeline> pipeline;
};

