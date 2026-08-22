#include "pch.h"
#include "MeshletPass.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "Rendering/GlobalPipeline.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Core/Variables.h"

namespace
{
constexpr uint32_t MAX_MESHLETS_PER_FRAME = 1 << 23;
constexpr uint32_t MAX_TRAVERSAL_TASKS = 1 << 22;
constexpr uint32_t PERSISTENT_THREAD_GROUPS = 64;
constexpr uint32_t INSTANCE_CULLING_THREADGROUP_SIZE = 32;
constexpr uint32_t MESHLET_CULLING_THREADGROUP_SIZE = 32;
constexpr uint32_t MESHLET_FIX_THREADGROUP_SIZE = 64;

struct TraversalItem
{
	uint32_t instance_id;
	uint32_t packed;
};

struct MeshletCandidate
{
	uint32_t instance_id;
	uint32_t meshlet_id;
};
}

void MeshletPass::addMainCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc)
{
	add_counter_init_pass(fg, desc, false);
	add_instance_culling_pass(fg, desc, false);
	add_traversal_pass(fg, desc, false);
	if (render_meshlets_mesh_shaders)
		add_dispatch_args_pass(fg, "Build Geometry Dispatch Args", GFXRID_ID(DispatchMeshIndirectArgs, desc.view_id), GFXRID_ID(VisibleMeshletsCount, desc.view_id), 1);
}

void MeshletPass::addFixCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc)
{
	add_counter_init_pass(fg, desc, true);

	// Instance fix
	add_dispatch_args_pass(fg, "Build Instance Fix Dispatch Args", GFXRID_ID(InstanceFixDispatchArgs, desc.view_id), GFXRID_ID(OccludedInstancesCount, desc.view_id), INSTANCE_CULLING_THREADGROUP_SIZE);
	add_instance_culling_pass(fg, desc, true);
	add_traversal_pass(fg, desc, true);

	// Meshlet fix
	add_dispatch_args_pass(fg, "Build Meshlet Fix Dispatch Args", GFXRID_ID(MeshletFixDispatchArgs, desc.view_id), GFXRID_ID(OccludedMeshletsCount, desc.view_id), MESHLET_FIX_THREADGROUP_SIZE);
	add_meshlet_fix_pass(fg, desc);

	if (render_meshlets_mesh_shaders)
		add_dispatch_args_pass(fg, "Build Geometry Dispatch Args", GFXRID_ID(DispatchMeshIndirectArgs, desc.view_id), GFXRID_ID(VisibleMeshletsCount, desc.view_id), 1);
}

void MeshletPass::add_counter_init_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix)
{
	uint32_t view_id = desc.view_id;
	fg.addCallbackPass("Init Counters",
	[&, is_fix, view_id, instance_count = desc.instance_count](RenderPassBuilder &builder)
	{
		if (!is_fix)
		{
			builder.createBuffer(GFXRID_ID(TraversalQueue, view_id), sizeof(TraversalItem), MAX_TRAVERSAL_TASKS, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(TraversalCtrl, view_id), sizeof(uint32_t) * 4, 1, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(VisibleMeshlets, view_id), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(VisibleMeshletsCount, view_id), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(DrawIndexedCount, view_id), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(OccludedMeshlets, view_id), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(OccludedMeshletsCount, view_id), sizeof(uint32_t), 1, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(OccludedInstances, view_id), sizeof(uint32_t), instance_count, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID_ID(OccludedInstancesCount, view_id), sizeof(uint32_t), 1, BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID_ID(TraversalCtrl, view_id));
		builder.writeBuffer(GFXRID_ID(TraversalQueue, view_id));
		builder.writeBuffer(GFXRID_ID(VisibleMeshletsCount, view_id));
		builder.writeBuffer(GFXRID_ID(DrawIndexedCount, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedMeshletsCount, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedInstancesCount, view_id));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->fillBuffer(resources.getBuffer(GFXRID_ID(TraversalQueue, view_id)), ~0u);

		struct
		{
			uint32_t traversal_ctrl_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_calls_count_buffer_id;
			uint32_t occluded_meshlets_count_buffer_id;
			uint32_t occluded_instances_count_buffer_id;
			uint32_t reset_occluded;
		} constants;
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraversalCtrl, view_id));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(VisibleMeshletsCount, view_id));
		constants.draw_calls_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawIndexedCount, view_id));
		constants.occluded_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedMeshletsCount, view_id));
		constants.occluded_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedInstancesCount, view_id));
		constants.reset_occluded = is_fix ? 0u : 1u;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/meshlet_init_counters.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}

