#pragma once
#include "RHI/DynamicRHI.h"

// Render multiple textured rotating cubes
class CubesDemo
{
public:
	static void initResources();
	static void render(RHICommandList *cmd_list);
	static void renderBindless(RHICommandList *cmd_list);
private:
	inline static RHIBufferRef vertex_buffer;
	inline static RHIBufferRef index_buffer;
	inline static RHIShaderRef vertex_shader;
	inline static RHIShaderRef pixel_shader;
	inline static RHITextureRef depth_stencil_texture;
	inline static RHIPipelineRef pso;

	inline static std::vector<RHITextureRef> checker_textures;

	inline static RHIShaderRef vertex_shader_bindless;
	inline static RHIShaderRef pixel_shader_bindless;
	inline static RHIPipelineRef pso_bindless;

	inline static float value = 0.0f;
};

// Render meshes into multiple render targets
class RenderTargetsDemo
{
public:
	static void initResources();
	static void render(RHICommandList *cmd_list);
	static void renderFrameGraph(RHICommandList *cmd_list);
private:
	inline static RHIBufferRef vertex_buffer;
	inline static RHIBufferRef index_buffer;
	inline static RHIShaderRef vertex_shader;
	inline static RHIShaderRef pixel_shader;

	inline static RHIShaderRef vertex_shader_quad;
	inline static RHIShaderRef pixel_shader_quad;

	inline static RHITextureRef depth_stencil_texture;
	inline static RHITextureRef result_texture;
	inline static RHIPipelineRef pso;

	inline static std::vector<RHITextureRef> checker_textures;

	inline static float value = 0.0f;
};
