#pragma once
#include <cstdint>
#include <span>

namespace Game
{
	inline constexpr const char* ModelLibraryPath = "res/Models/Models.bin";
	inline constexpr uint32_t ModelTextureMaxSize = 256;
	inline constexpr float ModelClipFrameRate = 30.0f;

	enum class ModelSourceKind : uint8_t
	{
		Character,
		Animations,
		Prop,
		PropFolder,
		Scan,
		Material,
		Environment,
	};

	struct ModelSource
	{
		const char* Path;
		ModelSourceKind Kind;
		uint32_t TextureSize = 0;
		float Size = 0.0f;
		bool SplitNodes = false;
		bool KeepOrigin = false;
	};

	struct ModelPack
	{
		const char* Path;
		std::span<const ModelSource> Sources;
		bool Required = true;
	};

	inline constexpr ModelSource CharacterSources[] = {
		{ "KayKit/Animations/Rig_Medium_General.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_MovementBasic.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_MovementAdvanced.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_CombatMelee.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_CombatRanged.glb", ModelSourceKind::Animations },
		{ "KayKit/Animations/Rig_Medium_Special.glb", ModelSourceKind::Animations },
		{ "KayKit/Adventurers/Knight.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Mage.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Ranger.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Rogue.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Rogue_Hooded.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/Barbarian.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Minion.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Rogue.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Warrior.glb", ModelSourceKind::Character },
		{ "KayKit/Skeletons/Skeleton_Mage.glb", ModelSourceKind::Character },
		{ "KayKit/Adventurers/wand.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/staff.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/sword_1handed.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/axe_1handed.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/dagger.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/bow_withString.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/crossbow_2handed.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/spellbook_open.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/arrow_bow.gltf", ModelSourceKind::Prop },
		{ "KayKit/Adventurers/arrow_crossbow.gltf", ModelSourceKind::Prop },
		{ "KayKit/FantasyWeapons/spear_A.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Axe.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Blade.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Crossbow.gltf", ModelSourceKind::Prop },
		{ "KayKit/Skeletons/Skeleton_Staff.gltf", ModelSourceKind::Prop },
		{ "KayKit/Forest", ModelSourceKind::PropFolder },
		{ "KayKit/Dungeon", ModelSourceKind::PropFolder },
		{ "Reference/LightingTest.gltf", ModelSourceKind::Prop },
	};

	inline constexpr ModelSource OriginalSources[] = {
		{ .Path = "Original/ObsidianCluster", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Fountain", .Kind = ModelSourceKind::Scan, .TextureSize = 1024 },
		{ .Path = "Original/Critters", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/HubProps", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Stations", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Arcana", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Nature", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Emberhold", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Market", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Chess", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true },
		{ .Path = "Original/Hero", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Armory", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Glacier", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Ruins", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Summit", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true },
		{ .Path = "Original/Citadel", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Crawler", .Kind = ModelSourceKind::Scan, .TextureSize = 512, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Abyss", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Leviathan", .Kind = ModelSourceKind::Scan, .TextureSize = 2048, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Harbor", .Kind = ModelSourceKind::Scan, .TextureSize = 1024, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Abyssal", .Kind = ModelSourceKind::Scan, .TextureSize = 2048, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Forge", .Kind = ModelSourceKind::Scan, .TextureSize = 2048, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/ForgeBackdrop", .Kind = ModelSourceKind::Scan, .TextureSize = 2048, .SplitNodes = true, .KeepOrigin = true },
		{ .Path = "Original/Materials/PavingStone", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.0f },
		{ .Path = "Original/Materials/PavingBed", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.0f },
		{ .Path = "Original/Materials/GroundGrass", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundDirt", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundRock", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/GroundSand", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundSnow", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundWater", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundCliff", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/GroundFrostRock", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/GroundBasalt", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/GroundScree", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/GroundAlpine", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/CitadelMoss", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 1.8f },
		{ .Path = "Original/Materials/ObservatoryFloor", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 4.4f },
		{ .Path = "Original/Materials/ObservatoryBlock", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 3.0f },
		{ .Path = "Original/Materials/ObservatoryAshlar", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.2f },
		{ .Path = "Original/Materials/ObservatoryBlue", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.0f },
		{ .Path = "Original/Materials/ObservatoryGold", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.0f },
		{ .Path = "Original/Materials/ObservatoryBronze", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
		{ .Path = "Original/Materials/ForgeJointSilver", .Kind = ModelSourceKind::Material, .TextureSize = 512, .Size = 1.5f },
		{ .Path = "Original/Materials/ObservatoryMural", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 4.2f },
		{ .Path = "PolyHaven/Textures/castle_wall_varriation", .Kind = ModelSourceKind::Material, .TextureSize = 1024, .Size = 2.0f },
	};

	inline constexpr ModelPack ModelPacks[] = {
		{ ModelLibraryPath, CharacterSources },
		{ "res/Models/Original.bin", OriginalSources },
	};
}
