#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>

#include <EASTL/string.h>
#include <EASTL/queue.h>
#include <EASTL/array.h>
#include <EASTL/algorithm.h>
#include <EASTL/sort.h>
#include <EASTL/map.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/fixed_map.h>
#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/hash_set.h>
#include <EASTL/list.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>

#include "d3d12.h"
#include <D3Dcompiler.h>
#include <dxgi1_4.h>
#include <dxcapi.h>
#include <d3dx12/d3dx12.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

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
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

#define YAML_CPP_DLL
#include "yaml-cpp/yaml.h"

#include "Core/Memory.h"
#include "Core/Log.h"

#include "Utils/StringUtils.h"

#define ENGINE_ASSERT(condition) assert(condition)
#define ENGINE_ASSERT_MSG(condition, message) assert(condition && message)

template<typename T>
struct EnableEnumBits
{
	static constexpr bool enable = false;
};

template<typename T>
typename std::enable_if_t<EnableEnumBits<T>::enable, T> operator|(T lhs, T rhs) 
{
	return static_cast<T>(
		static_cast<std::underlying_type<T>::type>(lhs) |
		static_cast<std::underlying_type<T>::type>(rhs));
}

template<typename T>
typename std::enable_if_t<EnableEnumBits<T>::enable, T> operator&(T lhs, T rhs) 
{
	return static_cast<T>(
		static_cast<std::underlying_type<T>::type>(lhs) &
		static_cast<std::underlying_type<T>::type>(rhs));
}

template<typename T>
typename std::enable_if_t<EnableEnumBits<T>::enable, T> &operator|=(T &lhs, T rhs) 
{
	lhs = lhs | rhs;
	return lhs;
}

#define ALLOW_ENUM_BITS(type) template<> struct EnableEnumBits<type> { static constexpr bool enable = true; };

template<typename T>
inline constexpr bool hasAnyFlags(T value, T flags)
{
	return (static_cast<std::underlying_type_t<T>>(value) & static_cast<std::underlying_type_t<T>>(flags)) != 0;
}