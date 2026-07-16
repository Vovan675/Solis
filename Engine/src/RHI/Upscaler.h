#pragma once
#include "RHI/RHIDefinitions.h"

class RHICommandList;

struct UpscalerInputs
{
	RHITextureRef color_input;
	RHITextureRef color_output;
	RHITextureRef depth;
	RHITextureRef motion_vectors;

	bool reset_history = false;
};

class Upscaler
{
public:
	virtual ~Upscaler() = default;

	virtual bool isAvailable() const = 0;
	virtual glm::ivec2 getRenderResolution(glm::ivec2 output_resolution) = 0;
	virtual void freeResources() {}
	virtual void evaluate(RHICommandList *cmd_list, const UpscalerInputs &inputs) = 0;
};
