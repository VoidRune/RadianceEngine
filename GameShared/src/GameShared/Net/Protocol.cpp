#include "Protocol.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr uint32_t MaxListSize = 4096;
		constexpr float AngleScale = 65536.0f / (2.0f * Pi);

		uint16_t QuantizeAngle(float angle)
		{
			return uint16_t(int32_t(std::lround(std::remainder(angle, 2.0f * Pi) * AngleScale)) & 0xFFFF);
		}

		float DequantizeAngle(uint16_t value)
		{
			return float(int16_t(value)) / AngleScale;
		}

		uint8_t QuantizeOwner(uint32_t owner)
		{
			return owner < 255 ? uint8_t(owner) : uint8_t(255);
		}

		uint32_t DequantizeOwner(uint8_t owner)
		{
			return owner == 255 ? NoPlayer : owner;
		}

		template<typename T, size_t Size>
		void WriteBytes(ByteWriter& writer, const std::array<T, Size>& values)
		{
			for (T value : values)
				writer.Write(value);
		}

		template<typename T, size_t Size>
		void ReadBytes(ByteReader& reader, std::array<T, Size>& values)
		{
			for (T& value : values)
				value = reader.Read<T>();
		}

		template<size_t Size>
		void WriteItems(ByteWriter& writer, const std::array<Item, Size>& items)
		{
			for (const Item& item : items)
				WriteItem(writer, item);
		}

		template<size_t Size>
		bool ReadItems(ByteReader& reader, std::array<Item, Size>& items)
		{
			for (Item& item : items)
				if (!ReadItem(reader, item))
					return false;
			return true;
		}

		void WriteSkill(ByteWriter& writer, const SkillTraits& skill)
		{
			writer.Write(skill.Ability);
			writer.Write(uint8_t(skill.Usable ? 1 : 0));
			writer.Write(skill.Level);
			writer.Write(skill.CastTime);
			writer.Write(skill.Cooldown);
			writer.Write(skill.ManaCost);
			writer.Write(skill.MoveScale);
			writer.Write(skill.DamageScale);
			writer.Write(skill.AreaScale);
			writer.Write(skill.ExtraProjectiles);
		}

		bool ReadSkill(ByteReader& reader, SkillTraits& skill)
		{
			skill.Ability = reader.Read<AbilityId>();
			skill.Usable = reader.Read<uint8_t>() != 0;
			skill.Level = reader.Read<uint8_t>();
			skill.CastTime = reader.Read<float>();
			skill.Cooldown = reader.Read<float>();
			skill.ManaCost = reader.Read<float>();
			skill.MoveScale = reader.Read<float>();
			skill.DamageScale = reader.Read<float>();
			skill.AreaScale = reader.Read<float>();
			skill.ExtraProjectiles = reader.Read<uint8_t>();
			return skill.Ability <= AbilityId::Count;
		}

		void WriteTraits(ByteWriter& writer, const PlayerTraits& traits)
		{
			for (const SkillTraits& skill : traits.Skills)
				WriteSkill(writer, skill);
			WriteSkill(writer, traits.Dodge);
			writer.Write(traits.MoveSpeed);
			writer.Write(traits.AreaScale);
			writer.Write(traits.ProjectileSpeedScale);
			writer.Write(traits.ExtraProjectiles);
			writer.Write(traits.ExtraPierce);
			writer.Write(traits.MaxHealth);
			writer.Write(traits.MaxMana);
			writer.Write(traits.ManaRegen);
			writer.Write(traits.Regeneration);
			writer.Write(traits.CritChance);
			writer.Write(traits.CritMultiplier);
			writer.Write(traits.LifeSteal);
			writer.Write(traits.Armor);
			WriteBytes(writer, traits.Resist);
		}

		bool ReadTraits(ByteReader& reader, PlayerTraits& traits)
		{
			for (SkillTraits& skill : traits.Skills)
				if (!ReadSkill(reader, skill))
					return false;
			if (!ReadSkill(reader, traits.Dodge) || traits.Dodge.Ability >= AbilityId::Count)
				return false;
			traits.MoveSpeed = reader.Read<float>();
			traits.AreaScale = reader.Read<float>();
			traits.ProjectileSpeedScale = reader.Read<float>();
			traits.ExtraProjectiles = reader.Read<uint8_t>();
			traits.ExtraPierce = reader.Read<uint8_t>();
			traits.MaxHealth = reader.Read<int32_t>();
			traits.MaxMana = reader.Read<float>();
			traits.ManaRegen = reader.Read<float>();
			traits.Regeneration = reader.Read<float>();
			traits.CritChance = reader.Read<float>();
			traits.CritMultiplier = reader.Read<float>();
			traits.LifeSteal = reader.Read<float>();
			traits.Armor = reader.Read<float>();
			ReadBytes(reader, traits.Resist);
			return !reader.Failed();
		}

		void WriteState(ByteWriter& writer, const PlayerState& state)
		{
			writer.Write(state.DashDirection);
			writer.Write(state.DashDistance);
			writer.Write(state.DashSpeed);
			writer.Write(state.Invulnerable);
			writer.Write(state.PortalCooldown);
			WriteBytes(writer, state.Cooldowns);
			writer.Write(state.DodgeCooldown);
			writer.Write(state.Mana);
			writer.Write(state.CastTimer);
			writer.Write(state.Stun);
			writer.Write(state.Chill);
			writer.Write(state.StunGuard);
			writer.Write(state.CastSlot);
			writer.Write(state.HeldButtons);
			writer.Write(uint8_t(state.Alive ? 1 : 0));
		}

		bool ReadState(ByteReader& reader, PlayerState& state)
		{
			state.DashDirection = reader.ReadVec2();
			state.DashDistance = reader.Read<float>();
			state.DashSpeed = reader.Read<float>();
			state.Invulnerable = reader.Read<float>();
			state.PortalCooldown = reader.Read<float>();
			ReadBytes(reader, state.Cooldowns);
			state.DodgeCooldown = reader.Read<float>();
			state.Mana = reader.Read<float>();
			state.CastTimer = reader.Read<float>();
			state.Stun = reader.Read<float>();
			state.Chill = reader.Read<float>();
			state.StunGuard = reader.Read<float>();
			state.CastSlot = reader.Read<int8_t>();
			state.HeldButtons = reader.Read<uint8_t>();
			state.Alive = reader.Read<uint8_t>() != 0;
			return !reader.Failed() && state.CastSlot >= -1 && state.CastSlot < int8_t(AbilitySlots);
		}
	}

	void WriteItem(ByteWriter& writer, const Item& item)
	{
		writer.Write(item.Id);
		if (!item.IsValid())
			return;
		writer.Write(item.Base);
		writer.Write(item.Quality);
		writer.Write(item.Level);
		writer.Write(item.Power);
		writer.Write(item.Skill);
		writer.Write(item.GemQuality);
		writer.Write(item.Mod);
		writer.Write(uint8_t(item.Corrupted ? 1 : 0));
		writer.Write(item.Pity);
		writer.Write(item.AffixCount);
		for (uint8_t i = 0; i < item.AffixCount; i++)
		{
			writer.Write(item.Affixes[i].Kind);
			writer.Write(item.Affixes[i].Value);
			writer.Write(item.Affixes[i].Tier);
		}
	}

	bool ReadItem(ByteReader& reader, Item& item)
	{
		item = {};
		item.Id = reader.Read<uint32_t>();
		if (!item.IsValid())
			return !reader.Failed();
		item.Base = reader.Read<ItemBase>();
		item.Quality = reader.Read<Rarity>();
		item.Level = reader.Read<uint8_t>();
		item.Power = reader.Read<float>();
		item.Skill = reader.Read<AbilityId>();
		item.GemQuality = reader.Read<uint8_t>();
		item.Mod = reader.Read<GemMod>();
		item.Corrupted = reader.Read<uint8_t>() != 0;
		item.Pity = reader.Read<uint8_t>();
		item.AffixCount = reader.Read<uint8_t>();
		if (item.Base >= ItemBase::Count || item.Quality >= Rarity::Count || item.Skill > AbilityId::Count || item.AffixCount > MaxAffixes || item.Mod >= GemMod::Count)
			return false;
		if (item.Base == ItemBase::Gem && item.Skill >= AbilityId::Count)
			return false;
		for (uint8_t i = 0; i < item.AffixCount; i++)
		{
			item.Affixes[i].Kind = reader.Read<Stat>();
			item.Affixes[i].Value = reader.Read<float>();
			item.Affixes[i].Tier = reader.Read<uint8_t>();
			if (item.Affixes[i].Kind >= Stat::Count)
				return false;
		}
		return !reader.Failed();
	}

	namespace
	{
		void WriteSession(ByteWriter& writer, const SessionInfo& session)
		{
			writer.Write(session.Mode);
			writer.Write(session.Map);
			writer.Write(session.MapSeed);
			writer.Write(session.Tier);
			writer.Write(session.Modifiers);
			writer.Write(EncodeRules(session.Rules));
			writer.Write(session.Epoch);
			writer.Write(uint8_t(session.UnlimitedMaterials ? 1 : 0));
		}

		bool ReadSession(ByteReader& reader, SessionInfo& session)
		{
			session.Mode = reader.Read<GameMode>();
			session.Map = reader.Read<MapKind>();
			session.MapSeed = reader.Read<uint32_t>();
			session.Tier = reader.Read<uint8_t>();
			session.Modifiers = reader.Read<ModifierMask>();
			session.Rules = DecodeRules(reader.Read<uint32_t>());
			session.Epoch = reader.Read<uint32_t>();
			session.UnlimitedMaterials = reader.Read<uint8_t>() != 0;
			return !reader.Failed() && session.Mode < GameMode::Count && session.Map < MapKind::Count;
		}
	}

	std::optional<MessageType> PeekType(std::span<const uint8_t> data)
	{
		if (data.empty() || data[0] >= uint8_t(MessageType::Count))
			return std::nullopt;
		return MessageType(data[0]);
	}

	void Write(ByteWriter& writer, const ClientHello& message)
	{
		writer.Write(message.Version);
		writer.Write(message.Name);
		writer.Write(message.Kit);
		writer.Write(uint8_t(message.HasGear ? 1 : 0));
		if (!message.HasGear)
			return;
		WriteItems(writer, message.Gear.Gear);
		WriteItems(writer, message.Gear.Gems);
		WriteItems(writer, message.Gear.Pack);
		WriteItems(writer, message.Gear.Stash);
		WriteBytes(writer, message.Gear.Materials);
	}

	bool Read(ByteReader& reader, ClientHello& message)
	{
		message.Version = reader.Read<uint32_t>();
		if (reader.Failed() || message.Version != ProtocolVersion)
			return !reader.Failed();
		message.Name = reader.ReadString();
		message.Kit = reader.Read<uint8_t>();
		message.HasGear = reader.Read<uint8_t>() != 0;
		if (message.HasGear && (!ReadItems(reader, message.Gear.Gear) || !ReadItems(reader, message.Gear.Gems) || !ReadItems(reader, message.Gear.Pack) || !ReadItems(reader, message.Gear.Stash)))
			return false;
		if (message.HasGear)
			ReadBytes(reader, message.Gear.Materials);
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ClientInput& message)
	{
		writer.Write(uint8_t(message.Inputs.size()));
		for (const PlayerInput& input : message.Inputs)
		{
			writer.Write(input.Sequence);
			writer.Write(input.Move);
			writer.Write(input.Aim);
			writer.Write(uint16_t(std::lround(QuantizeAimDistance(input.AimDistance) * 32.0f)));
			writer.Write(input.Buttons);
			writer.Write(input.Interact);
			writer.Write(input.ViewTick);
			writer.Write(input.ViewFraction);
		}
	}

	bool Read(ByteReader& reader, ClientInput& message)
	{
		const uint8_t count = reader.Read<uint8_t>();
		message.Inputs.resize(count);
		for (PlayerInput& input : message.Inputs)
		{
			input.Sequence = reader.Read<uint32_t>();
			input.Move = reader.ReadVec2();
			input.Aim = reader.Read<float>();
			input.AimDistance = float(reader.Read<uint16_t>()) / 32.0f;
			input.Buttons = reader.Read<uint8_t>();
			input.Interact = reader.Read<uint8_t>();
			input.ViewTick = reader.Read<uint32_t>();
			input.ViewFraction = reader.Read<uint8_t>();
		}
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ClientAction& message)
	{
		writer.Write(message.Action);
		writer.Write(message.Value);
		writer.Write(message.Target);
		writer.Write(message.Extra);
	}

	bool Read(ByteReader& reader, ClientAction& message)
	{
		message.Action = reader.Read<ActionType>();
		message.Value = reader.Read<uint16_t>();
		message.Target = reader.Read<uint16_t>();
		message.Extra = reader.Read<uint32_t>();
		return !reader.Failed() && message.Action < ActionType::Count;
	}

	void Write(ByteWriter& writer, const ServerWelcome& message)
	{
		writer.Write(message.PlayerId);
		writer.Write(message.PlayerNetId);
		WriteSession(writer, message.Session);
		writer.Write(message.TickRate);
		writer.Write(message.SnapshotInterval);
		writer.Write(message.ServerTick);
		writer.Write(message.PropHash);
	}

	bool Read(ByteReader& reader, ServerWelcome& message)
	{
		message.PlayerId = reader.Read<uint32_t>();
		message.PlayerNetId = reader.Read<uint32_t>();
		if (!ReadSession(reader, message.Session))
			return false;
		message.TickRate = reader.Read<uint32_t>();
		message.SnapshotInterval = reader.Read<uint32_t>();
		message.ServerTick = reader.Read<uint32_t>();
		message.PropHash = reader.Read<uint32_t>();
		return !reader.Failed() && message.TickRate > 0 && message.SnapshotInterval > 0;
	}

	void Write(ByteWriter& writer, const ServerTravel& message)
	{
		writer.Write(message.PlayerNetId);
		WriteSession(writer, message.Session);
		writer.Write(message.ServerTick);
	}

	bool Read(ByteReader& reader, ServerTravel& message)
	{
		message.PlayerNetId = reader.Read<uint32_t>();
		if (!ReadSession(reader, message.Session))
			return false;
		message.ServerTick = reader.Read<uint32_t>();
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerStash& message)
	{
		WriteItems(writer, message.Stash);
	}

	bool Read(ByteReader& reader, ServerStash& message)
	{
		return ReadItems(reader, message.Stash) && !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerNotice& message)
	{
		writer.Write(message.Text);
		writer.Write(uint8_t(message.Success ? 1 : 0));
	}

	bool Read(ByteReader& reader, ServerNotice& message)
	{
		message.Text = reader.ReadString();
		message.Success = reader.Read<uint8_t>() != 0;
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerKillFeed& message)
	{
		const size_t count = std::min<size_t>(message.Kills.size(), 255);
		writer.Write(uint8_t(count));
		for (size_t i = 0; i < count; i++)
		{
			writer.Write(QuantizeOwner(message.Kills[i].Killer));
			writer.Write(QuantizeOwner(message.Kills[i].Victim));
			writer.Write(message.Kills[i].Ability);
		}
	}

	bool Read(ByteReader& reader, ServerKillFeed& message)
	{
		message.Kills.resize(reader.Read<uint8_t>());
		for (KillRecord& kill : message.Kills)
		{
			kill.Killer = DequantizeOwner(reader.Read<uint8_t>());
			kill.Victim = DequantizeOwner(reader.Read<uint8_t>());
			kill.Ability = std::min(reader.Read<uint8_t>(), uint8_t(AbilityId::Count));
		}
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerReject& message)
	{
		writer.Write(message.Reason);
	}

	bool Read(ByteReader& reader, ServerReject& message)
	{
		message.Reason = reader.ReadString();
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerPlayerList& message)
	{
		writer.Write(uint8_t(message.Players.size()));
		for (const PlayerInfo& player : message.Players)
		{
			writer.Write(player.PlayerId);
			writer.Write(player.Name);
			writer.Write(player.Kills);
			writer.Write(player.Deaths);
			writer.Write(player.Ping);
			WriteBytes(writer, player.Buffs);
			writer.Write(player.Look.Weapon);
			writer.Write(player.Look.Head);
			writer.Write(player.Look.Body);
			writer.Write(player.Look.Feet);
			writer.Write(player.Look.WeaponQuality);
			writer.Write(player.Team);
			writer.Write(player.Placement);
			writer.Write(uint8_t(player.Host ? 1 : 0));
		}
	}

	bool Read(ByteReader& reader, ServerPlayerList& message)
	{
		message.Players.resize(reader.Read<uint8_t>());
		for (PlayerInfo& player : message.Players)
		{
			player.PlayerId = reader.Read<uint32_t>();
			player.Name = reader.ReadString();
			player.Kills = reader.Read<uint32_t>();
			player.Deaths = reader.Read<uint32_t>();
			player.Ping = reader.Read<int32_t>();
			ReadBytes(reader, player.Buffs);
			player.Look.Weapon = reader.Read<ItemBase>();
			player.Look.Head = reader.Read<ItemBase>();
			player.Look.Body = reader.Read<ItemBase>();
			player.Look.Feet = reader.Read<ItemBase>();
			player.Look.WeaponQuality = reader.Read<Rarity>();
			player.Team = reader.Read<uint8_t>();
			player.Placement = reader.Read<uint8_t>();
			player.Host = reader.Read<uint8_t>() != 0;
			if (player.Look.Weapon >= ItemBase::Count || player.Look.Head > ItemBase::Count || player.Look.Body > ItemBase::Count || player.Look.Feet > ItemBase::Count || player.Look.WeaponQuality >= Rarity::Count)
				return false;
		}
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerSnapshot& message)
	{
		writer.Write(message.Epoch);
		writer.Write(message.Tick);
		writer.Write(message.LastProcessedInput);

		const GameStatus& status = message.Status;
		writer.Write(status.Mode);
		writer.Write(status.Map);
		writer.Write(status.Tier);
		writer.Write(status.Modifiers);
		writer.Write(status.Phase);
		writer.Write(status.Wave);
		writer.Write(status.EnemiesLeft);
		writer.Write(status.PhaseTime);
		writer.Write(status.PendingChoices);
		writer.Write(status.Mission);
		writer.Write(status.Kills);
		writer.Write(status.KillGoal);
		writer.Write(status.Revives);
		writer.Write(status.BossNetId);
		writer.Write(status.BossHealth);
		writer.Write(status.MissionTime);
		if (IsPvpMode(status.Mode))
		{
			writer.Write(status.Match);
			writer.Write(status.MatchTime);
			WriteBytes(writer, status.TeamScores);
			writer.Write(QuantizeOwner(status.Winner));
			writer.Write(status.WinningTeam);
			writer.Write(status.Alive);
			writer.Write(status.Entrants);
		}
		if (status.Mode == GameMode::Royale)
		{
			const StormState& storm = status.Storm;
			writer.Write(storm.Center);
			writer.Write(storm.Radius);
			writer.Write(storm.Target);
			writer.Write(storm.TargetRadius);
			writer.Write(storm.Stage);
			writer.Write(storm.Timer);
			writer.Write(uint8_t(storm.Shrinking ? 1 : 0));
			writer.Write(storm.Damage);
		}

		const LocalPlayerState& local = message.LocalPlayer;
		writer.Write(uint8_t(local.Valid ? 1 : 0));
		if (local.Valid)
		{
			writer.Write(local.Motion.Position);
			writer.Write(local.Motion.Facing);
			WriteState(writer, local.State);
			WriteBytes(writer, local.Buffs);
			writer.Write(uint8_t(local.HasTraits ? 1 : 0));
			if (local.HasTraits)
				WriteTraits(writer, local.Traits);
			WriteBytes(writer, local.Offer);
			writer.Write(uint8_t(local.OfferPending ? 1 : 0));
			writer.Write(local.Health);
			writer.Write(local.MaxHealth);
			writer.Write(local.RespawnTime);
			WriteBytes(writer, local.Materials);
			writer.Write(QuantizeOwner(local.Watching));
			writer.Write(QuantizeOwner(local.KilledBy));
		}

		const size_t count = std::min<size_t>(message.Entities.size(), MaxListSize);
		writer.Write(uint16_t(count));
		for (size_t i = 0; i < count; i++)
		{
			const EntityState& entity = message.Entities[i];
			writer.Write(entity.NetId);
			writer.Write(entity.Kind);
			writer.Write(QuantizePosition(entity.Position.x));
			writer.Write(QuantizePosition(entity.Position.y));
			writer.Write(QuantizeAngle(entity.Facing));
			writer.Write(QuantizeOwner(entity.Owner));
			writer.Write(entity.Tag);
			writer.Write(entity.Variant);
			writer.Write(entity.Scale);
			writer.Write(entity.Health);
			writer.Write(entity.Flags);
		}

		const size_t hits = std::min<size_t>(message.Hits.size(), MaxListSize);
		writer.Write(uint16_t(hits));
		for (size_t i = 0; i < hits; i++)
		{
			const HitEvent& hit = message.Hits[i];
			writer.Write(hit.Target);
			writer.Write(QuantizePosition(hit.Position.x));
			writer.Write(QuantizePosition(hit.Position.y));
			writer.Write(hit.Damage);
			writer.Write(uint8_t((hit.Critical ? 1 : 0) | (hit.OnPlayer ? 2 : 0) | (uint32_t(hit.Damages) << 2)));
		}
	}

	bool Read(ByteReader& reader, ServerSnapshot& message)
	{
		message.Epoch = reader.Read<uint32_t>();
		message.Tick = reader.Read<uint32_t>();
		message.LastProcessedInput = reader.Read<uint32_t>();

		GameStatus& status = message.Status;
		status.Mode = reader.Read<GameMode>();
		status.Map = reader.Read<MapKind>();
		status.Tier = reader.Read<uint8_t>();
		status.Modifiers = reader.Read<ModifierMask>();
		status.Phase = reader.Read<GamePhase>();
		status.Wave = reader.Read<uint16_t>();
		status.EnemiesLeft = reader.Read<uint16_t>();
		status.PhaseTime = reader.Read<float>();
		status.PendingChoices = reader.Read<uint8_t>();
		status.Mission = reader.Read<MissionPhase>();
		status.Kills = reader.Read<uint16_t>();
		status.KillGoal = reader.Read<uint16_t>();
		status.Revives = reader.Read<uint8_t>();
		status.BossNetId = reader.Read<uint32_t>();
		status.BossHealth = reader.Read<uint8_t>();
		status.MissionTime = reader.Read<float>();
		if (uint8_t(status.Phase) > uint8_t(GamePhase::Defeat) || status.Mode >= GameMode::Count || status.Map >= MapKind::Count || uint8_t(status.Mission) > uint8_t(MissionPhase::Failed))
			return false;
		if (IsPvpMode(status.Mode))
		{
			status.Match = reader.Read<MatchPhase>();
			status.MatchTime = reader.Read<float>();
			ReadBytes(reader, status.TeamScores);
			status.Winner = DequantizeOwner(reader.Read<uint8_t>());
			status.WinningTeam = reader.Read<uint8_t>();
			status.Alive = reader.Read<uint8_t>();
			status.Entrants = reader.Read<uint8_t>();
			if (uint8_t(status.Match) > uint8_t(MatchPhase::Ended))
				return false;
		}
		if (status.Mode == GameMode::Royale)
		{
			StormState& storm = status.Storm;
			storm.Center = reader.ReadVec2();
			storm.Radius = reader.Read<float>();
			storm.Target = reader.ReadVec2();
			storm.TargetRadius = reader.Read<float>();
			storm.Stage = reader.Read<uint8_t>();
			storm.Timer = reader.Read<float>();
			storm.Shrinking = reader.Read<uint8_t>() != 0;
			storm.Damage = reader.Read<float>();
		}

		LocalPlayerState& local = message.LocalPlayer;
		local.Valid = reader.Read<uint8_t>() != 0;
		if (local.Valid)
		{
			local.Motion.Position = reader.ReadVec2();
			local.Motion.Facing = reader.Read<float>();
			if (!ReadState(reader, local.State))
				return false;
			ReadBytes(reader, local.Buffs);
			local.HasTraits = reader.Read<uint8_t>() != 0;
			if (local.HasTraits && !ReadTraits(reader, local.Traits))
				return false;
			ReadBytes(reader, local.Offer);
			local.OfferPending = reader.Read<uint8_t>() != 0;
			local.Health = reader.Read<int32_t>();
			local.MaxHealth = reader.Read<int32_t>();
			local.RespawnTime = reader.Read<float>();
			ReadBytes(reader, local.Materials);
			local.Watching = DequantizeOwner(reader.Read<uint8_t>());
			local.KilledBy = DequantizeOwner(reader.Read<uint8_t>());
			if (std::any_of(local.Offer.begin(), local.Offer.end(), [](UpgradeId id) { return id > UpgradeId::Count; }))
				return false;
		}

		const uint16_t count = reader.Read<uint16_t>();
		if (reader.Failed() || count > MaxListSize)
			return false;
		message.Entities.resize(count);
		for (EntityState& entity : message.Entities)
		{
			entity.NetId = reader.Read<uint32_t>();
			entity.Kind = reader.Read<EntityKind>();
			entity.Position.x = DequantizePosition(reader.Read<uint16_t>());
			entity.Position.y = DequantizePosition(reader.Read<uint16_t>());
			entity.Facing = DequantizeAngle(reader.Read<uint16_t>());
			entity.Owner = DequantizeOwner(reader.Read<uint8_t>());
			entity.Tag = reader.Read<uint16_t>();
			entity.Variant = reader.Read<uint8_t>();
			entity.Scale = reader.Read<uint8_t>();
			entity.Health = reader.Read<uint8_t>();
			entity.Flags = reader.Read<uint8_t>();
			if (uint8_t(entity.Kind) >= uint8_t(EntityKind::Count))
				return false;
		}

		const uint16_t hits = reader.Read<uint16_t>();
		if (reader.Failed() || hits > MaxListSize)
			return false;
		message.Hits.resize(hits);
		for (HitEvent& hit : message.Hits)
		{
			hit.Target = reader.Read<uint32_t>();
			hit.Position.x = DequantizePosition(reader.Read<uint16_t>());
			hit.Position.y = DequantizePosition(reader.Read<uint16_t>());
			hit.Damage = reader.Read<uint16_t>();
			const uint8_t flags = reader.Read<uint8_t>();
			hit.Critical = (flags & 1) != 0;
			hit.OnPlayer = (flags & 2) != 0;
			hit.Damages = Element(std::min<uint32_t>(flags >> 2, uint32_t(Element::Count) - 1));
		}
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ServerInventory& message)
	{
		WriteItems(writer, message.Gear);
		WriteItems(writer, message.Gems);
		WriteItems(writer, message.Pack);
		WriteBytes(writer, message.Upgrades);
	}

	bool Read(ByteReader& reader, ServerInventory& message)
	{
		if (!ReadItems(reader, message.Gear) || !ReadItems(reader, message.Gems) || !ReadItems(reader, message.Pack))
			return false;
		ReadBytes(reader, message.Upgrades);
		return !reader.Failed();
	}
}
