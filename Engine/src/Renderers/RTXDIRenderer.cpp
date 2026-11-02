#include "pch.h"
#include "RTXDIRenderer.h"
#include "Rendering/GlobalPipeline.h"
#include "DebugRenderer.h"

static const uint32_t RTXDI_GRADIENT_FACTOR = 3;

struct RTXDIConstants
{
	RTXDI_RuntimeParameters runtime_params;
	RTXDI_LightBufferParameters light_buffer_params;
	RTXDI_ReservoirBufferParameters reservoir_params;
	RTXDI_DIBufferIndices buffer_indices;
	RTXDI_DIInitialSamplingParameters initial_sampling_params;
	RTXDI_DITemporalResamplingParameters temporal_params;
	RTXDI_DISpatialResamplingParameters spatial_params;
	RTXDI_ShadingParameters shading_params;
	uint32_t depth_tex_id;
	uint32_t normal_tex_id;
	uint32_t albedo_tex_id;
	uint32_t shading_tex_id;
	uint32_t prev_depth_tex_id;
	uint32_t prev_normal_tex_id;
	uint32_t prev_albedo_tex_id;
	uint32_t prev_shading_tex_id;
	uint32_t motion_vectors_tex_id;
	uint32_t diffuse_tex_id;
	uint32_t specular_tex_id;
	uint32_t reservoirs_buffer_id;
	uint32_t neighbor_offsets_buffer_id;
	uint32_t temporal_sample_positions_tex_id;
	uint32_t luminance_tex_id;
	uint32_t prev_luminance_tex_id;
	uint32_t gradients_tex_id;
};

static void read_gbuffer(RenderPassBuilder &builder)
{
	builder.readTexture(GFXRID(GBufferDepth));
	builder.readTexture(GFXRID(GBufferNormal));
	builder.readTexture(GFXRID(GBufferAlbedo));
	builder.readTexture(GFXRID(GBufferShading));
	builder.readTexture(GFXRID(RTXDIPrevDepth));
	builder.readTexture(GFXRID(RTXDIPrevNormal));
	builder.readTexture(GFXRID(RTXDIPrevAlbedo));
	builder.readTexture(GFXRID(RTXDIPrevShading));
}

static void bind_gbuffer(RTXDIConstants &constants, const RenderPassResources &resources)
{
	constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
	constants.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
	constants.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
	constants.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
	constants.prev_depth_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevDepth));
	constants.prev_normal_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevNormal));
	constants.prev_albedo_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevAlbedo));
	constants.prev_shading_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevShading));
}

void RTXDIRenderer::create_context()
{
	rtxdi::ReSTIRDIStaticParameters static_params;
	static_params.RenderWidth = Renderer::getRenderWidth();
	static_params.RenderHeight = Renderer::getRenderHeight();
	if (context && context->GetStaticParameters().RenderWidth == static_params.RenderWidth && context->GetStaticParameters().RenderHeight == static_params.RenderHeight)
		return;

	// Context
	context = std::make_unique<rtxdi::ReSTIRDIContext>(static_params);
	context->SetResamplingMode(rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial);

	// No emissive geometry or environment map to sample yet
	RTXDI_DIInitialSamplingParameters initial_sampling_params = context->GetInitialSamplingParameters();
	initial_sampling_params.numEnvironmentSamples = 0;
	initial_sampling_params.numBrdfSamples = 0;
	context->SetInitialSamplingParameters(initial_sampling_params);

	// Neighbor offsets
	int offsets_count = static_params.NeighborOffsetCount;
	eastl::vector<uint8_t> packed_offsets(offsets_count * 2);
	rtxdi::FillNeighborOffsetBuffer(packed_offsets.data(), offsets_count);

	eastl::vector<glm::vec2> offsets(offsets_count);
	for (int i = 0; i < offsets_count; i++)
		offsets[i] = glm::vec2((int8_t)packed_offsets[i * 2], (int8_t)packed_offsets[i * 2 + 1]) / 127.0f;

	BufferDescription desc;
	desc.size = offsets_count * sizeof(glm::vec2);
	desc.stride = sizeof(glm::vec2);
	desc.usage_flags = BufferUsage::SHADER_RESOURCE;
	neighbor_offsets_buffer = gDynamicRHI->createBuffer(desc, offsets.data());
	neighbor_offsets_buffer->setDebugName("RTXDI Neighbor Offsets");
}

