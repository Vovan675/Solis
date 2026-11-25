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
		static ShaderSet fromFile(const wchar_t *file, eastl::vector<eastl::pair<const char *, const char *>> defines = {}, const char *pixel_entry = "");
	};

	struct RenderView
	{
		uint32_t view_id = 0;
		uint32_t pass_mask;
		uint32_t instance_count;
		glm::ivec2 render_size;
		GraphicsResourceName hiz;
		TextureDescription hiz_description{};
		uint32_t layer = 0;
		bool use_two_pass_occlusion = true;
		bool freeze_occlusion = false;
		bool depth_clip = true;
		bool depth_write = true;
		CompareFunc depth_func = CompareFunc::GREATER;
		BlendState blend{};
		CullMode cull_mode = CullMode::BACK;
		bool pick = false;
		ShaderSet shaders;

		bool isReverseZ() const { return depth_func == CompareFunc::GREATER || depth_func == CompareFunc::GREATER_EQUAL; }
		float getDepthClear() const { return isReverseZ() ? 0.0f : 1.0f; }
		uint32_t getViewPassId() const { return view_id << 8 | pass_mask; }
	};

	struct GBufferOutput
	{
		GraphicsResourceName albedo;
		GraphicsResourceName normal;
		GraphicsResourceName shading;
		GraphicsResourceName emissive;
		GraphicsResourceName motion_vectors;
		GraphicsResourceName depth;
	};

	struct DepthOutput
	{
		GraphicsResourceName depth;
	};

	struct DecalOutput
	{
		GraphicsResourceName albedo;
		GraphicsResourceName shading;
		GraphicsResourceName depth;
	};

	void renderGBuffer(FrameGraph &fg, const RenderView &view, const GBufferOutput &output);
	void renderDepth(FrameGraph &fg, const RenderView &view, const DepthOutput &output);
	void renderDecals(FrameGraph &fg, const RenderView &view, const DecalOutput &output);

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
