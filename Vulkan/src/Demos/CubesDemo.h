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
	inline static std::shared_ptr<RHIBuffer> vertex_buffer;
	inline static std::shared_ptr<RHIBuffer> index_buffer;
	inline static std::shared_ptr<RHIShader> vertex_shader;
	inline static std::shared_ptr<RHIShader> pixel_shader;
	inline static std::shared_ptr<RHITexture> depth_stencil_texture;
	inline static std::shared_ptr<RHIPipeline> pso;

	inline static std::vector<std::shared_ptr<RHITexture>> checker_textures;

	inline static std::shared_ptr<RHIShader> vertex_shader_bindless;
	inline static std::shared_ptr<RHIShader> pixel_shader_bindless;
	inline static std::shared_ptr<RHIPipeline> pso_bindless;

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
	inline static std::shared_ptr<RHIBuffer> vertex_buffer;
	inline static std::shared_ptr<RHIBuffer> index_buffer;
	inline static std::shared_ptr<RHIShader> vertex_shader;
	inline static std::shared_ptr<RHIShader> pixel_shader;

	inline static std::shared_ptr<RHIShader> vertex_shader_quad;
	inline static std::shared_ptr<RHIShader> pixel_shader_quad;

	inline static std::shared_ptr<RHITexture> depth_stencil_texture;
	inline static std::shared_ptr<RHITexture> result_texture;
	inline static std::shared_ptr<RHIPipeline> pso;

	inline static std::vector<std::shared_ptr<RHITexture>> checker_textures;

	inline static float value = 0.0f;
};
