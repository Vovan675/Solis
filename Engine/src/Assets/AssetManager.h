#pragma once
#include "Core/GUID.h"
#include "RHI/RHIDefinitions.h"
#include "Asset.h"
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <entt/entt.hpp>

class RHITexture;
class Model;
struct TextureDescription;

class AssetManager
{
public:
	static void init();
	static void shutdown();

	static RHITextureRef getTextureAsset(eastl::string path, TextureDescription desc);
	static RHITextureRef getTextureAsset(eastl::string path);
	static RHITextureRef getTextureAssetByGuid(Engine::GUID guid);
	static Ref<Model> getModelAsset(eastl::string path);
	static Ref<Model> getModelAssetByGuid(Engine::GUID guid);

	static const AssetMetadata &getOrCreateMetadata(const std::filesystem::path &source_path);
	static void reimport(const std::filesystem::path &source_path);
	static void moveAsset(const std::filesystem::path &from, const std::filesystem::path &to);
	static void deleteAsset(const std::filesystem::path &source_path);
	static entt::sink<entt::sigh<void (Asset *)>> onPreReimport() { return {pre_reimport_signal}; }
	static entt::sink<entt::sigh<void (Asset *)>> onPostReimport() { return {post_reimport_signal}; }

	static std::filesystem::path getPathFromGUID(Engine::GUID guid);
	static Engine::GUID getGUIDFromPath(const std::filesystem::path &path);
	static AssetMetadata &getMetadata(const std::filesystem::path &source_path);
	static void saveMetadata(const AssetMetadata &metadata);

	static std::filesystem::path getRuntimeAssetPath(const std::filesystem::path &path);
	static bool isRuntimeExists(const std::filesystem::path &source_path);
	static void recreateRuntime(const std::filesystem::path &source_path);

	static AssetType getAssetTypeFromExtension(eastl::string extension);
	static const std::filesystem::path &getAssetsRoot() { return assets_root; }
private:
	AssetManager() = delete;

	static void index_assets(const std::filesystem::path &root);
	static AssetMetadata &register_metadata(const AssetMetadata &metadata);
	static eastl::string get_runtime_extension(AssetType asset_type);
	static std::filesystem::path compute_runtime_path(const std::filesystem::path &source_path, Engine::GUID runtime_guid, const eastl::string &extension);
	static Ref<Asset> load_texture_asset(eastl::string path, TextureDescription desc);
	static Ref<Asset> load_model_asset(eastl::string path);

	template<class T, class Loader>
	static Ref<T> get_or_load(const eastl::string &path, Loader &&loader);

	static std::filesystem::path assets_root;
	static eastl::unordered_map<Engine::GUID, AssetMetadata> guid_to_metadata;
	static eastl::unordered_map<eastl::string, Ref<Asset>> cache_key_to_asset;
	static eastl::unordered_map<eastl::string, Engine::GUID> path_to_guid;

	static entt::sigh<void (Asset *)> pre_reimport_signal;
	static entt::sigh<void (Asset *)> post_reimport_signal;
};
