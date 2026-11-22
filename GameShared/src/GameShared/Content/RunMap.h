#pragma once
#include "GameShared/Core/Random.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace Game
{
	inline constexpr uint8_t MapLayers = 7;
	inline constexpr uint8_t MapColumns = 3;
	inline constexpr uint8_t MapRooms = MapLayers * MapColumns;
	inline constexpr uint8_t GuardianLayer = MapLayers - 1;
	inline constexpr uint8_t NoRoom = 0xFF;
	inline constexpr uint8_t DoorSlots = 3;
	inline constexpr uint8_t AllDoors = 0b111;
	inline constexpr uint8_t FirstEliteLayer = 2;
	inline constexpr uint8_t FirstRestLayer = 3;
	inline constexpr uint8_t FirstShopLayer = 2;
	inline constexpr uint8_t LastShopLayer = 4;

	enum class RoomKind : uint8_t
	{
		None,
		Fight,
		Elite,
		Rest,
		Shop,
		Shrine,
		Guardian,
		Count,
	};

	enum class RoomReward : uint8_t
	{
		None,
		Spell,
		Rank,
		Card,
		Cache,
		Treasure,
		Rune,
		Count,
	};

	enum class RoomMedal : uint8_t
	{
		None,
		Bronze,
		Silver,
		Gold,
		Count,
	};

	inline constexpr bool IsCalmRoom(RoomKind kind)
	{
		return kind == RoomKind::Rest || kind == RoomKind::Shop || kind == RoomKind::Shrine;
	}

	inline constexpr const char* RoomKindNames[] = { "", "Fight", "Champion", "Rest", "Shop", "Shrine", "Guardian" };
	inline constexpr const char* RoomRewardNames[] = { "", "New spell", "Rank up", "Card", "Shard cache", "Champion's chest", "Rune" };
	inline constexpr const char* RoomMedalNames[] = { "", "Bronze", "Silver", "Gold" };
	static_assert(std::size(RoomKindNames) == size_t(RoomKind::Count));
	static_assert(std::size(RoomRewardNames) == size_t(RoomReward::Count));
	static_assert(std::size(RoomMedalNames) == size_t(RoomMedal::Count));

	struct MapRoom
	{
		RoomKind Kind = RoomKind::None;
		RoomReward Reward = RoomReward::None;
		uint8_t Next = 0;

		bool Exists() const { return Kind != RoomKind::None; }
		bool operator==(const MapRoom&) const = default;
	};

	inline constexpr uint8_t RoomIndex(uint8_t layer, uint8_t column) { return uint8_t(layer * MapColumns + column); }
	inline constexpr uint8_t RoomLayer(uint8_t room) { return uint8_t(room / MapColumns); }
	inline constexpr uint8_t RoomColumn(uint8_t room) { return uint8_t(room % MapColumns); }
	inline constexpr uint32_t RoomBit(uint8_t room) { return room < MapRooms ? 1u << room : 0u; }

	struct ActMap
	{
		std::array<MapRoom, MapRooms> Rooms{};
		std::array<uint8_t, MapLayers> Width{};

		const MapRoom& At(uint8_t room) const
		{
			static const MapRoom empty;
			return room < MapRooms ? Rooms[room] : empty;
		}

		bool operator==(const ActMap&) const = default;
	};

	inline uint64_t MixSeed(uint64_t seed, uint64_t a, uint64_t b = 0)
	{
		uint64_t z = seed * 0x9E3779B97F4A7C15ull + a * 0xBF58476D1CE4E5B9ull + b * 0x94D049BB133111EBull + 0x2545F4914F6CDD1Dull;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	inline uint8_t DoorSlot(uint8_t column, uint8_t width)
	{
		if (width <= 1)
			return 1;
		if (width == 2)
			return column == 0 ? 0 : 2;
		return std::min<uint8_t>(column, DoorSlots - 1);
	}

	inline uint8_t DoorColumn(uint8_t slot, uint8_t width)
	{
		for (uint8_t column = 0; column < width; column++)
			if (DoorSlot(column, width) == slot)
				return column;
		return NoRoom;
	}

	inline uint8_t DoorMaskOf(const ActMap& map, uint8_t room)
	{
		const uint8_t layer = RoomLayer(room);
		if (room >= MapRooms || layer >= GuardianLayer)
			return 0;
		uint8_t mask = 0;
		for (uint8_t column = 0; column < map.Width[layer + 1]; column++)
			if (map.Rooms[room].Next & (1u << column))
				mask |= uint8_t(1u << DoorSlot(column, map.Width[layer + 1]));
		return mask;
	}

	inline uint8_t DoorTarget(const ActMap& map, uint8_t room, uint8_t slot)
	{
		const uint8_t layer = RoomLayer(room);
		if (room >= MapRooms || layer >= GuardianLayer || slot >= DoorSlots)
			return NoRoom;
		const uint8_t column = DoorColumn(slot, map.Width[layer + 1]);
		if (column == NoRoom || (map.Rooms[room].Next & (1u << column)) == 0)
			return NoRoom;
		return RoomIndex(uint8_t(layer + 1), column);
	}

	inline uint8_t RoomTier(uint8_t act, uint8_t room, RoomKind kind)
	{
		constexpr uint8_t LayerTiers[] = { 1, 1, 2, 2, 3, 3, 4 };
		static_assert(std::size(LayerTiers) == MapLayers);
		const uint32_t tier = LayerTiers[std::min<uint8_t>(RoomLayer(room), GuardianLayer)] + (std::max<uint8_t>(act, 1) - 1u) + (kind == RoomKind::Elite ? 1u : 0u);
		return uint8_t(std::min<uint32_t>(tier, 4u));
	}

	inline ActMap MakeActMap(uint32_t runSeed, uint8_t act)
	{
		Random random(MixSeed(runSeed, 0xAC7u, act));
		ActMap map;
		map.Width[0] = 1;
		map.Width[GuardianLayer] = 1;
		for (uint8_t layer = 1; layer < GuardianLayer; layer++)
			map.Width[layer] = uint8_t(2 + random.Below(2));
		for (uint8_t layer = 0; layer < GuardianLayer; layer++)
		{
			const uint8_t from = map.Width[layer];
			const uint8_t to = map.Width[layer + 1];
			uint8_t i = 0;
			uint8_t j = 0;
			map.Rooms[RoomIndex(layer, i)].Next |= uint8_t(1u << j);
			while (i + 1 < from || j + 1 < to)
			{
				if (i + 1 >= from)
					j++;
				else if (j + 1 >= to)
					i++;
				else if (const uint32_t step = random.Below(3); step == 0)
					i++;
				else if (step == 1)
					j++;
				else
				{
					i++;
					j++;
				}
				map.Rooms[RoomIndex(layer, i)].Next |= uint8_t(1u << j);
			}
		}
		constexpr RoomReward Rewards[] = { RoomReward::Rune, RoomReward::Card, RoomReward::Rank, RoomReward::Spell, RoomReward::Cache };
		constexpr uint32_t RewardWeights[] = { 28, 22, 20, 18, 12 };
		const auto rollReward = [&](uint8_t layer)
		{
			RoomReward reward = RoomReward::Card;
			for (uint32_t attempt = 0; attempt < 4; attempt++)
			{
				uint32_t roll = random.Below(100);
				for (size_t i = 0; i < std::size(Rewards); i++)
				{
					if (roll < RewardWeights[i])
					{
						reward = Rewards[i];
						break;
					}
					roll -= RewardWeights[i];
				}
				bool repeated = false;
				for (uint8_t column = 0; column < map.Width[layer]; column++)
					repeated = repeated || map.Rooms[RoomIndex(layer, column)].Reward == reward;
				if (!repeated)
					break;
			}
			return reward;
		};
		map.Rooms[RoomIndex(0, 0)] = { RoomKind::Fight, RoomReward::Rune, map.Rooms[RoomIndex(0, 0)].Next };
		map.Rooms[RoomIndex(GuardianLayer, 0)].Kind = RoomKind::Guardian;
		for (uint8_t layer = 1; layer < GuardianLayer; layer++)
		{
			bool rested = false;
			for (uint8_t column = 0; column < map.Width[layer]; column++)
			{
				MapRoom& room = map.Rooms[RoomIndex(layer, column)];
				const uint32_t elite = layer >= FirstEliteLayer ? 2u : 0u;
				const uint32_t rest = layer >= FirstRestLayer && layer + 2 != GuardianLayer && !rested ? 2u : 0u;
				const uint32_t roll = random.Below(6u + elite + rest);
				room.Kind = roll < 6u ? RoomKind::Fight : roll < 6u + elite ? RoomKind::Elite : RoomKind::Rest;
				rested = rested || room.Kind == RoomKind::Rest;
			}
		}
		const uint8_t last = GuardianLayer - 1;
		bool restBefore = false;
		for (uint8_t column = 0; column < map.Width[last]; column++)
			restBefore = restBefore || map.Rooms[RoomIndex(last, column)].Kind == RoomKind::Rest;
		if (!restBefore)
			map.Rooms[RoomIndex(last, uint8_t(random.Below(map.Width[last])))].Kind = RoomKind::Rest;
		bool elite = false;
		for (uint8_t layer = FirstEliteLayer; layer < last; layer++)
			for (uint8_t column = 0; column < map.Width[layer]; column++)
				elite = elite || map.Rooms[RoomIndex(layer, column)].Kind == RoomKind::Elite;
		if (!elite)
		{
			const uint8_t layer = uint8_t(FirstRestLayer + random.Below(last - FirstRestLayer));
			map.Rooms[RoomIndex(layer, uint8_t(random.Below(map.Width[layer])))].Kind = RoomKind::Elite;
		}
		const auto convert = [&](uint8_t layer, RoomKind kind)
		{
			uint32_t options = 0;
			uint8_t chosen = NoRoom;
			for (uint8_t column = 0; column < map.Width[layer]; column++)
				if (map.Rooms[RoomIndex(layer, column)].Kind == RoomKind::Fight && random.Below(++options) == 0)
					chosen = column;
			if (chosen != NoRoom)
				map.Rooms[RoomIndex(layer, chosen)].Kind = kind;
			return chosen != NoRoom;
		};
		const uint8_t shopLayer = uint8_t(FirstShopLayer + random.Below(LastShopLayer - FirstShopLayer + 1));
		for (uint8_t step = 0; step <= LastShopLayer - FirstShopLayer; step++)
			if (convert(uint8_t(FirstShopLayer + (shopLayer - FirstShopLayer + step) % (LastShopLayer - FirstShopLayer + 1)), RoomKind::Shop))
				break;
		for (uint8_t layer = 1; layer < last; layer++)
			if (layer != shopLayer && random.Below(4) == 0)
				convert(layer, RoomKind::Shrine);
		for (uint8_t layer = 1; layer < GuardianLayer; layer++)
			for (uint8_t column = 0; column < map.Width[layer]; column++)
			{
				MapRoom& room = map.Rooms[RoomIndex(layer, column)];
				room.Reward = room.Kind == RoomKind::Fight ? rollReward(layer) : room.Kind == RoomKind::Elite ? RoomReward::Treasure : RoomReward::None;
			}
		return map;
	}

	inline uint32_t RoomSeed(uint32_t runSeed, uint8_t act, uint8_t room, uint8_t doors, bool rest)
	{
		return (uint32_t(MixSeed(runSeed, 0x500Du + act, room)) << 4) | (rest ? 8u : 0u) | (doors & AllDoors);
	}

	inline uint8_t RoomSeedDoors(uint32_t seed)
	{
		const uint8_t doors = uint8_t(seed & AllDoors);
		return doors != 0 ? doors : AllDoors;
	}

	inline bool RoomSeedRest(uint32_t seed)
	{
		return (seed & 8u) != 0;
	}

	inline constexpr float RoomParBase = 10.0f;
	inline constexpr float RoomParPerFighter = 5.5f;
	inline constexpr float RoomParPerCrawler = 0.5f;
	inline constexpr float SilverPar = 1.5f;
	inline constexpr float BronzePar = 2.25f;
	inline constexpr float MomentumGain[] = { 0.0f, 0.05f, 0.15f, 0.25f };
	inline constexpr float MomentumGrace = 12.0f;
	inline constexpr float MomentumDecay = 0.015f;
	inline constexpr float MomentumShards = 0.5f;
	inline constexpr float DoorRadius = 2.0f;
	inline constexpr float DoorHoldSeconds = 1.0f;
	inline constexpr float AltarRadius = 1.8f;
	inline constexpr float RestHeal = 0.3f;
	inline constexpr float RoomReviveHealth = 0.25f;
	inline constexpr float EliteClearWorth = 1.5f;

	inline float RoomPar(uint32_t fighters, uint32_t crawlers)
	{
		return RoomParBase + RoomParPerFighter * float(fighters) + RoomParPerCrawler * float(crawlers);
	}

	inline RoomMedal MedalFor(float clock, float par)
	{
		if (par <= 0.0f)
			return RoomMedal::None;
		if (clock <= par)
			return RoomMedal::Gold;
		if (clock <= par * SilverPar)
			return RoomMedal::Silver;
		return clock <= par * BronzePar ? RoomMedal::Bronze : RoomMedal::None;
	}
}
