#pragma once
#include <cstdint>

namespace Game
{
	enum class GameMode : uint8_t
	{
		Hub,
		Brawl,
		Shardfall,
		Count,
	};

	inline bool IsPvpMode(GameMode mode)
	{
		return mode == GameMode::Brawl;
	}
}
