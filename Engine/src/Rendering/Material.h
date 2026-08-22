#pragma once
#include "RHI/BindlessResources.h"
#include "RHI/RHITexture.h"
#include "Core/Reflection.h"
#include "Assets/AssetManager.h"

struct LightingOnlyMaterial
{
	static constexpr float albedo = 0.3f;
	static constexpr float metalness = 0.0f;
	static constexpr float roughness = 1.0f;
	static constexpr float specular = 0.5f;
};

struct MaterialTexture
{
	AssetReference asset;
	int bindless_id = 0;
	Engine::GUID resolved_handle = 0;
};

REFLECT_BEGIN(MaterialTexture)
	REFLECT_FIELD(asset).label("Texture").asset<RHITexture>(),
REFLECT_END()

class Material : public Asset
{
public:
	MaterialTexture albedo_tex;
	glm::vec4 albedo = glm::vec4(1, 1, 1, 1);

	MaterialTexture metalness_tex;
	float metalness = 0.0f;

	MaterialTexture roughness_tex;
	float roughness = 0.6f;

	MaterialTexture specular_tex;
	float specular = 0.5f;

	MaterialTexture normal_tex;

	void update()
	{
		auto update_texture = [](MaterialTexture &material_texture, Format format)
		{
			if (material_texture.resolved_handle != material_texture.asset.guid)
			{
				material_texture.bindless_id = 0;
				material_texture.resolved_handle = material_texture.asset.guid;
			}

			if (material_texture.bindless_id == 0 && material_texture.asset.isValid())
			{
				TextureDescription desc{};
				desc.format = format;
				desc.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;
				auto tex = AssetManager::getTextureAsset(material_texture.asset, desc);
				if (!tex || !tex->isValid())
					return;
				material_texture.bindless_id = tex->getShaderResourceView()->getBindlessIndex();
			}
		};

		update_texture(albedo_tex, FORMAT_R8G8B8A8_SRGB);
		update_texture(metalness_tex, FORMAT_R8G8B8A8_UNORM);
		update_texture(roughness_tex, FORMAT_R8G8B8A8_UNORM);
		update_texture(specular_tex, FORMAT_R8G8B8A8_UNORM);
		update_texture(normal_tex, FORMAT_R8G8B8A8_UNORM);
	}

	bool usesTexture(Engine::GUID guid) const
	{
		return albedo_tex.asset.guid == guid ||
			metalness_tex.asset.guid == guid ||
			roughness_tex.asset.guid == guid ||
			specular_tex.asset.guid == guid ||
			normal_tex.asset.guid == guid;
	}

	void invalidateTextures()
	{
		albedo_tex.bindless_id = 0;
		metalness_tex.bindless_id = 0;
		roughness_tex.bindless_id = 0;
		specular_tex.bindless_id = 0;
		normal_tex.bindless_id = 0;
	}
};

REFLECT_BEGIN(Material)
	REFLECT_FIELD(albedo).label("Base Color").color(),
	REFLECT_FIELD(albedo_tex).label("Base Color Texture"),
	REFLECT_FIELD(metalness).range(0.0f, 1.0f).format("%.2f"),
	REFLECT_FIELD(metalness_tex).label("Metalness Texture"),
	REFLECT_FIELD(roughness).range(0.0f, 1.0f).format("%.2f"),
	REFLECT_FIELD(roughness_tex).label("Roughness Texture"),
	REFLECT_FIELD(specular).range(0.0f, 1.0f).format("%.2f"),
	REFLECT_FIELD(specular_tex).label("Specular Texture"),
	REFLECT_FIELD(normal_tex).label("Normal Texture"),
REFLECT_END()
