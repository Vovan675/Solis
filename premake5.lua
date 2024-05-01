workspace "LearnVulkan"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "vendor/GLFW/include"
IncludeDir["GLM"] = "vendor/glm"
IncludeDir["Vulkan"] = "vendor/Vulkan/include"
IncludeDir["SpdLog"] = "vendor/SpdLog/include"
IncludeDir["Assimp"] = "vendor/Assimp/assimp/include"
IncludeDir["STB_IMAGE"] = "vendor/stb"
IncludeDir["ImGui"] = "vendor/imgui"

LibDir = {}
LibDir["Vulkan"] = "../vendor/Vulkan/Lib"

group "Dependencies"
include "vendor/GLFW"
include "vendor/Assimp"
include "vendor/ImGui"
group ""

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
		"%{prj.name}/src/**.cpp"
	}

	includedirs
	{
		"Vulkan/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.GLM}",
		"%{IncludeDir.Vulkan}",
		"%{IncludeDir.SpdLog}",
		"%{IncludeDir.Assimp}",
		"%{IncludeDir.STB_IMAGE}",
		"%{IncludeDir.ImGui}"
	}

	links
	{
		"GLFW",
		"%{LibDir.Vulkan}/vulkan-1.lib",
		"%{LibDir.Vulkan}/shaderc_shared.lib",
		"%{LibDir.Vulkan}/spirv-cross-c-shared.lib",
		"Assimp",
		"ImGui"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"