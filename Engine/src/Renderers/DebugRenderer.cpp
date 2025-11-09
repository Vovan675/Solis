#include "pch.h"
#include "DebugRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"

DebugRenderer::DebugRenderer()
{
	vertex_shader_lines = gDynamicRHI->createShader(L"shaders/debug_lines.hlsl", VERTEX_SHADER);
	fragment_shader_lines = gDynamicRHI->createShader(L"shaders/debug_lines.hlsl", FRAGMENT_SHADER);

	const size_t count = 1024;
	uint32_t indices[2048];
	for (size_t i = 0; i < 2048; i++)
	{
		indices[i] = i;
	}

	BufferDescription desc;
	desc.size = sizeof(uint32_t) * 2048;
	desc.use_staging_buffer = true;
	desc.usage = BufferUsage::INDEX_BUFFER;

	lines_index_buffer = gDynamicRHI->createBuffer(desc);
	lines_index_buffer->fill(indices);
	lines_index_buffer->setDebugName("Lines Index Buffer");

	vertices = new LineVertex[2048];
	
	desc.size = sizeof(LineVertex) * 2048;
	desc.storage_stride = sizeof(LineVertex);
	desc.use_staging_buffer = false;
	desc.usage = BufferUsage::VERTEX_BUFFER;

	lines_vertex_buffer = gDynamicRHI->createBuffer(desc);
	lines_vertex_buffer->map((void **)&vertices);
	lines_vertex_buffer->setDebugName("Lines Vertex Buffer");
}

DebugRenderer::~DebugRenderer()
{
}

bool DebugRenderer::isEnabled() const
{
	return !render_path_tracing;
}

void DebugRenderer::addPasses(FrameGraph &fg)
{
	addVisualizerPass(fg);
	if (render_debug_rendering)
		addTextureDebugPass(fg);
}

void DebugRenderer::addBox(glm::vec3 half_extents, glm::mat4 transform)
{
	if (!isEnabled()) return;
	glm::vec3 corners[8] =
	{
		glm::vec3(-half_extents.x, -half_extents.y, -half_extents.z),
		glm::vec3( half_extents.x, -half_extents.y, -half_extents.z),
		glm::vec3( half_extents.x,  half_extents.y, -half_extents.z),
		glm::vec3(-half_extents.x,  half_extents.y, -half_extents.z),
		glm::vec3(-half_extents.x, -half_extents.y,  half_extents.z),
		glm::vec3( half_extents.x, -half_extents.y,  half_extents.z),
		glm::vec3( half_extents.x,  half_extents.y,  half_extents.z),
		glm::vec3(-half_extents.x,  half_extents.y,  half_extents.z) 
	};

	for (int i = 0; i < 8; ++i)
	{
		corners[i] = glm::vec3(transform * glm::vec4(corners[i], 1.0f));
	}

	int edges[12][2] =
	{
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};

	for (int i = 0; i < 12; ++i)
	{
		glm::vec3 p0 = corners[edges[i][0]];
		glm::vec3 p1 = corners[edges[i][1]];
		addLine(p0, p1);
	}
}

void DebugRenderer::addBoundBox(BoundBox bbox)
{
	if (!isEnabled()) return;
	addLine(bbox.min, glm::vec3(bbox.max.x, bbox.min.y, bbox.min.z));
	addLine(bbox.min, glm::vec3(bbox.min.x, bbox.max.y, bbox.min.z));
	addLine(bbox.min, glm::vec3(bbox.min.x, bbox.min.y, bbox.max.z));

	addLine(bbox.max, glm::vec3(bbox.min.x, bbox.max.y, bbox.max.z));
	addLine(bbox.max, glm::vec3(bbox.max.x, bbox.min.y, bbox.max.z));
	addLine(bbox.max, glm::vec3(bbox.max.x, bbox.max.y, bbox.min.z));

	addLine(glm::vec3(bbox.min.x, bbox.max.y, bbox.min.z), glm::vec3(bbox.min.x, bbox.max.y, bbox.max.z));
	addLine(glm::vec3(bbox.min.x, bbox.max.y, bbox.min.z), glm::vec3(bbox.max.x, bbox.max.y, bbox.min.z));

	addLine(glm::vec3(bbox.max.x, bbox.max.y, bbox.min.z), glm::vec3(bbox.max.x, bbox.min.y, bbox.min.z));
	addLine(glm::vec3(bbox.max.x, bbox.min.y, bbox.min.z), glm::vec3(bbox.max.x, bbox.min.y, bbox.max.z));

	addLine(glm::vec3(bbox.min.x, bbox.min.y, bbox.max.z), glm::vec3(bbox.max.x, bbox.min.y, bbox.max.z));
	addLine(glm::vec3(bbox.min.x, bbox.min.y, bbox.max.z), glm::vec3(bbox.min.x, bbox.max.y, bbox.max.z));
}

