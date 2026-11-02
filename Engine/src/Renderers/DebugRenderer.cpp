#include "pch.h"
#include "DebugRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"
#include "Rendering/ShaderStructs.h"
#include "UtilityPasses.h"

#define MAX_NUM_LINES 731072

DebugRenderer::DebugRenderer()
{
	vertex_shader_lines = gDynamicRHI->createShader(L"shaders/debug_lines.hlsl", ShaderType::VERTEX);
	fragment_shader_lines = gDynamicRHI->createShader(L"shaders/debug_lines.hlsl", ShaderType::FRAGMENT);

	vertex_shader_gpu_lines = gDynamicRHI->createShader(L"shaders/debug_lines.hlsl", ShaderType::VERTEX, "VSGpuLines");

	BufferDescription desc;

	vertices = new LineVertex[MAX_NUM_LINES];
	
	desc.size = sizeof(LineVertex) * MAX_NUM_LINES;
	desc.stride = sizeof(LineVertex);
	desc.memory = BufferMemory::UPLOAD;
	desc.usage_flags = BufferUsage::VERTEX;

	lines_vertex_buffer = gDynamicRHI->createBuffer(desc);
	lines_vertex_buffer->map((void **)&vertices);
	lines_vertex_buffer->setDebugName("Lines Vertex Buffer");

	desc.size = sizeof(uint32_t) + sizeof(LineVertex) * MAX_NUM_LINES;
	desc.stride = sizeof(uint32_t);
	desc.memory = BufferMemory::GPU;
	desc.usage_flags = BufferUsage::UNORDERED_ACCESS;
	lines_gpu_buffer = gDynamicRHI->createBuffer(desc);
	lines_gpu_buffer->setDebugName("Lines GPU Vertex Buffer");

	desc.size = sizeof(DrawIndirect);
	desc.stride = sizeof(DrawIndirect);
	desc.usage_flags = BufferUsage::INDIRECT_ARGS | BufferUsage::UNORDERED_ACCESS;
	lines_draw_args_buffer = gDynamicRHI->createBuffer(desc);
	lines_draw_args_buffer->setDebugName("Lines GPU Draw Args Buffer");
}

bool DebugRenderer::isEnabled() const
{
	return enabled && !GFXOPTIONS(path_tracing);
}

eastl::vector<DebugRenderer::DebugOverlay> DebugRenderer::overlays;

void DebugRenderer::addPasses(FrameGraph &fg)
{
	if (!isEnabled())
	{
		overlays.clear();
		return;
	}

	UtilityPasses::addDepthBlitPass(fg, GFXRID(GBufferDepth), GFXRID(DebugDepth), Renderer::getOutputResolution());
	addVisualizerPass(fg);
	if (render_debug_rendering)
		addTextureDebugPass(fg);
	for (DebugOverlay overlay : overlays)
		UtilityPasses::addBlitPass(fg, overlay.texture, GFXRID(FinalTexture), UtilityPasses::BLIT_BLEND_ALPHA, overlay.shader);
	overlays.clear();
}

void DebugRenderer::addBox(glm::vec3 half_extents, glm::mat4 transform, glm::vec3 color)
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
		addLine(p0, p1, color);
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

void DebugRenderer::addFrustum(glm::mat4 frustum, glm::vec3 color)
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

	addLine(corners[0], corners[1], color);
	addLine(corners[1], corners[2], color);
	addLine(corners[2], corners[3], color);
	addLine(corners[3], corners[0], color);

	addLine(corners[4], corners[5], color);
	addLine(corners[5], corners[6], color);
	addLine(corners[6], corners[7], color);
	addLine(corners[7], corners[4], color);

	addLine(corners[0], corners[4], color);
	addLine(corners[1], corners[5], color);
	addLine(corners[2], corners[6], color);
	addLine(corners[3], corners[7], color);
}

void DebugRenderer::addSphere(glm::vec3 center, float radius, int segments, glm::vec3 color)
{
	if (!isEnabled()) return;
	if (segments < 3) segments = 3;
	
	addCirlce(center, glm::vec3(0, 0, 1), radius, segments, color);
	addCirlce(center, glm::vec3(0, 1, 0), radius, segments, color);
	addCirlce(center, glm::vec3(1, 0, 0), radius, segments, color);
}

eastl::vector<glm::vec3> DebugRenderer::addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments, glm::vec3 color, bool use_depth)
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
		addLine(circle_points[i], circle_points[(i + 1) % segments], color, use_depth);
	}

	return circle_points;
}

void DebugRenderer::addArrow(glm::vec3 p0, glm::vec3 p1, float arrow_size, glm::vec3 color, bool use_depth)
{
	if (!isEnabled()) return;
	// Add the main line from p0 to p1
	addLine(p0, p1, color, use_depth);

	// Calculate the direction from p0 to p1
	glm::vec3 direction = glm::normalize(p1 - p0);

	// Calculate circle at the base of the arrow
	float circle_radius = arrow_size * 0.5f; // Adjust circle radius as needed
	eastl::vector<glm::vec3> circle_points = addCirlce(p1 - direction * arrow_size, direction, circle_radius, 12, color, use_depth);

	// Add lines from the circle to the arrow tip
	for (const auto &point : circle_points)
	{
		addLine(point, p1, color, use_depth);
	}
}

