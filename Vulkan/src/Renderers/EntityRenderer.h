#pragma once

#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "Scene/Entity.h"
#include "MeshRenderer.h"
#include "Material.h"


class EntityRenderer: public RendererBase
{
public:
	struct ShadowUBO
	{
		glm::mat4 light_space_matrix;
		glm::vec4 light_pos;
		float z_far;
	};

	struct ShadowPushConstact
	{
		glm::mat4 model;
	};

	EntityRenderer();
	virtual ~EntityRenderer();

	void renderEntity(RHICommandList *cmd_list, Entity entity);
	void renderEntityShadow(RHICommandList *cmd_list, glm::mat4 &transform_matrix, MeshRendererComponent &mesh_renderer);

private:
	void render_entity(RHICommandList *cmd_list, Entity entityx);
	std::shared_ptr<RHIShader> vertex_shader;
	std::shared_ptr<RHIShader> fragment_shader;
	std::shared_ptr<Material> default_material;
};

