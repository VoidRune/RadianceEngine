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

project "Balance"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	debugdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")

	dependson
	{
		"GameShared",
		"RadianceEngine",
	}

	files
	{
		"tools/Balance/**.h",
		"tools/Balance/**.cpp",
	}

	includedirs
	{
		"src",
		"../RadianceEngine/src",
	}

	externalincludedirs
	{
		"dependencies/nlohmann/include",
	}

	links
	{
		"GameShared",
		"RadianceEngine",
	}

	filter "system:windows"
		systemversion "latest"
		defines { "WINDOWS" }

	filter "configurations:Debug"
		targetname "BalanceDebug"
		defines { "DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		targetname "BalanceRelease"
		defines { "RELEASE" }
		runtime "Release"
		optimize "On"
		symbols "On"

	filter "configurations:Dist"
		targetname "Balance"
		defines { "DIST" }
		runtime "Release"
		optimize "On"
		symbols "Off"

	filter {}
