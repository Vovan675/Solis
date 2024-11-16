project "Assimp"
    kind "StaticLib"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

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
        ".",
        "include",
        "code",
        "code/common",
        "contrib",
		"contrib/zlib",
	 	"contrib/zip/src",
	 	"contrib/unzip",
		"contrib/pugixml/src",
		"contrib/utf8cpp/source",
		"contrib/rapidjson/include"
    }

    defines {
        -- "SWIG",
        "ASSIMP_BUILD_NO_OWN_ZLIB",

        "ASSIMP_BUILD_NO_X_IMPORTER",
        "ASSIMP_BUILD_NO_3DS_IMPORTER",
        "ASSIMP_BUILD_NO_MD3_IMPORTER",
        "ASSIMP_BUILD_NO_MDL_IMPORTER",
        "ASSIMP_BUILD_NO_MD2_IMPORTER",
        -- "ASSIMP_BUILD_NO_PLY_IMPORTER",
        "ASSIMP_BUILD_NO_ASE_IMPORTER",
        -- "ASSIMP_BUILD_NO_OBJ_IMPORTER",
        "ASSIMP_BUILD_NO_AMF_IMPORTER",
        "ASSIMP_BUILD_NO_HMP_IMPORTER",
        "ASSIMP_BUILD_NO_SMD_IMPORTER",
        "ASSIMP_BUILD_NO_MDC_IMPORTER",
        "ASSIMP_BUILD_NO_MD5_IMPORTER",
        "ASSIMP_BUILD_NO_STL_IMPORTER",
        "ASSIMP_BUILD_NO_LWO_IMPORTER",
        "ASSIMP_BUILD_NO_DXF_IMPORTER",
        "ASSIMP_BUILD_NO_NFF_IMPORTER",
        "ASSIMP_BUILD_NO_RAW_IMPORTER",
        "ASSIMP_BUILD_NO_OFF_IMPORTER",
        "ASSIMP_BUILD_NO_AC_IMPORTER",
        "ASSIMP_BUILD_NO_BVH_IMPORTER",
        "ASSIMP_BUILD_NO_IRRMESH_IMPORTER",
        "ASSIMP_BUILD_NO_IRR_IMPORTER",
        "ASSIMP_BUILD_NO_Q3D_IMPORTER",
        "ASSIMP_BUILD_NO_B3D_IMPORTER",
        -- "ASSIMP_BUILD_NO_COLLADA_IMPORTER",
        "ASSIMP_BUILD_NO_TERRAGEN_IMPORTER",
        "ASSIMP_BUILD_NO_CSM_IMPORTER",
        "ASSIMP_BUILD_NO_3D_IMPORTER",
        "ASSIMP_BUILD_NO_LWS_IMPORTER",
        "ASSIMP_BUILD_NO_OGRE_IMPORTER",
        "ASSIMP_BUILD_NO_OPENGEX_IMPORTER",
        "ASSIMP_BUILD_NO_MS3D_IMPORTER",
        "ASSIMP_BUILD_NO_COB_IMPORTER",
        "ASSIMP_BUILD_NO_BLEND_IMPORTER",
        "ASSIMP_BUILD_NO_Q3BSP_IMPORTER",
        "ASSIMP_BUILD_NO_NDO_IMPORTER",
		"ASSIMP_BUILD_NO_COMPRESSED_IFC",
        "ASSIMP_BUILD_NO_IFC_IMPORTER",
        "ASSIMP_BUILD_NO_XGL_IMPORTER",
        -- "ASSIMP_BUILD_NO_FBX_IMPORTER",
        "ASSIMP_BUILD_NO_ASSBIN_IMPORTER",
        -- "ASSIMP_BUILD_NO_GLTF_IMPORTER",
        "ASSIMP_BUILD_NO_C4D_IMPORTER",
        "ASSIMP_BUILD_NO_3MF_IMPORTER",
        "ASSIMP_BUILD_NO_X3D_IMPORTER",
        "ASSIMP_BUILD_NO_MMD_IMPORTER",
            
        "ASSIMP_BUILD_NO_STEP_EXPORTER",
        "ASSIMP_BUILD_NO_SIB_IMPORTER",

		"ASSIMP_BUILD_NO_OPENGEX_EXPORTER",
		"ASSIMP_BUILD_NO_GLTF_EXPORTER",
		"ASSIMP_BUILD_NO_EXPORT",

        -- "ASSIMP_BUILD_NO_MAKELEFTHANDED_PROCESS",
        -- "ASSIMP_BUILD_NO_FLIPUVS_PROCESS",
        -- "ASSIMP_BUILD_NO_FLIPWINDINGORDER_PROCESS",
        -- "ASSIMP_BUILD_NO_CALCTANGENTS_PROCESS",
        "ASSIMP_BUILD_NO_JOINVERTICES_PROCESS",
        -- "ASSIMP_BUILD_NO_TRIANGULATE_PROCESS",
        --"ASSIMP_BUILD_NO_GENFACENORMALS_PROCESS",
        -- "ASSIMP_BUILD_NO_GENVERTEXNORMALS_PROCESS",
        "ASSIMP_BUILD_NO_REMOVEVC_PROCESS",
        "ASSIMP_BUILD_NO_SPLITLARGEMESHES_PROCESS",
        "ASSIMP_BUILD_NO_PRETRANSFORMVERTICES_PROCESS",
        "ASSIMP_BUILD_NO_LIMITBONEWEIGHTS_PROCESS",
        -- "ASSIMP_BUILD_NO_VALIDATEDS_PROCESS",
        "ASSIMP_BUILD_NO_IMPROVECACHELOCALITY_PROCESS",
        "ASSIMP_BUILD_NO_FIXINFACINGNORMALS_PROCESS",
        "ASSIMP_BUILD_NO_REMOVE_REDUNDANTMATERIALS_PROCESS",
        "ASSIMP_BUILD_NO_FINDINVALIDDATA_PROCESS",
        "ASSIMP_BUILD_NO_FINDDEGENERATES_PROCESS",
        -- "ASSIMP_BUILD_NO_SORTBYPTYPE_PROCESS",
        "ASSIMP_BUILD_NO_GENUVCOORDS_PROCESS",
        "ASSIMP_BUILD_NO_TRANSFORMTEXCOORDS_PROCESS",
        "ASSIMP_BUILD_NO_FINDINSTANCES_PROCESS",
        -- "ASSIMP_BUILD_NO_OPTIMIZEMESHES_PROCESS",
        "ASSIMP_BUILD_NO_OPTIMIZEGRAPH_PROCESS",
        "ASSIMP_BUILD_NO_SPLITBYBONECOUNT_PROCESS",
        "ASSIMP_BUILD_NO_DEBONE_PROCESS",
        "ASSIMP_BUILD_NO_EMBEDTEXTURES_PROCESS",
        "ASSIMP_BUILD_NO_GLOBALSCALE_PROCESS",
    }

    files {
        "revision.h",
        "include/**.h",
        "include/**.cpp",
        "code/**.h",
        "code/**.cpp",
        "contrib/poly2tri/poly2tri/**.h",
		"contrib/poly2tri/poly2tri/**.cc",
		"contrib/pugixml/src/**.cpp",
		"contrib/pugixml/src/**.hpp",
		"contrib/rapidjson/include/rapidjson/**.h",
		"contrib/stb/**.h",
		"contrib/unzip/**.h",
		"contrib/unzip/**.c",
		"contrib/utf8cpp/source/**/h",
		"contrib/zip/src/**.h",
		"contrib/zip/src/**.c",
		"contrib/zlib/**.h",
		"contrib/zlib/**.c"
    }
