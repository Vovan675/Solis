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

	void reloadShaders() override;

	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
	void updateUniformBuffer(uint32_t image_index) override;

private:
	DescriptorLayout descriptor_layout;

	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
};

