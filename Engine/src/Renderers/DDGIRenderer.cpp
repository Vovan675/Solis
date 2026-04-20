#include "pch.h"
#include "DDGIRenderer.h"
#include "Rendering/Model.h"
#include <imgui.h>
#include <random>
#include "Utils/Math.h"

//static uint32_t cascade_size_xz = 16;
//static uint32_t cascade_size_y = 8;
//static uint32_t cascades_count = 4;

static uint32_t cascade_size_xz = 16;
static uint32_t cascade_size_y = 8;
static uint32_t cascades_count = 4;

// Cascades have same size, so we can calculate cascade index easily

// 12x12 pixels for one probe (should be enough, AC shadows uses this)
static int distance_probe_texels = 10;

// 8x8 pixels for one probe
static int irradiance_probe_texels = 6;

// TODO: snowdrop also stores less blurred version of irradiance (aka light cache) and uses it on ray miss when tracing
static int radiance_probe_texels = 14;

DDGIRenderer::DDGIRenderer()
{
	// Dense
	volume.origin = {0.0f, 0.5f, -2.0f};
	//volume.size = {8, 4, 6};
	volume.spacing = {1, 1, 1};
	volume.rays_per_probe = 128;

	// Less dense
	volume.origin = {-5.5f, 0.5f, -3.5f};
	//volume.size = {8, 4, 6};
	volume.spacing = {2, 2, 2};
	volume.rays_per_probe = 128;

	volume.origin = {-14.0f, 0.0f, -6.0f};
	volume.origin = {-10.0f, 10.0f, -4.4f};
	volume.origin = {0.0f, 5.0f, 0.0f};
	volume.size = {cascade_size_xz, cascade_size_y, cascade_size_xz, cascade_size_xz * cascade_size_xz * cascade_size_y};
	volume.spacing = {0.5, 1.0, 0.5};
	//volume.spacing = {1.0, 2.0, 1.0};
	volume.rays_per_probe = 20 * 20;
	volume.cascades_count = cascades_count;

	BufferDescription desc;
	desc.size = sizeof(uint32_t) * volume.getProbesCount();
	desc.usage = BufferUsage::SHADER_READ_BUFFER;
	desc.use_staging_buffer = false;
	desc.storage_stride = sizeof(uint32_t);
	probes_to_update_buffer = gDynamicRHI->createBuffer(desc);
	probes_to_update_buffer->setDebugName("DDGI Update Buffer");
	volume.probes_to_update_buffer_id = probes_to_update_buffer->getShaderResourceView()->getBindlessIndex();

	desc.size = sizeof(volume);
	desc.usage = BufferUsage::SHADER_READ_BUFFER;
	desc.use_staging_buffer = false;
	desc.storage_stride = sizeof(volume);
	volume_buffer = gDynamicRHI->createBuffer(desc);
	volume_buffer->setDebugName("DDGI Volume Buffer");

	auto model = AssetManager::getModelAsset("assets/icosphere_3.fbx");
	sphere_mesh = model->getRootNode()->children[0]->primitives[0].mesh;

	visualize_vertex_shader = gDynamicRHI->createShader(L"shaders/ddgi/ddgi_visualize.hlsl", VERTEX_SHADER);
	visualize_fragment_shader = gDynamicRHI->createShader(L"shaders/ddgi/ddgi_visualize.hlsl", FRAGMENT_SHADER);
}

