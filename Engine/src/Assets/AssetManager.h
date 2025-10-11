#pragma once
#include "Core/GUID.h"
#include "RHI/RHIDefinitions.h"
#include "Asset.h"
#include <filesystem>
#include <assimp/Importer.hpp>
#include <yaml-cpp/yaml.h>

class RHITexture;
class Model;
struct TextureDescription;

class AssetManager
{
public:
	static void init();
	static void shutdown();

	static RHITextureRef getTextureAsset(eastl::string path);
	static RHITextureRef getTextureAsset(eastl::string path, TextureDescription desc);
	static Ref<Model> getModelAsset(eastl::string path);

	static std::filesystem::path getPathFromGUID(Engine::GUID guid);
	static Engine::GUID getGUIDFromPath(const std::filesystem::path &path);

	static std::filesystem::path getRuntimeAssetPath(const std::filesystem::path &path);
	static std::filesystem::path getRuntimeAssetPath(Engine::GUID runtime_guid, eastl::string extension);

	static eastl::string getRuntimeExtension(AssetType asset_type);
	static AssetType getAssetTypeFromExtension(const eastl::string &extension);
	static const AssetMetadata &getMetadata(const std::filesystem::path &source_path);
	static AssetMetadata &getMutableMetadata(const std::filesystem::path &source_path);
	static void saveMetadata(const AssetMetadata &metadata);
	static bool isRuntimeExists(const std::filesystem::path &source_path);
	static void recreateRuntime(const std::filesystem::path &source_path);
	static void reloadAsset(const std::filesystem::path &source_path);
	static void removeAsset(const std::filesystem::path &source_path);
	static const AssetMetadata &importAsset(const std::filesystem::path &path);
	static void reloadAssets(const std::filesystem::path &path);
private:
	AssetManager() = delete;

	static Ref<Asset> load_texture_asset(eastl::string path, TextureDescription desc);
	static Ref<Asset> load_model_asset(eastl::string path);

	static eastl::unordered_map<Engine::GUID, AssetMetadata> registered_metadata;
	static eastl::unordered_map<eastl::string, Ref<Asset>> loaded_assets;
};