void MeshletPass::add_instance_culling_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix)
{
	uint32_t view_id = desc.view_id;
	fg.addCallbackPass("Cull Instances",
	[&, is_fix, view_id](RenderPassBuilder &builder)
	{
		builder.writeBuffer(GFXRID_ID(TraversalQueue, view_id));
		builder.writeBuffer(GFXRID_ID(TraversalCtrl, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedInstances, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedInstancesCount, view_id));
		builder.readBuffer(GFXRID(InstancesPassMask));
		if (desc.use_occlusion)
			builder.readTexture(desc.hiz);
		if (is_fix)
			builder.readIndirectArgsBuffer(GFXRID_ID(InstanceFixDispatchArgs, view_id));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/meshlet_cull_instances.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"IS_FIX", is_fix ? "1" : "0"},
												  {"IS_ORTHO_FRUSTUM", desc.is_ortho ? "1" : "0"},
												  {"REVERSE_Z", desc.reverse_z ? "1" : "0"},
												  {"USE_OCCLUSION", desc.use_occlusion ? "1" : "0"},
												  {"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"},
												  {"THREADGROUP_SIZE", std::to_string(INSTANCE_CULLING_THREADGROUP_SIZE).c_str()}
											  }));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t traversal_queue_buffer_id;
			uint32_t traversal_ctrl_buffer_id;
			uint32_t occluded_instances_buffer_id;
			uint32_t occluded_instances_count_buffer_id;
			uint32_t instances_pass_mask_buffer_id;
			uint32_t instances_count;
			uint32_t hiz_tex_id;
			uint32_t hiz_width;
			uint32_t hiz_height;
			uint32_t hiz_mips;
			uint32_t current_pass_mask;
		} constants = {};
		constants.frustum_view_projection = desc.view_projection;
		constants.traversal_queue_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraversalQueue, view_id));
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraversalCtrl, view_id));
		constants.occluded_instances_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedInstances, view_id));
		constants.occluded_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedInstancesCount, view_id));
		constants.instances_pass_mask_buffer_id = resources.getReadBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = desc.instance_count;
		constants.current_pass_mask = desc.pass_mask;

		if (desc.use_occlusion)
		{
			RHITexture *hiz = resources.getTexture(desc.hiz);
			constants.hiz_tex_id = hiz->getShaderResourceView(-1, desc.hiz_layer)->getBindlessIndex();
			constants.hiz_width = hiz->getWidth();
			constants.hiz_height = hiz->getHeight();
			constants.hiz_mips = hiz->getMipLevels();
		}

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		if (is_fix)
		{
			cmd_list->dispatchIndirect(resources.getBuffer(GFXRID_ID(InstanceFixDispatchArgs, view_id)), 1);
		} else
		{
			uint32_t num_groups = (desc.instance_count + INSTANCE_CULLING_THREADGROUP_SIZE - 1) / INSTANCE_CULLING_THREADGROUP_SIZE;
			cmd_list->dispatch(num_groups, 1, 1);
		}
	});
}

