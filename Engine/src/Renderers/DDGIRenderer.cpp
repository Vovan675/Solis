#include "pch.h"
#include "DDGIRenderer.h"
#include "Rendering/Model.h"
#include "Utils/Math.h"
#include "Core/Variables.h"
#include "Rendering/UploadManager.h"

static uint32_t cascades_count = 4;

// 12x12 pixels for one probe (should be enough, AC shadows uses this)
static int distance_probe_texels = 10;

// 8x8 pixels for one probe
static int irradiance_probe_texels = 6;

// TODO: snowdrop also stores less blurred version of irradiance (aka light cache) and uses it on ray miss when tracing
static int radiance_probe_texels = 14;

struct ProbePassConstants
{
	uint32_t output_atlas_tex_id;
	uint32_t ray_data_buffer_id;
	uint32_t probes_to_update_buffer_id;
	uint32_t probes_to_update_count;
};

DDGIRenderer::DDGIRenderer()
{
	volume.rays_per_probe = 20 * 20;
	volume.cascades_count = cascades_count;

	auto model = AssetManager::getModelAsset("assets/models/primitives/icosphere_3.fbx");
	sphere_mesh = model->getRootNode()->children[0]->primitives[0].mesh;

	visualize_vertex_shader = gDynamicRHI->createShader(L"shaders/ddgi/ddgi_visualize.hlsl", ShaderType::VERTEX);
	visualize_fragment_shader = gDynamicRHI->createShader(L"shaders/ddgi/ddgi_visualize.hlsl", ShaderType::FRAGMENT);
}

void DDGIRenderer::update()
{
	const DDGISettings &ddgi = GFXOPTIONS(ddgi);
	volume.origin = ddgi.origin;
	volume.size = glm::ivec4(ddgi.size, ddgi.size.x * ddgi.size.y * ddgi.size.z);
	volume.spacing = ddgi.spacing;
	volume.use_relocation = ddgi.use_relocation;
	volume.use_classification = ddgi.use_classification;

	glm::vec3 center = volume.origin;
	for (int i = 0; i < volume.cascades_count; i++)
	{
		volume.cascades[i].spacing = glm::vec4(volume.spacing * float(1 << i), 0.0f);

		glm::vec3 volume_size = (glm::vec3(volume.size) - 1.0f) * glm::vec3(volume.cascades[i].spacing);
		volume.cascades[i].min = glm::vec4(center - volume_size / 2.0f, 0.0f);
	}

	update_probes();
}

void DDGIRenderer::addPasses(FrameGraph & fg, Ref<RayTracingScene> rt_scene)
{
	const DDGISettings &ddgi = GFXOPTIONS(ddgi);

	addPreparePass(fg);
	addTraceRaysPass(fg, rt_scene);
	addUpdatePass(fg);

	if (ddgi.use_relocation)
		addRelocationPass(fg);
	if (ddgi.use_classification)
		addClassificationPass(fg);
}

void DDGIRenderer::readAtlases(RenderPassBuilder &builder)
{
	builder.readTexture(GFXRID(DDGIDistance));
	builder.readTexture(GFXRID(DDGIIrradiance));
	builder.readTexture(GFXRID(DDGIMetadata));
}

void DDGIRenderer::setAtlasConstants(const RenderPassResources &resources)
{
	struct
	{
		uint32_t distance_atlas_tex_id;
		uint32_t irradiance_atlas_tex_id;
		uint32_t metadata_atlas_tex_id;
	} constants;
	constants.distance_atlas_tex_id = resources.getReadTexture(GFXRID(DDGIDistance));
	constants.irradiance_atlas_tex_id = resources.getReadTexture(GFXRID(DDGIIrradiance));
	constants.metadata_atlas_tex_id = resources.getReadTexture(GFXRID(DDGIMetadata));
	gDynamicRHI->setConstantBufferData(2, &constants, sizeof(constants));
}

void DDGIRenderer::addVisualizePass(FrameGraph &fg)
{
	fg.addCallbackPass("DDGI Visualize Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalTexture));
		builder.writeTexture(GFXRID(DebugDepth));
		readAtlases(builder);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));
		auto depth = resources.getTexture(GFXRID(DebugDepth));

		cmd_list->setRenderTargets({final}, depth, 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, visualize_vertex_shader, visualize_fragment_shader, Engine::Vertex::GetVertexInputsDescription());
		gGlobalPipeline->setDepthWrite(true);
		gGlobalPipeline->setDepthFunc(CompareFunc::GREATER_EQUAL);
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(1, render_ddgi_visualize_mode.getPtr(), sizeof(int));
		setAtlasConstants(resources);

		cmd_list->setVertexBuffer(sphere_mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
		cmd_list->setIndexBuffer(sphere_mesh->indexed->index_buffer, 0);
		cmd_list->drawIndexedInstanced(sphere_mesh->indexed->indices.size(), volume.getProbesCount(), 0, 0, 0);

		cmd_list->resetRenderTargets();
	});
}

