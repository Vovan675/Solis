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
	struct ShadowUBO
	{
		alignas(16) glm::mat4 light_space_matrix;
		alignas(16) glm::mat4 model;
		alignas(16) glm::vec4 light_pos;
	};

	EntityRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Entity> mesh);
	virtual ~EntityRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	void renderEntity(CommandBuffer &command_buffer, Entity *entity, uint32_t image_index);
	void renderEntityShadow(CommandBuffer &command_buffer, Entity *entity, uint32_t image_index, glm::mat4 light_space, glm::vec4 light_pos);

	void setPosition(glm::vec3 pos) { position = pos; }
	void setRotation(glm::quat rot) { rotation = rot; }
	void setScale(glm::vec3 scale) { entity->transform.scale = scale; }
	void setTransform(glm::mat4 transform)
	{
		entity->transform.local_model_matrix = transform;
		entity->updateTransform();
	}
	void setMaterial(Material mat) { this->mat = mat; }

	std::shared_ptr<Entity> getEntity() const { return entity; }
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::shared_ptr<Texture> texture;
	std::shared_ptr<Entity> entity;

	std::shared_ptr<Camera> camera;
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
	Material mat{};
};

