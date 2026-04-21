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

void MeshletPass::addEarlyCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc)
{
	addCullingPasses(fg, desc, false);
}

void MeshletPass::addLateCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc)
{
	addCullingPasses(fg, desc, true);
}

void MeshletPass::addCullingPasses(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late)
{
	addCounterInitPass(fg, is_late);
	addInstanceCullingPass(fg, desc, is_late);
	addTraversalPass(fg, desc, is_late);
	addGeometryDispatchArgsPass(fg, is_late);
}

void MeshletPass::addCounterInitPass(FrameGraph &fg, bool is_late)
{
	fg.addCallbackPass("Init Counters",
	[&, is_late](RenderPassBuilder &builder)
	{
		if (!is_late)
		{
			builder.createBuffer(GFXRID(TraversalQueue), sizeof(TraversalItem), MAX_TRAVERSAL_TASKS, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(TraversalCtrl), sizeof(uint32_t) * 4, 1, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(VisibleMeshlets), sizeof(MeshletCandidate), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(VisibleMeshletsCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
			builder.createBuffer(GFXRID(DrawIndexedCount), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		}

		builder.writeBuffer(GFXRID(TraversalCtrl));
		builder.writeBuffer(GFXRID(TraversalQueue));
		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DrawIndexedCount));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->fillBuffer(resources.getBuffer(GFXRID(TraversalQueue)), ~0u);

		struct
		{
			uint32_t traversal_ctrl_buffer_id;
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t draw_calls_count_buffer_id;
		} constants;
		constants.traversal_ctrl_buffer_id = resources.getReadWriteBuffer(GFXRID(TraversalCtrl));
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.draw_calls_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/init_draw_calls.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}

void MeshletPass::addInstanceCullingPass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late)
{
	fg.addCallbackPass("Cull Instances",
	[&, is_late, instance_count = desc.instance_count](RenderPassBuilder &builder)
	{
		if (!is_late)
			builder.createBuffer(GFXRID(IndirectVisibility), sizeof(uint32_t), instance_count, BufferUsage::SHADER_WRITE_BUFFER);

		builder.writeBuffer(GFXRID(TraversalQueue));
		builder.writeBuffer(GFXRID(TraversalCtrl));
		builder.writeBuffer(GFXRID(IndirectVisibility));
		builder.readBuffer(GFXRID(InstancesPassMask));

		if (is_late)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_instances.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"IS_LATE", is_late ? "1" : "0"},
												  {"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
												  {"HIZ_OCCLUSION_DEBUG", render_culling_hiz_debug ? "1" : "0"},
												  {"THREADGROUP_SIZE", std::to_string(INSTANCE_CULLING_THREADGROUP_SIZE).c_str()}
											  }));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t traversal_queue_buffer_id;
			uint32_t traversal_ctrl_buffer_id;
			uint32_t instances_visibility_buffer_id;
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
		constants.instances_visibility_buffer_id = resources.getReadWriteBuffer(GFXRID(IndirectVisibility));
		constants.instances_pass_mask_buffer_id = resources.getReadBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = desc.instance_count;
		constants.current_pass_mask = desc.pass_mask;
		if (is_late)
		{
			RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
			constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
			constants.hiz_width = hiz->getWidth();
			constants.hiz_height = hiz->getHeight();
			constants.hiz_mips = hiz->getMipLevels();
		}
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		uint32_t num_groups = (desc.instance_count + INSTANCE_CULLING_THREADGROUP_SIZE - 1) / INSTANCE_CULLING_THREADGROUP_SIZE;
		cmd_list->dispatch(num_groups, 1, 1);
	});
}

void MeshletPass::addTraversalPass(FrameGraph &fg, const MeshletCullDesc &desc, bool is_late)
{
	fg.addCallbackPass("Traversal",
	[&, is_late](RenderPassBuilder &builder)
	{
		if (!is_late)
		{
			builder.createBuffer(GFXRID(MeshletVisibility), sizeof(uint32_t), MAX_MESHLETS_PER_FRAME, BufferUsage::SHADER_WRITE_BUFFER);

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
		builder.writeBuffer(GFXRID(MeshletVisibility));
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));
		builder.writeBuffer(GFXRID(StreamRequestsBuffer));
		builder.writeBuffer(GFXRID(GroupAgesBuffer));

		if (!render_meshlets_use_mesh_shaders)
		{
			builder.writeBuffer(GFXRID(DrawIndexedArgs));
			builder.writeBuffer(GFXRID(DrawIndexedCount));
			builder.writeBuffer(GFXRID(DrawCallsInstances));
		}

		if (is_late)
			builder.readTexture(GFXRID(HiZ));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/cull_meshlets.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"USE_MESH_SHADERS", render_meshlets_use_mesh_shaders ? "1" : "0"},
												  {"IS_LATE", is_late ? "1" : "0"},
												  {"FREEZE_CULLING", render_freeze_culling ? "1" : "0"},
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
			uint32_t meshlet_visibility_buffer_id;
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
		constants.meshlet_visibility_buffer_id = resources.getReadWriteBuffer(GFXRID(MeshletVisibility));
		if (!render_meshlets_use_mesh_shaders)
		{
			constants.draw_indexed_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedArgs));
			constants.draw_indexed_count_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawIndexedCount));
			constants.draw_calls_indirect_instances_buffer_id = resources.getReadWriteBuffer(GFXRID(DrawCallsInstances));
		}
		if (is_late)
		{
			RHITexture *hiz = resources.getTexture(GFXRID(HiZ));
			constants.hiz_tex_id = hiz->getShaderResourceView()->getBindlessIndex();
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

void MeshletPass::addGeometryDispatchArgsPass(FrameGraph &fg, bool is_late)
{
	if (!render_meshlets_use_mesh_shaders)
		return;

	fg.addCallbackPass("Build Geometry Dispatch Args",
	[&, is_late](RenderPassBuilder &builder)
	{
		if (!is_late)
			builder.createBuffer(GFXRID(DispatchMeshIndirectArgs), sizeof(DispatchIndirect), MAX_MESHLETS_PER_FRAME, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);

		builder.writeBuffer(GFXRID(VisibleMeshletsCount));
		builder.writeBuffer(GFXRID(DispatchMeshIndirectArgs));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct
		{
			uint32_t visible_meshlets_count_buffer_id;
			uint32_t dispatch_indirect_args_buffer_id;
			uint32_t group_size;
		} constants;
		constants.visible_meshlets_count_buffer_id = resources.getReadWriteBuffer(GFXRID(VisibleMeshletsCount));
		constants.dispatch_indirect_args_buffer_id = resources.getReadWriteBuffer(GFXRID(DispatchMeshIndirectArgs));
		constants.group_size = 1;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/build_indirect_args.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
		cmd_list->dispatch(1, 1, 1);
	});
}
