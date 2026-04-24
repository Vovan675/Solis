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
	if (render_meshlets_use_mesh_shaders)
		add_dispatch_args_pass(fg, "Build Geometry Dispatch Args", GFXRID(DispatchMeshIndirectArgs), GFXRID(VisibleMeshletsCount), 1);
}

void MeshletPass::addFixCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc)
{
	add_counter_init_pass(fg, desc, true);

	add_dispatch_args_pass(fg, "Build Instance Fix Dispatch Args", GFXRID(InstanceFixDispatchArgs), GFXRID(OccludedInstancesCount), INSTANCE_CULLING_THREADGROUP_SIZE);
	add_instance_culling_pass(fg, desc, true);

	add_traversal_pass(fg, desc, true);

	add_dispatch_args_pass(fg, "Build Meshlet Fix Dispatch Args", GFXRID(MeshletFixDispatchArgs), GFXRID(OccludedMeshletsCount), MESHLET_FIX_THREADGROUP_SIZE);
	add_meshlet_fix_pass(fg, desc);

	if (render_meshlets_use_mesh_shaders)
		add_dispatch_args_pass(fg, "Build Geometry Dispatch Args", GFXRID(DispatchMeshIndirectArgs), GFXRID(VisibleMeshletsCount), 1);
}

void MeshletPass::add_counter_init_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix)
{
	fg.addCallbackPass("Init Counters",
	[&, is_fix, instance_count = desc.instance_count](RenderPassBuilder &builder)
	{
		if (!is_fix)
		{
			builder.createBuffer(GFXRID(TraversalQueue), sizeof(TraversalItem), MAX_TRAVERSAL_TASKS, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(TraversalCtrl), sizeof(uint32_t) * 4, 1, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(VisibleMeshlets), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(VisibleMeshletsCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(DrawIndexedCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(OccludedMeshlets), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(OccludedMeshletsCount), sizeof(uint32_t), 1, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(OccludedInstances), sizeof(uint32_t), instance_count, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(OccludedInstancesCount), sizeof(uint32_t), 1, BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(TraversalCtrl));
		builder.writeBuffer(GFXRID(TraversalQueue));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DrawIndexedCount));
		builder.writeBuffer(GFXRID(OccludedMeshletsCount));
		builder.writeBuffer(GFXRID(OccludedInstancesCount));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->fillBuffer(resources.getBuffer(GFXRID(TraversalQueue)), ~0u);

		struct
		{
			uint32_t traversal_ctrl_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_calls_count_buffer_id;
			uint32_t occluded_meshlets_count_buffer_id;
			uint32_t occluded_instances_count_buffer_id;
			uint32_t reset_occluded;
		} constants;
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID(TraversalCtrl));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.draw_calls_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));
		constants.occluded_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedMeshletsCount));
		constants.occluded_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedInstancesCount));
		constants.reset_occluded = is_fix ? 0u : 1u;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/init_meshlet_counters.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}

void MeshletPass::add_instance_culling_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix)
{
	fg.addCallbackPass("Cull Instances",
	[&, is_fix](RenderPassBuilder &builder)
	{
		builder.writeBuffer(GFXRID(TraversalQueue));
		builder.writeBuffer(GFXRID(TraversalCtrl));
		builder.writeBuffer(GFXRID(OccludedInstances));
		builder.writeBuffer(GFXRID(OccludedInstancesCount));
		builder.readBuffer(GFXRID(InstancesPassMask));
		builder.readTexture(GFXRID(HiZ));
		if (is_fix)
			builder.readIndirectArgsBuffer(GFXRID(InstanceFixDispatchArgs));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_instances.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"IS_FIX", is_fix ? "1" : "0"},
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
		constants.traversal_queue_buffer_id = resources.getReadWriteBuffer(GFXRID(TraversalQueue));
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID(TraversalCtrl));
		constants.occluded_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedInstances));
		constants.occluded_instances_count_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedInstancesCount));
		constants.instances_pass_mask_buffer_id = resources.getReadBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = desc.instance_count;
		constants.current_pass_mask = desc.pass_mask;

		RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
		constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
		constants.hiz_width = hiz->getWidth();
		constants.hiz_height = hiz->getHeight();
		constants.hiz_mips = hiz->getMipLevels();

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		if (is_fix)
		{
			cmd_list->dispatchIndirect(resources.getBuffer(GFXRID(InstanceFixDispatchArgs)), 1);
		} else
		{
			uint32_t num_groups = (desc.instance_count + INSTANCE_CULLING_THREADGROUP_SIZE - 1) / INSTANCE_CULLING_THREADGROUP_SIZE;
			cmd_list->dispatch(num_groups, 1, 1);
		}
	});
}