void MeshletPass::add_traversal_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix)
{
	uint32_t view_id = desc.view_id;
	fg.addCallbackPass("Traversal",
	[&, is_fix, view_id](RenderPassBuilder &builder)
	{
		if (!is_fix)
		{
			if (!render_meshlets_mesh_shaders)
			{
				builder.createBuffer(GFXRID_ID(DrawIndexedArgs, view_id), sizeof(DrawIndexedIndirect), MAX_MESHLETS_PER_FRAME, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
				builder.createBuffer(GFXRID_ID(DrawCallsInstances, view_id), sizeof(uint32_t), MAX_MESHLETS_PER_FRAME, BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			}
		}

		builder.writeBuffer(GFXRID_ID(TraversalQueue, view_id));
		builder.writeBuffer(GFXRID_ID(TraversalCtrl, view_id));
		builder.writeBuffer(GFXRID_ID(VisibleMeshlets, view_id));
		builder.writeBuffer(GFXRID_ID(VisibleMeshletsCount, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedMeshlets, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedMeshletsCount, view_id));
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));
		builder.writeBuffer(GFXRID(StreamRequestsBuffer));
		builder.writeBuffer(GFXRID(GroupAgesBuffer));

		if (!render_meshlets_mesh_shaders)
		{
			builder.writeBuffer(GFXRID_ID(DrawIndexedArgs, view_id));
			builder.writeBuffer(GFXRID_ID(DrawIndexedCount, view_id));
			builder.writeBuffer(GFXRID_ID(DrawCallsInstances, view_id));
		}

		if (desc.use_occlusion)
			builder.readTexture(desc.hiz);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/meshlet_traverse.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"USE_MESH_SHADERS", render_meshlets_mesh_shaders ? "1" : "0"},
												  {"IS_FIX", is_fix ? "1" : "0"},
												  {"IS_ORTHO_FRUSTUM", desc.is_ortho ? "1" : "0"},
												  {"REVERSE_Z", desc.reverse_z ? "1" : "0"},
												  {"USE_OCCLUSION", desc.use_occlusion ? "1" : "0"},
												  {"THREADGROUP_SIZE", std::to_string(MESHLET_CULLING_THREADGROUP_SIZE).c_str()}
											  }));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t queue_buffer_id;
			uint32_t traversal_ctrl_buffer_id;
			uint32_t visible_meshlets_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_indexed_args_buffer_id;
			uint32_t draw_indexed_count_buffer_id;
			uint32_t draw_calls_indirect_instances_buffer_id;
			uint32_t max_queue_size;
			uint32_t occluded_meshlets_buffer_id;
			uint32_t occluded_meshlets_count_buffer_id;
			uint32_t hiz_tex_id;
			uint32_t hiz_width;
			uint32_t hiz_height;
			uint32_t hiz_mips;
			uint32_t group_residency_buffer_id;
			uint32_t stream_requests_buffer_id;
			uint32_t group_ages_buffer_id;
		} constants = {};
		constants.frustum_view_projection = desc.view_projection;
		constants.queue_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraversalQueue, view_id));
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraversalCtrl, view_id));
		constants.max_queue_size = MAX_TRAVERSAL_TASKS;
		constants.visible_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(VisibleMeshlets, view_id));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(VisibleMeshletsCount, view_id));
		constants.occluded_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedMeshlets, view_id));
		constants.occluded_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedMeshletsCount, view_id));

		if (!render_meshlets_mesh_shaders)
		{
			constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawIndexedArgs, view_id));
			constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawIndexedCount, view_id));
			constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawCallsInstances, view_id));
		}

		if (desc.use_occlusion)
		{
			RHITexture *hiz = resources.getTexture(desc.hiz);
			constants.hiz_tex_id = hiz->getShaderResourceView(-1, desc.hiz_layer)->getBindlessIndex();
			constants.hiz_width = hiz->getWidth();
			constants.hiz_height = hiz->getHeight();
			constants.hiz_mips = hiz->getMipLevels();
		}

		constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
		constants.stream_requests_buffer_id = resources.getReadWriteBuffer(GFXRID(StreamRequestsBuffer));
		constants.group_ages_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupAgesBuffer));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(PERSISTENT_THREAD_GROUPS, 1, 1);
	});
}

