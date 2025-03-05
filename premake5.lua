workspace "LearnVulkan"
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
IncludeDir["GLFW"] = "vendor/GLFW/include"
IncludeDir["GLM"] = "vendor/glm"
IncludeDir["Vulkan"] = "%{VULKAN_SDK}/include"
IncludeDir["DirectX"] = "vendor/directx/include"
IncludeDir["SpdLog"] = "vendor/SpdLog/include"
IncludeDir["Assimp"] = "vendor/Assimp/include"
IncludeDir["STB_IMAGE"] = "vendor/stb"
IncludeDir["ImGui"] = "vendor/imgui"
IncludeDir["ImGuizmo"] = "vendor/imguizmo"
IncludeDir["YamlCpp"] = "vendor/yaml-cpp/include"
IncludeDir["Entt"] = "vendor/entt/include"
IncludeDir["PhysX"] = "vendor/physx/physx/include"
IncludeDir["SPIRV_Reflect"] = "vendor/spirv-reflect"
IncludeDir["Tracy"] = "vendor/tracy"
IncludeDir["WinPixRuntime"] = "vendor/WinPixEventRuntime"

LibDir = {}
LibDir["Vulkan"] = "%{VULKAN_SDK}/Lib"

group "Dependencies"
include "vendor/GLFW"
include "vendor/Assimp"
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

project "Vulkan"
	location "Vulkan"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "pch.h"
	pchsource "Vulkan/src/pch.cpp"
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{IncludeDir.YamlCpp}/../**.natvis",
		"%{IncludeDir.Entt}/../natvis/**.natvis",
		--"%{IncludeDir.SPIRV_Reflect}/spirv_reflect.c",
		"%{IncludeDir.SPIRV_Reflect}/spirv_reflect.cpp",
		"%{IncludeDir.Tracy}/TracyClient.cpp",
	}

	includedirs
	{
		"Vulkan/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.GLM}",
		"%{IncludeDir.Vulkan}",
		"%{IncludeDir.DirectX}",
		"%{IncludeDir.SpdLog}",
		"%{IncludeDir.Assimp}",
		"%{IncludeDir.STB_IMAGE}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.YamlCpp}",
		"%{IncludeDir.Entt}",
		"%{IncludeDir.PhysX}",
		"%{IncludeDir.SPIRV_Reflect}",
		"%{IncludeDir.Tracy}/tracy",
		"%{IncludeDir.WinPixRuntime}/include",
	}

	libdirs
	{
		"%{IncludeDir.WinPixRuntime}/lib"
	}

	links
	{
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

	filter "configurations:Fast Debug"
		editandcontinue "Off"
		symbols "On"
		defines
		{
			"TRACY_ENABLE"
		}

	filter "configurations:Release"
		optimize "On"
		defines
		{
			"TRACY_ENABLE",
			"NDEBUG"
		}