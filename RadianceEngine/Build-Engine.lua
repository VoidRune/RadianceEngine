project "RadianceEngine"
   	kind "StaticLib"
   	language "C++"
   	cppdialect "C++20"
   	staticruntime "off"

	targetdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	objdir ("../build/%{cfg.system}-%{cfg.architecture}/%{prj.name}")
	debugdir ("../bin/%{cfg.system}-%{cfg.architecture}/%{prj.name}")

   	files 
	{ 
		"src/**.h", 
		"src/**.cpp",
		"dependencies/volk/**.h",
		"dependencies/volk/**.c",
		--"dependencies/imgui-master/**.h",
		--"dependencies/imgui-master/**.cpp",
		"dependencies/VulkanMemoryAllocator/**.h",
		"dependencies/VulkanMemoryAllocator/**.cpp",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

   	includedirs
   	{
		"src",
		"$(VULKAN_SDK)/Include",
		"dependencies/glfw/include",
		"dependencies/VulkanMemoryAllocator",
		"dependencies",
		--"dependencies/GameNetworkingSockets/include",
   	}

	links
	{
		--"$(VULKAN_SDK)/Lib/vulkan-1.lib",
		"dependencies/glfw/lib-vc2022/glfw3",
	}

	filter { "system:windows", "configurations:Debug" }	
		links
		{
			"$(VULKAN_SDK)/Lib/shaderc_combinedd.lib",
			"$(VULKAN_SDK)/Lib/spirv-cross-cored.lib",
			--"dependencies/GameNetworkingSockets/bin/Windows/Debug/GameNetworkingSockets.lib"
		}

  	filter { "system:windows", "configurations:Release or configurations:Dist" }	
		links
		{
			"$(VULKAN_SDK)/Lib/shaderc_combined.lib",
			"$(VULKAN_SDK)/Lib/spirv-cross-core.lib",
			--"dependencies/GameNetworkingSockets/bin/Windows/Release/GameNetworkingSockets.lib"
		}

   	filter "system:windows"
       		systemversion "latest"
       		defines { }

   	filter "configurations:Debug"
		targetname "RadianceEngineDebug"
       		defines { "DEBUG" }
       		runtime "Debug"
       		symbols "On"

   	filter "configurations:Release"
		targetname "RadianceEngineRelease"
       		defines { "RELEASE" }
       		runtime "Release"
       		optimize "On"
       		symbols "On"

   	filter "configurations:Dist"
		targetname "RadianceEngine"
       		defines { "DIST" }
       		runtime "Release"
       		optimize "On"
       		symbols "Off"