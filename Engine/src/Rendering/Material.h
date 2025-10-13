#pragma once
#include "RHI/BindlessResources.h"
#include "RHI/RHITexture.h"
#include "Utils/YamlExtensions.h"
#include "Assets/AssetManager.h"
#include "Utils/Stream.h"

class Material : public RefCounted
{
public:
		struct MaterialTexture
		{
			Engine::GUID asset_handle = 0;
			int bindless_id = -1;
		};

		MaterialTexture albedo_tex;
		glm::vec4 albedo = glm::vec4(1, 1, 1, 1);

		MaterialTexture metalness_tex;
		float metalness = 0.0f;

		MaterialTexture roughness_tex;
		float roughness = 0.6f;

		MaterialTexture specular_tex;
		float specular = 0.5f;

		MaterialTexture normal_tex;

		struct PushConstant 
		{
			glm::mat4 model;
			glm::mat4 imodel;

			glm::vec4 albedo = glm::vec4(0, 0, 0, 1);
			int albedo_tex_id = -1;
			int metalness_tex_id = -1;
			int roughness_tex_id = -1;
			int specular_tex_id = -1;
			// metalness, roughness, specular
			glm::vec4 shading = glm::vec4(0, 0, 0.5, 1);
			int normal_tex_id = -1;
		};

		void update()
		{
			auto update_texture = [](MaterialTexture &material_texture)
			{
				if (material_texture.bindless_id == -1 && material_texture.asset_handle.isValid())
				{
					auto runtime_path = AssetManager::getPathFromGUID(material_texture.asset_handle);
					if (runtime_path.empty())
						return;
					auto tex = AssetManager::getTextureAsset(runtime_path.string().c_str()); // TODO: desc
					if (!tex || !tex->isValid())
						return;
					material_texture.bindless_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
				}
			};

			update_texture(albedo_tex);
			update_texture(metalness_tex);
			update_texture(roughness_tex);
			update_texture(specular_tex);
			update_texture(normal_tex);
		}

		PushConstant getPushConstant(glm::mat4 model, glm::mat4 imodel) const
		{
			PushConstant push_constant;
			push_constant.model = model;
			push_constant.imodel = imodel;
			push_constant.albedo = albedo;
			push_constant.albedo_tex_id = albedo_tex.bindless_id;
			push_constant.metalness_tex_id = metalness_tex.bindless_id;
			push_constant.roughness_tex_id = roughness_tex.bindless_id;
			push_constant.specular_tex_id = specular_tex.bindless_id;
			push_constant.shading = glm::vec4(metalness, roughness, specular, 1.0f);
			push_constant.normal_tex_id = normal_tex.bindless_id;
			return push_constant;
		}

		void serialize(Stream &stream)
		{
			stream.write(albedo);
			stream.write(metalness);
			stream.write(roughness);
			stream.write(specular);

			const auto write_tex = [&stream](MaterialTexture &tex)
			{
				stream.write(tex.asset_handle);
			};

			write_tex(albedo_tex);
			write_tex(metalness_tex);
			write_tex(roughness_tex);
			write_tex(specular_tex);
			write_tex(normal_tex);
		}

		void deserialize(Stream &stream)
		{
			stream.read(albedo);
			stream.read(metalness);
			stream.read(roughness);
			stream.read(specular);

			TextureDescription non_srgb_description = {};
			non_srgb_description.format = FORMAT_R8G8B8A8_UNORM;
			non_srgb_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

			const auto read_tex = [&stream, &non_srgb_description](MaterialTexture &tex, bool non_srgb = false)
			{
				tex.bindless_id = -1;

				Engine::GUID tex_guid;
				stream.read(tex_guid);
				tex.asset_handle = tex_guid;
			};

			read_tex(albedo_tex);
			read_tex(metalness_tex, true);
			read_tex(roughness_tex, true);
			read_tex(specular_tex, true);
			read_tex(normal_tex, true);
		}
};

namespace YAML
{
	static YAML::Emitter &operator <<(YAML::Emitter &out, const Ref<Material> &mat)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << mat->albedo;
		out << YAML::Key << "Metalness" << YAML::Value << mat->metalness;
		out << YAML::Key << "Roughness" << YAML::Value << mat->roughness;
		out << YAML::Key << "Specular" << YAML::Value << mat->specular;
		out << YAML::Key << "Normal" << YAML::Value << mat->specular;
		
		eastl::string no_texture = "no";
		if (mat->albedo_tex.bindless_id != -1)
			out << YAML::Key << "AlbedoTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->albedo_tex.bindless_id)->asset_handle;

		if (mat->metalness_tex.bindless_id != -1)
			out << YAML::Key << "MetalnessTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->metalness_tex.bindless_id)->asset_handle;

		if (mat->roughness_tex.bindless_id != -1)
			out << YAML::Key << "RoughnessTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->roughness_tex.bindless_id)->asset_handle;

		if (mat->specular_tex.bindless_id != -1)
			out << YAML::Key << "SpecularTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->specular_tex.bindless_id)->asset_handle;

		if (mat->normal_tex.bindless_id != -1)
			out << YAML::Key << "NormalTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->normal_tex.bindless_id)->asset_handle;
		out << YAML::EndMap;
		return out;
	}

	template<>
	struct convert<Ref<Material>>
	{
		static bool decode(const Node &node, Ref<Material> &mat)
		{
			if (!node.IsMap())
				return false;

			mat = new Material();
			mat->albedo = node["Albedo"].as<glm::vec4>();
			mat->metalness = node["Metalness"].as<float>();
			mat->roughness = node["Roughness"].as<float>();
			mat->specular = node["Specular"].as<float>();

			if (node["AlbedoTexture"])
			{
				auto tex = AssetManager::getTextureAsset(node["AlbedoTexture"].as<eastl::string>());
				mat->albedo_tex.bindless_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["MetalnessTexture"])
			{
				TextureDescription tex_description{};
				tex_description.format = FORMAT_R8G8B8A8_UNORM;
				tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

				auto tex = AssetManager::getTextureAsset(node["MetalnessTexture"].as<eastl::string>(), tex_description);
				mat->metalness_tex.bindless_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["RoughnessTexture"])
			{
				TextureDescription tex_description{};
				tex_description.format = FORMAT_R8G8B8A8_UNORM;
				tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

				auto tex = AssetManager::getTextureAsset(node["RoughnessTexture"].as<eastl::string>(), tex_description);
				mat->roughness_tex.bindless_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["SpecularTexture"])
			{
				TextureDescription tex_description{};
				tex_description.format = FORMAT_R8G8B8A8_UNORM;
				tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

				auto tex = AssetManager::getTextureAsset(node["SpecularTexture"].as<eastl::string>(), tex_description);
				mat->specular_tex.bindless_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["NormalTexture"])
			{
				TextureDescription tex_description{};
				tex_description.format = FORMAT_R8G8B8A8_UNORM;
				tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

				auto tex = AssetManager::getTextureAsset(node["NormalTexture"].as<eastl::string>(), tex_description);
				mat->normal_tex.bindless_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			return true;
		}
	};
}
