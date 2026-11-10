#pragma once
#include "FrameGraph.h"

#define FinalNoPostTexture
#define FinalTexture
#define BackbufferTexture

#define LightsBuffer
#define OldLightsBuffer
#define LightIndexMapping
#define MaterialsBuffer
#define InstancesBuffer
#define MeshesBuffer
#define CullViews
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

#define PageTableBuffer
#define StreamRequestsBuffer
#define PageFramesBuffer

#define DispatchMeshIndirectArgs
#define MeshletFixDispatchArgs
#define InstanceFixDispatchArgs
#define MeshletDrawArgs
#define MeshletDrawCount
#define DrawCallsInstances

#define TraditionalDrawArgs
#define TraditionalDrawCount
#define TraditionalDrawInstances

#define GBufferAlbedo
#define GBufferNormal
#define GBufferDepth
#define GBufferShading
#define GBufferEmissive
#define MotionVectors

#define HiZTexture
#define CascadeHiZ

#define SSAONoiseTexture
#define SSAORaw
#define SSAOBlurred

#define DiffuseLight
#define SpecularLight

#define SSR

#define Bloom

#define TonemapLUT

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
#define DDGIRayData
#define DDGIProbesToUpdate

#define RTXDIPrevDepth
#define RTXDIPrevNormal
#define RTXDIPrevAlbedo
#define RTXDIPrevShading

#define NRDViewZ
#define NRDNormalRoughness
#define NRDMotion

#define RTXDIDiffuseNoisy
#define RTXDISpecularNoisy
#define RTXDIDiffuseDenoised
#define RTXDISpecularDenoised
#define RTXDIReservoirs
#define RTXDITemporalSamplePositions
#define RTXDILuminance
#define RTXDIPrevLuminance
#define RTXDIGradients
#define RTXDIDiffuseConfidence
#define RTXDISpecularConfidence
#define RTXDIPrevDiffuseConfidence
#define RTXDIPrevSpecularConfidence

#define NRDPermanent
#define NRDTransient
#define NRDValidation

#define DebugDepth
#define DebugLinesBuffer
#define DebugLinesDrawArgsBuffer

struct ShadowPasses
{
	eastl::vector<GraphicsResourceName> shadow_maps;
};