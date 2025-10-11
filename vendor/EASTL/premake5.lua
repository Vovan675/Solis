project "EASTL"
	kind "StaticLib"
	language "C++"

	targetdir "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/"
    objdir "obj/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/"

	files
	{
		"include/**.h",
		"include/**.cpp",
		"source/**.h",
		"source/**.cpp"
	}

	includedirs 
	{
		"include",
		"source",
		"../EABase/include/Common",
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"
		staticruntime "On"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++17"
		staticruntime "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Fast Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
