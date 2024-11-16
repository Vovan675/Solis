#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory.h>
#include <Windows.h>
#include <dwmapi.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include "vulkan/vulkan.h"

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/matrix_decompose.hpp"

#define YAML_CPP_DLL
#include "yaml-cpp/yaml.h"

#include "Log.h"