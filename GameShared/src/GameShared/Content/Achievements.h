#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Content/Hub.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>

namespace Game
{
	enum class Achievement : uint8_t
	{
		FirstFling,
		HatTrick,
		BowlingNight,
		ReturnToSender,
		Hooked,
		SwapMeet,
		Pacifist,
		ByAThread,
		ComebackMage,
		DeckBuilder,
		Scholar,
		Skydiver,
		Stargazer,
		Faithful,
		Count,
	};

	using AchievementMask = uint32_t;

	struct AchievementDef
	{
		Achievement Id = Achievement::FirstFling;
		const char* Key = "";
		const char* Name = "";
		const char* Description = "";
		bool Hidden = false;
		const char* Stat = nullptr;
		uint32_t Goal = 0;
	};

	inline constexpr AchievementDef Achievements[] = {
		{ Achievement::FirstFling, "FIRST_FLING", "First Fling", "Knock a rival into the lava" },
		{ Achievement::HatTrick, "HAT_TRICK", "Hat Trick", "Knock out three rivals in one round" },
		{ Achievement::BowlingNight, "BOWLING_NIGHT", "Bowling Night", "Fling three rivals with one spell" },
		{ Achievement::ReturnToSender, "RETURN_TO_SENDER", "Return to Sender", "Knock out a rival with their own reflected spell" },
		{ Achievement::Hooked, "HOOKED", "Hooked", "Pull a rival into the lava with Chain Hook" },
		{ Achievement::SwapMeet, "SWAP_MEET", "Swap Meet", "Swap a rival into the lava you were standing in" },
		{ Achievement::Pacifist, "PACIFIST", "Pacifist", "Win a round without casting a spell" },
		{ Achievement::ByAThread, "BY_A_THREAD", "By a Thread", "Win a round with less than 10 health" },
		{ Achievement::ComebackMage, "COMEBACK_MAGE", "Comeback Mage", "Win a match after losing the first two rounds" },
		{ Achievement::DeckBuilder, "DECK_BUILDER", "Deck Builder", "Stack two upgrades on one spell" },
		{ Achievement::Scholar, "SCHOLAR", "Scholar", "Win a round with every spell", false, "scholar_spells", uint32_t(std::size(BrawlSpellPool)) },
		{ Achievement::Skydiver, "SKYDIVER", "Skydiver", "Fall off Emberhold 25 times", false, "falls", SkydiverFalls },
		{ Achievement::Stargazer, "STARGAZER", "Stargazer", "Light every constellation above the observatory" },
		{ Achievement::Faithful, "FAITHFUL", "Faithful", "Reach the cat's islet", true },
	};
	static_assert(std::size(Achievements) == size_t(Achievement::Count));

	inline constexpr Achievement HonorAchievements[] = { Achievement::Skydiver, Achievement::Stargazer, Achievement::Faithful };
	static_assert(std::size(HonorAchievements) == size_t(Honor::Count));

	inline constexpr uint8_t RoundsForComeback = 3;
	inline constexpr int32_t ThreadHealth = 10;
	inline constexpr uint32_t HatTrickKnockouts = 3;
	inline constexpr uint32_t BowlingFlings = 3;

	inline const AchievementDef& GetAchievement(Achievement id)
	{
		return Achievements[size_t(id) < std::size(Achievements) ? size_t(id) : 0];
	}

	inline constexpr AchievementMask AchievementBit(Achievement id) { return AchievementMask(1) << uint32_t(id); }
	inline constexpr AchievementMask AllAchievements = (AchievementMask(1) << uint32_t(Achievement::Count)) - 1u;
	inline constexpr uint32_t FullSpellPool = (1u << uint32_t(std::size(BrawlSpellPool))) - 1u;

	inline AchievementMask AchievementsFromHonors(uint8_t honors)
	{
		AchievementMask mask = 0;
		for (size_t i = 0; i < std::size(HonorAchievements); i++)
			if ((honors >> i) & 1u)
				mask |= AchievementBit(HonorAchievements[i]);
		return mask;
	}

	inline uint32_t SpellPoolBits(const BrawlSpells& spells)
	{
		uint32_t bits = 0;
		for (size_t i = 0; i < std::size(BrawlSpellPool); i++)
			for (const AbilityId spell : spells)
				if (spell == BrawlSpellPool[i])
					bits |= 1u << uint32_t(i);
		return bits;
	}

	inline uint32_t ProgressOf(Achievement id, uint32_t falls, uint32_t scholar)
	{
		return id == Achievement::Skydiver ? std::min(falls, SkydiverFalls) : id == Achievement::Scholar ? uint32_t(std::popcount(scholar & FullSpellPool)) : 0u;
	}
}