void MeshletPass::add_traversal_pass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_fix)
{
	fg.addCallbackPass("Traversal",
	[&, is_fix](RenderPassBuilder &builder)
	{
		if (!is_fix)
		{
			if (!render_meshlets_use_mesh_shaders)
			{
				builder.createBuffer(GFXRID(DrawIndexedArgs), sizeof(DrawIndexedIndirect), MAX_MESHLETS_PER_FRAME, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
				builder.createBuffer(GFXRID(DrawCallsInstances), sizeof(uint32_t), MAX_MESHLETS_PER_FRAME, BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			}
		}

		builder.writeBuffer(GFXRID(TraversalQueue));
		builder.writeBuffer(GFXRID(TraversalCtrl));
		builder.writeBuffer(GFXRID(VisibleMeshlets));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(OccludedMeshlets));
		builder.writeBuffer(GFXRID(OccludedMeshletsCount));
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));
		builder.writeBuffer(GFXRID(StreamRequestsBuffer));
		builder.writeBuffer(GFXRID(GroupAgesBuffer));

		if (!render_meshlets_use_mesh_shaders)
		{
			builder.writeBuffer(GFXRID(DrawIndexedArgs));
			builder.writeBuffer(GFXRID(DrawIndexedCount));
			builder.writeBuffer(GFXRID(DrawCallsInstances));
		}

		builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_meshlets.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"USE_MESH_SHADERS", render_meshlets_use_mesh_shaders ? "1" : "0"},
												  {"IS_FIX", is_fix ? "1" : "0"},
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
		constants.queue_buffer_id = resources.getReadWriteBuffer(GFXRID(TraversalQueue));
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID(TraversalCtrl));
		constants.max_queue_size = MAX_TRAVERSAL_TASKS;
		constants.visible_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshlets));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.occluded_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedMeshlets));
		constants.occluded_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedMeshletsCount));

		if (!render_meshlets_use_mesh_shaders)
		{
			constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedArgs));
			constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));
			constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawCallsInstances));
		}

		RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
		constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
		constants.hiz_width = hiz->getWidth();
		constants.hiz_height = hiz->getHeight();
		constants.hiz_mips = hiz->getMipLevels();

		constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
		constants.stream_requests_buffer_id = resources.getReadWriteBuffer(GFXRID(StreamRequestsBuffer));
		constants.group_ages_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupAgesBuffer));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(PERSISTENT_THREAD_GROUPS, 1, 1);
	});
}

void MeshletPass::add_meshlet_fix_pass(FrameGraph &fg, const MeshletCullDesc &desc)
{
	fg.addCallbackPass("Cull Meshlets Fix",
	[&](RenderPassBuilder &builder)
	{
		builder.writeBuffer(GFXRID(VisibleMeshlets));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(OccludedMeshlets));
		builder.writeBuffer(GFXRID(OccludedMeshletsCount));
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));
		if (!render_meshlets_use_mesh_shaders)
		{
			builder.writeBuffer(GFXRID(DrawIndexedArgs));
			builder.writeBuffer(GFXRID(DrawIndexedCount));
			builder.writeBuffer(GFXRID(DrawCallsInstances));
		}
		builder.readIndirectArgsBuffer(GFXRID(MeshletFixDispatchArgs));
		builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_meshlets_fix.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"USE_MESH_SHADERS", render_meshlets_use_mesh_shaders ? "1" : "0"},
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
		constants.visible_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshlets));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.occluded_meshlets_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedMeshlets));
		constants.occluded_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(OccludedMeshletsCount));

		if (!render_meshlets_use_mesh_shaders)
		{
			constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedArgs));
			constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));
			constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawCallsInstances));
		}

		RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
		constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
		constants.hiz_width = hiz->getWidth();
		constants.hiz_height = hiz->getHeight();
		constants.hiz_mips = hiz->getMipLevels();

		constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatchIndirect(resources.getBuffer(GFXRID(MeshletFixDispatchArgs)), 1);
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