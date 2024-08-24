#pragma once
enum ASSET_TYPE
{
	ASSET_TYPE_TEXTURE,
	ASSET_TYPE_MODEL,
};

class Asset
{
public:
	uint32_t asset_id; // TODO: use UUID

	virtual ASSET_TYPE getAssetType() const = 0;
};