void RTXDIRenderer::set_context_parameters()
{
	const RTXDISettings &settings = GFXOPTIONS(lighting).rtxdi;

	RTXDI_DITemporalResamplingParameters temporal_params = context->GetTemporalResamplingParameters();
	temporal_params.maxHistoryLength = settings.max_history_length;
	temporal_params.biasCorrectionMode = (ReSTIRDI_TemporalBiasCorrectionMode)(settings.bias_correction == BIAS_CORRECTION_PAIRWISE ? BIAS_CORRECTION_BASIC : settings.bias_correction);
	temporal_params.depthThreshold = settings.depth_threshold;
	temporal_params.normalThreshold = settings.normal_threshold;
	temporal_params.enableVisibilityShortcut = settings.visibility_shortcut;
	temporal_params.enablePermutationSampling = settings.permutation_sampling;
	context->SetTemporalResamplingParameters(temporal_params);

	RTXDI_DISpatialResamplingParameters spatial_params = context->GetSpatialResamplingParameters();
	spatial_params.numSamples = settings.spatial_samples;
	spatial_params.numDisocclusionBoostSamples = settings.disocclusion_boost_samples;
	spatial_params.targetHistoryLength = 0;
	spatial_params.samplingRadius = settings.sampling_radius;
	spatial_params.biasCorrectionMode = (ReSTIRDI_SpatialBiasCorrectionMode)settings.bias_correction;
	spatial_params.depthThreshold = settings.depth_threshold;
	spatial_params.normalThreshold = settings.normal_threshold;
	spatial_params.enableMaterialSimilarityTest = settings.material_similarity_test;
	spatial_params.discountNaiveSamples = settings.discount_naive_samples;
	context->SetSpatialResamplingParameters(spatial_params);

	RTXDI_ShadingParameters shading_params = context->GetShadingParameters();
	shading_params.reuseFinalVisibility = settings.reuse_final_visibility;
	context->SetShadingParameters(shading_params);
}

