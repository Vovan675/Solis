#pragma once
#include "FrameGraph.h"

struct DefaultResourcesData
{
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

struct DefferedLightingData
{
	FrameGraphResource diffuse_light;
	FrameGraphResource specular_light;
};

struct SSAOData
{
	FrameGraphResource ssao_raw;
	FrameGraphResource ssao_blurred;
};

struct CompositeData
{
	FrameGraphResource composite;
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