#include "UiAssets.h"
#include "UI/UiAtlasSources.h"
#include "UI/UiRenderer.h"
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Core/Timer.h>
#include <algorithm>
#include <cmath>

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb/stb_image.h>
#pragma warning(pop)

namespace Game
{
	namespace
	{
		constexpr uint32_t ImagePadding = 8;
		constexpr uint32_t ImageAtlasWidth = 512;
		constexpr uint32_t ImageMipLevels = 4;

		static_assert(std::size(UiIconFiles) == size_t(UiIcon::Count));
		static_assert(std::size(UiFontFiles) == size_t(UiFont::Count));

		constexpr const char* ImageFiles[] = { "GemRare", "GemLegendary", "GemMythic" };
		static_assert(std::size(ImageFiles) == size_t(UiImage::Count));

		struct LoadedImage
		{
			int Width = 0;
			int Height = 0;
			std::vector<uint8_t> Pixels;
			uint32_t X = 0;
			uint32_t Y = 0;
		};
	}

	bool UiAssets::Load(UiRenderer& renderer, std::string& error)
	{
		Rdn::Timer timer;
		const VectorAtlasDesc desc = MakeUiAtlasDesc();
		if (!LoadVectorAtlas(UiAtlasPath, m_Atlas))
		{
			error = std::string("Can't read ") + UiAtlasPath + ", building the UiAtlas project bakes it";
			return false;
		}
		if (m_Atlas.LayoutHash != HashAtlasLayout(desc) || m_Atlas.Fonts.size() != std::size(UiFontFiles) || m_Atlas.Icons.size() != std::size(UiIconFiles))
		{
			error = std::string(UiAtlasPath) + " was baked for other fonts or icons, building the UiAtlas project rebakes it";
			return false;
		}
#if !defined(DIST)
		if (m_Atlas.SourceHash != HashAtlasSources(desc))
			RDN_LOG_WARNING("The UI fonts or icons changed after {} was baked, building the UiAtlas project rebakes it", UiAtlasPath);
#endif
		for (size_t i = 0; i < m_Atlas.Icons.size(); i++)
			if (!m_Atlas.Icons[i].Valid)
				RDN_LOG_WARNING("UI icon {} failed to load", UiIconFiles[i]);

		const float padding = (UiDistanceRange * 0.5f + 1.0f) / UiGlyphSize;
		for (size_t font = 0; font < m_Atlas.Fonts.size(); font++)
		{
			const AtlasGlyph* capital = m_Atlas.Fonts[font].Find('H');
			m_CapHeights[font] = capital && capital->Visible ? -capital->PlaneMin.y - padding : 0.7f;
		}

		const std::optional<uint32_t> texture = renderer.CreateTexture(m_Atlas.Width, m_Atlas.Height, m_Atlas.Pixels.data());
		if (!texture)
		{
			error = "No texture slot left for the UI atlas";
			return false;
		}
		m_AtlasTexture = *texture;
		renderer.SetDistanceRange(m_Atlas.DistanceRange);
		m_Atlas.Pixels.clear();
		m_Atlas.Pixels.shrink_to_fit();
		if (!LoadImages(renderer, error))
			return false;
		RDN_LOG("UI assets ready in {:.0f} ms (atlas {}x{})", timer.ElapsedMilliseconds(), m_Atlas.Width, m_Atlas.Height);
		return true;
	}

