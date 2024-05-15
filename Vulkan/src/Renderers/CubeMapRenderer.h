#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class CubeMapRenderer: public RendererBase
{
public:
	CubeMapRenderer(std::shared_ptr<Camera> cam);
	virtual ~CubeMapRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	std::shared_ptr<Texture> cube_texture;
private:
	// TODO: move to Renderer::setDefaultResources
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
		alignas(16) glm::vec3 camera_position;
	};

	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::shared_ptr<Engine::Mesh> mesh;

	std::shared_ptr<Camera> camera;
};

