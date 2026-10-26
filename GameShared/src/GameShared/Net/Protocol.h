#pragma once
#include "GameShared/Content/Achievements.h"
#include "GameShared/Content/Cosmetics.h"
#include "GameShared/Content/Crafting.h"
#include "GameShared/Content/Items.h"
#include "GameShared/Content/Missions.h"
#include "GameShared/Content/Traits.h"
#include "GameShared/Content/Upgrades.h"
#include "GameShared/Core/ByteStream.h"
#include "GameShared/Entity/Components.h"
#include "GameShared/Simulation/GameStatus.h"
#include "GameShared/Simulation/HitEvent.h"
#include "GameShared/Simulation/PlayerInput.h"
#include "GameShared/World/Level.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Game
{
	constexpr uint32_t ProtocolVersion = 38;
	constexpr uint16_t DefaultPort = 27020;
	constexpr uint32_t InputRedundancy = 4;

	enum class MessageType : uint8_t
	{
		ClientHello,
		ClientInput,
		ClientAction,
		ServerWelcome,
		ServerReject,
		ServerPlayerList,
		ServerSnapshot,
		ServerInventory,
		ServerTravel,
		ServerStash,
		ServerNotice,
		ServerKillFeed,
		ServerChess,
		ServerAchievement,
		Count,
	};

	enum class EntityFlags : uint32_t
	{
		None = 0,
		Dead = 1 << 0,
		Dashing = 1 << 1,
		Attacking = 1 << 2,
		Elite = 1 << 3,
		Pending = 1 << 4,
		Chilled = 1 << 5,
		Stunned = 1 << 6,
		Warded = 1 << 7,
		Burning = 1 << 8,
		Flung = 1 << 9,
		Falling = 1 << 10,
		Lifted = 1 << 11,
		Static = 1 << 12,
		Ignited = 1 << 13,
		Marked = 1 << 14,
		Barrier = 1 << 15,
		Leyline = 1u << 16,
	};

	enum class ActionType : uint8_t
	{
		Equip,
		EquipTo,
		Unequip,
		Drop,
		SlotGem,
		UnslotGem,
		ChooseUpgrade,
		StashDeposit,
		StashWithdraw,
		Salvage,
		Forge,
		GemCraft,
		StartMission,
		ReturnToHub,
		OpenChest,
		UseGateway,
		StartMatch,
		Spectate,
		BrawlSpell,
		BrawlCard,
		BrawlReady,
		Wish,
		SetHonors,
		Chess,
		SetCosmetics,
		JoinMatch,
		Count,
	};

	struct ClientHello
	{
		static constexpr MessageType Type = MessageType::ClientHello;
		uint32_t Version = ProtocolVersion;
		std::string Name;
		uint8_t Kit = 0;
		uint8_t Honors = 0;
		Cosmetics Style;
		bool HasGear = false;
		GearSet Gear;
	};

	struct ClientInput
	{
		static constexpr MessageType Type = MessageType::ClientInput;
		std::vector<PlayerInput> Inputs;
	};

	struct ClientAction
	{
		static constexpr MessageType Type = MessageType::ClientAction;
		ActionType Action = ActionType::Equip;
		uint16_t Value = 0;
		uint16_t Target = 0;
		uint32_t Extra = 0;
		MatchRules Rules;
	};

	constexpr uint16_t AnySlot = UINT16_MAX;

	inline uint16_t EncodeItemRef(ItemRef ref)
	{
		return uint16_t((uint32_t(ref.Location) << 8) | (ref.Index & 0xFFu));
	}

	inline ItemRef DecodeItemRef(uint16_t value)
	{
		return { ItemLocation(std::min<uint32_t>(value >> 8, uint32_t(ItemLocation::Count))), uint16_t(value & 0xFFu) };
	}

	struct SessionInfo
	{
		GameMode Mode = GameMode::Hub;
		MapKind Map = MapKind::Hub;
		uint32_t MapSeed = 0;
		uint8_t Tier = 1;
		ModifierMask Modifiers = 0;
		MatchRules Rules;
		uint32_t Epoch = 0;
		bool UnlimitedMaterials = false;
	};

	struct ServerWelcome
	{
		static constexpr MessageType Type = MessageType::ServerWelcome;
		uint32_t PlayerId = 0;
		uint32_t PlayerNetId = 0;
		SessionInfo Session;
		uint32_t TickRate = 0;
		uint32_t SnapshotInterval = 1;
		uint32_t ServerTick = 0;
		uint32_t PropHash = 0;
	};

	struct ServerTravel
	{
		static constexpr MessageType Type = MessageType::ServerTravel;
		uint32_t PlayerNetId = 0;
		SessionInfo Session;
		uint32_t ServerTick = 0;
	};

	struct ServerStash
	{
		static constexpr MessageType Type = MessageType::ServerStash;
		StashTab Stash{};
	};

	struct ServerNotice
	{
		static constexpr MessageType Type = MessageType::ServerNotice;
		std::string Text;
		bool Success = true;
	};

	struct ServerChess
	{
		static constexpr MessageType Type = MessageType::ServerChess;
		uint32_t Revision = 0;
		std::array<uint8_t, 2> Seats{ Chess::NoSeat, Chess::NoSeat };
		Chess::Verdict Result = Chess::Verdict::Playing;
		uint8_t Winner = Chess::NoWinner;
		uint8_t DrawOffer = 0;
		std::vector<uint16_t> Moves;
	};

	struct ServerReject
	{
		static constexpr MessageType Type = MessageType::ServerReject;
		std::string Reason;
	};

	struct ServerKillFeed
	{
		static constexpr MessageType Type = MessageType::ServerKillFeed;
		std::vector<KillRecord> Kills;
	};

	struct ServerAchievement
	{
		static constexpr MessageType Type = MessageType::ServerAchievement;
		Achievement Id = Achievement::FirstFling;
		uint32_t Detail = 0;
	};

	struct PlayerLook
	{
		ItemBase Weapon = ItemBase::Wand;
		ItemBase Head = ItemBase::Count;
		ItemBase Body = ItemBase::Count;
		ItemBase Feet = ItemBase::Count;
		Rarity WeaponQuality = Rarity::Common;
		Cosmetics Style;
	};

	struct PlayerInfo
	{
		uint32_t PlayerId = 0;
		std::string Name;
		uint32_t Kills = 0;
		uint32_t Deaths = 0;
		uint16_t Assists = 0;
		uint32_t Damage = 0;
		int32_t Ping = -1;
		BuffStacks Buffs{};
		PlayerLook Look;
		uint8_t Team = NoTeam;
		uint8_t Placement = 0;
		bool Host = false;
		uint16_t Score = 0;
		uint8_t Rounds = 0;
		uint8_t Honors = 0;
		CardMask Cards = 0;
		bool Choosing = false;
		bool Bot = false;
		bool Spectator = false;
	};

	struct ServerPlayerList
	{
		static constexpr MessageType Type = MessageType::ServerPlayerList;
		std::vector<PlayerInfo> Players;
	};

	struct EntityState
	{
		uint32_t NetId = 0;
		EntityKind Kind = EntityKind::Player;
		Vec2 Position;
		float Facing = 0.0f;
		uint32_t Owner = NoPlayer;
		uint16_t Tag = 0;
		uint8_t Variant = 0;
		uint8_t Scale = 16;
		uint8_t Health = 255;
		uint32_t Flags = 0;
		uint8_t Charge = 0;

		bool Has(EntityFlags flag) const { return (Flags & uint32_t(flag)) != 0; }
		float GetScale() const { return float(Scale) / 16.0f; }
		float GetChargePower() const { return float(std::min(Charge, ChargeFull)) / float(ChargeFull); }
		float GetOvercharge() const { return Charge > ChargeFull ? float(Charge - ChargeFull) / float(255 - ChargeFull) : 0.0f; }

		static constexpr uint8_t ChargeFull = 200;
	};

	inline uint8_t PackCharge(float power, float over)
	{
		if (power < 1.0f)
			return uint8_t(std::lround(std::clamp(power, 0.0f, 1.0f) * float(EntityState::ChargeFull)));
		return uint8_t(EntityState::ChargeFull + std::lround(std::clamp(over, 0.0f, 1.0f) * float(255 - EntityState::ChargeFull)));
	}

	struct LocalPlayerState
	{
		bool Valid = false;
		Transform Motion;
		PlayerState State;
		BuffStacks Buffs{};
		bool HasTraits = false;
		PlayerTraits Traits;
		UpgradeOffer Offer{};
		bool OfferPending = false;
		int32_t Health = 0;
		int32_t MaxHealth = 0;
		float RespawnTime = 0.0f;
		MaterialStock Materials{};
		uint32_t Watching = NoPlayer;
		uint32_t KilledBy = NoPlayer;
		BrawlLoadout Brawl;
		uint16_t Starlit = 0;
		uint8_t Shield = 0;
	};

	struct ServerSnapshot
	{
		static constexpr MessageType Type = MessageType::ServerSnapshot;
		uint32_t Epoch = 0;
		uint32_t Tick = 0;
		uint32_t LastProcessedInput = 0;
		GameStatus Status;
		LocalPlayerState LocalPlayer;
		std::vector<EntityState> Entities;
		std::vector<HitEvent> Hits;
	};

	struct ServerInventory
	{
		static constexpr MessageType Type = MessageType::ServerInventory;
		Equipment Gear{};
		GemSlots Gems{};
		Backpack Pack{};
		UpgradeStacks Upgrades{};
	};

	void WriteItem(ByteWriter& writer, const Item& item);
	bool ReadItem(ByteReader& reader, Item& item);

	void Write(ByteWriter& writer, const ClientHello& message);
	void Write(ByteWriter& writer, const ClientInput& message);
	void Write(ByteWriter& writer, const ClientAction& message);
	void Write(ByteWriter& writer, const ServerWelcome& message);
	void Write(ByteWriter& writer, const ServerReject& message);
	void Write(ByteWriter& writer, const ServerPlayerList& message);
	void Write(ByteWriter& writer, const ServerSnapshot& message);
	void Write(ByteWriter& writer, const ServerInventory& message);
	void Write(ByteWriter& writer, const ServerTravel& message);
	void Write(ByteWriter& writer, const ServerStash& message);
	void Write(ByteWriter& writer, const ServerNotice& message);
	void Write(ByteWriter& writer, const ServerKillFeed& message);
	void Write(ByteWriter& writer, const ServerChess& message);
	void Write(ByteWriter& writer, const ServerAchievement& message);

	bool Read(ByteReader& reader, ClientHello& message);
	bool Read(ByteReader& reader, ClientInput& message);
	bool Read(ByteReader& reader, ClientAction& message);
	bool Read(ByteReader& reader, ServerWelcome& message);
	bool Read(ByteReader& reader, ServerReject& message);
	bool Read(ByteReader& reader, ServerPlayerList& message);
	bool Read(ByteReader& reader, ServerSnapshot& message);
	bool Read(ByteReader& reader, ServerInventory& message);
	bool Read(ByteReader& reader, ServerTravel& message);
	bool Read(ByteReader& reader, ServerStash& message);
	bool Read(ByteReader& reader, ServerNotice& message);
	bool Read(ByteReader& reader, ServerKillFeed& message);
	bool Read(ByteReader& reader, ServerChess& message);
	bool Read(ByteReader& reader, ServerAchievement& message);

	template<typename T>
	std::vector<uint8_t> Encode(const T& message)
	{
		ByteWriter writer;
		writer.Write(T::Type);
		Write(writer, message);
		return writer.Take();
	}

	std::optional<MessageType> PeekType(std::span<const uint8_t> data);

	template<typename T>
	bool Decode(std::span<const uint8_t> data, T& message)
	{
		ByteReader reader(data);
		return reader.Read<MessageType>() == T::Type && Read(reader, message) && !reader.Failed();
	}
}
