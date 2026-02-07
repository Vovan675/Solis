#pragma once
#include "Math.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHITexture.h"

class GlobalPipeline;
extern GlobalPipeline *gGlobalPipeline;

class GlobalPipeline
{
public:
	GlobalPipeline();
	~GlobalPipeline();

	void setVertexShader(RHIShaderRef shader) { current_description.vertex_shader = shader; }
	void setFragmentShader(RHIShaderRef shader) { current_description.fragment_shader = shader; }
	void setVertexInputsDescription(VertexInputsDescription desc) { current_description.vertex_inputs_descriptions = desc; }
	void setUseBlending(bool use_blending) { current_description.use_blending = use_blending; }
	void setBlendMode(Blend src_color_blend, Blend dst_color_blend, BlendOp color_blend_op,
					  Blend src_alpha_blend, Blend dst_alpha_blend, BlendOp alpha_blend_op);
	void setDepthTest(bool depth_test) { current_description.use_depth_test = depth_test; }
	void setDepthWrite(bool depth_write) { current_description.use_depth_write = depth_write; }
	void setDepthFunc(CompareFunc func) { current_description.depth_compare_func = func; }
	void setCullMode(CullMode cull_mode) { current_description.cull_mode = cull_mode; }
	void setPrimitiveTopology(Topology topology) { current_description.primitive_topology = topology; }
	void setComputeShader(RHIShaderRef shader) { current_description.compute_shader = shader; }
	void setRayGenerationShader(RHIShaderRef shader) { current_description.ray_generation_shader = shader; }
	void setMissShader(RHIShaderRef shader) { current_description.miss_shader = shader; }
	void setClosestHitShader(RHIShaderRef shader) { current_description.closest_hit_shader = shader; }
	void setRenderTargets(eastl::vector<RHITexture *> attachments);
	void setRenderTargets(RHICommandList* cmd_list);

	void setMeshShader(RHIShaderRef shader) { current_description.mesh_shader = shader; }

	void setupRayTracing(const wchar_t* shader_path);
	void setupRayTracing(const wchar_t* shader_path, eastl::vector<eastl::pair<const char *, const char *>> defines);
	void setupRayTracing(RHIShaderRef ray_gen, RHIShaderRef miss, RHIShaderRef closest_hit);
	void setupComputePipeline(RHIShaderRef compute_shader);
	void setupGraphicsPipeline(RHIShaderRef vertex_shader, RHIShaderRef fragment_shader,
							   const eastl::vector<RHITexture*>& render_targets,
							   VertexInputsDescription vertex_inputs = VertexInputsDescription{},
							   bool use_blending = false,
							   bool depth_test = true,
							   CullMode cull_mode = CULL_MODE_BACK);
	void setupGraphicsPipeline(RHICommandList* cmd_list,
							   RHIShaderRef vertex_shader, RHIShaderRef fragment_shader,
							   VertexInputsDescription vertex_inputs = VertexInputsDescription{},
							   bool use_blending = false,
							   bool depth_test = true,
							   CullMode cull_mode = CULL_MODE_BACK);
	void setupMeshPipeline(RHICommandList* cmd_list,
							RHIShaderRef mesh_shader, RHIShaderRef fragment_shader,
							bool use_blending = false,
							bool depth_test = true,
							CullMode cull_mode = CULL_MODE_BACK);

	void flushAndBind(RHICommandList *cmd_list);

	void bindScreenQuadPipeline(RHICommandList *cmd_list, RHIShaderRef fragment_shader);

private:
	void reset();
	void flush();
	void bind(RHICommandList *cmd_list);

	PipelineDescription current_description;
	RHIPipelineRef current_pipeline;
	eastl::unordered_map<size_t, RHIPipelineRef> cached_pipelines;
};

