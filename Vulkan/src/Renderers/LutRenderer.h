#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class LutRenderer: public RendererBase
{
public:
	LutRenderer();
	virtual ~LutRenderer();

	void fillCommandBuffer(CommandBuffer &command_buffer) override;

private:
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
};

