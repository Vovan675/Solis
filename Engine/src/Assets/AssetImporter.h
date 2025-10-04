#pragma once
#include "Asset.h"

// Loads any asset data using AssetSerializers
class AssetImporter
{
public:
	static bool loadAsset(const AssetMetadata &metadata, Ref<Asset> &asset);

};