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
		uint32_t shading_tex_id = 0;
		uint32_t brdf_lut_tex_id = 0;
		uint32_t ssao_tex_id = 0;
	} ubo;

	DefferedCompositeRenderer();
	virtual ~DefferedCompositeRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer) override;

	void renderImgui();
private:
};

