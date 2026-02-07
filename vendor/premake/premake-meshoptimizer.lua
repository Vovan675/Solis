project "Meshoptimizer"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "On"

	targetdir ("meshoptimizer/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("meshoptimizer/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"../meshoptimizer/src/**.h",
		"../meshoptimizer/src/**.cpp",
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Fast Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
