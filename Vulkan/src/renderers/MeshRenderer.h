#pragma once

#include "RendererBase.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Texture.h"
#include "Camera.h"
#include "Descriptors.h"

class MeshRenderer : public RendererBase
{
public:
	MeshRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Engine::Mesh> mesh);
	virtual ~MeshRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void setPosition(glm::vec3 pos) { position = pos; }
	void setRotation(glm::quat rot) { rotation = rot; }
	void setScale(glm::vec3 scale) { this->scale = scale; }
	void updateUniformBuffer(uint32_t image_index) override;
private:
	VkDescriptorSetLayout descriptor_set_layout;

	std::shared_ptr<Pipeline> pipeline;
	std::vector<VkDescriptorSet> image_descriptor_sets;
	std::vector<std::shared_ptr<Buffer>> image_uniform_buffers;
	std::vector<void *> image_uniform_buffers_mapped;

	std::shared_ptr<Texture> texture;
	std::shared_ptr<Engine::Mesh> mesh;

	std::shared_ptr<Camera> camera;
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
};

