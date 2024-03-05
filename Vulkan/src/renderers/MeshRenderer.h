#pragma once

#include "RendererBase.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Texture.h"
#include "Camera.h"

class MeshRenderer : public RendererBase
{
public:
	MeshRenderer(std::shared_ptr<Camera> cam);
	virtual ~MeshRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	void updateUniformBuffer(uint32_t image_index) override;
private:
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_pool;

	std::shared_ptr<Pipeline> pipeline;
	std::vector<VkDescriptorSet> image_descriptor_sets;
	std::vector<std::shared_ptr<Buffer>> image_uniform_buffers;
	std::vector<void *> image_uniform_buffers_mapped;

	std::shared_ptr<Texture> texture;
	std::shared_ptr<Engine::Mesh> mesh;

	std::shared_ptr<Camera> camera;
};

