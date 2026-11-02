#include "pch.h"
#include "NRDDenoiser.h"
#include "Rendering/GlobalPipeline.h"
#include "DebugRenderer.h"

static const nrd::Identifier RELAX_IDENTIFIER = 0;
static const nrd::Identifier REFERENCE_DIFFUSE_IDENTIFIER = 1;
static const nrd::Identifier REFERENCE_SPECULAR_IDENTIFIER = 2;

static nrd::RelaxSettings to_nrd_relax_settings(const RelaxSettings &relax)
{
	nrd::RelaxSettings settings;
	settings.diffuseMaxAccumulatedFrameNum = relax.max_accumulated_frames;
	settings.specularMaxAccumulatedFrameNum = relax.max_accumulated_frames;
	settings.diffuseMaxFastAccumulatedFrameNum = relax.max_fast_accumulated_frames;
	settings.specularMaxFastAccumulatedFrameNum = relax.max_fast_accumulated_frames;
	settings.spatialVarianceEstimationHistoryThreshold = 1;
	settings.diffusePrepassBlurRadius = relax.diffuse_prepass_blur_radius;
	settings.specularPrepassBlurRadius = relax.specular_prepass_blur_radius;
	settings.minHitDistanceWeight = relax.min_hit_distance_weight;
	settings.diffusePhiLuminance = relax.diffuse_phi_luminance;
	settings.specularPhiLuminance = relax.specular_phi_luminance;
	settings.atrousIterationNum = relax.atrous_iterations;
	settings.depthThreshold = relax.depth_threshold;
	settings.lobeAngleFraction = relax.lobe_angle_fraction;
	settings.roughnessFraction = relax.roughness_fraction;
	settings.enableAntiFirefly = relax.anti_firefly;
	return settings;
}

static GraphicsResourceName get_graphics_resource_name(const nrd::ResourceDesc &resource, const NRDResourceNames &resource_names)
{
	if (resource.type == nrd::ResourceType::TRANSIENT_POOL)
		return GFXRID_ID(NRDTransient, resource.indexInPool);
	if (resource.type == nrd::ResourceType::PERMANENT_POOL)
		return GFXRID_ID(NRDPermanent, resource.indexInPool);
	return resource_names[(size_t)resource.type];
}

static Format from_nrd_format(nrd::Format format)
{
	switch (format)
	{
		case nrd::Format::R8_UNORM: return Format::R8_UNORM;
		case nrd::Format::RGBA8_UNORM: return Format::R8G8B8A8_UNORM;
		case nrd::Format::R16_SFLOAT: return Format::R16_SFLOAT;
		case nrd::Format::RGBA16_SFLOAT: return Format::R16G16B16A16_SFLOAT;
		case nrd::Format::R32_SFLOAT: return Format::R32_SFLOAT;
		case nrd::Format::RGBA32_SFLOAT: return Format::R32G32B32A32_SFLOAT;
	}
	ENGINE_ASSERT(false);
	return Format::UNDEFINED;
}

static TextureDescription from_nrd_texture_desc(const nrd::TextureDesc &texture, glm::ivec2 resolution)
{
	TextureDescription desc;
	desc.width = Math::divideRoundUp(resolution.x, texture.downsampleFactor);
	desc.height = Math::divideRoundUp(resolution.y, texture.downsampleFactor);
	desc.format = from_nrd_format(texture.format);
	desc.usage_flags = TextureUsage::UNORDERED_ACCESS;
	return desc;
}

static RHIShaderRef create_nrd_shader(const char *shader_identifier)
{
	// shader_identifier is like "RELAX_HitDistReconstruction.cs.hlsl|NRD_SIGNAL=BOTH|NRD_MODE=RADIANCE|MODE_5X5=0"

	// Split by '|'
	eastl::vector<eastl::string> tokens;
	eastl::string identifier = shader_identifier;
	size_t start = 0;
	while (start <= identifier.size())
	{
		size_t end = identifier.find('|', start);
		if (end == eastl::string::npos)
			end = identifier.size();
		tokens.push_back(identifier.substr(start, end - start));
		start = end + 1;
	}

	tokens[0] = "NRD_SHADER_FILE=\"" + tokens[0] + "\"";

	// Convert to defines
	eastl::vector<eastl::pair<const char *, const char *>> defines;
	for (eastl::string &token : tokens)
	{
		size_t equals = token.find('=');
		token[equals] = '\0';
		defines.push_back({token.c_str(), token.c_str() + equals + 1});
	}
	return gDynamicRHI->createShader(L"shaders/nrd/nrd_dispatch.hlsl", ShaderType::COMPUTE, "main", defines);
}

