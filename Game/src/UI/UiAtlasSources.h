#pragma once
#include "UI/VectorAtlas.h"
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace Game
{
	inline constexpr const char* UiAtlasPath = "res/UI/UiAtlas.bin";
	inline constexpr float UiGlyphSize = 42.0f;
	inline constexpr float UiIconSize = 72.0f;
	inline constexpr float UiDistanceRange = 12.0f;

	inline constexpr std::pair<uint32_t, uint32_t> UiTextRanges[] = {
		{ 0x20, 0x7E }, { 0xA0, 0xFF }, { 0x2013, 0x2014 }, { 0x2018, 0x201D }, { 0x2022, 0x2022 }, { 0x2026, 0x2026 }, { 0x2190, 0x2193 }, { 0x221E, 0x221E },
	};

	inline constexpr std::pair<uint32_t, uint32_t> UiTitleRanges[] = { { 0x20, 0x7E } };

	struct UiFontFile
	{
		const char* Path;
		std::span<const std::pair<uint32_t, uint32_t>> Ranges;
	};

	inline constexpr UiFontFile UiFontFiles[] = {
		{ "res/Fonts/AlegreyaSans-Medium.ttf", UiTextRanges },
		{ "res/Fonts/AlegreyaSans-Bold.ttf", UiTextRanges },
		{ "res/Fonts/Cinzel.ttf", UiTitleRanges },
	};

	inline constexpr const char* UiIconFiles[] = {
		"crystal-wand", "wizard-staff", "broadsword", "battle-axe", "plain-dagger", "pocket-bow", "crossbow",
		"pointy-hat", "hood", "visored-helm", "robe", "leather-vest", "breastplate", "sandal", "leather-boot", "greaves",
		"gem-pendant", "ring", "cut-diamond", "emerald", "gems", "sparkles",
		"arcing-bolt", "frozen-orb", "sword-slice", "axe-swing", "curvy-knife", "high-shot", "arrow-flights",
		"fireball", "meteor-impact", "snowflake-2", "ice-spear", "lightning-arc", "teleport", "whirlwind", "earth-crack",
		"striking-arrows", "arrow-cluster", "target-arrows", "healing", "dodge",
		"heart-plus", "drop", "shield", "skull-crossed-bones", "backpack", "door", "upgrade", "biceps", "hourglass",
		"sprint", "run", "padlock", "cog", "crossed-swords", "exit-door", "magic-portal", "crown", "cross-mark", "check-mark",
		"anvil-impact", "hand", "magic-swirl", "droplets", "waves", "heart-drop", "crosshair", "skull-crack", "split-arrows",
		"thorned-arrow", "aura", "life-in-the-balance", "stopwatch", "checked-shield", "laurels", "scroll-unfurled", "heart-organ",
		"coins", "shard", "ingot", "dust", "void-shard", "chest", "spear-hook", "fencer",
		"chess-king", "chess-queen", "chess-rook", "chess-bishop", "chess-knight", "chess-pawn",
		"potion-ball", "cycle", "vortex", "fizzing-flask", "bleeding-heart", "broken-shield", "heartburn", "burning-embers",
		"fire-ring", "frozen-block", "ice-bolt", "lightning-helix", "power-lightning", "energy-shield", "heart-bottle", "on-target",
		"star-swirl", "sunbeams", "psychic-waves", "dripping-blade", "echo-ripples", "unstable-orb", "meditation", "eye",
	};

	inline VectorAtlasDesc MakeUiAtlasDesc()
	{
		VectorAtlasDesc desc;
		for (const UiFontFile& font : UiFontFiles)
			desc.Fonts.push_back({ font.Path, { font.Ranges.begin(), font.Ranges.end() } });
		for (const char* icon : UiIconFiles)
			desc.Icons.push_back(std::string("res/Icons/") + icon + ".svg");
		desc.GlyphSize = UiGlyphSize;
		desc.IconSize = UiIconSize;
		desc.DistanceRange = UiDistanceRange;
		return desc;
	}
}