void DebugRenderer::addFrustum(glm::mat4 frustum)
{
	if (!isEnabled()) return;
	eastl::array<glm::vec3, 8> corners
	{
		{
			{ -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f },
			{ -1.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, -1.0f }, { -1.0f, 1.0f, -1.0f },
		}
	};

	for (int i = 0; i < corners.size(); i++)
	{
		glm::vec4 current = frustum * glm::vec4(corners[i], 1.0);
		corners[i] = glm::vec3(current) / current.w;
	}

	addLine(corners[0], corners[1]);
	addLine(corners[1], corners[2]);
	addLine(corners[2], corners[3]);
	addLine(corners[3], corners[0]);

	addLine(corners[4], corners[5]);
	addLine(corners[5], corners[6]);
	addLine(corners[6], corners[7]);
	addLine(corners[7], corners[4]);

	addLine(corners[0], corners[4]);
	addLine(corners[1], corners[5]);
	addLine(corners[2], corners[6]);
	addLine(corners[3], corners[7]);
}

eastl::vector<glm::vec3> DebugRenderer::addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments)
{
	if (!isEnabled()) return {};
	eastl::vector<glm::vec3> circle_points;
	glm::vec3 tangent1, tangent2;

	// Create tangents based on the normal
	if (fabs(normal.x) > fabs(normal.y))
	{
		tangent1 = glm::normalize(glm::vec3(normal.z, 0, -normal.x));
	} else
	{
		tangent1 = glm::normalize(glm::vec3(0, -normal.z, normal.y));
	}
	tangent2 = glm::cross(normal, tangent1);

	for (int i = 0; i < segments; ++i)
	{
		float theta = glm::two_pi<float>() * float(i) / float(segments);
		glm::vec3 circle_point = center + radius * (cos(theta) * tangent1 + sin(theta) * tangent2);
		circle_points.push_back(circle_point);
	}

	// Add lines to draw the circle
	for (int i = 0; i < segments; ++i)
	{
		addLine(circle_points[i], circle_points[(i + 1) % segments]);
	}

	return circle_points;
}

void DebugRenderer::addArrow(glm::vec3 p0, glm::vec3 p1, float arrow_size)
{
	if (!isEnabled()) return;
	// Add the main line from p0 to p1
	addLine(p0, p1);

	// Calculate the direction from p0 to p1
	glm::vec3 direction = glm::normalize(p1 - p0);

	// Calculate circle at the base of the arrow
	float circle_radius = arrow_size * 0.5f; // Adjust circle radius as needed
	eastl::vector<glm::vec3> circle_points = addCirlce(p1 - direction * arrow_size, direction, circle_radius, 12);

	// Add lines from the circle to the arrow tip
	for (const auto &point : circle_points)
	{
		addLine(point, p1);
	}

	// Calculate perpendicular vectors to create the arrowhead
	glm::vec3 perpendicular1 = glm::normalize(glm::cross(direction, glm::vec3(0, 1, 0)));
	glm::vec3 perpendicular2 = glm::normalize(glm::cross(direction, perpendicular1));

	glm::vec3 arrowhead_point1 = p1 - direction * arrow_size + perpendicular1 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point2 = p1 - direction * arrow_size - perpendicular1 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point3 = p1 - direction * arrow_size + perpendicular2 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point4 = p1 - direction * arrow_size - perpendicular2 * (arrow_size / 2.0f);

	// Add the arrowhead lines
	addLine(p1, arrowhead_point1);
	addLine(p1, arrowhead_point2);
	addLine(p1, arrowhead_point3);
	addLine(p1, arrowhead_point4);
}

