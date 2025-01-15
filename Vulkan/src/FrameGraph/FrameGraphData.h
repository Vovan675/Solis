#pragma once
#include "FrameGraph.h"

struct DefaultResourcesData
{
	FrameGraphResource final_no_post;
	FrameGraphResource final;
	FrameGraphResource backbuffer;
};

struct GBufferData
{
	FrameGraphResource albedo;
	FrameGraphResource normal;
	FrameGraphResource depth;
	FrameGraphResource shading;
};

struct DeferredLightingData
{
	FrameGraphResource diffuse_light;
	FrameGraphResource specular_light;
};

struct SSAOData
{
	FrameGraphResource ssao_raw;
	FrameGraphResource ssao_blurred;
};

struct SSRData
{
	FrameGraphResource ssr;
};

struct CompositeData
{
	FrameGraphResource composite_indirect_ambient;
	FrameGraphResource composite_indirect_specular;
};

struct SkyData
{
	FrameGraphResource sky;
};

struct LutData
{
	FrameGraphResource brdf_lut;
};

struct IBLData
{
	FrameGraphResource irradiance;
	FrameGraphResource prefilter;
};

struct CascadeShadowPass
{
	FrameGraphResource shadow_map = -1;
};

struct RayTracedShadowPass
{
	FrameGraphResource visibility;
};

struct ShadowPasses
{
	std::vector<FrameGraphResource> shadow_maps;
};