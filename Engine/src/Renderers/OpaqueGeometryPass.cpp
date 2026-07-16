#include "pch.h"
#include "OpaqueGeometryPass.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalBufferCache.h"
#include "Rendering/GlobalPipeline.h"
#include "FrameGraph/FrameGraphData.h"
#include "Core/Variables.h"

namespace
{
constexpr uint32_t MAX_MESHLETS_PER_FRAME = 1 << 23;
constexpr uint32_t TRADITIONAL_CULLING_THREADGROUP_SIZE = 32;
}

OpaqueGeometryPass::ShaderSet OpaqueGeometryPass::ShaderSet::fromFile(const wchar_t *file)
{
	ShaderSet shaders;
	if (render_meshlets_use_mesh_shaders)
		shaders.meshlet_mesh_shader = gDynamicRHI->createShader(file, MESH_SHADER, "MSMainMeshlet");
	else
		shaders.meshlet_vertex_shader = gDynamicRHI->createShader(file, VERTEX_SHADER, "VSMainMeshlet");
	shaders.traditional_vertex_shader = gDynamicRHI->createShader(file, VERTEX_SHADER, "VSMainTraditional");
	shaders.pixel_shader = gDynamicRHI->createShader(file, FRAGMENT_SHADER);
	return shaders;
}

void OpaqueGeometryPass::renderGBuffer(FrameGraph &fg, const RenderView &view, const GBufferOutput &output)
{
	RenderTargets targets;
	targets.color.push_back({output.albedo, FORMAT_R8G8B8A8_UNORM});
	targets.color.push_back({output.normal, FORMAT_R8G8B8A8_UNORM});
	targets.color.push_back({output.shading, FORMAT_R8G8B8A8_UNORM});
	targets.color.push_back({output.motion_vectors, FORMAT_R16G16_SFLOAT});
	targets.depth = {output.depth, FORMAT_D32S8};
	targets.layer = view.layer;
	render(fg, view, targets);
}

void OpaqueGeometryPass::renderDepth(FrameGraph &fg, const RenderView &view, const DepthOutput &output)
{
	RenderTargets targets;
	targets.depth = {output.depth, FORMAT_D32S8};
	targets.layer = view.layer;
	render(fg, view, targets);
}

void OpaqueGeometryPass::render(FrameGraph &fg, const RenderView &view, const RenderTargets &targets)
{
	MeshletCullDesc cull;
	cull.pass_mask = view.pass_mask;
	cull.view_projection = view.view_projection;
	cull.instance_count = view.instance_count;
	cull.view_id = view.view_id;
	cull.hiz = view.hiz;
	cull.hiz_layer = view.layer;
	cull.is_ortho = view.ortho_frustum;
	cull.reverse_z = view.use_reverse_z;
	cull.use_occlusion = view.use_two_pass_occlusion;

	meshlet_pass.addMainCullingPasses(fg, cull);
	render_meshlets(fg, view, targets, true);

	cull_traditional(fg, view);
	render_traditional(fg, view, targets);

	if (view.use_two_pass_occlusion)
	{
		HiZ::build(fg, view.hiz, targets.depth.name, view.layer, view.use_reverse_z);
		if (!render_freeze_culling)
		{
			meshlet_pass.addFixCullingPasses(fg, cull);
			render_meshlets(fg, view, targets, false);
			HiZ::build(fg, view.hiz, targets.depth.name, view.layer, view.use_reverse_z);
		}
	}
}