void DebugRenderer::addTextureDebugPass(FrameGraph &fg)
{
	auto *ray_tracing_shadows_data = fg.getBlackboard().tryGet<RayTracedShadowPass>();
	fg.addCallbackPass<EmptyData>("Debug Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.writeTexture(GFXRID(FinalTexture));

		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(DiffuseLight));
		builder.readTexture(GFXRID(SpecularLight));
		builder.readTexture(GFXRID(LutBRDF));
		if (builder.isTextureCreated(GFXRID(SSAOBlurred)))
			builder.readTexture(GFXRID(SSAOBlurred));
		if (builder.isTextureCreated(GFXRID(DDGIDistance)))
			builder.readTexture(GFXRID(DDGIDistance));
		if (builder.isTextureCreated(GFXRID(DDGIIrradiance)))
			builder.readTexture(GFXRID(DDGIIrradiance));
		if (builder.isTextureCreated(GFXRID(DDGIMetadata)))
			builder.readTexture(GFXRID(DDGIMetadata));
		builder.readTexture(GFXRID(FinalNoPostTexture));
		if (ray_tracing_shadows_data)
			builder.read(ray_tracing_shadows_data->visibility);
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));

		cmd_list->setRenderTargets({final}, nullptr, -1, 0, true);

		ubo.albedo_tex_id = resources.getBindlessId(GFXRID(GBufferAlbedo));
		ubo.shading_tex_id = resources.getBindlessId(GFXRID(GBufferShading));
		ubo.normal_tex_id = resources.getBindlessId(GFXRID(GBufferNormal));
		ubo.depth_tex_id = resources.getBindlessId(GFXRID(GBufferDepth));
		ubo.light_diffuse_id = resources.getBindlessId(GFXRID(DiffuseLight));
		ubo.light_specular_id = resources.getBindlessId(GFXRID(SpecularLight));
		ubo.brdf_lut_id = resources.getBindlessId(GFXRID(LutBRDF));
		if (resources.has(GFXRID(SSAOBlurred)))
			ubo.ssao_id = resources.getBindlessId(GFXRID(SSAOBlurred));
		else
			ubo.ssao_id = 0;
		ubo.ddgi_distance_tex_id = resources.has(GFXRID(DDGIDistance)) ? resources.getBindlessId(GFXRID(DDGIDistance)) : 0;
		ubo.ddgi_irradiance_tex_id = resources.has(GFXRID(DDGIIrradiance)) ? resources.getBindlessId(GFXRID(DDGIIrradiance)) : 0;
		ubo.ddgi_metadata_tex_id = resources.has(GFXRID(DDGIMetadata)) ? resources.getBindlessId(GFXRID(DDGIMetadata)) : 0;
		ubo.composite_final_tex_id = resources.getBindlessId(GFXRID(FinalNoPostTexture));

		if (ray_tracing_shadows_data)
			ubo.light_diffuse_id = resources.getBindlessId(ray_tracing_shadows_data->visibility);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/debug_quad.hlsl", FRAGMENT_SHADER));

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(PresentUBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}

void DebugRenderer::addVisualizerPass(FrameGraph & fg)
{
	fg.addCallbackPass<EmptyData>("Debug Visualizer Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.writeTexture(GFXRID(FinalTexture));
		builder.setSideEffect(true);
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));

		cmd_list->setRenderTargets({final}, nullptr, -1, 0, false);

		auto &p = gGlobalPipeline;
		VertexInputsDescription input_desc;
		input_desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32A32_SFLOAT});
		input_desc.inputs.push_back({"COLOR", 0, FORMAT_R32G32B32_SFLOAT});
		p->setupGraphicsPipeline(cmd_list, vertex_shader_lines, fragment_shader_lines,
								 input_desc, false, false, CULL_MODE_NONE);
		p->setPrimitiveTopology(TOPOLOGY_LINE_LIST);
		p->flushAndBind(cmd_list);

		cmd_list->setVertexBuffer(lines_vertex_buffer, 0, sizeof(LineVertex));
		cmd_list->setIndexBuffer(lines_index_buffer, 0, IndexFormat::UINT32);
		cmd_list->drawIndexedInstanced(lines_index_count, 1, 0, 0, 0);
		cmd_list->resetRenderTargets();

		lines_index_count = 0;
	});
}