void DDGIRenderer::addPasses(FrameGraph & fg, Ref<RayTracingScene> rt_scene)
{
	if (!engine_ray_tracing)
		return;

	int border_size = 1;
	int layers = volume.size.y;
	int depth_width = (distance_probe_texels + border_size * 2) * volume.size.x * layers;
	int depth_height = (distance_probe_texels + border_size * 2) * volume.size.z;

	if (!distance_atlas_texture || distance_atlas_texture->getWidth() != depth_width || distance_atlas_texture->getHeight() != depth_height)
	{
		TextureDescription desc;
		desc.width = depth_width;
		desc.height = depth_height;
		desc.array_levels = cascades_count;
		desc.format = FORMAT_R16G16_SFLOAT;
		desc.usage_flags = TEXTURE_USAGE_STORAGE;
		distance_atlas_texture = gDynamicRHI->createTexture(desc);
		distance_atlas_texture->fill();

		volume.distance_atlas_tex_id = distance_atlas_texture->getShaderResourceView()->getBindlessIndex();
	}

	int irradiance_width = (irradiance_probe_texels + border_size * 2) * volume.size.x * layers;
	int irradiance_height = (irradiance_probe_texels + border_size * 2) * volume.size.z;
	if (!irradiance_atlas_texture || irradiance_atlas_texture->getWidth() != irradiance_width || irradiance_atlas_texture->getHeight() != irradiance_height)
	{
		TextureDescription desc;
		desc.width = irradiance_width;
		desc.height = irradiance_height;
		desc.array_levels = cascades_count;
		desc.format = FORMAT_R16G16B16A16_SFLOAT;
		desc.usage_flags = TEXTURE_USAGE_STORAGE;
		irradiance_atlas_texture = gDynamicRHI->createTexture(desc);
		irradiance_atlas_texture->fill();

		volume.irradiance_atlas_tex_id = irradiance_atlas_texture->getShaderResourceView()->getBindlessIndex();
	}

	int metadata_width = volume.size.x * layers;
	int metadata_height = volume.size.z;
	if (!metadata_atlas_texture || metadata_atlas_texture->getWidth() != metadata_width || metadata_atlas_texture->getHeight() != metadata_height)
	{
		TextureDescription desc;
		desc.width = metadata_width;
		desc.height = metadata_height;
		desc.array_levels = cascades_count;
		desc.format = FORMAT_R16G16B16A16_SFLOAT;
		desc.usage_flags = TEXTURE_USAGE_STORAGE;
		metadata_atlas_texture = gDynamicRHI->createTexture(desc);
		metadata_atlas_texture->fill();

		volume.metadata_atlas_tex_id = metadata_atlas_texture->getShaderResourceView()->getBindlessIndex();
	}

	int ray_data_size = sizeof(glm::vec4) * volume.rays_per_probe * volume.getProbesCount();
	if (!ray_data_buffer || ray_data_buffer->getSize() != ray_data_size)
	{
		BufferDescription desc;
		desc.size = sizeof(glm::vec4) * volume.rays_per_probe * volume.getProbesCount();
		desc.storage_stride = sizeof(glm::vec4);
		desc.usage = BufferUsage::SHADER_READ_BUFFER | BufferUsage::SHADER_WRITE_BUFFER;
		ray_data_buffer = gDynamicRHI->createBuffer(desc);
		volume.ray_data_buffer_id = ray_data_buffer->getShaderResourceView()->getBindlessIndex();
	}

	volume.use_relocation = use_relocation;
	volume.use_classification = use_classification;

	fg.importTexture(GFXRID(DDGIDistance), distance_atlas_texture);
	fg.importTexture(GFXRID(DDGIIrradiance), irradiance_atlas_texture);
	fg.importTexture(GFXRID(DDGIMetadata), metadata_atlas_texture);

	// Generate random rotation for ray directions
	static std::default_random_engine generator(time(0));
	static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	
	glm::vec3 random_vector(
		2.0f * dist(generator) - 1.0f,
		2.0f * dist(generator) - 1.0f,
		2.0f * dist(generator) - 1.0f
	);
	random_vector = glm::normalize(random_vector);
	float random_angle = dist(generator) * 2.0f * glm::pi<float>();

	volume.random_vector = random_vector;
	volume.random_angle = random_angle;

	glm::vec3 center = volume.origin;
	for (int i = 0; i < volume.cascades_count; i++)
	{
		volume.cascades[i].spacing = glm::vec4(volume.spacing * float(i + 1), 0.0f);

		glm::vec3 volume_size = glm::vec3(glm::vec4(volume.size) * volume.cascades[i].spacing);
		volume.cascades[i].min = glm::vec4(center - volume_size / 2.0f, 0.0f);
	}

	auto lights = Scene::getCurrentScene()->getEntitiesWith<LightComponent>().each();
	for (auto &&[entity, light_component]: lights)
	{
		if (light_component.getType() == LIGHT_TYPE_DIRECTIONAL)
		{
			Entity light_entity(entity);
			glm::vec3 scale, position, skew;
			glm::vec4 persp;
			glm::quat rotation;
			glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

			volume.sun_dir = rotation * glm::vec4(0, 0, -1, 1);
			volume.sun_color = glm::vec4(light_component.color, 1.0);
			break;
		}
	}

	volume_buffer->fill(&volume);

	if (!Math::isPowerOfTwo(probes_per_frame))
	{
		CORE_ERROR("DDGIRenderer::addPasses() skipped because probes_per_frame must be power of two!");
		return;
	}

	static bool prev_use_relocation = false;
	if (use_relocation != prev_use_relocation)
		addResetRelocationPass(fg);
	prev_use_relocation = use_relocation;

	static bool prev_use_classification = false;
	if (use_classification != prev_use_classification)
		addResetClassificationPass(fg);
	prev_use_classification = use_classification;

	update_probes();
	addTraceRaysPass(fg, rt_scene);
	addUpdatePass(fg);

	if (use_relocation)
		addRelocationPass(fg);
	if (use_classification)
		addClassificationPass(fg);
}

