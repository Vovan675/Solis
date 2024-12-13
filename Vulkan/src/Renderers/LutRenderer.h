#pragma once

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"

class LutRenderer: public RendererBase
{
public:
	void fillCommandBuffer(CommandBuffer &command_buffer) override;
};