void DDGIRenderer::update_probes()
{
	probes_to_update.clear();

	bool update_all = false;
	if (update_all)
	{
		for (int i = 0; i < volume.getProbesCount(); i++)
			probes_to_update.push_back(i);
	} else
	{
		uint32_t budget_per_cascade = GFXOPTIONS(ddgi).probes_per_frame / volume.cascades_count;

		for (int c = 0; c < volume.cascades_count; c++)
		{
			uint32_t cascade_offset = c * volume.size.w;

			uint32_t cascade_budget = std::min((uint32_t)volume.size.w, budget_per_cascade);

			for (int p = 0; p < cascade_budget; p++)
				probes_to_update.push_back((p + cascades_update[c].last_local_index) % volume.size.w + cascade_offset);
			cascades_update[c].last_local_index = (cascade_budget + cascades_update[c].last_local_index) % volume.size.w;
		}
	}
	ENGINE_ASSERT(Math::isPowerOfTwo(probes_to_update.size()));
}

void DDGIRenderer::addPreparePass(FrameGraph &fg)
{
	const DDGISettings &ddgi = GFXOPTIONS(ddgi);
	bool reset_relocation = ddgi.use_relocation != last_use_relocation;
	bool reset_classification = ddgi.use_classification != last_use_classification;
	last_use_relocation = ddgi.use_relocation;
	last_use_classification = ddgi.use_classification;

	fg.addCallbackPass("DDGI Prepare Pass",
	[&](RenderPassBuilder &builder)
	{
		int border_size = 1;
		int layers = volume.size.y;

		TextureDescription distance_desc;
		distance_desc.width = (distance_probe_texels + border_size * 2) * volume.size.x * layers;
		distance_desc.height = (distance_probe_texels + border_size * 2) * volume.size.z;
		distance_desc.array_layers = cascades_count;
		distance_desc.format = Format::R16G16_SFLOAT;
		distance_desc.usage_flags = TextureUsage::UNORDERED_ACCESS;

		TextureDescription irradiance_desc;
		irradiance_desc.width = (irradiance_probe_texels + border_size * 2) * volume.size.x * layers;
		irradiance_desc.height = (irradiance_probe_texels + border_size * 2) * volume.size.z;
		irradiance_desc.array_layers = cascades_count;
		irradiance_desc.format = Format::R16G16B16A16_SFLOAT;
		irradiance_desc.usage_flags = TextureUsage::UNORDERED_ACCESS;

		TextureDescription metadata_desc;
		metadata_desc.width = volume.size.x * layers;
		metadata_desc.height = volume.size.z * 2; // Rows: current offset + state, candidate offset + lighting scale
		metadata_desc.array_layers = cascades_count;
		metadata_desc.format = Format::R16G16B16A16_SFLOAT;
		metadata_desc.usage_flags = TextureUsage::UNORDERED_ACCESS;

		builder.createPersistentTexture(GFXRID(DDGIDistance), distance_desc);
		builder.createPersistentTexture(GFXRID(DDGIIrradiance), irradiance_desc);
		builder.createPersistentTexture(GFXRID(DDGIMetadata), metadata_desc);

		builder.createBuffer(GFXRID(DDGIProbesToUpdate), sizeof(uint32_t), probes_to_update.size(), BufferUsage::SHADER_RESOURCE);
		builder.writeCopyBuffer(GFXRID(DDGIProbesToUpdate));

		if (reset_relocation || reset_classification)
			builder.writeUAVTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		if (reset_relocation || reset_classification)
		{
			gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_prepare.hlsl", ShaderType::COMPUTE, "CS_ResetMetadata"));
			gGlobalPipeline->flushAndBind(cmd_list);

			struct
			{
				uint32_t metadata_atlas_tex_id;
				uint32_t reset_relocation;
				uint32_t reset_classification;
			} constants;
			constants.metadata_atlas_tex_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
			constants.reset_relocation = reset_relocation;
			constants.reset_classification = reset_classification;
			gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));

			cmd_list->dispatch(Math::divideRoundUp(volume.getProbesCount(), 32), 1, 1);
		}

		uint32_t size = probes_to_update.size() * sizeof(uint32_t);
		UploadManager::StagedRange staged = gUploadManager->stage(size);
		memcpy(staged.data, probes_to_update.data(), size);
		cmd_list->copyBuffer(staged.buffer, resources.getBuffer(GFXRID(DDGIProbesToUpdate)), staged.offset, 0, size);
	});
}

