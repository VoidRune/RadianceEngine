project "GameShared"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
	}

	includedirs
	{
		"src",
		"../RadianceEngine/src",
	}

	externalincludedirs
	{
		"../Dependencies/GameNetworkingSockets/include",
		"dependencies/nlohmann/include",
	}

	links
	{
		"../Dependencies/GameNetworkingSockets/bin/Windows/GameNetworkingSockets",
	}

	filter "system:windows"
		systemversion "latest"
		defines { "WINDOWS" }

	filter "configurations:Debug"
		targetname "GameSharedDebug"
		defines { "DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		targetname "GameSharedRelease"
		defines { "RELEASE" }
		runtime "Release"
		optimize "On"
		symbols "On"

	filter "configurations:Dist"
		targetname "GameShared"
		defines { "DIST" }
		runtime "Release"
		optimize "On"
		symbols "Off"

	filter {}
