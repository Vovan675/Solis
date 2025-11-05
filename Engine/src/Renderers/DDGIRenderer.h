#pragma once
#include "FrameGraph/FrameGraph.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "Rendering/Mesh.h"

class DDGIRenderer
{
public:
	struct DDGIVolumeGPU
	{
		glm::vec3 origin;
		glm::ivec3 size;
		glm::vec3 spacing;
		glm::vec3 sun_dir;
		glm::vec4 sun_color;
		glm::vec3 random_vector;
		float random_angle;
		float use_relocation;
		float use_classification;
		uint32_t rays_per_probe;
		uint32_t ray_data_buffer_id;
		uint32_t distance_altas_tex_id;
		uint32_t irradiance_altas_tex_id;
		uint32_t metadata_altas_tex_id;
		uint32_t getProbesCount() const { return size.x * size.y * size.z; }
	};

	DDGIRenderer();

	void addPasses(FrameGraph &fg, Ref<RayTracingScene> rt_scene);
	void addVisualizePass(FrameGraph &fg);
	void renderImgui();

	DDGIVolumeGPU getVolume() const { return volume; }
	uint32_t getVolumeBufferId() const { return gDynamicRHI->getBindlessResources()->addBuffer(volume_buffer); }
private:
	void addTraceRaysPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene);
	void addUpdatePass(FrameGraph &fg);
	void addRelocationPass(FrameGraph &fg);
	void addResetRelocationPass(FrameGraph &fg);
	void addClassificationPass(FrameGraph &fg);
	void addResetClassificationPass(FrameGraph &fg);

	bool use_relocation = false;
	bool use_classification = false;

	struct VisualizationSettings
	{
		// 0 - irradiance, 1 - distance, 2 - state, 3 - state not disabled
		int mode = 0;
	} visualization_settings;

	DDGIVolumeGPU volume;
	RHIBufferRef volume_buffer;

	RHIBufferRef ray_data_buffer;
	RHITextureRef distance_atlas_texture;
	RHITextureRef irradiance_atlas_texture;
	RHITextureRef metadata_atlas_texture;

	Engine::Mesh *sphere_mesh;
	RHIShaderRef visualize_vertex_shader;
	RHIShaderRef visualize_fragment_shader;
};