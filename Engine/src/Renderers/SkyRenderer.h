#pragma once

#include "RendererBase.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Core/Variables.h"
#include "Rendering/ShaderStructs.h"

class SkyRenderer: public RendererBase
{
public:
	SkyRenderer();
	~SkyRenderer() {}
	void addProceduralPasses(FrameGraph &fg, const eastl::vector<LightGPU> &lights, uint32_t sun_light_index);
	void addCompositePasses(FrameGraph &fg);

	bool isDirty() const { return is_dirty; }

	struct Uniforms
	{
		glm::vec3 sun_direction = glm::vec3(1, 0.7, 0);
		glm::mat4 mvp;
		float sky_luminance_scale = 1000.0f;
	} procedural_uniforms;

	RHITextureRef cube_texture;
private:
	bool update_resources();

	RHIShaderRef vertex_shader;
	RHIShaderRef fragment_shader;
	RHIShaderRef vertex_procedural_shader;
	RHIShaderRef fragment_procedural_shader;

	Engine::Mesh *mesh;
	SkyMode created_mode = SKY_MODE_CUBEMAP;
	AssetReference created_hdri;

	Uniforms prev_uniform;

	uint32_t sun_light_index = INVALID_LIGHT_INDEX;

	bool is_dirty = false;
};