	bool UiAssets::LoadImages(UiRenderer& renderer, std::string& error)
	{
		std::vector<LoadedImage> images(m_Images.size());
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t shelf = 0;
		for (size_t i = 0; i < images.size(); i++)
		{
			const std::string path = std::string("res/Textures/UI/") + ImageFiles[i] + ".png";
			int channels = 0;
			stbi_uc* pixels = stbi_load(path.c_str(), &images[i].Width, &images[i].Height, &channels, 4);
			if (!pixels)
			{
				RDN_LOG_WARNING("UI image {} failed to load: {}", path, stbi_failure_reason());
				continue;
			}
			LoadedImage& image = images[i];
			image.Pixels.assign(pixels, pixels + size_t(image.Width) * image.Height * 4);
			stbi_image_free(pixels);
			for (size_t p = 0; p < image.Pixels.size(); p += 4)
				for (size_t c = 0; c < 3; c++)
					image.Pixels[p + c] = uint8_t((uint32_t(image.Pixels[p + c]) * image.Pixels[p + 3] + 127) / 255);
			const uint32_t width = (uint32_t(image.Width) + 2 * ImagePadding + 7) / 8 * 8;
			const uint32_t height = (uint32_t(image.Height) + 2 * ImagePadding + 7) / 8 * 8;
			if (x + width > ImageAtlasWidth)
			{
				x = 0;
				y += shelf;
				shelf = 0;
			}
			image.X = x + ImagePadding;
			image.Y = y + ImagePadding;
			x += width;
			shelf = std::max(shelf, height);
		}
		const uint32_t atlasHeight = std::max((y + shelf + 7) / 8 * 8, 8u);
		std::vector<uint8_t> atlas(size_t(ImageAtlasWidth) * atlasHeight * 4, 0);
		for (const LoadedImage& image : images)
			for (int row = 0; row < image.Height; row++)
				std::copy_n(image.Pixels.begin() + ptrdiff_t(row) * image.Width * 4, size_t(image.Width) * 4, atlas.begin() + (ptrdiff_t(image.Y + uint32_t(row)) * ImageAtlasWidth + image.X) * 4);
		const std::optional<uint32_t> texture = renderer.CreateTexture(ImageAtlasWidth, atlasHeight, atlas.data(), ImageMipLevels);
		if (!texture)
		{
			error = "No texture slot left for the UI images";
			return false;
		}
		const glm::vec2 size{ float(ImageAtlasWidth), float(atlasHeight) };
		for (size_t i = 0; i < images.size(); i++)
		{
			if (images[i].Pixels.empty())
				continue;
			m_Images[i] = { *texture, glm::vec2(float(images[i].X), float(images[i].Y)) / size, glm::vec2(float(images[i].X + uint32_t(images[i].Width)), float(images[i].Y + uint32_t(images[i].Height))) / size, true };
		}
		return true;
	}

	UiIcon IconFor(ItemBase base)
	{
		switch (base)
		{
		case ItemBase::Wand: return UiIcon::CrystalWand;
		case ItemBase::Staff: return UiIcon::WizardStaff;
		case ItemBase::Sword: return UiIcon::Broadsword;
		case ItemBase::Axe: return UiIcon::BattleAxe;
		case ItemBase::Dagger: return UiIcon::PlainDagger;
		case ItemBase::Spear: return UiIcon::SpearHook;
		case ItemBase::Bow: return UiIcon::PocketBow;
		case ItemBase::Crossbow: return UiIcon::Crossbow;
		case ItemBase::WizardHat: return UiIcon::PointyHat;
		case ItemBase::Hood: return UiIcon::Hood;
		case ItemBase::Helm: return UiIcon::VisoredHelm;
		case ItemBase::Robe: return UiIcon::Robe;
		case ItemBase::Tunic: return UiIcon::LeatherVest;
		case ItemBase::Plate: return UiIcon::Breastplate;
		case ItemBase::Sandals: return UiIcon::Sandal;
		case ItemBase::Boots: return UiIcon::LeatherBoot;
		case ItemBase::Greaves: return UiIcon::Greaves;
		case ItemBase::Amulet: return UiIcon::GemPendant;
		case ItemBase::Ring: return UiIcon::Ring;
		default: return UiIcon::Emerald;
		}
	}