NRDDenoiser::~NRDDenoiser()
{
	destroy_instance();
}

bool NRDDenoiser::create_instance()
{
	glm::ivec2 resolution = Renderer::getRenderResolution();
	if (instance && instance_resolution == resolution)
		return false;

	destroy_instance();
	instance_resolution = resolution;

	// Instance
	nrd::DenoiserDesc denoisers[] = {
		{RELAX_IDENTIFIER, nrd::Denoiser::RELAX_DIFFUSE_SPECULAR},
		{REFERENCE_DIFFUSE_IDENTIFIER, nrd::Denoiser::REFERENCE},
		{REFERENCE_SPECULAR_IDENTIFIER, nrd::Denoiser::REFERENCE},
	};
	nrd::InstanceCreationDesc creation_desc{};
	creation_desc.denoisers = denoisers;
	creation_desc.denoisersNum = _countof(denoisers);
	nrd::CreateInstance(creation_desc, instance);

	const nrd::InstanceDesc *instance_desc = nrd::GetInstanceDesc(*instance);

	// Pipelines
	for (int i = 0; i < instance_desc->pipelinesNum; i++)
		shaders.push_back(create_nrd_shader(instance_desc->pipelines[i].shaderIdentifier));
	return true;
}

void NRDDenoiser::destroy_instance()
{
	if (instance)
		nrd::DestroyInstance(*instance);
	instance = nullptr;
	shaders.clear();
}

void NRDDenoiser::add_guide_passes(FrameGraph &fg)
{
	fg.addCallbackPass("NRD Guides",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(NRDViewZ), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R32_SFLOAT);
		builder.writeUAVTexture(GFXRID(NRDViewZ));
		builder.createTexture(GFXRID(NRDNormalRoughness), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_UNORM);
		builder.writeUAVTexture(GFXRID(NRDNormalRoughness));
		builder.createTexture(GFXRID(NRDMotion), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
		builder.writeUAVTexture(GFXRID(NRDMotion));

		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(MotionVectors));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/nrd/nrd_guides.hlsl", ShaderType::COMPUTE, "CSMain"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t shading_tex_id;
			uint32_t motion_vectors_tex_id;
			uint32_t view_z_tex_id;
			uint32_t normal_roughness_tex_id;
			uint32_t motion_tex_id;
		} constants;
		constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		constants.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
		constants.motion_vectors_tex_id = resources.getReadTexture(GFXRID(MotionVectors));
		constants.view_z_tex_id = resources.getReadWriteTexture(GFXRID(NRDViewZ));
		constants.normal_roughness_tex_id = resources.getReadWriteTexture(GFXRID(NRDNormalRoughness));
		constants.motion_tex_id = resources.getReadWriteTexture(GFXRID(NRDMotion));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(Renderer::getRenderWidth(), 8), Math::divideRoundUp(Renderer::getRenderHeight(), 8), 1);
	});
}

