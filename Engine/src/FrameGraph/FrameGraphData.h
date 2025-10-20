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


struct RayTracedShadowPass
{
	FrameGraphTextureId visibility;
};

struct ShadowPasses
{
	eastl::vector<FrameGraphTextureId> shadow_maps;
};