	UiIcon IconFor(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Bolt: return UiIcon::ArcingBolt;
		case AbilityId::Orb: return UiIcon::FrozenOrb;
		case AbilityId::Slash: return UiIcon::SwordSlice;
		case AbilityId::Chop: return UiIcon::AxeSwing;
		case AbilityId::Stab: return UiIcon::CurvyKnife;
		case AbilityId::Thrust: return UiIcon::Fencer;
		case AbilityId::Arrow: return UiIcon::HighShot;
		case AbilityId::Quarrel: return UiIcon::ArrowFlights;
		case AbilityId::Fireball: return UiIcon::Fireball;
		case AbilityId::Meteor: return UiIcon::MeteorImpact;
		case AbilityId::FrostNova: return UiIcon::Snowflake;
		case AbilityId::IceSpear: return UiIcon::IceSpear;
		case AbilityId::ChainLightning: return UiIcon::LightningArc;
		case AbilityId::Blink: return UiIcon::Teleport;
		case AbilityId::Cyclone: return UiIcon::Whirlwind;
		case AbilityId::GroundSlam: return UiIcon::EarthCrack;
		case AbilityId::Volley: return UiIcon::StrikingArrows;
		case AbilityId::RainOfArrows: return UiIcon::ArrowCluster;
		case AbilityId::PiercingShot: return UiIcon::TargetArrows;
		case AbilityId::Rejuvenate: return UiIcon::Healing;
		case AbilityId::Roll: return UiIcon::Dodge;
		case AbilityId::EmberBolt: return UiIcon::Fireball;
		case AbilityId::Gust: return UiIcon::Whirlwind;
		case AbilityId::Comet: return UiIcon::MeteorImpact;
		case AbilityId::Warp: return UiIcon::Teleport;
		case AbilityId::Shockwave: return UiIcon::Aura;
		case AbilityId::GravityWell: return UiIcon::MagicSwirl;
		case AbilityId::ArcLightning: return UiIcon::LightningArc;
		case AbilityId::Swap: return UiIcon::MagicPortal;
		case AbilityId::MirrorWard: return UiIcon::CheckedShield;
		case AbilityId::Frostbite: return UiIcon::IceSpear;
		case AbilityId::ChainHook: return UiIcon::SpearHook;
		case AbilityId::Firebomb: return UiIcon::HighShot;
		case AbilityId::SplittingBolt: return UiIcon::SplitArrows;
		case AbilityId::StormStrike: return UiIcon::ArcingBolt;
		case AbilityId::Ignite: return UiIcon::Fireball;
		case AbilityId::ShatterBurst: return UiIcon::Snowflake;
		default: return UiIcon::Gems;
		}
	}

	UiIcon IconFor(CardId card)
	{
		switch (card)
		{
		case CardId::SplittingEmber: return UiIcon::SplitArrows;
		case CardId::LingeringGale: return UiIcon::Whirlwind;
		case CardId::DeflectingGale: return UiIcon::Shield;
		case CardId::ShatteringImpact: return UiIcon::EarthCrack;
		case CardId::TwinComets: return UiIcon::MeteorImpact;
		case CardId::Afterimage: return UiIcon::Teleport;
		case CardId::Aftershock: return UiIcon::Aura;
		case CardId::Implosion: return UiIcon::MagicSwirl;
		case CardId::Pinball: return UiIcon::ArcingBolt;
		case CardId::Overcharge: return UiIcon::LightningArc;
		case CardId::HeartSwap: return UiIcon::HeartDrop;
		case CardId::ThornedMirror: return UiIcon::ThornedArrow;
		case CardId::BlackIce: return UiIcon::Snowflake;
		case CardId::GlassCannon: return UiIcon::SkullCrack;
		case CardId::Featherfall: return UiIcon::HighShot;
		case CardId::Echo: return UiIcon::Dodge;
		case CardId::HeavyRobes: return UiIcon::Robe;
		case CardId::LongChain: return UiIcon::SpearHook;
		case CardId::Whiplash: return UiIcon::Sprint;
		case CardId::ClusterBomb: return UiIcon::ArrowCluster;
		case CardId::StickyFire: return UiIcon::Fireball;
		case CardId::Fivefold: return UiIcon::StrikingArrows;
		case CardId::SeekerShards: return UiIcon::Crosshair;
		case CardId::ForkedLightning: return UiIcon::LightningArc;
		case CardId::StaticCharge: return UiIcon::Padlock;
		case CardId::DeepWell: return UiIcon::PotionBall;
		case CardId::Quickening: return UiIcon::Cycle;
		case CardId::Siphon: return UiIcon::Vortex;
		case CardId::Brimming: return UiIcon::FizzingFlask;
		case CardId::BloodPact: return UiIcon::BleedingHeart;
		case CardId::LastStand: return UiIcon::BrokenShield;
		case CardId::VampiricEmbers: return UiIcon::Heartburn;
		case CardId::Kindling: return UiIcon::BurningEmbers;
		case CardId::Wildfire: return UiIcon::FireRing;
		case CardId::Brittle: return UiIcon::FrozenBlock;
		case CardId::Shatter: return UiIcon::IceBolt;
		case CardId::Conductive: return UiIcon::LightningHelix;
		case CardId::Overload: return UiIcon::PowerLightning;
		case CardId::Barrier: return UiIcon::EnergyShield;
		case CardId::SecondWind: return UiIcon::HeartBottle;
		case CardId::Precise: return UiIcon::OnTarget;
		case CardId::ApexOfMagic: return UiIcon::StarSwirl;
		case CardId::ManaSingularity: return UiIcon::Sunbeams;
		case CardId::MindOverMatter: return UiIcon::PsychicWaves;
		case CardId::BloodMagic: return UiIcon::DrippingBlade;
		case CardId::EchoChamber: return UiIcon::EchoRipples;
		case CardId::UnstableArcana: return UiIcon::UnstableOrb;
		case CardId::Leyline: return UiIcon::Meditation;
		default: return UiIcon::Upgrade;
		}
	}

	UiIcon IconFor(UpgradeId upgrade)
	{
		switch (upgrade)
		{
		case UpgradeId::Sharpened: return UiIcon::AnvilImpact;
		case UpgradeId::QuickHands: return UiIcon::Hand;
		case UpgradeId::Incantation: return UiIcon::MagicSwirl;
		case UpgradeId::FleetFoot: return UiIcon::Run;
		case UpgradeId::IronSkin: return UiIcon::HeartPlus;
		case UpgradeId::DeepWell: return UiIcon::Droplets;
		case UpgradeId::Flow: return UiIcon::Waves;
		case UpgradeId::Vampirism: return UiIcon::HeartDrop;
		case UpgradeId::Precision: return UiIcon::Crosshair;
		case UpgradeId::Brutality: return UiIcon::SkullCrack;
		case UpgradeId::SplitShot: return UiIcon::SplitArrows;
		case UpgradeId::Penetration: return UiIcon::ThornedArrow;
		case UpgradeId::Expanse: return UiIcon::Aura;
		case UpgradeId::SecondWind: return UiIcon::LifeInTheBalance;
		case UpgradeId::Focus: return UiIcon::Stopwatch;
		case UpgradeId::Bulwark: return UiIcon::CheckedShield;
		default: return UiIcon::Upgrade;
		}
	}

	UiIcon IconFor(LootKind loot)
	{
		switch (loot)
		{
		case LootKind::Health: return UiIcon::HeartOrgan;
		case LootKind::Might: return UiIcon::Biceps;
		case LootKind::Haste: return UiIcon::Hourglass;
		case LootKind::Swiftness: return UiIcon::Sprint;
		case LootKind::Vitality: return UiIcon::HeartPlus;
		case LootKind::Mana: return UiIcon::Drop;
		default: return UiIcon::Gems;
		}
	}

	UiIcon IconFor(GearSlot slot)
	{
		switch (slot)
		{
		case GearSlot::Weapon: return UiIcon::Broadsword;
		case GearSlot::Head: return UiIcon::VisoredHelm;
		case GearSlot::Body: return UiIcon::Breastplate;
		case GearSlot::Feet: return UiIcon::LeatherBoot;
		case GearSlot::Amulet: return UiIcon::GemPendant;
		default: return UiIcon::Ring;
		}
	}

	UiIcon IconFor(StarterKit kit)
	{
		switch (kit)
		{
		case StarterKit::Knight: return UiIcon::VisoredHelm;
		case StarterKit::Ranger: return UiIcon::PocketBow;
		case StarterKit::Lancer: return UiIcon::SpearHook;
		default: return UiIcon::PointyHat;
		}
	}

	UiIcon IconFor(Material material)
	{
		switch (material)
		{
		case Material::Shard: return UiIcon::Shard;
		case Material::Starsteel: return UiIcon::Ingot;
		case Material::ArcaneDust: return UiIcon::Dust;
		case Material::VoidShard: return UiIcon::VoidShard;
		default: return UiIcon::Coins;
		}
	}

	UiIcon IconFor(StationKind station)
	{
		switch (station)
		{
		case StationKind::Forge: return UiIcon::AnvilImpact;
		case StationKind::Lapidary: return UiIcon::Gems;
		case StationKind::MissionBoard: return UiIcon::ScrollUnfurled;
		case StationKind::ProvingGrounds: return UiIcon::CrossedSwords;
		case StationKind::WishingVoid: return UiIcon::Coins;
		case StationKind::Chess: return UiIcon::ChessKnight;
		default: return UiIcon::Chest;
		}
	}

	UiIcon IconFor(Headwear headwear)
	{
		switch (headwear)
		{
		case Headwear::Hood: return UiIcon::Hood;
		case Headwear::Bare: return UiIcon::Aura;
		default: return UiIcon::PointyHat;
		}
	}

	const UiImage* RarityGem(Rarity rarity)
	{
		static constexpr UiImage Gems[] = { UiImage::GemRare, UiImage::GemLegendary, UiImage::GemMythic };
		if (rarity < Rarity::Rare || rarity >= Rarity::Count)
			return nullptr;
		return &Gems[size_t(rarity) - size_t(Rarity::Rare)];
	}
}
