#pragma once
#include "Upscaler.h"

class DLSSUpscaler : public Upscaler
{
public:
	void init();
	void shutdown();

	bool isAvailable() const override { return available; }
	glm::ivec2 getRenderResolution(glm::ivec2 output_resolution) override;
	void freeResources() override;
	void evaluate(RHICommandList *cmd_list, const UpscalerInputs &inputs) override;

private:
	bool available = false;
};
