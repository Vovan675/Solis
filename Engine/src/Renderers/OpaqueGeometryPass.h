#pragma once
#include "FrameGraph/FrameGraph.h"
#include "MeshletPass.h"
#include "HiZ.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHITexture.h"

class OpaqueGeometryPass
{
public:
	struct ShaderSet
	{
		RHIShaderRef meshlet_mesh_shader;
		RHIShaderRef meshlet_vertex_shader;
		RHIShaderRef traditional_vertex_shader;
		RHIShaderRef pixel_shader;
		static ShaderSet fromFile(const wchar_t *file);
	};

	struct RenderView
	{
		glm::mat4 view_projection;
		uint32_t pass_mask;
		uint32_t instance_count;
		uint32_t view_id = 0;
		glm::ivec2 render_size;
		GraphicsResourceName hiz;
		uint32_t layer = 0;
		bool use_two_pass_occlusion = true;
		bool ortho_frustum = false;
		bool use_reverse_z = true;
		CullMode cull_mode = CULL_MODE_BACK;
		ShaderSet shaders;

		CompareFunc getDepthFunc() const { return use_reverse_z ? COMPARE_FUNC_GREATER : COMPARE_FUNC_LESS_EQUAL; }
		float getDepthClear() const { return use_reverse_z ? 0.0f : 1.0f; }
	};

	struct GBufferOutput
	{
		GraphicsResourceName albedo;
		GraphicsResourceName normal;
		GraphicsResourceName shading;
		GraphicsResourceName motion_vectors;
		GraphicsResourceName depth;
	};

	struct DepthOutput
	{
		GraphicsResourceName depth;
	};

	void renderGBuffer(FrameGraph &fg, const RenderView &view, const GBufferOutput &output);
	void renderDepth(FrameGraph &fg, const RenderView &view, const DepthOutput &output);

private:
	struct Target
	{
		GraphicsResourceName name;
		Format format;
	};
	struct RenderTargets
	{
		eastl::vector<Target> color;
		Target depth;
		uint32_t layer = 0;
	};

	void render(FrameGraph &fg, const RenderView &view, const RenderTargets &targets);
	void render_meshlets(FrameGraph &fg, const RenderView &view, const RenderTargets &targets, bool clear);
	void cull_traditional(FrameGraph &fg, const RenderView &view);
	void render_traditional(FrameGraph &fg, const RenderView &view, const RenderTargets &targets);

	MeshletPass meshlet_pass;
};
