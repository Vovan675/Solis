#pragma once
#include <vector>
#include "RHI/Texture.h"
#include "BindlessResources.h"

enum RENDER_TARGETS
{
	RENDER_TARGET_GBUFFER_ALBEDO,
	RENDER_TARGET_GBUFFER_NORMAL,
	RENDER_TARGET_GBUFFER_DEPTH_STENCIL,
	RENDER_TARGET_GBUFFER_POSITION, // TODO: Remove (get from depth in shaders)
	RENDER_TARGET_GBUFFER_SHADING, // R - metalness, G - roughness, B - specular, A - ?

	RENDER_TARGET_LIGHTING_DIFFUSE,
	RENDER_TARGET_LIGHTING_SPECULAR,

	RENDER_TARGET_COMPOSITE,

	RENDER_TARGET_TEXTURES_COUNT
};

class Renderer
{
public:
	Renderer() = delete;

	static void init();
	static void recreateScreenResources();

	static std::shared_ptr<Texture> getRenderTarget(RENDER_TARGETS rt) { return screen_resources[rt]; }
	static uint32_t getRenderTargetBindlessId(RENDER_TARGETS rt) { return BindlessResources::getTextureIndex(screen_resources[rt].get()); }
private:
	static std::array<std::shared_ptr<Texture>, RENDER_TARGET_TEXTURES_COUNT> screen_resources;
};

