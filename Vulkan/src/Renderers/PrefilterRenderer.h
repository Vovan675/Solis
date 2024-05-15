#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class PrefilterRenderer: public RendererBase
{
public:
	struct PushConstantVert
	{
		glm::mat4 mvp = glm::mat4(1.0f);
	} constants_vert;

	struct PushConstantFrag
	{
		float roughness = 0;
	} constants_frag;

	PrefilterRenderer();
	virtual ~PrefilterRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	std::shared_ptr<Texture> cube_texture;
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::shared_ptr<Engine::Mesh> mesh;
};