void RTXDIRenderer::add_denoise_confidence_passes(FrameGraph &fg, const RTXDIConstants &frame_constants)
{
	uint32_t gradient_width = Math::divideRoundUp(Renderer::getRenderWidth(), RTXDI_GRADIENT_FACTOR);
	uint32_t gradient_height = Math::divideRoundUp(Renderer::getRenderHeight(), RTXDI_GRADIENT_FACTOR);

	fg.addCallbackPass("RTXDI Gradients",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID_ID(RTXDIGradients, 0), gradient_width, gradient_height, Format::R16G16B16A16_SFLOAT);
		builder.createTexture(GFXRID_ID(RTXDIGradients, 1), gradient_width, gradient_height, Format::R16G16B16A16_SFLOAT);
		builder.createPersistentTexture(GFXRID(RTXDIPrevLuminance), builder.getTextureDescription(GFXRID(RTXDILuminance)));
		builder.writeUAVTexture(GFXRID_ID(RTXDIGradients, 0));
		builder.writeBuffer(GFXRID(RTXDIReservoirs));
		read_gbuffer(builder);
		builder.readTexture(GFXRID(MotionVectors));
		builder.readTexture(GFXRID(RTXDITemporalSamplePositions));
		builder.readTexture(GFXRID(RTXDILuminance));
		builder.readTexture(GFXRID(RTXDIPrevLuminance));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/rtxdi/confidence/rtxdi_gradients.hlsl", ShaderType::COMPUTE, "CSMain"));
		gGlobalPipeline->flushAndBind(cmd_list);

		RTXDIConstants constants = frame_constants;
		bind_gbuffer(constants, resources);
		constants.reservoirs_buffer_id = resources.getReadWriteBuffer(GFXRID(RTXDIReservoirs));
		constants.motion_vectors_tex_id = resources.getReadTexture(GFXRID(MotionVectors));
		constants.temporal_sample_positions_tex_id = resources.getReadTexture(GFXRID(RTXDITemporalSamplePositions));
		constants.luminance_tex_id = resources.getReadTexture(GFXRID(RTXDILuminance));
		constants.prev_luminance_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevLuminance));
		constants.gradients_tex_id = resources.getReadWriteTexture(GFXRID_ID(RTXDIGradients, 0));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(gradient_width, 8), Math::divideRoundUp(gradient_height, 8), 1);
	});

	const int filter_passes = 4;
	for (int pass = 0; pass < filter_passes; pass++)
	{
		GraphicsResourceName input = GFXRID_ID(RTXDIGradients, pass & 1);
		GraphicsResourceName output = GFXRID_ID(RTXDIGradients, (pass + 1) & 1);
		fg.addCallbackPass("RTXDI Gradients Filter",
		[&](RenderPassBuilder &builder)
		{
			builder.readTexture(input);
			builder.writeUAVTexture(output);
		},
		[=](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/rtxdi/confidence/rtxdi_gradients_filter.hlsl", ShaderType::COMPUTE, "CSMain"));
			gGlobalPipeline->flushAndBind(cmd_list);

			struct Constants
			{
				uint32_t input_tex_id;
				uint32_t output_tex_id;
				uint32_t pass_index;
			} constants;
			constants.input_tex_id = resources.getReadTexture(input);
			constants.output_tex_id = resources.getReadWriteTexture(output);
			constants.pass_index = pass;
			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			cmd_list->dispatch(Math::divideRoundUp(gradient_width, 8), Math::divideRoundUp(gradient_height, 8), 1);
		});
	}

	GraphicsResourceName filtered_gradients = GFXRID_ID(RTXDIGradients, filter_passes & 1);
	fg.addCallbackPass("RTXDI Confidence",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(RTXDIDiffuseConfidence), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R8_UNORM);
		builder.createTexture(GFXRID(RTXDISpecularConfidence), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R8_UNORM);
		builder.writeUAVTexture(GFXRID(RTXDIDiffuseConfidence));
		builder.writeUAVTexture(GFXRID(RTXDISpecularConfidence));
		builder.createPersistentTexture(GFXRID(RTXDIPrevDiffuseConfidence), builder.getTextureDescription(GFXRID(RTXDIDiffuseConfidence)));
		builder.createPersistentTexture(GFXRID(RTXDIPrevSpecularConfidence), builder.getTextureDescription(GFXRID(RTXDISpecularConfidence)));

		builder.readTexture(filtered_gradients);
		builder.readTexture(GFXRID(MotionVectors));
		builder.readTexture(GFXRID(RTXDIPrevDiffuseConfidence));
		builder.readTexture(GFXRID(RTXDIPrevSpecularConfidence));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/rtxdi/confidence/rtxdi_confidence.hlsl", ShaderType::COMPUTE, "CSMain"));
		gGlobalPipeline->flushAndBind(cmd_list);

		const ConfidenceSettings &settings = GFXOPTIONS(lighting).confidence;
		struct Constants
		{
			uint32_t gradients_tex_id;
			uint32_t motion_vectors_tex_id;
			uint32_t prev_diffuse_confidence_tex_id;
			uint32_t prev_specular_confidence_tex_id;
			uint32_t diffuse_confidence_tex_id;
			uint32_t specular_confidence_tex_id;
			float darkness_bias;
			float sensitivity;
			float blend_factor;
		} constants;
		constants.gradients_tex_id = resources.getReadTexture(filtered_gradients);
		constants.motion_vectors_tex_id = resources.getReadTexture(GFXRID(MotionVectors));
		constants.prev_diffuse_confidence_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevDiffuseConfidence));
		constants.prev_specular_confidence_tex_id = resources.getReadTexture(GFXRID(RTXDIPrevSpecularConfidence));
		constants.diffuse_confidence_tex_id = resources.getReadWriteTexture(GFXRID(RTXDIDiffuseConfidence));
		constants.specular_confidence_tex_id = resources.getReadWriteTexture(GFXRID(RTXDISpecularConfidence));
		constants.darkness_bias = glm::exp2(settings.darkness_bias_ev);
		constants.sensitivity = settings.sensitivity;
		constants.blend_factor = 1.0f / (settings.history_length + 1.0f);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(Renderer::getRenderWidth(), 8), Math::divideRoundUp(Renderer::getRenderHeight(), 8), 1);
	});

	ConfidenceVisualize visualize = GFXOPTIONS(lighting).confidence.visualize;
	if (visualize == CONFIDENCE_VISUALIZE_DIFFUSE)
		DebugRenderer::addDebugOverlay(GFXRID(RTXDIDiffuseConfidence), L"shaders/rtxdi/confidence/rtxdi_confidence_visualize.hlsl");
	else if (visualize == CONFIDENCE_VISUALIZE_SPECULAR)
		DebugRenderer::addDebugOverlay(GFXRID(RTXDISpecularConfidence), L"shaders/rtxdi/confidence/rtxdi_confidence_visualize.hlsl");
}

