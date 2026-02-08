#pragma once
#include "FrameGraph.h"

#define FinalNoPostTexture
#define FinalTexture
#define BackbufferTexture

#define InstancesPassMask

#define CandidateMeshlets
#define CandidateMeshletsCount
#define VisibleMeshlets
#define VisibleMeshletsCount

#define DispatchMeshIndirectArgs
#define DispatchMeshletIndirectArgs
#define DrawIndexedArgs
#define DrawIndexedCount
#define DrawCallsInstances
#define IndirectVisibility
#define MeshletVisibility

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

#define RayTracedVisibility

#define PathTraceAccumulation

#define DDGIDistance
#define DDGIIrradiance
#define DDGIMetadata

struct ShadowPasses
{
	eastl::vector<GraphicsResourceName> shadow_maps;
};