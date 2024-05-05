#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"


class DefferedLightingRenderer: public RendererBase
{
public:
	struct UBO
	{
		uint32_t position_tex_id = 0;
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t shading_tex_id = 0;
	} ubo;

	// TODO: use uniform buffer, move to Renderer::setDefaultResources
	struct PushConstant
	{
		glm::vec4 cam_pos;
	} constants;

	DefferedLightingRenderer();
	virtual ~DefferedLightingRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void renderImgui();
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	uint32_t preview_index = 0;
};

