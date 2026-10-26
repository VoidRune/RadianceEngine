project "UiAtlas"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	debugdir ("../bin/%{cfg.system}-%{cfg.architecture}/Game")

	files
	{
		"tools/UiAtlas/**.h",
		"tools/UiAtlas/**.cpp",
		"src/UI/UiAtlasSources.h",
		"src/UI/VectorAtlas.h",
		"src/UI/VectorAtlas.cpp",
		"dependencies/msdfgen/msdfgen/*.h",
		"dependencies/msdfgen/msdfgen/core/*.h",
		"dependencies/msdfgen/msdfgen/core/*.hpp",
		"dependencies/msdfgen/msdfgen/core/*.cpp",
		"dependencies/nanosvg/*.h",
		"../Dependencies/stb/stb_truetype.h",
	}

	includedirs
	{
		"src",
	}

	externalincludedirs
	{
		"dependencies",
		"dependencies/msdfgen",
		"../Dependencies",
	}

	defines { "_CRT_SECURE_NO_WARNINGS" }
	runtime "Release"
	optimize "Speed"
	symbols "On"
	editandcontinue "Off"
	runtimechecks "Off"

	filter "files:dependencies/msdfgen/**.cpp"
		warnings "Off"

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		targetname "UiAtlasDebug"

	filter "configurations:Release"
		targetname "UiAtlasRelease"

	filter "configurations:Dist"
		targetname "UiAtlas"

	filter {}

project "ModelBaker"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	debugdir ("../bin/%{cfg.system}-%{cfg.architecture}/Game")

	files
	{
		"tools/ModelBaker/**.h",
		"tools/ModelBaker/**.cpp",
		"src/Rendering/ModelSources.h",
		"src/Rendering/ModelSurfaces.h",
		"src/Rendering/ModelData.h",
		"src/Rendering/ModelData.cpp",
		"dependencies/bc7enc/bc7enc.h",
		"dependencies/bc7enc/bc7enc.c",
		"dependencies/bc7enc/rgbcx.h",
		"dependencies/mikktspace/mikktspace.h",
		"dependencies/mikktspace/mikktspace.c",
	}

	includedirs
	{
		"src",
	}

	externalincludedirs
	{
		"dependencies",
		"../Dependencies",
	}

	defines { "_CRT_SECURE_NO_WARNINGS" }
	runtime "Release"
	optimize "Speed"
	symbols "On"
	editandcontinue "Off"
	runtimechecks "Off"

	filter "files:dependencies/**.c"
		warnings "Off"

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		targetname "ModelBakerDebug"

	filter "configurations:Release"
		targetname "ModelBakerRelease"

	filter "configurations:Dist"
		targetname "ModelBaker"

	filter {}

project "Game"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	debugdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")

	dependson
	{
		"RadianceEngine",
		"TimbreAudio",
		"GameShared",
		"UiAtlas",
		"ModelBaker",
	}

	files
	{
		"src/**.h",
		"src/**.cpp",
		"dependencies/imgui/*.h",
		"dependencies/imgui/*.cpp",
	}

	includedirs
	{
		"src",
		"../GameShared/src",
		"../RadianceEngine/src",
		"../TimbreAudio/src",
	}

	externalincludedirs
	{
		"dependencies/imgui",
		"dependencies",
		"../Dependencies",
		"../Dependencies/Steamworks/public",
	}

	links
	{
		"GameShared",
		"RadianceEngine",
		"TimbreAudio",
		"../Dependencies/Steamworks/redistributable_bin/win64/steam_api64",
	}

	postbuildcommands
	{
		'{COPYFILE} %[../Dependencies/GameNetworkingSockets/bin/Windows/*.dll] %[%{cfg.targetdir}]',
		'{COPYFILE} %[../Dependencies/Steamworks/redistributable_bin/win64/steam_api64.dll] %[%{cfg.targetdir}]',
		'{COPYFILE} %[steam_appid.txt] %[%{cfg.targetdir}]',
	}

	filter "system:windows"
		systemversion "latest"
		defines { "WINDOWS" }

	filter "configurations:Debug"
		targetname "GameDebug"
		defines { "DEBUG" }
		runtime "Debug"
		symbols "On"
		prebuildcommands { '%[../bin/%{cfg.system}-%{cfg.architecture}/UiAtlas/UiAtlasDebug.exe] %[%{cfg.targetdir}]', '%[../bin/%{cfg.system}-%{cfg.architecture}/ModelBaker/ModelBakerDebug.exe] %[assets] %[%{cfg.targetdir}]' }

	filter "configurations:Release"
		targetname "GameRelease"
		defines { "RELEASE" }
		runtime "Release"
		optimize "On"
		symbols "On"
		prebuildcommands { '%[../bin/%{cfg.system}-%{cfg.architecture}/UiAtlas/UiAtlasRelease.exe] %[%{cfg.targetdir}]', '%[../bin/%{cfg.system}-%{cfg.architecture}/ModelBaker/ModelBakerRelease.exe] %[assets] %[%{cfg.targetdir}]' }

	filter "configurations:Dist"
		targetname "Game"
		defines { "DIST" }
		runtime "Release"
		optimize "On"
		symbols "Off"
		prebuildcommands { '%[../bin/%{cfg.system}-%{cfg.architecture}/UiAtlas/UiAtlas.exe] %[%{cfg.targetdir}]', '%[../bin/%{cfg.system}-%{cfg.architecture}/ModelBaker/ModelBaker.exe] %[assets] %[%{cfg.targetdir}]' }

	filter {}