void DDGIRenderer::addVisualizePass(FrameGraph &fg)
{
	if (!engine_ray_tracing || !render_ddgi_visualize)
		return;

	fg.addCallbackPass("DDGI Visualize Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalNoPostTexture));
		builder.writeTexture(GFXRID(GBufferDepth));

		builder.readTexture(GFXRID(DDGIIrradiance));
		builder.readTexture(GFXRID(DDGIDistance));
		if (use_relocation || use_classification)
			builder.readTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalNoPostTexture));
		auto depth = resources.getTexture(GFXRID(GBufferDepth));

		cmd_list->setRenderTargets({final}, depth, 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, visualize_vertex_shader, visualize_fragment_shader, Engine::Vertex::GetVertexInputsDescription());
		gGlobalPipeline->setDepthWrite(true);
		gGlobalPipeline->setDepthFunc(COMPARE_FUNC_GREATER_EQUAL);
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(1, &visualization_settings, sizeof(visualization_settings));

		cmd_list->setVertexBuffer(sphere_mesh->indexed->vertex_buffer, 0, sizeof(Engine::Vertex));
		cmd_list->setIndexBuffer(sphere_mesh->indexed->index_buffer, 0);
		cmd_list->drawIndexedInstanced(sphere_mesh->indexed->indices.size(), volume.getProbesCount(), 0, 0, 0);

		cmd_list->resetRenderTargets();
		//gDynamicRHI->waitGPU();
	});
}

void DDGIRenderer::renderImgui()
{
	if (ImGui::CollapsingHeader("DDGI Volume", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Origin", volume.origin.data.data, 0.1f, -10, 10);
		ImGui::DragInt3("Size", volume.size.data.data, 1.0f, 1, 10);
		ImGui::DragFloat3("Spacing", volume.spacing.data.data, 0.05f, 0.05, 5);

		ImGui::InputInt("Probes Per Frame", (int*)&probes_per_frame, 1, 128, ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::Checkbox("Use Relocation", &use_relocation);
		ImGui::Checkbox("Use Classification", &use_classification);
		char* items[] = { "Irradiance", "Distance", "State", "State Not Disabled", "Cascades" };
		ImGui::Combo("Vis Mode", &visualization_settings.mode, items, _countof(items));
		ImGui::Checkbox("Use Fixed Rays", &use_fixed_rays);
		ImGui::Checkbox("Trace Random Direction", &trace_random_direction);
	}
}

eastl::vector<eastl::pair<const char *, const char *>> DDGIRenderer::calculateDefines(eastl::vector<eastl::pair<const char *, const char *>> additional)
{
	eastl::vector<eastl::pair<const char *, const char *>> defines;
	for (auto &define : additional)
		defines.emplace_back(define);

	if (use_fixed_rays)
		defines.push_back({"USE_FIXED_RAYS", "1"});

	if (trace_random_direction)
		defines.push_back({"TRACE_RANDOM_DIRECTION", "1"});
	return defines;
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
		uint32_t budget_per_cascade = probes_per_frame / volume.cascades_count;

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

	probes_to_update_buffer->fill(probes_to_update.data());	
}

void DDGIRenderer::addTraceRaysPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene)
{
	fg.addCallbackPass("DDGI Trace Rays Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.setSideEffect(true);
		builder.readTexture(GFXRID(Sky));
		if (use_relocation || use_classification)
			builder.readTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupRayTracing(L"shaders/ddgi/ddgi_trace_rays.hlsl", calculateDefines());
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t output_buffer_id;
			uint32_t environment_tex_id;
		} constants;
		constants.output_buffer_id = ray_data_buffer->getUnorderedAccessView()->getBindlessIndex();
		constants.environment_tex_id = resources.getReadTexture(GFXRID(Sky));
		gDynamicRHI->setConstantBufferData(3, &constants, sizeof(constants));

		cmd_list->dispatchRays(volume.rays_per_probe, probes_to_update.size(), 1);
	});
}

