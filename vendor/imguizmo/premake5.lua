project "ImGuizmo"
	kind "StaticLib"
	language "C++"

	targetdir "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/"
    objdir "obj/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/"

	files
	{
		"ImGuizmo.h",
		"ImGuizmo.cpp"
	}

	includedirs
	{
		"../imgui"
	}

	links 
	{ 
		"ImGui"
	}

    filter "system:windows"
		systemversion "latest"
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
