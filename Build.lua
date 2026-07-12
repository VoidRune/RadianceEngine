workspace "RadianceEngine"
   	architecture "x64"
   	configurations { "Debug", "Release", "Dist" }
   	startproject "Application"

	multiprocessorcompile "On"

include "RadianceEngine/Build-Engine.lua"
include "Application/Build-App.lua"