void RTXDIRenderer::add_denoise_resolve_pass(FrameGraph &fg)
{
	fg.addCallbackPass("RTXDI Resolve",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(DiffuseLight), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
		builder.writeUAVTexture(GFXRID(DiffuseLight));
		builder.createTexture(GFXRID(SpecularLight), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
		builder.writeUAVTexture(GFXRID(SpecularLight));

		builder.readTexture(GFXRID(GBufferDepth));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferAlbedo));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(RTXDIDiffuseDenoised));
		builder.readTexture(GFXRID(RTXDISpecularDenoised));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/rtxdi/rtxdi_resolve.hlsl", ShaderType::COMPUTE, "CSMain"));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t depth_tex_id;
			uint32_t normal_tex_id;
			uint32_t albedo_tex_id;
			uint32_t shading_tex_id;
			uint32_t denoised_diffuse_tex_id;
			uint32_t denoised_specular_tex_id;
			uint32_t diffuse_tex_id;
			uint32_t specular_tex_id;
		} constants;
		constants.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		constants.albedo_tex_id = resources.getReadTexture(GFXRID(GBufferAlbedo));
		constants.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
		constants.denoised_diffuse_tex_id = resources.getReadTexture(GFXRID(RTXDIDiffuseDenoised));
		constants.denoised_specular_tex_id = resources.getReadTexture(GFXRID(RTXDISpecularDenoised));
		constants.diffuse_tex_id = resources.getReadWriteTexture(GFXRID(DiffuseLight));
		constants.specular_tex_id = resources.getReadWriteTexture(GFXRID(SpecularLight));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(Math::divideRoundUp(Renderer::getRenderWidth(), 8), Math::divideRoundUp(Renderer::getRenderHeight(), 8), 1);
	});
}

