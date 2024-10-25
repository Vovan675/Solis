#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class IrradianceRenderer: public RendererBase
{
public:
	IrradianceRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer) override;

	std::shared_ptr<Texture> cube_texture;
	std::shared_ptr<Texture> output_irradiance_texture;
private:
	std::shared_ptr<Shader> compute_shader;
};

