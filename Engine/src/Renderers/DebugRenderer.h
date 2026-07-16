#pragma once
#include "RendererBase.h"
#include "RHI/RHIPipeline.h"
#include "Rendering/Mesh.h"
#include "RHI/RHITexture.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class DebugRenderer : public RendererBase
{
public:
	struct PresentUBO
	{
		uint32_t present_mode = 0;
		uint32_t composite_final_tex_id = 0;
		uint32_t albedo_tex_id = 0;
		uint32_t shading_tex_id = 0;
		uint32_t normal_tex_id = 0;
		uint32_t depth_tex_id = 0;
		uint32_t light_diffuse_id = 0;
		uint32_t light_specular_id = 0;
		uint32_t brdf_lut_id = 0;
		uint32_t ssao_id = 0;
		uint32_t ddgi_distance_tex_id = 0;
		uint32_t ddgi_irradiance_tex_id = 0;
		uint32_t ddgi_metadata_tex_id = 0;
		uint32_t hiz_tex_id = 0;
		uint32_t debug_tex_id = 0;
		uint32_t overdraw_tex_id = 0;
		uint32_t motion_vectors_tex_id = 0;
	} ubo;

	DebugRenderer();
	virtual ~DebugRenderer();

	bool isEnabled() const;

	RHIBufferRef getLinesGpuBuffer() { return lines_gpu_buffer; }

	void addPasses(FrameGraph &fg);

	void addLine(glm::vec3 p0, glm::vec3 p1, glm::vec3 color = glm::vec3(0, 0, 0))
	{
		if (!isEnabled()) return;

		LineVertex &vertex_0 = vertices[lines_index_count];
		LineVertex &vertex_1 = vertices[lines_index_count + 1];
		vertex_0.pos = glm::vec4(p0, 0.0);
		vertex_0.color = color;

		vertex_1.pos = glm::vec4(p1, 0.0);
		vertex_1.color = color;
		lines_index_count += 2;
	}

	void addBox(glm::vec3 half_extents, glm::mat4 transform, glm::vec3 color = glm::vec3(0, 0, 0));
	void addBoundBox(BoundBox bbox);
	void addFrustum(glm::mat4 frustum, glm::vec3 color = glm::vec3(0, 0, 0));
	void addSphere(glm::vec3 center, float radius, int segments = 32, glm::vec3 color = glm::vec3(0, 0, 0));

	eastl::vector<glm::vec3> addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments, glm::vec3 color = glm::vec3(0, 0, 0));
	void addArrow(glm::vec3 p0, glm::vec3 p1, float arrow_size);
private:
	void addTextureDebugPass(FrameGraph &fg);
	void addVisualizerPass(FrameGraph &fg);

	RHIShaderRef vertex_shader_lines;
	RHIShaderRef fragment_shader_lines;

	RHIShaderRef vertex_shader_gpu_lines;

	struct LineVertex
	{
		glm::vec4 pos;
		glm::vec3 color;
	};
	LineVertex *vertices;
	RHIBufferRef lines_vertex_buffer;

	RHIBufferRef lines_gpu_buffer;
	RHIBufferRef lines_draw_args_buffer;

	int lines_index_count = 0;
};

