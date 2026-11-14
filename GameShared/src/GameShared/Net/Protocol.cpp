#include "Protocol.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
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

		void WriteSkill(ByteWriter& writer, const SkillTraits& skill)
		{
			writer.Write(skill.Ability);
			writer.Write(uint8_t(skill.Usable ? 1 : 0));
			writer.Write(skill.Level);
			writer.Write(skill.CastTime);
			writer.Write(skill.ChargeTime);
			writer.Write(skill.Cooldown);
			writer.Write(skill.ManaCost);
			writer.Write(skill.MoveScale);
			writer.Write(skill.DamageScale);
			writer.Write(skill.AreaScale);
			writer.Write(skill.PushScale);
			writer.Write(skill.ExtraProjectiles);
			writer.Write(skill.RangeScale);
			writer.Write(skill.Durability);
		}

		bool ReadSkill(ByteReader& reader, SkillTraits& skill)
		{
			skill.Ability = reader.Read<AbilityId>();
			skill.Usable = reader.Read<uint8_t>() != 0;
			skill.Level = reader.Read<uint8_t>();
			skill.CastTime = reader.Read<float>();
			skill.ChargeTime = reader.Read<float>();
			skill.Cooldown = reader.Read<float>();
			skill.ManaCost = reader.Read<float>();
			skill.MoveScale = reader.Read<float>();
			skill.DamageScale = reader.Read<float>();
			skill.AreaScale = reader.Read<float>();
			skill.PushScale = reader.Read<float>();
			skill.ExtraProjectiles = reader.Read<uint8_t>();
			skill.RangeScale = reader.Read<float>();
			skill.Durability = reader.Read<float>();
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
			writer.Write(traits.BloodRate);
			writer.Write(traits.OverloadEvery);
			writer.Write(traits.TempoStep);
			writer.Write(traits.SnapWindow);
			writer.Write(traits.StallRegen);
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
			traits.BloodRate = reader.Read<float>();
			traits.OverloadEvery = reader.Read<uint8_t>();
			traits.TempoStep = reader.Read<float>();
			traits.SnapWindow = reader.Read<float>();
			traits.StallRegen = reader.Read<float>();
			return !reader.Failed();
		}

		void WriteBrawlStatus(ByteWriter& writer, const BrawlStatus& brawl)
		{
			writer.Write(brawl.Phase);
			writer.Write(brawl.Round);
			writer.Write(brawl.Rounds);
			writer.Write(brawl.Timer);
			writer.Write(brawl.Clock);
			writer.Write(brawl.Plan.Center);
			writer.Write(brawl.Plan.StartRadius);
			writer.Write(brawl.Plan.Seed);
			writer.Write(brawl.Plan.Shape);
			const uint8_t craters = uint8_t(std::min<size_t>(brawl.Plan.CraterCount, MaxCraters));
			writer.Write(craters);
			for (uint32_t i = 0; i < craters; i++)
			{
				writer.Write(brawl.Plan.Craters[i].Center);
				writer.Write(brawl.Plan.Craters[i].Radius);
				writer.Write(brawl.Plan.Craters[i].Time);
			}
			const uint8_t bolts = uint8_t(std::min<size_t>(brawl.Plan.BoltCount, MaxBolts));
			writer.Write(bolts);
			for (uint32_t i = 0; i < bolts; i++)
			{
				writer.Write(brawl.Plan.Bolts[i].Center);
				writer.Write(brawl.Plan.Bolts[i].Time);
			}
			const uint8_t crusts = uint8_t(std::min<size_t>(brawl.Plan.CrustCount, MaxCrusts));
			writer.Write(crusts);
			for (uint32_t i = 0; i < crusts; i++)
			{
				writer.Write(brawl.Plan.Crusts[i].Center);
				writer.Write(brawl.Plan.Crusts[i].Radius);
				writer.Write(brawl.Plan.Crusts[i].Time);
			}
			writer.Write(uint8_t(brawl.Plan.Steady ? 1 : 0));
			writer.Write(QuantizeOwner(brawl.RoundWinner));
			writer.Write(brawl.RoundTeam);
			writer.Write(brawl.Ready);
			writer.Write(brawl.Players);
		}

		bool ReadBrawlStatus(ByteReader& reader, BrawlStatus& brawl)
		{
			brawl.Phase = reader.Read<BrawlPhase>();
			brawl.Round = reader.Read<uint8_t>();
			brawl.Rounds = reader.Read<uint8_t>();
			brawl.Timer = reader.Read<float>();
			brawl.Clock = reader.Read<float>();
			brawl.Plan.Center = reader.ReadVec2();
			brawl.Plan.StartRadius = reader.Read<float>();
			brawl.Plan.Seed = reader.Read<uint32_t>();
			brawl.Plan.Shape = reader.Read<BrawlShape>();
			if (brawl.Plan.Shape >= BrawlShape::Count)
				return false;
			brawl.Plan.CraterCount = reader.Read<uint8_t>();
			if (brawl.Plan.CraterCount > MaxCraters)
				return false;
			for (uint32_t i = 0; i < brawl.Plan.CraterCount; i++)
			{
				brawl.Plan.Craters[i].Center = reader.ReadVec2();
				brawl.Plan.Craters[i].Radius = reader.Read<float>();
				brawl.Plan.Craters[i].Time = reader.Read<float>();
			}
			brawl.Plan.BoltCount = reader.Read<uint8_t>();
			if (brawl.Plan.BoltCount > MaxBolts)
				return false;
			for (uint32_t i = 0; i < brawl.Plan.BoltCount; i++)
			{
				brawl.Plan.Bolts[i].Center = reader.ReadVec2();
				brawl.Plan.Bolts[i].Time = reader.Read<float>();
			}
			brawl.Plan.CrustCount = reader.Read<uint8_t>();
			if (brawl.Plan.CrustCount > MaxCrusts)
				return false;
			for (uint32_t i = 0; i < brawl.Plan.CrustCount; i++)
			{
				brawl.Plan.Crusts[i].Center = reader.ReadVec2();
				brawl.Plan.Crusts[i].Radius = reader.Read<float>();
				brawl.Plan.Crusts[i].Time = reader.Read<float>();
			}
			brawl.Plan.Steady = reader.Read<uint8_t>() != 0;
			brawl.RoundWinner = DequantizeOwner(reader.Read<uint8_t>());
			brawl.RoundTeam = reader.Read<uint8_t>();
			brawl.Ready = reader.Read<uint8_t>();
			brawl.Players = reader.Read<uint8_t>();
			return !reader.Failed() && uint8_t(brawl.Phase) <= uint8_t(BrawlPhase::Pick);
		}

		void WriteLoadout(ByteWriter& writer, const BrawlLoadout& loadout)
		{
			WriteBytes(writer, loadout.Spells);
			writer.Write(loadout.Cards);
			WriteBytes(writer, loadout.Offer);
			writer.Write(uint8_t((loadout.OfferPending ? 1u : 0u) | (loadout.Ready ? 2u : 0u)));
		}

		bool ReadLoadout(ByteReader& reader, BrawlLoadout& loadout)
		{
			ReadBytes(reader, loadout.Spells);
			loadout.Cards = reader.Read<CardMask>();
			ReadBytes(reader, loadout.Offer);
			const uint8_t flags = reader.Read<uint8_t>();
			loadout.OfferPending = (flags & 1u) != 0;
			loadout.Ready = (flags & 2u) != 0;
			const bool spells = std::all_of(loadout.Spells.begin(), loadout.Spells.end(), [](AbilityId id) { return id < AbilityId::Count; });
			const bool offer = std::all_of(loadout.Offer.begin(), loadout.Offer.end(), [](CardId id) { return id <= CardId::Count; });
			return !reader.Failed() && spells && offer;
		}

		void WriteState(ByteWriter& writer, const PlayerState& state)
		{
			writer.Write(state.DashDirection);
			writer.Write(state.DashDistance);
			writer.Write(state.DashSpeed);
			writer.Write(state.Push);
			writer.Write(state.Ward);
			writer.Write(state.WardLeft);
			writer.Write(state.WardShatter);
			writer.Write(state.Invulnerable);
			writer.Write(state.PortalCooldown);
			WriteBytes(writer, state.Cooldowns);
			writer.Write(state.DodgeCooldown);
			writer.Write(state.Mana);
			writer.Write(state.CastTimer);
			writer.Write(state.Charge);
			writer.Write(state.CastPoint);
			writer.Write(state.CastAim);
			writer.Write(state.Stun);
			writer.Write(state.Chill);
			writer.Write(state.Slick);
			writer.Write(state.StunGuard);
			writer.Write(state.Fall);
			writer.Write(state.Ledge);
			writer.Write(state.Footing);
			writer.Write(state.CastSlot);
			writer.Write(state.HeldButtons);
			writer.Write(state.RecastSlot);
			writer.Write(state.RecastTimer);
			writer.Write(state.RecastAge);
			writer.Write(state.RecastOrigin);
			writer.Write(state.RecastAim);
			writer.Write(state.RecastReach);
			writer.Write(state.Static);
			writer.Write(state.Overload);
			writer.Write(state.RecastAbility);
			writer.Write(state.ManaStall);
			writer.Write(state.Momentum);
			writer.Write(state.Tempo);
			writer.Write(state.TempoStacks);
			writer.Write(state.Snap);
			writer.Write(state.Surge);
			writer.Write(state.GuardLost);
			writer.Write(state.GuardIdle);
			writer.Write(uint8_t((state.Alive ? 1 : 0) | (state.Airborne ? 2 : 0) | (state.GuardUp ? 4 : 0)));
		}

		bool ReadState(ByteReader& reader, PlayerState& state)
		{
			state.DashDirection = reader.ReadVec2();
			state.DashDistance = reader.Read<float>();
			state.DashSpeed = reader.Read<float>();
			state.Push = reader.ReadVec2();
			state.Ward = reader.Read<float>();
			state.WardLeft = reader.Read<float>();
			state.WardShatter = reader.Read<float>();
			state.Invulnerable = reader.Read<float>();
			state.PortalCooldown = reader.Read<float>();
			ReadBytes(reader, state.Cooldowns);
			state.DodgeCooldown = reader.Read<float>();
			state.Mana = reader.Read<float>();
			state.CastTimer = reader.Read<float>();
			state.Charge = reader.Read<float>();
			state.CastPoint = reader.ReadVec2();
			state.CastAim = reader.ReadVec2();
			state.Stun = reader.Read<float>();
			state.Chill = reader.Read<float>();
			state.Slick = reader.Read<float>();
			state.StunGuard = reader.Read<float>();
			state.Fall = reader.Read<float>();
			state.Ledge = reader.ReadVec2();
			state.Footing = reader.Read<float>();
			state.CastSlot = reader.Read<int8_t>();
			state.HeldButtons = reader.Read<uint8_t>();
			state.RecastSlot = reader.Read<int8_t>();
			state.RecastTimer = reader.Read<float>();
			state.RecastAge = reader.Read<float>();
			state.RecastOrigin = reader.ReadVec2();
			state.RecastAim = reader.ReadVec2();
			state.RecastReach = reader.Read<float>();
			state.Static = reader.Read<float>();
			state.Overload = reader.Read<uint8_t>();
			state.RecastAbility = reader.Read<uint8_t>();
			state.ManaStall = reader.Read<float>();
			state.Momentum = reader.Read<float>();
			state.Tempo = reader.Read<float>();
			state.TempoStacks = reader.Read<uint8_t>();
			state.Snap = reader.Read<float>();
			state.Surge = reader.Read<uint8_t>();
			state.GuardLost = reader.Read<float>();
			state.GuardIdle = reader.Read<float>();
			const uint8_t life = reader.Read<uint8_t>();
			state.Alive = (life & 1u) != 0;
			state.Airborne = (life & 2u) != 0;
			state.GuardUp = (life & 4u) != 0;
			return !reader.Failed() && state.CastSlot >= -1 && state.CastSlot < int8_t(AbilitySlots) && state.RecastSlot >= -1 && state.RecastSlot < int8_t(AbilitySlots) && (state.RecastAbility == UINT8_MAX || state.RecastAbility < uint8_t(AbilityId::Count));
		}
	}

	namespace
	{
		void WriteRules(ByteWriter& writer, const MatchRules& rules)
		{
			writer.Write(rules.Teams);
			writer.Write(rules.ScoreGoal);
			writer.Write(rules.Minutes);
			writer.Write(rules.Rounds);
			writer.Write(rules.Bots);
			writer.Write(rules.BotLevel);
			writer.Write(rules.Mode);
			writer.Write(rules.Health);
			writer.Write(rules.Mana);
			writer.Write(rules.Damage);
			writer.Write(rules.CastSpeed);
			writer.Write(rules.CooldownSpeed);
			writer.Write(rules.MoveSpeed);
			writer.Write(rules.ManaRegen);
			writer.Write(rules.Knockback);
			writer.Write(uint8_t((rules.Apex ? 1u : 0u) | (rules.ApexDraft ? 2u : 0u) | (rules.Crumble ? 4u : 0u) | (rules.WinnerCard ? 8u : 0u) | (rules.RandomSpells ? 16u : 0u)));
			WriteBytes(writer, rules.TeamBots);
			WriteBytes(writer, rules.Seats);
		}

		MatchRules ReadRules(ByteReader& reader)
		{
			MatchRules rules;
			rules.Teams = reader.Read<uint8_t>();
			rules.ScoreGoal = reader.Read<uint8_t>();
			rules.Minutes = reader.Read<uint8_t>();
			rules.Rounds = reader.Read<uint8_t>();
			rules.Bots = reader.Read<uint8_t>();
			rules.BotLevel = reader.Read<uint8_t>();
			rules.Mode = reader.Read<BrawlMode>();
			rules.Health = reader.Read<uint16_t>();
			rules.Mana = reader.Read<uint16_t>();
			rules.Damage = reader.Read<uint16_t>();
			rules.CastSpeed = reader.Read<uint16_t>();
			rules.CooldownSpeed = reader.Read<uint16_t>();
			rules.MoveSpeed = reader.Read<uint16_t>();
			rules.ManaRegen = reader.Read<uint16_t>();
			rules.Knockback = reader.Read<uint16_t>();
			const uint8_t flags = reader.Read<uint8_t>();
			rules.Apex = (flags & 1u) != 0;
			rules.ApexDraft = (flags & 2u) != 0;
			rules.Crumble = (flags & 4u) != 0;
			rules.WinnerCard = (flags & 8u) != 0;
			rules.RandomSpells = (flags & 16u) != 0;
			ReadBytes(reader, rules.TeamBots);
			ReadBytes(reader, rules.Seats);
			return SanitizeRules(rules);
		}

		void WriteSession(ByteWriter& writer, const SessionInfo& session)
		{
			writer.Write(session.Mode);
			writer.Write(session.Map);
			writer.Write(session.MapSeed);
			WriteRules(writer, session.Rules);
			writer.Write(session.Run.StartMinute);
			writer.Write(session.Run.StartLevel);
			writer.Write(session.Run.Spawns);
			writer.Write(session.Run.Xp);
			writer.Write(session.Run.Only);
			writer.Write(uint8_t(session.Run.Immortal ? 1 : 0));
			writer.Write(session.Epoch);
		}

		bool ReadSession(ByteReader& reader, SessionInfo& session)
		{
			session.Mode = reader.Read<GameMode>();
			session.Map = reader.Read<MapKind>();
			session.MapSeed = reader.Read<uint32_t>();
			session.Rules = ReadRules(reader);
			session.Run.StartMinute = reader.Read<uint8_t>();
			session.Run.StartLevel = reader.Read<uint8_t>();
			session.Run.Spawns = reader.Read<uint16_t>();
			session.Run.Xp = reader.Read<uint16_t>();
			session.Run.Only = reader.Read<uint8_t>();
			session.Run.Immortal = reader.Read<uint8_t>() != 0;
			session.Run = SanitizeRunRules(session.Run);
			session.Epoch = reader.Read<uint32_t>();
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
		writer.Write(message.Honors);
		writer.Write(PackCosmetics(message.Style));
		WriteBytes(writer, message.Spells);
	}

	bool Read(ByteReader& reader, ClientHello& message)
	{
		message.Version = reader.Read<uint32_t>();
		if (reader.Failed() || message.Version != ProtocolVersion)
			return !reader.Failed();
		message.Name = reader.ReadString();
		message.Honors = reader.Read<uint8_t>();
		message.Style = UnpackCosmetics(reader.Read<uint32_t>());
		ReadBytes(reader, message.Spells);
		return !reader.Failed() && std::all_of(message.Spells.begin(), message.Spells.end(), [](AbilityId id) { return id < AbilityId::Count; });
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
		writer.Write(message.Acked);
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
		message.Acked = reader.Read<uint32_t>();
		return !reader.Failed();
	}

	void Write(ByteWriter& writer, const ClientAction& message)
	{
		writer.Write(message.Action);
		writer.Write(message.Value);
		writer.Write(message.Target);
		writer.Write(message.Extra);
		if (message.Action == ActionType::StartMatch)
			WriteRules(writer, message.Rules);
	}

	bool Read(ByteReader& reader, ClientAction& message)
	{
		message.Action = reader.Read<ActionType>();
		message.Value = reader.Read<uint16_t>();
		message.Target = reader.Read<uint16_t>();
		message.Extra = reader.Read<uint32_t>();
		if (message.Action == ActionType::StartMatch)
			message.Rules = ReadRules(reader);
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

	void Write(ByteWriter& writer, const ServerAchievement& message)
	{
		writer.Write(message.Id);
		writer.Write(message.Detail);
	}

	bool Read(ByteReader& reader, ServerAchievement& message)
	{
		message.Id = reader.Read<Achievement>();
		message.Detail = reader.Read<uint32_t>();
		return !reader.Failed() && message.Id < Achievement::Count;
	}

	void Write(ByteWriter& writer, const ServerChess& message)
	{
		writer.Write(message.Revision);
		writer.Write(message.Seats[0]);
		writer.Write(message.Seats[1]);
		writer.Write(message.Result);
		writer.Write(message.Winner);
		writer.Write(message.DrawOffer);
		const size_t count = std::min(message.Moves.size(), Chess::MoveLimit);
		writer.Write(uint16_t(count));
		for (size_t i = 0; i < count; i++)
			writer.Write(message.Moves[i]);
	}

	bool Read(ByteReader& reader, ServerChess& message)
	{
		message.Revision = reader.Read<uint32_t>();
		message.Seats[0] = reader.Read<uint8_t>();
		message.Seats[1] = reader.Read<uint8_t>();
		message.Result = reader.Read<Chess::Verdict>();
		message.Winner = reader.Read<uint8_t>();
		message.DrawOffer = reader.Read<uint8_t>();
		const uint16_t count = reader.Read<uint16_t>();
		if (count > Chess::MoveLimit || message.Result > Chess::Verdict::MoveLimit)
			return false;
		message.Moves.resize(count);
		for (uint16_t& move : message.Moves)
			move = reader.Read<uint16_t>();
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
			writer.Write(player.Assists);
			writer.Write(player.Damage);
			writer.Write(player.Ping);
			writer.Write(PackCosmetics(player.Look.Style));
			writer.Write(player.Team);
			writer.Write(player.Placement);
			writer.Write(uint8_t((player.Host ? 1 : 0) | (player.Choosing ? 2 : 0) | (player.Bot ? 4 : 0) | (player.Spectator ? 8 : 0)));
			writer.Write(player.Score);
			writer.Write(player.Rounds);
			writer.Write(player.Honors);
			writer.Write(player.Cards);
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
			player.Assists = reader.Read<uint16_t>();
			player.Damage = reader.Read<uint32_t>();
			player.Ping = reader.Read<int32_t>();
			player.Look.Style = UnpackCosmetics(reader.Read<uint32_t>());
			player.Team = reader.Read<uint8_t>();
			player.Placement = reader.Read<uint8_t>();
			const uint8_t flags = reader.Read<uint8_t>();
			player.Host = (flags & 1u) != 0;
			player.Choosing = (flags & 2u) != 0;
			player.Bot = (flags & 4u) != 0;
			player.Spectator = (flags & 8u) != 0;
			player.Score = reader.Read<uint16_t>();
			player.Rounds = reader.Read<uint8_t>();
			player.Honors = reader.Read<uint8_t>();
			player.Cards = reader.Read<CardMask>();
		}
		return !reader.Failed();
	}

	void WriteSnapshotBlock(ByteWriter& writer, const ServerSnapshot& message)
	{
		const GameStatus& status = message.Status;
		writer.Write(status.Mode);
		writer.Write(status.Map);
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
		if (status.Mode == GameMode::Brawl)
			WriteBrawlStatus(writer, status.Brawl);
		if (status.Mode == GameMode::Hub)
			writer.Write(status.HubFalls);
		if (status.Mode == GameMode::Shardfall)
		{
			const RunStatus& run = status.Run;
			writer.Write(run.Phase);
			writer.Write(run.Act);
			writer.Write(run.ActClock);
			writer.Write(run.RunClock);
			writer.Write(run.Alive);
			writer.Write(run.Goal);
			writer.Write(run.Level);
			writer.Write(run.Xp);
			writer.Write(run.XpGoal);
		}

		const LocalPlayerState& local = message.LocalPlayer;
		writer.Write(uint8_t(local.Valid ? 1 : 0));
		if (local.Valid)
		{
			writer.Write(local.Motion.Position);
			writer.Write(local.Motion.Facing);
			WriteState(writer, local.State);
			WriteTraits(writer, local.Traits);
			writer.Write(local.Health);
			writer.Write(local.MaxHealth);
			writer.Write(local.RespawnTime);
			writer.Write(QuantizeOwner(local.Watching));
			writer.Write(QuantizeOwner(local.KilledBy));
			if (status.Mode == GameMode::Brawl)
			{
				WriteLoadout(writer, local.Brawl);
				writer.Write(local.Shield);
			}
			if (status.Mode == GameMode::Hub)
				writer.Write(local.Starlit);
		}
	}

	bool ReadSnapshotBlock(ByteReader& reader, ServerSnapshot& message)
	{
		GameStatus& status = message.Status;
		status.Mode = reader.Read<GameMode>();
		status.Map = reader.Read<MapKind>();
		if (status.Mode >= GameMode::Count || status.Map >= MapKind::Count)
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
		if (status.Mode == GameMode::Hub)
			status.HubFalls = reader.Read<uint16_t>();
		if (status.Mode == GameMode::Brawl && !ReadBrawlStatus(reader, status.Brawl))
			return false;
		if (status.Mode == GameMode::Shardfall)
		{
			RunStatus& run = status.Run;
			run.Phase = reader.Read<RunPhase>();
			run.Act = reader.Read<uint8_t>();
			run.ActClock = reader.Read<float>();
			run.RunClock = reader.Read<float>();
			run.Alive = reader.Read<uint16_t>();
			run.Goal = reader.Read<uint16_t>();
			run.Level = reader.Read<uint16_t>();
			run.Xp = reader.Read<uint32_t>();
			run.XpGoal = reader.Read<uint32_t>();
			if (uint8_t(run.Phase) > uint8_t(RunPhase::Travel))
				return false;
		}

		LocalPlayerState& local = message.LocalPlayer;
		local.Valid = reader.Read<uint8_t>() != 0;
		if (local.Valid)
		{
			local.Motion.Position = reader.ReadVec2();
			local.Motion.Facing = reader.Read<float>();
			if (!ReadState(reader, local.State))
				return false;
			if (!ReadTraits(reader, local.Traits))
				return false;
			local.Health = reader.Read<int32_t>();
			local.MaxHealth = reader.Read<int32_t>();
			local.RespawnTime = reader.Read<float>();
			local.Watching = DequantizeOwner(reader.Read<uint8_t>());
			local.KilledBy = DequantizeOwner(reader.Read<uint8_t>());
			if (status.Mode == GameMode::Hub)
				local.Starlit = reader.Read<uint16_t>();
			if (status.Mode == GameMode::Brawl)
			{
				if (!ReadLoadout(reader, local.Brawl))
					return false;
				local.Shield = reader.Read<uint8_t>();
			}
		}
		return !reader.Failed();
	}

}
