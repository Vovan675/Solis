#pragma once

#include "RendererBase.h"
#include "Mesh.h"
#include "RHI/RHITexture.h"
#include "Camera.h"
#include "Material.h"

class MeshRenderer : public RendererBase
{
public:
	MeshRenderer(Ref<Engine::Mesh> mesh);
	virtual ~MeshRenderer();

	void fillCommandBuffer(RHICommandList *cmd_list) override;

	void setPosition(glm::vec3 pos) { position = pos; }
	void setRotation(glm::quat rot) { rotation = rot; }
	void setScale(glm::vec3 scale) { this->scale = scale; }
	void setMaterial(Material *mat) { this->mat = mat; }

private:
	struct PushConstant
	{
		alignas(16) glm::mat4 model;
	};

	RHIShaderRef vertex_shader;
	RHIShaderRef fragment_shader;

	RHITextureRef texture;
	Ref<Engine::Mesh> mesh;

	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.0f);
	Material *mat {};
};

