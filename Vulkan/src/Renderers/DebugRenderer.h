#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"
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
	} ubo;

	DebugRenderer();
	virtual ~DebugRenderer();

	void addPasses(FrameGraph &fg);
	void renderLines(FrameGraph &fg);

	void addLine(glm::vec3 p0, glm::vec3 p1)
	{
		vertices[lines_index_count].pos = p0;
		vertices[lines_index_count + 1].pos = p1;
		lines_index_count += 2;
	}

	void addBox(glm::vec3 half_extents, glm::mat4 transform);
	void addBoundBox(BoundBox bbox);
	void addFrustum(glm::mat4 frustum);

	std::vector<glm::vec3> addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments);
	void addArrow(glm::vec3 p0, glm::vec3 p1, float arrow_size);
private:
	std::shared_ptr<Shader> vertex_shader_lines;
	std::shared_ptr<Shader> fragment_shader_lines;

	Engine::Vertex *vertices;
	std::shared_ptr<Buffer> lines_index_buffer;
	std::shared_ptr<Buffer> lines_vertex_buffer;

	int lines_index_count = 0;
};

