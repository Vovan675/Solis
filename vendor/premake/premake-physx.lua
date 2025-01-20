project "PhysX"
	kind "StaticLib"

	targetdir ("physx/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("physx/bin-int/" .. outputdir .. "/%{prj.name}")

	characterset "ASCII"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Fast Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "system:windows"
		systemversion "latest"
		staticruntime "On"
		
	includedirs {
		"../physx/physx/include",
		"../physx/physx/source/geomutils/include",
		"../physx/physx/source/geomutils/src",
		"../physx/physx/source/geomutils/src/common",
		"../physx/physx/source/geomutils/src/distance",
		"../physx/physx/source/common/src",
		"../physx/physx/source/lowlevelaabb/include",
		"../physx/physx/source/lowlevel/api/include",
		"../physx/physx/source/scenequery/include",
		"../physx/physx/source/simulationcontroller/include",
		"../physx/physx/source/lowlevel/software/include",
		"../physx/physx/source/lowlevel/common/include/pipeline",
		"../physx/physx/source/lowleveldynamics/include",
		"../physx/physx/source/geomutils/src/convex",
		"../physx/physx/source/geomutils/src/gjk",
		"../physx/physx/source/geomutils/src/mesh",
		"../physx/physx/source/geomutils/src/pcm",
		"../physx/physx/source/lowlevel/common/include/utils",
		"../physx/physx/source/simulationcontroller/src",
		"../physx/physx/source/physxextensions/src",
		"../physx/physx/source/pvd/include",
		"../physx/physx/source/common/include",
		"../physx/physx/source/geomutils/src/sweep",
		"../physx/physx/source/geomutils/src/intersection",
		"../physx/physx/source/geomutils/src/hf",
		"../physx/physx/source/geomutils/src/contact",
		"../physx/physx/source/lowleveldynamics/shared",
		"../physx/physx/source/physxvehicle/src/physxmetadata/include",
		"../physx/physx/source/physxgpu/include",
		"../physx/physx/source/physxmetadata/core/include",
		"../physx/physx/source/physxmetadata/extensions/include",
		"../physx/physx/source/lowlevel/common/include/collision",
		"../physx/physx/source/physxextensions/src/serialization/File",
		"../physx/physx/source/physxextensions/src/serialization/Xml",
		"../physx/physx/source/geomutils/src/ccd",
		"../physx/physx/source/physx/src",
		"../physx/physx/source/physx/src/device",
		"../physx/physx/source/filebuf/include",
		"../physx/physx/source/physxvehicle/src",
		"../physx/physx/source/physxextensions/src/serialization/Binary",
	}

	defines {
		"PX_PHYSX_STATIC_LIB" 
	}

	files {
		"../physx/physx/include/**.h",
		"../physx/physx/source/**.cpp",
	}

	removefiles {
		"../physx/physx/source/foundation/unix/**.cpp",
	}
