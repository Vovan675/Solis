#pragma once
#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphData.h"

class SSAORenderer: public RendererBase
{
public:
	struct UBO_RAW
	{
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t noise_tex_id = 0;
		alignas(16) glm::vec4 kernel[64];
		int samples = 64;
		float sample_radius = 0.5f;
	} ubo_raw_pass;

	struct UBO_BLUR
	{
		uint32_t raw_tex_id = 0;
	} ubo_blur_pass;

	SSAORenderer();

	void addPasses(FrameGraph &fg);
private:
	RHITextureRef ssao_noise;
	eastl::vector<glm::vec4> ssao_kernel;

	RHIShaderRef fragment_shader_raw;
	RHIShaderRef fragment_shader_blur;
};

