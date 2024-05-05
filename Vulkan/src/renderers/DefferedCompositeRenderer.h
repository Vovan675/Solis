#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"

class DefferedCompositeRenderer: public RendererBase
{
public:
	struct UBO
	{
		uint32_t lighting_diffuse_tex_id = 0;
		uint32_t lighting_specular_tex_id = 0;
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
	} ubo;

	std::shared_ptr<Texture> irradiance_cubemap;

	DefferedCompositeRenderer();
	virtual ~DefferedCompositeRenderer();

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

	void renderImgui();
private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;

	uint32_t preview_index = 0;
};

