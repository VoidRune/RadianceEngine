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
	}

   	files
	{
		"src/**.h",
		"src/**.cpp",
		"dependencies/**.hpp",
		"dependencies/stb/**.h",
		"dependencies/stb/**.cpp",
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
		"dependencies/glm",
   	}

   	links
   	{
      		"RadianceEngine",
   	}


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