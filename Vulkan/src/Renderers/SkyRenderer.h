#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

enum SKY_MODE
{
	SKY_MODE_CUBEMAP,
	SKY_MODE_PROCEDURAL,
};

class SkyRenderer: public RendererBase
{
public:
	SkyRenderer();
	virtual ~SkyRenderer();

	void renderCubemap(CommandBuffer &command_buffer);
	void fillCommandBuffer(CommandBuffer &command_buffer) override;
	void renderImgui();

	void setMode(SKY_MODE mode);
	SKY_MODE getMode() const { return mode; }

	bool isDirty();

	struct Uniforms
	{
		glm::vec3 sun_direction = glm::vec3(1, 0.7, 0);
	} procedural_uniforms;

	std::shared_ptr<Texture> cube_texture;
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	std::shared_ptr<Engine::Mesh> mesh;
	SKY_MODE mode;

	Uniforms prev_uniform;
};

