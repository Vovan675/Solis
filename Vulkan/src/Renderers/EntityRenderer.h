#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "Scene/Entity.h"
#include "RHI/Descriptors.h"
#include "MeshRenderer.h"
#include "Material.h"


class EntityRenderer: public RendererBase
{
public:
	struct ShadowUBO
	{
		alignas(16) glm::mat4 light_space_matrix;
		alignas(16) glm::mat4 model;
		alignas(16) glm::vec3 light_pos;
	};

	EntityRenderer();
	virtual ~EntityRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	void renderEntity(CommandBuffer &command_buffer, Entity entity, uint32_t image_index);
	void renderEntityShadow(CommandBuffer &command_buffer, Entity entity, uint32_t image_index, glm::mat4 light_space, glm::vec3 light_pos);

private:
	void render_entity(CommandBuffer &command_buffer, Entity entity, uint32_t image_index);
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
	std::shared_ptr<Material> default_material;
};

