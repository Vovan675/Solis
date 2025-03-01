#include "pch.h"
#include "DebugRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

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
	desc.useStagingBuffer = true;
	desc.usage = INDEX_BUFFER;

	lines_index_buffer = gDynamicRHI->createBuffer(desc);
	lines_index_buffer->fill(indices);
	lines_index_buffer->setDebugName("Lines Index Buffer");

	vertices = new LineVertex[2048];
	
	desc.size = sizeof(LineVertex) * 2048;
	desc.vertex_buffer_stride = sizeof(LineVertex);
	desc.useStagingBuffer = false;
	desc.usage = VERTEX_BUFFER;

	lines_vertex_buffer = gDynamicRHI->createBuffer(desc);
	lines_vertex_buffer->map((void **)&vertices);
	lines_vertex_buffer->setDebugName("Lines Vertex Buffer");
}

DebugRenderer::~DebugRenderer()
{
}

void DebugRenderer::addPasses(FrameGraph &fg)
{
	auto &gbuffer_data = fg.getBlackboard().get<GBufferData>();
	auto &lighting_data = fg.getBlackboard().get<DeferredLightingData>();
	auto *ssao_data = fg.getBlackboard().tryGet<SSAOData>();
	auto &composite_data = fg.getBlackboard().get<CompositeData>();
	auto &lut_data = fg.getBlackboard().get<LutData>();
	auto *ray_tracing_shadows_data = fg.getBlackboard().tryGet<RayTracedShadowPass>();

	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	default_data = fg.addCallbackPass<DefaultResourcesData>("Debug Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		data.final = builder.write(default_data.final);

		builder.read(gbuffer_data.albedo);
		builder.read(gbuffer_data.normal);
		builder.read(gbuffer_data.depth);
		builder.read(gbuffer_data.shading);
		builder.read(lighting_data.diffuse_light);
		builder.read(lighting_data.specular_light);
		builder.read(lut_data.brdf_lut);
		builder.read(ssao_data->ssao_blurred);
		builder.read(default_data.final_no_post);
		if (ray_tracing_shadows_data)
			builder.read(ray_tracing_shadows_data->visibility);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &final = resources.getResource<FrameGraphTexture>(data.final);

		cmd_list->setRenderTargets({final.texture}, nullptr, -1, 0, true);

		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.albedo).getBindlessId();
		ubo.shading_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.shading).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.normal).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.depth).getBindlessId();
		ubo.light_diffuse_id = resources.getResource<FrameGraphTexture>(lighting_data.diffuse_light).getBindlessId();
		ubo.light_specular_id = resources.getResource<FrameGraphTexture>(lighting_data.specular_light).getBindlessId();
		ubo.brdf_lut_id = resources.getResource<FrameGraphTexture>(lut_data.brdf_lut).getBindlessId();
		ubo.ssao_id = resources.getResource<FrameGraphTexture>(ssao_data->ssao_blurred).getBindlessId();
		ubo.composite_final_tex_id = resources.getResource<FrameGraphTexture>(data.final_no_post).getBindlessId();

		if (ray_tracing_shadows_data)
			ubo.light_diffuse_id = resources.getResource<FrameGraphTexture>(ray_tracing_shadows_data->visibility).getBindlessId();

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/debug_quad.hlsl", FRAGMENT_SHADER));

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(PresentUBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}

void DebugRenderer::renderLines(FrameGraph &fg)
{
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();
	default_data = fg.addCallbackPass<DefaultResourcesData>("Debug Visualizer Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		data.final = builder.write(default_data.final);
		builder.setSideEffect(true);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &final = resources.getResource<FrameGraphTexture>(data.final);

		cmd_list->setRenderTargets({final.texture}, nullptr, -1, 0, false);

		auto &p = gGlobalPipeline;
		p->reset();

		p->setVertexShader(vertex_shader_lines);
		p->setFragmentShader(fragment_shader_lines);

		p->setRenderTargets(cmd_list->getCurrentRenderTargets());

		VertexInputsDescription input_desc;
		input_desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32A32_SFLOAT});
		input_desc.inputs.push_back({"COLOR", 0, FORMAT_R32G32B32_SFLOAT});
		p->setVertexInputsDescription(input_desc);

		p->setUseBlending(false);
		p->setPrimitiveTopology(TOPOLOGY_LINE_LIST);
		p->setDepthTest(false);
		p->setCullMode(CULL_MODE_NONE);

		p->flush();
		p->bind(cmd_list);

		cmd_list->setVertexBuffer(lines_vertex_buffer);
		cmd_list->setIndexBuffer(lines_index_buffer);
		cmd_list->drawIndexedInstanced(lines_index_count, 1, 0, 0, 0);
		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();

		lines_index_count = 0;
	});
}

void DebugRenderer::addBox(glm::vec3 half_extents, glm::mat4 transform)
{
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
	std::array<glm::vec3, 8> corners
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

std::vector<glm::vec3> DebugRenderer::addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments)
{
	std::vector<glm::vec3> circle_points;
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
	// Add the main line from p0 to p1
	addLine(p0, p1);

	// Calculate the direction from p0 to p1
	glm::vec3 direction = glm::normalize(p1 - p0);

	// Calculate circle at the base of the arrow
	float circle_radius = arrow_size * 0.5f; // Adjust circle radius as needed
	std::vector<glm::vec3> circle_points = addCirlce(p1 - direction * arrow_size, direction, circle_radius, 12);

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
