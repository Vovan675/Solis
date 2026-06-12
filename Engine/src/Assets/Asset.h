#pragma once
#include "Core/Core.h"

enum AssetType
{
	ASSET_TYPE_UNDEFINED,
	ASSET_TYPE_TEXTURE, // TODO: ASSET_TYPE_IMAGE instead?
	ASSET_TYPE_MODEL,
};

class Asset : public RefCounted
{
public:
	Engine::GUID asset_handle;

	virtual AssetType getAssetType() const = 0;
	virtual void reload() {}
};

struct AssetMetadata
{
	Engine::GUID asset_handle = 0;
	Engine::GUID runtime_handle = 0;
	std::filesystem::path source_path;
	AssetType type;
	YAML::Node params;

	bool isValid() const
	{
		return asset_handle != 0 && source_path != "";
	}
};