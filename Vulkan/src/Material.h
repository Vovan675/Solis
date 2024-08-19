#pragma once
#include "CerealExtensions.h"
#include "RHI/Texture.h"
#include "BindlessResources.h"

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
private:
		friend class cereal::access;
		template<class Archive>
		void save(Archive &archive) const
		{
			archive(cereal::make_nvp("albedo_value", albedo));
			archive(cereal::make_nvp("metalness_value", metalness));
			archive(cereal::make_nvp("roughness_value", roughness));
			archive(cereal::make_nvp("specular_value", specular));

			// TODO: replace with albedo_tex->save(); archive(albedo_tex.path)
			std::string no_texture = "no";
			if (albedo_tex_id != -1)
				archive(cereal::make_nvp("albedo_texture", BindlessResources::getTexture(albedo_tex_id)->getPath()));
			else
				archive(cereal::make_nvp("albedo_texture", no_texture));

			if (metalness_tex_id != -1)
				archive(cereal::make_nvp("metalness_texture", BindlessResources::getTexture(metalness_tex_id)->getPath()));
			else
				archive(cereal::make_nvp("metalness_texture", no_texture));

			if (roughness_tex_id != -1)
				archive(cereal::make_nvp("roughness_texture", BindlessResources::getTexture(roughness_tex_id)->getPath()));
			else
				archive(cereal::make_nvp("roughness_texture", no_texture));

			if (specular_tex_id != -1)
				archive(cereal::make_nvp("specular_texture", BindlessResources::getTexture(specular_tex_id)->getPath()));
			else
				archive(cereal::make_nvp("specular_texture", no_texture));
		}

		template<class Archive>
		void load(Archive &archive)
		{
			archive(cereal::make_nvp("albedo_value", albedo));
			archive(cereal::make_nvp("metalness_value", metalness));
			archive(cereal::make_nvp("roughness_value", roughness));
			archive(cereal::make_nvp("specular_value", specular));

			TextureDescription tex_description{};
			tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
			tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			
			Texture *tex;
			std::string tex_path;

			std::string no_texture = "no";
			archive(cereal::make_nvp("albedo_texture", tex_path));
			if (tex_path != no_texture)
			{
				tex = new Texture(tex_description);
				tex->load(tex_path.c_str());
				albedo_tex_id = BindlessResources::addTexture(tex);
			}

			archive(cereal::make_nvp("metalness_texture", tex_path));
			if (tex_path != no_texture)
			{
				tex = new Texture(tex_description);
				tex->load(tex_path.c_str());
				metalness_tex_id = BindlessResources::addTexture(tex);
			}

			archive(cereal::make_nvp("roughness_texture", tex_path));
			if (tex_path != no_texture)
			{
				tex = new Texture(tex_description);
				tex->load(tex_path.c_str());
				roughness_tex_id = BindlessResources::addTexture(tex);
			}

			archive(cereal::make_nvp("specular_texture", tex_path));
			if (tex_path != no_texture)
			{
				tex = new Texture(tex_description);
				tex->load(tex_path.c_str());
				specular_tex_id = BindlessResources::addTexture(tex);
			}
		}

};
