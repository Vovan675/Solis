#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"


class PostProcessingRenderer: public RendererBase
{
public:
	struct UBO
	{
		uint32_t composite_final_tex_id = 0;
		float use_vignette = 1;
		float vignette_radius = 0.7;
		float vignette_smoothness = 0.2;
	} ubo;

	PostProcessingRenderer();
	virtual ~PostProcessingRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void renderImgui();
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	uint32_t preview_index = 0;
};

