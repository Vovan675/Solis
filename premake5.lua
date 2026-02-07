workspace "RenderingEngine"
	architecture "x64"

	configurations
	{
		"Debug",
		"Fast Debug",
		"Release"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["EABase"] = "vendor/EABase/include/Common"
IncludeDir["EASTL"] = "vendor/EASTL/include"
IncludeDir["GLFW"] = "vendor/GLFW/include"
IncludeDir["GLM"] = "vendor/glm"
IncludeDir["Vulkan"] = "%{VULKAN_SDK}/include"
IncludeDir["DirectX"] = "vendor/directx/include"
IncludeDir["SpdLog"] = "vendor/SpdLog/include"
IncludeDir["Assimp"] = "vendor/Assimp/include"
IncludeDir["Meshoptimizer"] = "vendor/meshoptimizer/src"
IncludeDir["STB_IMAGE"] = "vendor/stb"
IncludeDir["ImGui"] = "vendor/imgui"
IncludeDir["ImGuizmo"] = "vendor/imguizmo"
IncludeDir["YamlCpp"] = "vendor/yaml-cpp/include"
IncludeDir["Entt"] = "vendor/entt/include"
IncludeDir["PhysX"] = "vendor/physx/physx/include"
IncludeDir["SPIRV_Reflect"] = "vendor/spirv-reflect"
IncludeDir["Tracy"] = "vendor/tracy"
IncludeDir["WinPixRuntime"] = "vendor/WinPixEventRuntime"
IncludeDir["Compressonator"] = "vendor/Compressonator/include"

LibDir = {}
LibDir["Vulkan"] = "%{VULKAN_SDK}/Lib"
LibDir["Compressonator"] = "vendor/Compressonator/lib/bin/x64"

group "Dependencies"
include "vendor/EASTL"
include "vendor/GLFW"
include "vendor/Assimp"
include "vendor/premake/premake-meshoptimizer.lua"
include "vendor/ImGui"
include "vendor/ImGuizmo"
include "vendor/yaml-cpp"
include "vendor/premake/premake-physx.lua"
group ""

function copy_file_to_target_dir(from_dir, folder, name)
	postbuildcommands
	{
		string.format("{ECHO} Copying %s/%s to $(targetdir)%s%s", from_dir, name, folder, name),
		string.format('if not exist "$(targetdir)%s" mkdir "$(targetdir)%s"', folder, folder),
		string.format("{COPYFILE} \"%s/%s\" \"$(targetdir)%s%s\"", from_dir, name, folder, name)
	}
end

project "Engine"
	location "Engine"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "pch.h"
	pchsource "Engine/src/pch.cpp"
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{IncludeDir.EASTL}/../**.natvis",
		"%{IncludeDir.YamlCpp}/../**.natvis",
		"%{IncludeDir.Entt}/../natvis/**.natvis",
		--"%{IncludeDir.SPIRV_Reflect}/spirv_reflect.c",
		"%{IncludeDir.SPIRV_Reflect}/spirv_reflect.cpp",
		"%{IncludeDir.Tracy}/TracyClient.cpp",
	}

	includedirs
	{
		"Engine/src",
		"%{IncludeDir.EABase}",
		"%{IncludeDir.EASTL}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.GLM}",
		"%{IncludeDir.Vulkan}",
		"%{IncludeDir.DirectX}",
		"%{IncludeDir.SpdLog}",
		"%{IncludeDir.Assimp}",
		"%{IncludeDir.Meshoptimizer}",
		"%{IncludeDir.STB_IMAGE}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.YamlCpp}",
		"%{IncludeDir.Entt}",
		"%{IncludeDir.PhysX}",
		"%{IncludeDir.SPIRV_Reflect}",
		"%{IncludeDir.Tracy}/tracy",
		"%{IncludeDir.WinPixRuntime}/include",
		"%{IncludeDir.Compressonator}",
	}

	libdirs
	{
		"%{IncludeDir.WinPixRuntime}/lib",
		"%{LibDir.Compressonator}"
	}

	links
	{
		"EASTL",
		"GLFW",
		"%{LibDir.Vulkan}/vulkan-1.lib",
		"%{LibDir.Vulkan}/shaderc_shared.lib",
		"%{LibDir.Vulkan}/spirv-cross-c-shared.lib",
		"d3d12.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
		"dxcompiler.lib",
		"Assimp",
		"Meshoptimizer",
		"ImGui",
		"ImGuizmo",
		"YamlCpp",
		"Dwmapi",
		"PhysX",
		"WinPixEventRuntime.lib",
	}

	defines
	{
		"YAML_CPP_STATIC_DEFINE",
		"PX_PHYSX_STATIC_LIB"
	}

	filter "system:windows"
		systemversion "latest"
		-- Agility SDK
		copy_file_to_target_dir("%{wks.location}%{IncludeDir.DirectX}/../dlls/D3D12", "D3D12/", "D3D12Core.dll")
		copy_file_to_target_dir("%{wks.location}%{IncludeDir.DirectX}/../dlls/D3D12", "D3D12/", "D3D12Core.pdb")
		copy_file_to_target_dir("%{wks.location}%{IncludeDir.DirectX}/../dlls/D3D12", "D3D12/", "d3d12SDKLayers.dll")
		copy_file_to_target_dir("%{wks.location}%{IncludeDir.DirectX}/../dlls/D3D12", "D3D12/", "d3d12SDKLayers.pdb")
		-- WinPixRuntime
		copy_file_to_target_dir("%{wks.location}%{IncludeDir.WinPixRuntime}/dlls/", "/", "WinPixEventRuntime.dll")

	filter "configurations:Debug"
		editandcontinue "On"
		symbols "On"
		defines
		{
			"DEBUG",
			"_DEBUG"
		}
		links
		{
			"Compressonator_MTd.lib",
		}

	filter "configurations:Fast Debug"
		editandcontinue "Off"
		symbols "On"
		defines
		{
			"TRACY_ENABLE",
			"TRACY_ON_DEMAND"
		}
		links
		{
			"Compressonator_MTd.lib",
		}

	filter "configurations:Release"
		optimize "On"
		defines
		{
			"TRACY_ENABLE",
			"TRACY_ON_DEMAND",
			"NDEBUG"
		}
		links
		{
			"Compressonator_MT.lib",
		}