void OpaqueGeometryPass::render_meshlets(FrameGraph &fg, const RenderView &view, const RenderTargets &targets, bool clear)
{
	fg.addCallbackPass("Geometry",
	[view, targets, clear](RenderPassBuilder &builder)
	{
		if (clear)
		{
			for (const Target &color : targets.color)
				if (!builder.isTextureCreated(color.name))
					builder.createTexture(color.name, view.render_size.x, view.render_size.y, color.format);
			if (!builder.isTextureCreated(targets.depth.name))
				builder.createTexture(targets.depth.name, view.render_size.x, view.render_size.y, targets.depth.format);
		}

		for (const Target &color : targets.color)
			builder.writeTexture(color.name);
		builder.writeTexture(targets.depth.name);
		builder.readBuffer(GFXRID_ID(VisibleMeshlets, view.view_id));
		builder.writeBuffer(GFXRID(GroupResidencyBuffer));

		if (render_meshlets_use_mesh_shaders)
			builder.readIndirectArgsBuffer(GFXRID_ID(DispatchMeshIndirectArgs, view.view_id));
		else
		{
			builder.readIndirectArgsBuffer(GFXRID_ID(DrawIndexedArgs, view.view_id));
			builder.readIndirectArgsBuffer(GFXRID_ID(DrawIndexedCount, view.view_id));
			builder.readVertexBuffer(GFXRID_ID(DrawCallsInstances, view.view_id));
		}
	},
	[view, targets, clear](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		eastl::vector<RHITexture *> color_textures;
		for (const Target &color : targets.color)
			color_textures.push_back(resources.getTexture(color.name));
		RHITexture *depth = resources.getTexture(targets.depth.name);

		if (!render_meshlets_use_mesh_shaders)
		{
			cmd_list->setVertexBuffer(resources.getBuffer(GFXRID_ID(DrawCallsInstances, view.view_id)), 0, sizeof(uint32_t), 0);
			cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalMeshletGeometryBuffer(), 0);
		}

		cmd_list->setRenderTargets(color_textures, depth, targets.layer, 0, clear, view.getDepthClear());

		if (render_meshlets_use_mesh_shaders)
		{
			gGlobalPipeline->setupMeshPipeline(cmd_list, view.shaders.meshlet_mesh_shader, view.shaders.pixel_shader, false, true, view.cull_mode);
		} else
		{
			VertexInputsDescription inputs;
			inputs.inputs.push_back({"INSTANCE_ID", 0, FORMAT_R32_UINT, true});
			gGlobalPipeline->setupGraphicsPipeline(cmd_list, view.shaders.meshlet_vertex_shader, view.shaders.pixel_shader, inputs, false, true, view.cull_mode);
		}
		gGlobalPipeline->setDepthFunc(view.getDepthFunc());
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 view_projection;
			uint32_t visible_meshlets_buffer_id;
			uint32_t group_residency_buffer_id;
		} constants;
		constants.view_projection = view.view_projection;
		constants.visible_meshlets_buffer_id = resources.getReadBuffer(GFXRID_ID(VisibleMeshlets, view.view_id));
		constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		if (render_meshlets_use_mesh_shaders)
			cmd_list->dispatchMeshIndirect(resources.getBuffer(GFXRID_ID(DispatchMeshIndirectArgs, view.view_id)), 1);
		else
			cmd_list->drawIndexedIndirect(resources.getBuffer(GFXRID_ID(DrawIndexedArgs, view.view_id)), MAX_MESHLETS_PER_FRAME, resources.getBuffer(GFXRID_ID(DrawIndexedCount, view.view_id)));

		cmd_list->resetRenderTargets();
	});
}