void MeshletPass::add_meshlet_fix_pass(FrameGraph &fg, const MeshletCullDesc &desc)
{
	uint32_t view_id = desc.view_id;
	fg.addCallbackPass("Cull Meshlets Fix",
	[&, view_id](RenderPassBuilder &builder)
	{
		builder.writeBuffer(GFXRID_ID(VisibleMeshlets, view_id));
		builder.writeBuffer(GFXRID_ID(VisibleMeshletsCount, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedMeshlets, view_id));
		builder.writeBuffer(GFXRID_ID(OccludedMeshletsCount, view_id));
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));
		if (!render_meshlets_mesh_shaders)
		{
			builder.writeBuffer(GFXRID_ID(DrawIndexedArgs, view_id));
			builder.writeBuffer(GFXRID_ID(DrawIndexedCount, view_id));
			builder.writeBuffer(GFXRID_ID(DrawCallsInstances, view_id));
		}
		builder.readIndirectArgsBuffer(GFXRID_ID(MeshletFixDispatchArgs, view_id));
		builder.readTexture(desc.hiz);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/meshlet_cull_fix.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"USE_MESH_SHADERS", render_meshlets_mesh_shaders ? "1" : "0"},
												  {"IS_ORTHO_FRUSTUM", desc.is_ortho ? "1" : "0"},
												  {"REVERSE_Z", desc.reverse_z ? "1" : "0"},
												  {"USE_OCCLUSION", desc.use_occlusion ? "1" : "0"},
												  {"THREADGROUP_SIZE", std::to_string(MESHLET_FIX_THREADGROUP_SIZE).c_str()}
											  }));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t visible_meshlets_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_indexed_args_buffer_id;
			uint32_t draw_indexed_count_buffer_id;
			uint32_t draw_calls_indirect_instances_buffer_id;
			uint32_t occluded_meshlets_buffer_id;
			uint32_t occluded_meshlets_count_buffer_id;
			uint32_t hiz_tex_id;
			uint32_t hiz_width;
			uint32_t hiz_height;
			uint32_t hiz_mips;
			uint32_t group_residency_buffer_id;
		} constants = {};
		constants.frustum_view_projection = desc.view_projection;
		constants.visible_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(VisibleMeshlets, view_id));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(VisibleMeshletsCount, view_id));
		constants.occluded_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedMeshlets, view_id));
		constants.occluded_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(OccludedMeshletsCount, view_id));

		if (!render_meshlets_mesh_shaders)
		{
			constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawIndexedArgs, view_id));
			constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawIndexedCount, view_id));
			constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(DrawCallsInstances, view_id));
		}

		RHITexture *hiz = resources.getTexture(desc.hiz);
		constants.hiz_tex_id = hiz->getShaderResourceView(-1, desc.hiz_layer)->getBindlessIndex();
		constants.hiz_width = hiz->getWidth();
		constants.hiz_height = hiz->getHeight();
		constants.hiz_mips = hiz->getMipLevels();

		constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatchIndirect(resources.getBuffer(GFXRID_ID(MeshletFixDispatchArgs, view_id)), 1);
	});
}

void MeshletPass::add_dispatch_args_pass(FrameGraph &fg, const char *name, GraphicsResourceName args_id, GraphicsResourceName count_id, uint32_t group_size)
{
	fg.addCallbackPass(name,
	[&](RenderPassBuilder &builder)
	{
		if (!builder.isBufferCreated(args_id))
			builder.createBuffer(args_id, sizeof(DispatchIndirect), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		builder.writeBuffer(args_id);
		builder.readBuffer(count_id);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct
		{
			uint32_t count_buffer_id;
			uint32_t dispatch_indirect_args_buffer_id;
			uint32_t group_size;
		} constants;
		constants.count_buffer_id = resources.getReadBuffer(count_id);
		constants.dispatch_indirect_args_buffer_id = resources.getReadWriteBuffer(args_id);
		constants.group_size = group_size;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/build_indirect_args.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}
