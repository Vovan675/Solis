#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "Entity.h"
#include "RHI/Descriptors.h"
#include "MeshRenderer.h"


class EntityRenderer: public RendererBase
{
public:
	EntityRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Entity> mesh);
	virtual ~EntityRenderer();

	void recreatePipeline();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	void renderEntity(CommandBuffer &command_buffer, Entity *entity);

	void setPosition(glm::vec3 pos) { position = pos; }
	void setRotation(glm::quat rot) { rotation = rot; }
	void setScale(glm::vec3 scale) { this->scale = scale; }
	void setMaterial(Material mat) { this->mat = mat; }
	void updateUniformBuffer(uint32_t image_index) override;
private:
	VkDescriptorSetLayout descriptor_set_layout;

	std::shared_ptr<Pipeline> pipeline;
	std::vector<VkDescriptorSet> image_descriptor_sets;
	std::vector<std::shared_ptr<Buffer>> image_uniform_buffers;
	std::vector<void *> image_uniform_buffers_mapped;

	std::vector<std::shared_ptr<Buffer>> material_uniform_buffers;
	std::vector<void *> material_uniform_buffers_mapped;

	std::shared_ptr<Texture> texture;
	std::shared_ptr<Entity> entity;

	std::shared_ptr<Camera> camera;
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
	Material mat{};
};

