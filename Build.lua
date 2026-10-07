workspace "RadianceEngine"
   	architecture "x64"
   	configurations { "Debug", "Release", "Dist" }
   	startproject "Application"

	multiprocessorcompile "On"

include "RadianceEngine/Build-Engine.lua"
include "TimbreAudio/Build-TimbreAudio.lua"
include "Application/Build-App.lua"
include "GameShared/Build-GameShared.lua"
include "Game/Build-Game.lua"