#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class PrefilterRenderer: public RendererBase
{
public:
	struct PushConstantFrag
	{
		float roughness = 0;
	} constants_frag;

	PrefilterRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer) override;

	std::shared_ptr<Texture> output_prefilter_texture;
	std::shared_ptr<Texture> cube_texture;
private:
	std::shared_ptr<Shader> compute_shader;
};

