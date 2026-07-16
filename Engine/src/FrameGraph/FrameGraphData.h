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
#define OccludedMeshlets
#define OccludedMeshletsCount
#define OccludedInstances
#define OccludedInstancesCount

#define GroupResidencyBuffer
#define StreamRequestsBuffer
#define GroupAgesBuffer

#define DispatchMeshIndirectArgs
#define MeshletFixDispatchArgs
#define InstanceFixDispatchArgs
#define DrawIndexedArgs
#define DrawIndexedCount
#define DrawCallsInstances

#define TraditionalDrawArgs
#define TraditionalDrawCount
#define TraditionalDrawInstances

#define GBufferAlbedo
#define GBufferNormal
#define GBufferDepth
#define GBufferShading
#define MotionVectors

#define HiZTexture
#define CascadeHiZ

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