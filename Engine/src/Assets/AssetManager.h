#pragma once
#include "Core/GUID.h"
#include "RHI/RHIDefinitions.h"
#include "Asset.h"
#include "Core/ReflectionSerialization.h"
#include <filesystem>
#include <entt/entt.hpp>

class RHITexture;
class Model;
struct TextureDescription;

class AssetManager
{
public:
	static void init();
	static void refresh();
	static void shutdown();

	static Ref<Asset> getAsset(const std::filesystem::path &path, const AssetTypeInfo *type);
	template<typename T>
	static Ref<T> getAsset(const std::filesystem::path &path) { return getAsset(path, getTypeInfo<T>()); }
	template<typename T>
	static Ref<T> getAsset(const AssetReference &reference) { return getAsset(getPath(reference), getTypeInfo<T>()); }

	static RHITextureRef getTextureAsset(eastl::string path, TextureDescription desc);
	static RHITextureRef getTextureAsset(const AssetReference &reference, TextureDescription desc);
	static RHITextureRef getTextureAsset(eastl::string path);

	static Ref<Model> getModelAsset(eastl::string path);

	static std::filesystem::path getPath(Engine::GUID guid) { return getMetadata(guid).sourcePath; }
	static std::filesystem::path getPath(const AssetReference &reference);
	static Engine::GUID getGuid(const std::filesystem::path &source_path) { return getMetadata(source_path).guid; }

	static void reimport(const std::filesystem::path &source_path);
	static void moveAsset(const std::filesystem::path &from, const std::filesystem::path &to);
	static void deleteAsset(const std::filesystem::path &source_path);

	static void notifyChanged(Asset *asset) { post_reimport_signal.publish(asset); }
	static entt::sink<entt::sigh<void (Asset *)>> onPreReimport() { return {pre_reimport_signal}; }
	static entt::sink<entt::sigh<void (Asset *)>> onPostReimport() { return {post_reimport_signal}; }

	static AssetMetadata &getOrCreateMetadata(const std::filesystem::path &source_path);
	static AssetMetadata &getMetadata(const std::filesystem::path &source_path);
	static AssetMetadata &getMetadata(Engine::GUID guid);
	static void saveMetadata(const AssetMetadata &metadata);

	static std::filesystem::path getRuntimePath(const std::filesystem::path &source_path);
	static bool hasValidRuntime(const std::filesystem::path &source_path);
	static void recreateRuntime(const std::filesystem::path &source_path);

	template<typename T>
	static const AssetTypeInfo *registerType(const AssetTypeInfo &type)
	{
		const AssetTypeInfo *info = new AssetTypeInfo(type);
		get_type_infos().push_back(info);
		AssetTypeInfo::registered<T> = info;
		return info;
	}

	template<typename T>
	static const AssetTypeInfo *registerSerializedType(const char *extension)
	{
		AssetTypeInfo type{Reflected<T>::name, {extension}};
		type.load = [](const std::filesystem::path &path) -> Ref<Asset>
		{
			Ref<Asset> asset = new T();
			ReflectionYaml::loadFromFile(Reflected<T>::getInfo(), asset.getReference(), path);
			return asset;
		};
		type.structInfo = &Reflected<T>::getInfo();
		return registerType<T>(type);
	}

	template<typename T>
	static const AssetTypeInfo *getTypeInfo() { return AssetTypeInfo::registered<T>; }

	static const AssetTypeInfo *findTypeInfoByExtension(const eastl::string &extension);
	static const AssetTypeInfo *findTypeInfoByName(const char *name);
	static const eastl::vector<const AssetTypeInfo *> &getTypeInfos() { return get_type_infos(); }

	static const eastl::unordered_map<Engine::GUID, AssetMetadata> &getAllMetadata() { return guid_to_metadata; }
	static const std::filesystem::path &getAssetsRoot() { return assets_root; }
private:
	AssetManager() = delete;

	static eastl::vector<const AssetTypeInfo *> &get_type_infos()
	{
		static eastl::vector<const AssetTypeInfo *> infos;
		return infos;
	}

	static std::filesystem::path calc_runtime_path(const AssetMetadata &metadata);

	template<class T, class Loader>
	static Ref<T> get_or_load(const std::filesystem::path &path, Loader &&loader);
	static Ref<Asset> get_or_load(const std::filesystem::path &path, const AssetTypeInfo *type);

	static std::filesystem::path assets_root;
	static eastl::unordered_map<Engine::GUID, AssetMetadata> guid_to_metadata;
	static eastl::unordered_map<Engine::GUID, Ref<Asset>> guid_to_asset;
	static eastl::unordered_map<eastl::string, Engine::GUID> path_to_guid;

	static entt::sigh<void (Asset *)> pre_reimport_signal;
	static entt::sigh<void (Asset *)> post_reimport_signal;
};
