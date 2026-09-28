project "TimbreAudio"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	conformancemode "On"
	warnings "Extra"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
		"src/**.c",
		"dependencies/**.h",
	}

	includedirs
	{
		"src",
	}

	externalincludedirs
	{
		"dependencies",
	}
	externalwarnings "Off"

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
	}

	-- The miniaudio and libvorbis implementations are third party code.
	filter "files:**.c"
		warnings "Off"

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		targetname "TimbreAudioDebug"
		defines { "DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		targetname "TimbreAudioRelease"
		defines { "RELEASE" }
		runtime "Release"
		optimize "On"
		symbols "On"

	filter "configurations:Dist"
		targetname "TimbreAudio"
		defines { "DIST" }
		runtime "Release"
		optimize "On"
		symbols "Off"

	filter {}
