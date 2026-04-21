#pragma once
#include "FrameGraph.h"

#define FinalNoPostTexture
#define FinalTexture
#define BackbufferTexture

#define InstancesPassMask

// Persistent thread traversal
#define TraversalQueue
#define TraversalCtrl
#define VisibleMeshlets
#define VisibleMeshletsCount
#define MeshletVisibility

#define GroupResidencyBuffer
#define StreamRequestsBuffer
#define GroupAgesBuffer

#define DispatchMeshIndirectArgs
#define DrawIndexedArgs
#define DrawIndexedCount
#define DrawCallsInstances
#define IndirectVisibility

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

#define DebugLinesBuffer
#define DebugLinesDrawArgsBuffer

struct ShadowPasses
{
	eastl::vector<GraphicsResourceName> shadow_maps;
};