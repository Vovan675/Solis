#pragma once
#include "Image.h"
#include <filesystem>

class ImageExtensions
{
	static bool save(const Ref<Image> image, std::filesystem::path path);
};