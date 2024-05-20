#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"


class DebugRenderer : public RendererBase
{
public:
	struct PresentUBO
	{
		uint32_t present_mode = 0;
		uint32_t composite_final_tex_id = 0;
		uint32_t albedo_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t position_tex_id = 0;
		uint32_t light_diffuse_id = 0;
		uint32_t light_specular_id = 0;
		uint32_t brdf_lut_id = 0;
		uint32_t ssao_id = 0;
	} ubo;

	DebugRenderer();
	virtual ~DebugRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;

private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
};