void DDGIRenderer::addTraceRaysPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene)
{
	fg.addCallbackPass("DDGI Trace Rays Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createBuffer(GFXRID(DDGIRayData), sizeof(glm::vec4), volume.rays_per_probe * probes_to_update.size(), BufferUsage::SHADER_RESOURCE | BufferUsage::UNORDERED_ACCESS);
		builder.writeBuffer(GFXRID(DDGIRayData));
		builder.readBuffer(GFXRID(DDGIProbesToUpdate));
		readAtlases(builder);
		builder.readTexture(GFXRID(Sky));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_trace_rays.hlsl", ShaderType::COMPUTE, "CS_TraceRays"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t output_buffer_id;
			uint32_t environment_tex_id;
			uint32_t probes_to_update_buffer_id;
		} constants;
		constants.output_buffer_id = resources.getReadWriteBuffer(GFXRID(DDGIRayData));
		constants.environment_tex_id = GFXOPTIONS(sky).enabled ? resources.getReadTexture(GFXRID(Sky)) : 0;
		constants.probes_to_update_buffer_id = resources.getReadBuffer(GFXRID(DDGIProbesToUpdate));
		gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));
		setAtlasConstants(resources);

		cmd_list->dispatch(Math::divideRoundUp(volume.rays_per_probe, 64), probes_to_update.size(), 1);
	});
}

void DDGIRenderer::addUpdatePass(FrameGraph & fg)
{
	fg.addCallbackPass("DDGI Update Irradiances Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIIrradiance));
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
		builder.readBuffer(GFXRID(DDGIRayData));
		builder.readBuffer(GFXRID(DDGIProbesToUpdate));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_update_probe.hlsl", ShaderType::COMPUTE, "CSMain", {{"NUM_TEXELS", "8"}, {"IRRADIANCE", "1"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t output_atlas_tex_id;
			uint32_t ray_data_buffer_id;
			uint32_t probes_to_update_buffer_id;
			uint32_t metadata_atlas_tex_id;
		} constants;
		constants.output_atlas_tex_id = resources.getReadWriteTexture(GFXRID(DDGIIrradiance));
		constants.ray_data_buffer_id = resources.getReadBuffer(GFXRID(DDGIRayData));
		constants.probes_to_update_buffer_id = resources.getReadBuffer(GFXRID(DDGIProbesToUpdate));
		constants.metadata_atlas_tex_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));

		cmd_list->dispatch(probes_to_update.size(), 1, 1);
	});

	fg.addCallbackPass("DDGI Update Distances Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIDistance));
		builder.readBuffer(GFXRID(DDGIRayData));
		builder.readBuffer(GFXRID(DDGIProbesToUpdate));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_update_probe.hlsl", ShaderType::COMPUTE, "CSMain", {{"NUM_TEXELS", "12"}}));
		gGlobalPipeline->flushAndBind(cmd_list);

		ProbePassConstants constants;
		constants.output_atlas_tex_id = resources.getReadWriteTexture(GFXRID(DDGIDistance));
		constants.ray_data_buffer_id = resources.getReadBuffer(GFXRID(DDGIRayData));
		constants.probes_to_update_buffer_id = resources.getReadBuffer(GFXRID(DDGIProbesToUpdate));
		constants.probes_to_update_count = probes_to_update.size();
		gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));

		cmd_list->dispatch(probes_to_update.size(), 1, 1);
	});
}

void DDGIRenderer::addRelocationPass(FrameGraph & fg)
{
	fg.addCallbackPass("DDGI Relocation Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
		builder.readBuffer(GFXRID(DDGIRayData));
		builder.readBuffer(GFXRID(DDGIProbesToUpdate));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_relocation.hlsl", ShaderType::COMPUTE, "CS_Relocate"));
		gGlobalPipeline->flushAndBind(cmd_list);

		ProbePassConstants constants;
		constants.output_atlas_tex_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		constants.ray_data_buffer_id = resources.getReadBuffer(GFXRID(DDGIRayData));
		constants.probes_to_update_buffer_id = resources.getReadBuffer(GFXRID(DDGIProbesToUpdate));
		constants.probes_to_update_count = probes_to_update.size();
		gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(probes_to_update.size(), 32), 1, 1);
	});
}

void DDGIRenderer::addClassificationPass(FrameGraph &fg)
{
	fg.addCallbackPass("DDGI Classification Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
		builder.readBuffer(GFXRID(DDGIRayData));
		builder.readBuffer(GFXRID(DDGIProbesToUpdate));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_classification.hlsl", ShaderType::COMPUTE, "CS_Classification"));
		gGlobalPipeline->flushAndBind(cmd_list);

		ProbePassConstants constants;
		constants.output_atlas_tex_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		constants.ray_data_buffer_id = resources.getReadBuffer(GFXRID(DDGIRayData));
		constants.probes_to_update_buffer_id = resources.getReadBuffer(GFXRID(DDGIProbesToUpdate));
		constants.probes_to_update_count = probes_to_update.size();
		gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(probes_to_update.size(), 32), 1, 1);
	});
}
