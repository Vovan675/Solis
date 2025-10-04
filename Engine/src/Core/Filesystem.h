#pragma once
#include <string>

class Filesystem
{
public:
	static std::string saveFileDialog();
	static std::string openFileDialog();
};