void OpaqueGeometryPass::cull_traditional(FrameGraph &fg, const RenderView &view)
{
	fg.addCallbackPass("Cull Traditional",
	[view](RenderPassBuilder &builder)
	{
		builder.createBuffer(GFXRID_ID(TraditionalDrawArgs, view.view_id), sizeof(DrawIndirect), view.instance_count, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		builder.createBuffer(GFXRID_ID(TraditionalDrawCount, view.view_id), sizeof(uint32_t), 1, BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);
		builder.createBuffer(GFXRID_ID(TraditionalDrawInstances, view.view_id), sizeof(uint32_t), view.instance_count, BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER);

		builder.writeBuffer(GFXRID_ID(TraditionalDrawArgs, view.view_id));
		builder.writeBuffer(GFXRID_ID(TraditionalDrawCount, view.view_id));
		builder.writeBuffer(GFXRID_ID(TraditionalDrawInstances, view.view_id));
		builder.readBuffer(GFXRID(InstancesPassMask));
	},
	[view](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->fillBuffer(resources.getBuffer(GFXRID_ID(TraditionalDrawCount, view.view_id)), 0);

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/traditional_cull_instances.hlsl", COMPUTE_SHADER, "CSMain",
											  {
												  {"IS_ORTHO_FRUSTUM", view.ortho_frustum ? "1" : "0"},
												  {"THREADGROUP_SIZE", std::to_string(TRADITIONAL_CULLING_THREADGROUP_SIZE).c_str()}
											  }));
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 frustum_view_projection;
			uint32_t draw_args_buffer_id;
			uint32_t draw_count_buffer_id;
			uint32_t draw_instances_buffer_id;
			uint32_t instances_pass_mask_buffer_id;
			uint32_t instances_count;
			uint32_t current_pass_mask;
		} constants = {};
		constants.frustum_view_projection = view.view_projection;
		constants.draw_args_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraditionalDrawArgs, view.view_id));
		constants.draw_count_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraditionalDrawCount, view.view_id));
		constants.draw_instances_buffer_id = resources.getReadWriteBuffer(GFXRID_ID(TraditionalDrawInstances, view.view_id));
		constants.instances_pass_mask_buffer_id = resources.getReadBuffer(GFXRID(InstancesPassMask));
		constants.instances_count = view.instance_count;
		constants.current_pass_mask = view.pass_mask;

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		uint32_t num_groups = (view.instance_count + TRADITIONAL_CULLING_THREADGROUP_SIZE - 1) / TRADITIONAL_CULLING_THREADGROUP_SIZE;
		cmd_list->dispatch(num_groups, 1, 1);
	});
}

void OpaqueGeometryPass::render_traditional(FrameGraph &fg, const RenderView &view, const RenderTargets &targets)
{
	fg.addCallbackPass("Traditional Geometry",
	[view, targets](RenderPassBuilder &builder)
	{
		for (const Target &color : targets.color)
			builder.writeTexture(color.name);
		builder.writeTexture(targets.depth.name);
		builder.readIndirectArgsBuffer(GFXRID_ID(TraditionalDrawArgs, view.view_id));
		builder.readIndirectArgsBuffer(GFXRID_ID(TraditionalDrawCount, view.view_id));
		builder.readVertexBuffer(GFXRID_ID(TraditionalDrawInstances, view.view_id));
	},
	[view, targets](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		eastl::vector<RHITexture *> color_textures;
		for (const Target &color : targets.color)
			color_textures.push_back(resources.getTexture(color.name));
		RHITexture *depth = resources.getTexture(targets.depth.name);

		cmd_list->setVertexBuffer(resources.getBuffer(GFXRID_ID(TraditionalDrawInstances, view.view_id)), 0, sizeof(uint32_t), 0);
		cmd_list->setRenderTargets(color_textures, depth, targets.layer, 0, false);

		VertexInputsDescription inputs;
		inputs.inputs.push_back({"INSTANCE_ID", 0, FORMAT_R32_UINT, true});
		gGlobalPipeline->setupGraphicsPipeline(cmd_list, view.shaders.traditional_vertex_shader, view.shaders.pixel_shader, inputs, false, true, view.cull_mode);
		gGlobalPipeline->setDepthFunc(view.getDepthFunc());
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			glm::mat4 view_projection;
			uint32_t visible_meshlets_buffer_id;
			uint32_t group_residency_buffer_id;
		} constants = {};
		constants.view_projection = view.view_projection;
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->drawIndirect(resources.getBuffer(GFXRID_ID(TraditionalDrawArgs, view.view_id)), view.instance_count, resources.getBuffer(GFXRID_ID(TraditionalDrawCount, view.view_id)));

		cmd_list->resetRenderTargets();
	});
}
