#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Rendering/ShaderStructs.h"

struct MeshletCullDesc
{
	uint32_t pass_mask;
	glm::mat4 view_projection;
	uint32_t instance_count;
};

// Adds GPU culling passes that produce VisibleMeshlets + dispatch args.
class MeshletPass
{
public:
	void addEarlyCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc);
	void addLateCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc);

private:
	void addPersistentCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late);
	void addFlatCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late);

	void addCounterInitPass(FrameGraph &fg, bool is_late);
	void addInstanceCullingPass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late);
	void addTraversalPass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late);
	void addGeometryDispatchArgsPass(FrameGraph &fg, bool is_late);
};
