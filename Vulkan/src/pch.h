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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/matrix_decompose.hpp"

#define YAML_CPP_DLL
#include "yaml-cpp/yaml.h"

#include "Log.h"