void NRDDenoiser::set_common_settings(bool reset_history, bool validation, bool history_confidence)
{
	const PassConstants &pass_constants = Renderer::getPassConstants();
	glm::mat4 old_view = reset_history ? pass_constants.view : pass_constants.old_view;
	glm::mat4 old_projection = reset_history ? pass_constants.projection_unjittered : pass_constants.old_projection_unjittered;
	glm::vec2 old_jitter = reset_history ? pass_constants.jitter : pass_constants.old_jitter;

	nrd::CommonSettings settings{};
	memcpy(settings.viewToClipMatrix, &pass_constants.projection_unjittered, sizeof(pass_constants.projection_unjittered));
	memcpy(settings.viewToClipMatrixPrev, &old_projection, sizeof(old_projection));
	memcpy(settings.worldToViewMatrix, &pass_constants.view, sizeof(pass_constants.view));
	memcpy(settings.worldToViewMatrixPrev, &old_view, sizeof(old_view));
	settings.motionVectorScale[0] = 1.0f;
	settings.motionVectorScale[1] = 1.0f;
	settings.motionVectorScale[2] = 1.0f;
	settings.cameraJitter[0] = pass_constants.jitter.x;
	settings.cameraJitter[1] = pass_constants.jitter.y;
	settings.cameraJitterPrev[0] = old_jitter.x;
	settings.cameraJitterPrev[1] = old_jitter.y;
	settings.resourceSize[0] = instance_resolution.x;
	settings.resourceSize[1] = instance_resolution.y;
	settings.resourceSizePrev[0] = instance_resolution.x;
	settings.resourceSizePrev[1] = instance_resolution.y;
	settings.rectSize[0] = instance_resolution.x;
	settings.rectSize[1] = instance_resolution.y;
	settings.rectSizePrev[0] = instance_resolution.x;
	settings.rectSizePrev[1] = instance_resolution.y;
	settings.denoisingRange = pass_constants.z_far;
	settings.frameIndex = gDynamicRHI->getFrame();
	settings.accumulationMode = reset_history ? nrd::AccumulationMode::CLEAR_AND_RESTART : nrd::AccumulationMode::CONTINUE;
	settings.enableValidation = validation;
	settings.splitScreen = GFXOPTIONS(lighting).denoiser_split_screen;
	settings.isHistoryConfidenceAvailable = history_confidence;
	nrd::SetCommonSettings(*instance, settings);
}

bool NRDDenoiser::begin_frame(nrd::Identifier identifier)
{
	bool recreated = create_instance();
	bool reset_history = recreated || gDynamicRHI->getFrame() != last_frame + 1 || identifier != last_identifier;

	last_frame = gDynamicRHI->getFrame();
	last_identifier = identifier;
	return reset_history;
}

void NRDDenoiser::add_dispatch_passes(FrameGraph &fg, nrd::Identifier identifier, const NRDResourceNames &resource_names)
{
	const nrd::InstanceDesc *instance_desc = nrd::GetInstanceDesc(*instance);

	const nrd::DispatchDesc *dispatches = nullptr;
	uint32_t dispatches_count = 0;
	nrd::GetComputeDispatches(*instance, &identifier, 1, dispatches, dispatches_count);

	struct ResourceIds
	{
		glm::uvec4 inputs[8];
		glm::uvec4 outputs[4];
	};

	eastl::vector<uint8_t> constant_data;
	for (int dispatch_index = 0; dispatch_index < dispatches_count; dispatch_index++)
	{
		const nrd::DispatchDesc &dispatch = dispatches[dispatch_index];
		eastl::vector<nrd::ResourceDesc> dispatch_resources(dispatch.resources, dispatch.resources + dispatch.resourcesNum);

		if (!dispatch.constantBufferDataMatchesPreviousDispatch)
			constant_data.assign(dispatch.constantBufferData, dispatch.constantBufferData + dispatch.constantBufferDataSize);

		bool has_constants = instance_desc->pipelines[dispatch.pipelineIndex].hasConstantData;
		RHIShaderRef shader = shaders[dispatch.pipelineIndex];
		uint32_t grid_width = dispatch.gridWidth;
		uint32_t grid_height = dispatch.gridHeight;

		fg.addCallbackPass(dispatch.name,
		[&](RenderPassBuilder &builder)
		{
			for (const nrd::ResourceDesc &resource : dispatch_resources)
			{
				GraphicsResourceName name = get_graphics_resource_name(resource, resource_names);

				if (resource.type == nrd::ResourceType::TRANSIENT_POOL && !builder.isTextureCreated(name))
					builder.createTexture(name, from_nrd_texture_desc(instance_desc->transientPool[resource.indexInPool], instance_resolution));
				else if (resource.type == nrd::ResourceType::PERMANENT_POOL)
					builder.createPersistentTexture(name, from_nrd_texture_desc(instance_desc->permanentPool[resource.indexInPool], instance_resolution));
				else if (resource.type == nrd::ResourceType::OUT_VALIDATION)
					builder.createPersistentTexture(name, instance_resolution.x, instance_resolution.y, Format::R8G8B8A8_UNORM);

				if (resource.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE)
					builder.writeUAVTexture(name);
				else
					builder.readTexture(name);
			}
		},
		[=](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			gGlobalPipeline->setupComputePipeline(shader);
			gGlobalPipeline->flushAndBind(cmd_list);

			ResourceIds ids = {};
			int input_index = 0;
			int output_index = 0;
			for (const nrd::ResourceDesc &resource : dispatch_resources)
			{
				GraphicsResourceName name = get_graphics_resource_name(resource, resource_names);
				if (resource.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE)
				{
					ids.outputs[output_index / 4][output_index % 4] = resources.getReadWriteTexture(name);
					output_index++;
				} else
				{
					ids.inputs[input_index / 4][input_index % 4] = resources.getReadTexture(name);
					input_index++;
				}
			}

			if (has_constants)
				gDynamicRHI->setConstantBufferData(0, (void *)constant_data.data(), constant_data.size());
			gDynamicRHI->setConstantBufferData(1, &ids, sizeof(ids));
			cmd_list->dispatch(grid_width, grid_height, 1);
		});
	}
}

