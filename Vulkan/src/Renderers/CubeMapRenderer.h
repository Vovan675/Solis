#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class CubeMapRenderer: public RendererBase
{
public:
	CubeMapRenderer();
	virtual ~CubeMapRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer) override;
	std::shared_ptr<Texture> cube_texture;
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::shared_ptr<Engine::Mesh> mesh;
};