void RTXDIRenderer::addPasses(FrameGraph &fg, LightTableLayout lights_layout)
{
	create_context();
	set_context_parameters();
	context->SetFrameIndex(gDynamicRHI->getFrame());

	DenoiserMode denoiser_mode = GFXOPTIONS(lighting).denoiser;
	bool denoise = denoiser_mode != DENOISER_OFF;
	bool confidence = denoiser_mode == DENOISER_RELAX && GFXOPTIONS(lighting).confidence.enabled;
	GraphicsResourceName diffuse_target_name = denoise ? GFXRID(RTXDIDiffuseNoisy) : GFXRID(DiffuseLight);
	GraphicsResourceName specular_target_name = denoise ? GFXRID(RTXDISpecularNoisy) : GFXRID(SpecularLight);

	eastl::vector<eastl::pair<const char *, const char *>> defines;
	if (denoise)
		defines.push_back({"RTXDI_DENOISER", "1"});
	if (confidence)
		defines.push_back({"RTXDI_CONFIDENCE", "1"});

	RTXDIConstants frame_constants{};
	frame_constants.runtime_params = context->GetRuntimeParams();
	frame_constants.light_buffer_params.infiniteLightBufferRegion.firstLightIndex = 0;
	frame_constants.light_buffer_params.infiniteLightBufferRegion.numLights = lights_layout.directional_count;
	frame_constants.light_buffer_params.localLightBufferRegion.firstLightIndex = lights_layout.directional_count;
	frame_constants.light_buffer_params.localLightBufferRegion.numLights = lights_layout.local_count;
	frame_constants.reservoir_params = context->GetReservoirBufferParameters();
	frame_constants.buffer_indices = context->GetBufferIndices();
	frame_constants.initial_sampling_params = context->GetInitialSamplingParameters();
	frame_constants.temporal_params = context->GetTemporalResamplingParameters();
	frame_constants.spatial_params = context->GetSpatialResamplingParameters();
	frame_constants.shading_params = context->GetShadingParameters();
	frame_constants.neighbor_offsets_buffer_id = neighbor_offsets_buffer->getShaderResourceView()->getBindlessIndex();

	auto add_lighting_pass = [&](const char *name, const char *entry_point)
	{
		fg.addCallbackPass(name,
		[&](RenderPassBuilder &builder)
		{
			if (!builder.isTextureCreated(diffuse_target_name))
			{
				builder.createTexture(diffuse_target_name, Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
				builder.createTexture(specular_target_name, Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
				builder.createPersistentTexture(GFXRID(RTXDIPrevDepth), builder.getTextureDescription(GFXRID(GBufferDepth)));
				builder.createPersistentTexture(GFXRID(RTXDIPrevNormal), builder.getTextureDescription(GFXRID(GBufferNormal)));
				builder.createPersistentTexture(GFXRID(RTXDIPrevAlbedo), builder.getTextureDescription(GFXRID(GBufferAlbedo)));
				builder.createPersistentTexture(GFXRID(RTXDIPrevShading), builder.getTextureDescription(GFXRID(GBufferShading)));
				builder.createPersistentBuffer(GFXRID(RTXDIReservoirs), sizeof(RTXDI_PackedDIReservoir), context->GetReservoirBufferParameters().reservoirArrayPitch * rtxdi::c_NumReSTIRDIReservoirBuffers, BufferUsage::SHADER_RESOURCE | BufferUsage::UNORDERED_ACCESS);
				if (confidence)
				{
					builder.createTexture(GFXRID(RTXDITemporalSamplePositions), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16_SINT);
					builder.createTexture(GFXRID(RTXDILuminance), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16_SFLOAT);
				}
				if (denoise)
				{
					builder.createTexture(GFXRID(RTXDIDiffuseDenoised), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
					builder.createTexture(GFXRID(RTXDISpecularDenoised), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R16G16B16A16_SFLOAT);
				}
			}

			read_gbuffer(builder);
			builder.readTexture(GFXRID(MotionVectors));
			builder.writeBuffer(GFXRID(RTXDIReservoirs));
			builder.writeUAVTexture(diffuse_target_name);
			builder.writeUAVTexture(specular_target_name);
			if (confidence)
			{
				builder.writeUAVTexture(GFXRID(RTXDITemporalSamplePositions));
				builder.writeUAVTexture(GFXRID(RTXDILuminance));
			}
		},
		[=](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/rtxdi/rtxdi_lighting.hlsl", ShaderType::COMPUTE, entry_point, defines));
			gGlobalPipeline->flushAndBind(cmd_list);

			RTXDIConstants constants = frame_constants;
			bind_gbuffer(constants, resources);
			constants.reservoirs_buffer_id = resources.getReadWriteBuffer(GFXRID(RTXDIReservoirs));
			constants.motion_vectors_tex_id = resources.getReadTexture(GFXRID(MotionVectors));
			constants.diffuse_tex_id = resources.getReadWriteTexture(diffuse_target_name);
			constants.specular_tex_id = resources.getReadWriteTexture(specular_target_name);
			if (confidence)
			{
				constants.temporal_sample_positions_tex_id = resources.getReadWriteTexture(GFXRID(RTXDITemporalSamplePositions));
				constants.luminance_tex_id = resources.getReadWriteTexture(GFXRID(RTXDILuminance));
			}
			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			cmd_list->dispatch(Math::divideRoundUp(Renderer::getRenderWidth(), 8), Math::divideRoundUp(Renderer::getRenderHeight(), 8), 1);
		});
	};
	add_lighting_pass("RTXDI Initial Sampling", "InitialSamplingCS");
	add_lighting_pass("RTXDI Temporal Resampling", "TemporalResamplingCS");
	add_lighting_pass("RTXDI Spatial Resampling", "SpatialResamplingCS");
	add_lighting_pass("RTXDI Shading", "ShadingCS");

	// Denoising
	if (denoise)
	{
		if (confidence)
			add_denoise_confidence_passes(fg, frame_constants);

		NRDRelaxInputs denoise_inputs;
		denoise_inputs.diffuse_radiance_hit_distance = GFXRID(RTXDIDiffuseNoisy);
		denoise_inputs.specular_radiance_hit_distance = GFXRID(RTXDISpecularNoisy);
		denoise_inputs.diffuse_output = GFXRID(RTXDIDiffuseDenoised);
		denoise_inputs.specular_output = GFXRID(RTXDISpecularDenoised);
		if (confidence)
		{
			denoise_inputs.diffuse_confidence = GFXRID(RTXDIDiffuseConfidence);
			denoise_inputs.specular_confidence = GFXRID(RTXDISpecularConfidence);
		}
		if (denoiser_mode == DENOISER_REFERENCE)
			denoiser.addReferencePasses(fg, denoise_inputs);
		else
			denoiser.addRelaxPasses(fg, denoise_inputs);

		add_denoise_resolve_pass(fg);
	}

	// History
	auto add_history_copy = [&](GraphicsResourceName source, GraphicsResourceName destination)
	{
		fg.addCallbackPass("RTXDI History Copy",
		[&](RenderPassBuilder &builder)
		{
			builder.readCopyTexture(source);
			builder.writeCopyTexture(destination);
		},
		[=](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			cmd_list->copyTexture(resources.getTexture(source), resources.getTexture(destination));
		});
	};
	add_history_copy(GFXRID(GBufferDepth), GFXRID(RTXDIPrevDepth));
	add_history_copy(GFXRID(GBufferNormal), GFXRID(RTXDIPrevNormal));
	add_history_copy(GFXRID(GBufferAlbedo), GFXRID(RTXDIPrevAlbedo));
	add_history_copy(GFXRID(GBufferShading), GFXRID(RTXDIPrevShading));
	if (confidence)
	{
		add_history_copy(GFXRID(RTXDILuminance), GFXRID(RTXDIPrevLuminance));
		add_history_copy(GFXRID(RTXDIDiffuseConfidence), GFXRID(RTXDIPrevDiffuseConfidence));
		add_history_copy(GFXRID(RTXDISpecularConfidence), GFXRID(RTXDIPrevSpecularConfidence));
	}
}
