#pragma once
#include "FrameGraph.h"

#define FinalNoPostTexture
#define FinalTexture
#define BackbufferTexture

struct EmptyData {};

#define GBufferAlbedo
#define GBufferNormal
#define GBufferDepth
#define GBufferShading

#define HiZ

#define SSAONoiseTexture
#define SSAORaw
#define SSAOBlurred

#define DiffuseLight
#define SpecularLight

#define SSR

#define CompositeIndirectAmbient
#define CompositeIndirectSpecular

#define Sky

#define LutBRDF

#define IBLIrradiance
#define IBLPrefilter

#define PathTraceAccumulation

#define DDGIDistance
#define DDGIIrradiance
#define DDGIMetadata

struct RayTracedShadowPass
{
	FrameGraphTextureId visibility;
};

struct ShadowPasses
{
	eastl::vector<FrameGraphTextureId> shadow_maps;
};