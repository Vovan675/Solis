#pragma once

#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"

enum SKY_MODE
{
	SKY_MODE_CUBEMAP,
	SKY_MODE_PROCEDURAL,
};

class SkyRenderer: public RendererBase
{
public:
	SkyRenderer();
	~SkyRenderer() {}
	void addProceduralPasses(FrameGraph &fg);
	void addCompositePasses(FrameGraph &fg);
	void renderImgui();

	void setMode(SKY_MODE mode);
	SKY_MODE getMode() const { return mode; }

	bool isDirty();

	struct Uniforms
	{
		glm::vec3 sun_direction = glm::vec3(1, 0.7, 0);
	} procedural_uniforms;

	RHITextureRef cube_texture;
private:
	RHIShaderRef vertex_shader;
	RHIShaderRef fragment_shader;

	Engine::Mesh *mesh;
	SKY_MODE mode;

	Uniforms prev_uniform;

	bool is_force_dirty = false;
};

