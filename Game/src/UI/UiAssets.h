#pragma once
#include "UI/UiTypes.h"
#include "UI/VectorAtlas.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Content/Items.h>
#include <GameShared/Content/Upgrades.h>
#include <GameShared/World/Level.h>
#include <array>
#include <cstdint>
#include <string>

namespace Game
{
	class UiRenderer;

	enum class UiFont : uint8_t
	{
		Body,
		Bold,
		Title,
		Count,
	};

	enum class UiIcon : uint16_t
	{
		CrystalWand,
		WizardStaff,
		Broadsword,
		BattleAxe,
		PlainDagger,
		PocketBow,
		Crossbow,
		PointyHat,
		Hood,
		VisoredHelm,
		Robe,
		LeatherVest,
		Breastplate,
		Sandal,
		LeatherBoot,
		Greaves,
		GemPendant,
		Ring,
		CutDiamond,
		Emerald,
		Gems,
		Sparkles,
		ArcingBolt,
		FrozenOrb,
		SwordSlice,
		AxeSwing,
		CurvyKnife,
		HighShot,
		ArrowFlights,
		Fireball,
		MeteorImpact,
		Snowflake,
		IceSpear,
		LightningArc,
		Teleport,
		Whirlwind,
		EarthCrack,
		StrikingArrows,
		ArrowCluster,
		TargetArrows,
		Healing,
		Dodge,
		HeartPlus,
		Drop,
		Shield,
		SkullCrossedBones,
		Backpack,
		Door,
		Upgrade,
		Biceps,
		Hourglass,
		Sprint,
		Run,
		Padlock,
		Cog,
		CrossedSwords,
		ExitDoor,
		MagicPortal,
		Crown,
		CrossMark,
		CheckMark,
		AnvilImpact,
		Hand,
		MagicSwirl,
		Droplets,
		Waves,
		HeartDrop,
		Crosshair,
		SkullCrack,
		SplitArrows,
		ThornedArrow,
		Aura,
		LifeInTheBalance,
		Stopwatch,
		CheckedShield,
		Laurels,
		ScrollUnfurled,
		HeartOrgan,
		Coins,
		Shard,
		Ingot,
		Dust,
		VoidShard,
		Chest,
		Count,
	};

	enum class UiImage : uint8_t
	{
		GemRare,
		GemLegendary,
		GemMythic,
		Count,
	};

	struct UiImageRegion
	{
		uint32_t Texture = 0;
		glm::vec2 UvMin{ 0.0f };
		glm::vec2 UvMax{ 0.0f };
		bool Valid = false;
	};

	class UiAssets
	{
	public:
		bool Load(UiRenderer& renderer, std::string& error);

		const FontFace& GetFont(UiFont font) const { return m_Atlas.Fonts[size_t(font)]; }
		float GetCapHeight(UiFont font) const { return m_CapHeights[size_t(font)]; }
		const AtlasIcon& GetIcon(UiIcon icon) const { return m_Atlas.Icons[size_t(icon)]; }
		const UiImageRegion& GetImage(UiImage image) const { return m_Images[size_t(image)]; }
		uint32_t GetAtlasTexture() const { return m_AtlasTexture; }
		float GetDistanceRange() const { return m_Atlas.DistanceRange; }

	private:
		bool LoadImages(UiRenderer& renderer, std::string& error);

		VectorAtlas m_Atlas;
		uint32_t m_AtlasTexture = 0;
		std::array<float, size_t(UiFont::Count)> m_CapHeights{};
		std::array<UiImageRegion, size_t(UiImage::Count)> m_Images{};
	};

	UiIcon IconFor(ItemBase base);
	UiIcon IconFor(AbilityId ability);
	UiIcon IconFor(UpgradeId upgrade);
	UiIcon IconFor(LootKind loot);
	UiIcon IconFor(GearSlot slot);
	UiIcon IconFor(StarterKit kit);
	UiIcon IconFor(Material material);
	UiIcon IconFor(StationKind station);
	const UiImage* RarityGem(Rarity rarity);
}
