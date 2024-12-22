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
		glm::vec4 light_pos;
		float z_far;
		float padding[3];
	};

	struct ShadowPushConstact
	{
		alignas(16) glm::mat4 model;
	};

	EntityRenderer();
	virtual ~EntityRenderer();

	void renderEntity(CommandBuffer &command_buffer, Entity entity);
	void renderEntityShadow(CommandBuffer &command_buffer, glm::mat4 &transform_matrix, MeshRendererComponent &mesh_renderer);

private:
	void render_entity(CommandBuffer &command_buffer, Entity entityx);
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
	std::shared_ptr<Material> default_material;
};

