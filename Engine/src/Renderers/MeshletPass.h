#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Rendering/ShaderStructs.h"

struct MeshletCullDesc
{
	uint32_t pass_mask;
	glm::mat4 view_projection;
	uint32_t instance_count;
};

// Uses two-pass occlusion culling:
// Main Pass: frustum + prev-frame HZB. Occluded saved to list
// Fix Pass: flat dispatch over occluded lists against current-frame HZB.
class MeshletPass
{
public:
	void addMainCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc);
	void addFixCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc);

private:
	void add_counter_init_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix);
	void add_instance_culling_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix);
	void add_traversal_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix);
	void add_meshlet_fix_pass(FrameGraph &fg, const MeshletCullDesc &desc);
	void add_dispatch_args_pass(FrameGraph &fg, const char *name, GraphicsResourceName args_id, GraphicsResourceName count_id, uint32_t group_size);
};