void DDGIRenderer::addUpdatePass(FrameGraph & fg)
{
	fg.addCallbackPass("DDGI Update Irradiances Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIIrradiance));
		if (use_relocation || use_classification)
			builder.readTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_update_probe.hlsl", COMPUTE_SHADER, "CSMain", calculateDefines({{"NUM_TEXELS", "8"}, {"IRRADIANCE", "1"}})));
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t irradiance_uav_id = resources.getReadWriteTexture(GFXRID(DDGIIrradiance));
		gDynamicRHI->setConstantBufferData(1, &irradiance_uav_id, sizeof(uint32_t));

		cmd_list->dispatch(probes_to_update.size(), 1, 1);
		gDynamicRHI->waitGPU();
	});

	fg.addCallbackPass("DDGI Update Distances Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIDistance));
		if (use_relocation || use_classification)
			builder.readTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_update_probe.hlsl", COMPUTE_SHADER, "CSMain", calculateDefines({{"NUM_TEXELS", "12"}})));
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t distance_uav_id = resources.getReadWriteTexture(GFXRID(DDGIDistance));
		gDynamicRHI->setConstantBufferData(1, &distance_uav_id, sizeof(uint32_t));

		cmd_list->dispatch(probes_to_update.size(), 1, 1);
		gDynamicRHI->waitGPU();
	});
}

void DDGIRenderer::addRelocationPass(FrameGraph & fg)
{
	fg.addCallbackPass("DDGI Relocation Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_relocation.hlsl", COMPUTE_SHADER, "CS_Relocate", calculateDefines()));
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t metadata_uav_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		gDynamicRHI->setConstantBufferData(1, &metadata_uav_id, sizeof(uint32_t));

		uint32_t num_groups = ceil(probes_to_update.size() / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
		gDynamicRHI->waitGPU();
	});
}

void DDGIRenderer::addResetRelocationPass(FrameGraph & fg)
{
	fg.addCallbackPass("DDGI Reset Relocation Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_relocation.hlsl", COMPUTE_SHADER, "CS_ResetRelocation", calculateDefines()));
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t metadata_uav_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		gDynamicRHI->setConstantBufferData(1, &metadata_uav_id, sizeof(uint32_t));

		uint32_t num_groups = ceil(volume.getProbesCount() / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
		gDynamicRHI->waitGPU();
	});
}

void DDGIRenderer::addClassificationPass(FrameGraph &fg)
{
	fg.addCallbackPass("DDGI Classification Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_classification.hlsl", COMPUTE_SHADER, "CS_Classification", calculateDefines()));
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t metadata_uav_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		gDynamicRHI->setConstantBufferData(1, &metadata_uav_id, sizeof(uint32_t));

		uint32_t num_groups = ceil(probes_to_update.size() / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
		gDynamicRHI->waitGPU();
	});
}

void DDGIRenderer::addResetClassificationPass(FrameGraph & fg)
{
	fg.addCallbackPass("DDGI Reset Classification Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(DDGIMetadata));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/ddgi/ddgi_classification.hlsl", COMPUTE_SHADER, "CS_ResetClassification", calculateDefines()));
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t metadata_uav_id = resources.getReadWriteTexture(GFXRID(DDGIMetadata));
		gDynamicRHI->setConstantBufferData(1, &metadata_uav_id, sizeof(uint32_t));

		uint32_t num_groups = ceil(volume.getProbesCount() / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
		gDynamicRHI->waitGPU();
	});
}
