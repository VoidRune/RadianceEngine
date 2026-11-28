#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace Game
{
	enum class Emote : uint8_t
	{
		None,
		Wave,
		Salute,
		Laugh,
		Shrug,
		Victory,
		Sit,
		Count,
	};

	struct EmoteDef
	{
		const char* Name = "";
		bool Loop = false;
		bool Mobile = false;
		float Seconds = 0.0f;
	};

	inline constexpr std::array<EmoteDef, size_t(Emote::Count)> EmoteDefs = { {
		{},
		{ "Wave", false, true, 2.9f },
		{ "Salute", false, true, 2.83f },
		{ "Laugh", false, true, 5.0f },
		{ "Shrug", false, true, 2.0f },
		{ "Victory", false, true, 1.87f },
		{ "Sit", true },
	} };

	inline constexpr uint8_t EmoteFlag = 0x80;
	inline constexpr uint8_t EmoteIdMask = 0x1F;
	inline constexpr float EmoteDrift = 0.15f;

	inline uint8_t PackEmote(Emote emote, uint8_t sequence)
	{
		return emote == Emote::None || emote >= Emote::Count ? uint8_t(0) : uint8_t(EmoteFlag | ((sequence & 3u) << 5) | uint8_t(emote));
	}

	inline Emote UnpackEmote(uint8_t packed)
	{
		return (packed & EmoteFlag) != 0 ? Emote(std::min<uint8_t>(packed & EmoteIdMask, uint8_t(Emote::Count) - 1)) : Emote::None;
	}

	inline const EmoteDef& GetEmote(Emote emote)
	{
		return EmoteDefs[std::min(size_t(emote), EmoteDefs.size() - 1)];
	}

	inline bool EmoteHolds(Emote emote, float age, float moved)
	{
		const EmoteDef& def = GetEmote(emote);
		return emote != Emote::None && (def.Loop || age <= def.Seconds) && (def.Mobile || moved <= EmoteDrift);
	}
}