void DebugRenderer::addTextureDebugPass(FrameGraph &fg)
{
	fg.addCallbackPass("Debug Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalTexture));

		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(GBufferEmissive));
		builder.readTexture(GFXRID(MotionVectors));
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
		if (builder.isTextureCreated(GFXRID(HiZTexture)))
			builder.readTexture(GFXRID(HiZTexture));
		builder.readTexture(GFXRID(FinalNoPostTexture));
		if (builder.isTextureCreated(GFXRID(RayTracedVisibility)))
			builder.readTexture(GFXRID(RayTracedVisibility));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));

		cmd_list->setRenderTargets({final}, nullptr, -1, 0, true);

		ubo.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
		ubo.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
		ubo.emissive_tex_id = resources.getReadTexture(GFXRID(GBufferEmissive));
		ubo.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		ubo.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		ubo.motion_vectors_tex_id = resources.getReadTexture(GFXRID(MotionVectors));
		ubo.light_diffuse_id = resources.getReadTexture(GFXRID(DiffuseLight));
		ubo.light_specular_id = resources.getReadTexture(GFXRID(SpecularLight));
		ubo.brdf_lut_id = resources.getReadTexture(GFXRID(LutBRDF));
		if (resources.has(GFXRID(SSAOBlurred)))
			ubo.ssao_id = resources.getReadTexture(GFXRID(SSAOBlurred));
		else
			ubo.ssao_id = 0;
		ubo.ddgi_distance_tex_id = resources.has(GFXRID(DDGIDistance)) ? resources.getReadTexture(GFXRID(DDGIDistance)) : 0;
		ubo.ddgi_irradiance_tex_id = resources.has(GFXRID(DDGIIrradiance)) ? resources.getReadTexture(GFXRID(DDGIIrradiance)) : 0;
		ubo.ddgi_metadata_tex_id = resources.has(GFXRID(DDGIMetadata)) ? resources.getReadTexture(GFXRID(DDGIMetadata)) : 0;
		ubo.composite_final_tex_id = resources.getReadTexture(GFXRID(FinalNoPostTexture));

		if (resources.has(GFXRID(RayTracedVisibility)))
			ubo.light_diffuse_id = resources.getReadTexture(GFXRID(RayTracedVisibility));

		if (resources.has(GFXRID(HiZTexture)))
			ubo.hiz_tex_id = resources.getReadTexture(GFXRID(HiZTexture));
		else
			ubo.hiz_tex_id = 0;

		gGlobalPipeline->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/debug_quad.hlsl", ShaderType::FRAGMENT));

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(PresentUBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}

void DebugRenderer::addVisualizerPass(FrameGraph & fg)
{
	fg.importBuffer(GFXRID(DebugLinesBuffer), lines_gpu_buffer);
	fg.importBuffer(GFXRID(DebugLinesDrawArgsBuffer), lines_draw_args_buffer);

	fg.addCallbackPass("Debug Visualizer Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalTexture));
		builder.readDepthTexture(GFXRID(DebugDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));
		auto depth = resources.getTexture(GFXRID(DebugDepth));

		cmd_list->setRenderTargets({final}, depth, 0, 0, false);

		VertexInputsDescription input_desc;
		input_desc.inputs.push_back({"POSITION", 0, Format::R32G32B32A32_SFLOAT});
		input_desc.inputs.push_back({"COLOR", 0, Format::R32G32B32_SFLOAT});
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader_lines, fragment_shader_lines,
								 input_desc, false, true, CullMode::NONE);
		gGlobalPipeline->setDepthWrite(false);
		gGlobalPipeline->setDepthFunc(CompareFunc::GREATER_EQUAL);
		gGlobalPipeline->setPrimitiveTopology(Topology::LINE_LIST);
		gGlobalPipeline->flushAndBind(cmd_list);

		cmd_list->setVertexBuffer(lines_vertex_buffer, 0, sizeof(LineVertex));
		cmd_list->drawInstanced(lines_index_count, 1, 0, 0);
		cmd_list->resetRenderTargets();

		lines_index_count = 0;
	});


	fg.addCallbackPass("Create Debug DrawCalls Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeBuffer(GFXRID(DebugLinesBuffer));
		builder.writeBuffer(GFXRID(DebugLinesDrawArgsBuffer));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct Constants
		{
			uint32_t lines_draw_args_buffer_id;
		} constants;
		constants.lines_draw_args_buffer_id = lines_draw_args_buffer->getUnorderedAccessView()->getBindlessIndex();

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/debug_lines.hlsl", ShaderType::COMPUTE, "CSGenerateDrawCalls"));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});

	fg.addCallbackPass("Debug GPU Visualizer Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalTexture));
		builder.readDepthTexture(GFXRID(DebugDepth));
		builder.readBuffer(GFXRID(DebugLinesBuffer));
		builder.readIndirectArgsBuffer(GFXRID(DebugLinesDrawArgsBuffer));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));
		auto depth = resources.getTexture(GFXRID(DebugDepth));

		cmd_list->setRenderTargets({final}, depth, 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, vertex_shader_gpu_lines, fragment_shader_lines,
								 {}, false, true, CullMode::NONE);
		gGlobalPipeline->setDepthWrite(false);
		gGlobalPipeline->setDepthFunc(CompareFunc::GREATER_EQUAL);
		gGlobalPipeline->setPrimitiveTopology(Topology::LINE_LIST);
		gGlobalPipeline->flushAndBind(cmd_list);

		cmd_list->drawIndirect(lines_draw_args_buffer, 1);
		cmd_list->resetRenderTargets();

		lines_index_count = 0;
	});
}