void NRDDenoiser::addRelaxPasses(FrameGraph &fg, const NRDRelaxInputs &inputs)
{
	bool reset_history = begin_frame(RELAX_IDENTIFIER);

	bool validation = GFXOPTIONS(lighting).denoiser_validation;

	add_guide_passes(fg);

	set_common_settings(reset_history, validation, inputs.diffuse_confidence.name != nullptr);

	nrd::RelaxSettings relax_settings = to_nrd_relax_settings(GFXOPTIONS(lighting).relax);
	nrd::SetDenoiserSettings(*instance, RELAX_IDENTIFIER, &relax_settings);

	NRDResourceNames resource_names;
	resource_names[(size_t)nrd::ResourceType::IN_MV] = GFXRID(NRDMotion);
	resource_names[(size_t)nrd::ResourceType::IN_NORMAL_ROUGHNESS] = GFXRID(NRDNormalRoughness);
	resource_names[(size_t)nrd::ResourceType::IN_VIEWZ] = GFXRID(NRDViewZ);
	resource_names[(size_t)nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST] = inputs.diffuse_radiance_hit_distance;
	resource_names[(size_t)nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST] = inputs.specular_radiance_hit_distance;
	resource_names[(size_t)nrd::ResourceType::IN_DIFF_CONFIDENCE] = inputs.diffuse_confidence;
	resource_names[(size_t)nrd::ResourceType::IN_SPEC_CONFIDENCE] = inputs.specular_confidence;
	resource_names[(size_t)nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST] = inputs.diffuse_output;
	resource_names[(size_t)nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST] = inputs.specular_output;
	resource_names[(size_t)nrd::ResourceType::OUT_VALIDATION] = GFXRID(NRDValidation);
	add_dispatch_passes(fg, RELAX_IDENTIFIER, resource_names);

	if (validation)
		DebugRenderer::addDebugOverlay(GFXRID(NRDValidation));
}

void NRDDenoiser::addReferencePasses(FrameGraph &fg, const NRDRelaxInputs &inputs)
{
	bool reset_history = begin_frame(REFERENCE_DIFFUSE_IDENTIFIER);

	set_common_settings(reset_history, false, false);

	nrd::ReferenceSettings settings;
	settings.maxAccumulatedFrameNum = nrd::REFERENCE_MAX_HISTORY_FRAME_NUM;
	nrd::SetDenoiserSettings(*instance, REFERENCE_DIFFUSE_IDENTIFIER, &settings);
	nrd::SetDenoiserSettings(*instance, REFERENCE_SPECULAR_IDENTIFIER, &settings);

	NRDResourceNames diffuse_names;
	diffuse_names[(size_t)nrd::ResourceType::IN_SIGNAL] = inputs.diffuse_radiance_hit_distance;
	diffuse_names[(size_t)nrd::ResourceType::OUT_SIGNAL] = inputs.diffuse_output;
	add_dispatch_passes(fg, REFERENCE_DIFFUSE_IDENTIFIER, diffuse_names);

	NRDResourceNames specular_names;
	specular_names[(size_t)nrd::ResourceType::IN_SIGNAL] = inputs.specular_radiance_hit_distance;
	specular_names[(size_t)nrd::ResourceType::OUT_SIGNAL] = inputs.specular_output;
	add_dispatch_passes(fg, REFERENCE_SPECULAR_IDENTIFIER, specular_names);
}
