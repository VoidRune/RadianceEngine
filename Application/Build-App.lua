project "Application"
   	kind "ConsoleApp"
	--kind "WindowedApp"
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
	}

   	files
	{
		"src/**.h",
		"src/**.cpp",
		"dependencies/**.hpp",
		"../Dependencies/stb/stb_image.h",
		"../Dependencies/stb/stb_image.cpp",
		"../Dependencies/stb/stb_image_write.h",
		"../Dependencies/stb/stb_image_write.cpp",
        "dependencies/tiny_obj_loader/**.h",
		"dependencies/tiny_obj_loader/**.cpp",
    	--"dependencies/tinygltf/**.h",
        --"dependencies/tinygltf/**.c",
	}

   	includedirs
   	{
		"src",
		"dependencies",


		"../RadianceEngine/src",
		"../TimbreAudio/src",
		"../Dependencies",
   	}

   	links
   	{
      		"RadianceEngine",
      		"TimbreAudio",
   	}


	filter "files:../Dependencies/**.cpp"
		warnings "Off"

   	filter "system:windows"
       		systemversion "latest"
       		defines { "WINDOWS" }

   	filter "configurations:Debug"
		targetname "ApplicationDebug"
       		defines { "DEBUG" }
       		runtime "Debug"
       		symbols "On"

   	filter "configurations:Release"
		targetname "ApplicationRelease"
       		defines { "RELEASE" }
       		runtime "Release"
       		optimize "On"
       		symbols "On"

   	filter "configurations:Dist"
		targetname "Application"
       		defines { "DIST" }
       		runtime "Release"
       		optimize "On"
       		symbols "Off"