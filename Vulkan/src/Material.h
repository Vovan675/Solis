#pragma once
#include "BindlessResources.h"
#include "RHI/Texture.h"
#include "YamlExtensions.h"
#include "Assets/AssetManager.h"

class Material
{
public:
		int albedo_tex_id = -1;
		glm::vec4 albedo = glm::vec4(1, 1, 1, 1);

		int metalness_tex_id = -1;
		float metalness = 0.0f;

		int roughness_tex_id = -1;
		float roughness = 0.0f;

		int specular_tex_id = -1;
		float specular = 0.5f;

		struct PushConstant 
		{
			alignas(16) glm::mat4 model;

			glm::vec4 albedo = glm::vec4(0, 0, 0, 1);
			int albedo_tex_id = -1;
			int metalness_tex_id = -1;
			int roughness_tex_id = -1;
			int specular_tex_id = -1;
			// metalness, roughness, specular
			glm::vec4 shading = glm::vec4(0, 0, 0.5, 1);
		};

		PushConstant getPushConstant(glm::mat4 model) const
		{
			PushConstant push_constant;
			push_constant.model = model;
			push_constant.albedo = albedo;
			push_constant.albedo_tex_id = albedo_tex_id;
			push_constant.metalness_tex_id = metalness_tex_id;
			push_constant.roughness_tex_id = roughness_tex_id;
			push_constant.specular_tex_id = specular_tex_id;
			push_constant.shading = glm::vec4(metalness, roughness, specular, 1.0f);
			return push_constant;
		}
};

namespace YAML
{
	static YAML::Emitter &operator <<(YAML::Emitter &out, const std::shared_ptr<Material> &mat)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << mat->albedo;
		out << YAML::Key << "Metalness" << YAML::Value << mat->metalness;
		out << YAML::Key << "Roughness" << YAML::Value << mat->roughness;
		out << YAML::Key << "Specular" << YAML::Value << mat->specular;
		
		std::string no_texture = "no";
		if (mat->albedo_tex_id != -1)
			out << YAML::Key << "AlbedoTexture" << YAML::Value << BindlessResources::getTexture(mat->albedo_tex_id)->getPath();

		if (mat->metalness_tex_id != -1)
			out << YAML::Key << "MetalnessTexture" << YAML::Value << BindlessResources::getTexture(mat->metalness_tex_id)->getPath();

		if (mat->roughness_tex_id != -1)
			out << YAML::Key << "RoughnessTexture" << YAML::Value << BindlessResources::getTexture(mat->roughness_tex_id)->getPath();

		if (mat->specular_tex_id != -1)
			out << YAML::Key << "SpecularTexture" << YAML::Value << BindlessResources::getTexture(mat->specular_tex_id)->getPath();
		out << YAML::EndMap;
		return out;
	}

	template<>
	struct convert<std::shared_ptr<Material>>
	{
		static bool decode(const Node &node, std::shared_ptr<Material> &mat)
		{
			if (!node.IsMap())
				return false;

			mat = std::make_shared<Material>();
			mat->albedo = node["Albedo"].as<glm::vec4>();
			mat->metalness = node["Metalness"].as<float>();
			mat->roughness = node["Roughness"].as<float>();
			mat->specular = node["Specular"].as<float>();

			TextureDescription tex_description{};
			tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
			tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			// TODO: use asset manager
			if (node["AlbedoTexture"])
			{
				auto tex = AssetManager::getAsset<Texture>(node["AlbedoTexture"].as<std::string>());
				mat->albedo_tex_id = BindlessResources::addTexture(tex.get());
			}

			if (node["MetalnessTexture"])
			{
				auto tex = AssetManager::getAsset<Texture>(node["MetalnessTexture"].as<std::string>());
				mat->metalness_tex_id = BindlessResources::addTexture(tex.get());
			}

			if (node["RoughnessTexture"])
			{
				auto tex = AssetManager::getAsset<Texture>(node["RoughnessTexture"].as<std::string>());
				mat->roughness_tex_id = BindlessResources::addTexture(tex.get());
			}

			if (node["SpecularTexture"])
			{
				auto tex = AssetManager::getAsset<Texture>(node["SpecularTexture"].as<std::string>());
				mat->specular_tex_id = BindlessResources::addTexture(tex.get());
			}

			return true;
		}
	};
}
