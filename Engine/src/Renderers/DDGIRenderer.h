#pragma once
#include "FrameGraph/FrameGraph.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "Rendering/Mesh.h"

class DDGIRenderer
{
public:
	struct DDGICascadeGPU
	{
		glm::vec4 min;
		glm::vec4 spacing;
	};

	struct DDGIVolumeGPU
	{
		glm::vec3 origin;
		glm::ivec4 size;
		glm::vec3 spacing;
		glm::vec3 sun_dir;
		glm::vec4 sun_color;
		glm::vec3 random_vector;
		float random_angle;
		float use_relocation;
		float use_classification;
		uint32_t cascades_count;
		DDGICascadeGPU cascades[5];
		uint32_t rays_per_probe;
		uint32_t probes_to_update_buffer_id;
		uint32_t ray_data_buffer_id;
		uint32_t distance_atlas_tex_id;
		uint32_t irradiance_atlas_tex_id;
		uint32_t metadata_atlas_tex_id;
		uint32_t getProbesCount() const { return size.x * size.y * size.z * cascades_count; }
	};

	DDGIRenderer();

	void addPasses(FrameGraph &fg, Ref<RayTracingScene> rt_scene);
	void addVisualizePass(FrameGraph &fg);

	DDGIVolumeGPU getVolume() const { return volume; }
	uint32_t getVolumeBufferId() const { return volume_buffer->getShaderResourceView()->getBindlessIndex(); }
private:
	eastl::vector<eastl::pair<const char *, const char *>> calculateDefines(eastl::vector<eastl::pair<const char *, const char *>> additional = {});

	void update_probes();

	void addTraceRaysPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene);
	void addUpdatePass(FrameGraph &fg);
	void addRelocationPass(FrameGraph &fg);
	void addResetRelocationPass(FrameGraph &fg);
	void addClassificationPass(FrameGraph &fg);
	void addResetClassificationPass(FrameGraph &fg);

	DDGIVolumeGPU volume;
	RHIBufferRef volume_buffer;

	eastl::vector<uint32_t> probes_to_update;
	RHIBufferRef probes_to_update_buffer;

	struct CascadeUpdateData
	{
		uint32_t last_local_index = 0;
	};
	eastl::vector<CascadeUpdateData> cascades_update {5};

	RHIBufferRef ray_data_buffer;
	RHITextureRef distance_atlas_texture;
	RHITextureRef irradiance_atlas_texture;
	RHITextureRef metadata_atlas_texture;

	Engine::Mesh *sphere_mesh;
	RHIShaderRef visualize_vertex_shader;
	RHIShaderRef visualize_fragment_shader;
};