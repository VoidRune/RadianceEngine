#include "GameClient.h"
#include "GameShared/Content/PropLibrary.h"
#include "GameShared/Simulation/PlayerSimulation.h"
#include "GameShared/World/Collision.h"
#include <RadianceEngine/Core/Log.h>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr size_t MaxSnapshots = 64;
		constexpr size_t MaxPendingInputs = 256;
		constexpr float MaxTicksPerUpdate = 8.0f;
		constexpr float CorrectionHalfLife = 0.06f;
		constexpr float TeleportDistance = 3.0f;
		constexpr float WarpDistance = 6.0f;
		constexpr float CorrectionThreshold = 1e-4f;
		constexpr double MinDelayMargin = 0.2;
		constexpr double StartDelayMargin = 1.5;
		constexpr double DelayGrowStep = 0.5;
		constexpr float DelayGrowCooldown = 0.5f;
		constexpr float DelayStableSeconds = 3.0f;
		constexpr double DelayRelaxRate = 0.5;
		constexpr double MaxExtrapolationTicks = 2.0;
		constexpr uint32_t MaxBodyLeadTicks = 8;
		constexpr double MaxProjectileLeadSeconds = 0.35;
		constexpr uint32_t PredictedEffectNetIdBase = GameClient::PredictedNetIdBase + 0x100000u;
		constexpr uint32_t PredictedImpactNetIdBase = GameClient::PredictedNetIdBase + 0x200000u;
	}

	bool GameClient::Connect(const std::string& address, const CharacterProfile& profile)
	{
		Reset({});
		m_Profile = profile;
		if (!m_Net.Connect(address, DefaultPort))
		{
			m_DisconnectReason = "\"" + address + "\" is not a valid address";
			return false;
		}
		m_State = ClientState::Connecting;
		return true;
	}

	void GameClient::ConnectLocal(ConnectionId connection, const CharacterProfile& profile)
	{
		Reset({});
		m_Profile = profile;
		m_Local = true;
		m_State = ClientState::Connecting;
		m_Net.ConnectLocal(connection);
	}

	void GameClient::Disconnect()
	{
		m_Net.Disconnect("Left the game");
		Reset("Left the game");
	}

	void GameClient::Reset(std::string reason)
	{
		m_State = ClientState::Disconnected;
		m_Local = false;
		m_TickLocked = false;
		m_HasViewAim = false;
		m_DisconnectReason = std::move(reason);
		m_Welcome = {};
		m_Session = {};
		m_Notices.clear();
		m_Kills.clear();
		m_Level.reset();
		m_Players.clear();
		m_LocalPlayer = {};
		m_Inventory = {};
		m_HasInventory = false;
		m_StarvedFrames = 0;
		m_Accumulator = 0.0f;
		m_InputSequence = 0;
		m_Corrections = 0;
		m_LastCorrection = 0.0f;
		ResetView();
	}

	void GameClient::ResetView()
	{
		m_Status = {};
		m_PendingHits.clear();
		m_NewHits.clear();
		m_Snapshots.clear();
		m_Entities.clear();
		m_View.clear();
		m_ClockStarted = false;
		m_RenderTick = 0.0;
		m_PresentTick = 0.0;
		m_StableTime = 0.0f;
		m_GrowCooldown = 0.0f;
		m_Starved = false;
		m_SnapshotAge = 0.0f;
		m_PendingInputs.clear();
		m_HasPrediction = false;
		m_PredictedMotion = {};
		m_PreviousMotion = {};
		m_PredictedState = {};
		m_CorrectionOffset = {};
		m_PredictedProjectiles.clear();
		m_PredictedEffects.clear();
		m_PredictedTagCount = 0;
	}

	void GameClient::OnTravel(const ServerTravel& travel)
	{
		ResetView();
		m_Welcome.PlayerNetId = travel.PlayerNetId;
		m_Session = travel.Session;
		m_Status.Mode = m_Session.Mode;
		m_Status.Map = m_Session.Map;
		m_Status.Tier = m_Session.Tier;
		m_Status.Modifiers = m_Session.Modifiers;
		m_Level = std::make_unique<Level>(BuildLevel(m_Session.Map, m_Session.MapSeed));
		m_InterpolationDelay = double(m_Welcome.SnapshotInterval) + StartDelayMargin;
		m_Travels++;
		RDN_LOG("Travelled to {} (map seed {})", GetMapDef(m_Session.Map).Name, m_Session.MapSeed);
	}

	void GameClient::Update(float dt, const InputSampler& sampleInput)
	{
		Predict(dt, sampleInput);
		Receive(dt);
	}

	void GameClient::Predict(float dt, const InputSampler& sampleInput)
	{
		if (m_State != ClientState::InGame)
			return;
		const float step = GetTickSeconds();
		m_Accumulator = std::min(m_Accumulator + dt, step * MaxTicksPerUpdate);
		while (m_Accumulator >= step)
		{
			m_Accumulator -= step;
			PredictTick(sampleInput ? sampleInput() : PlayerInput{});
		}
	}

	void GameClient::Receive(float dt)
	{
		m_Events.clear();
		m_Messages.clear();
		m_NewHits.clear();
		m_Net.Poll(m_Events, m_Messages);
		for (const NetEvent& event : m_Events)
			HandleEvent(event);
		for (const NetMessage& message : m_Messages)
		{
			HandleMessage(message);
			if (m_State == ClientState::Disconnected)
				return;
		}
		if (m_State != ClientState::InGame)
			return;

		m_SnapshotAge += dt;
		m_CorrectionOffset *= std::exp2(-dt / CorrectionHalfLife);
		AdvanceClock(dt);
		ReleaseHits();
		Interpolate();
		ComposeView(m_Accumulator / GetTickSeconds());
	}

	void GameClient::EquipItem(uint32_t packSlot)
	{
		SendAction(ActionType::Equip, packSlot);
	}

	void GameClient::EquipItemTo(uint32_t packSlot, GearSlot slot)
	{
		SendAction(ActionType::EquipTo, packSlot, uint32_t(slot));
	}

	void GameClient::UnequipItem(GearSlot slot)
	{
		SendAction(ActionType::Unequip, uint32_t(slot));
	}

	void GameClient::DropItem(uint32_t packSlot)
	{
		SendAction(ActionType::Drop, packSlot);
	}

	void GameClient::SlotGem(uint32_t packSlot, uint32_t gemSlot)
	{
		SendAction(ActionType::SlotGem, packSlot, gemSlot);
	}

	void GameClient::UnslotGem(uint32_t gemSlot)
	{
		SendAction(ActionType::UnslotGem, gemSlot);
	}

	void GameClient::ChooseUpgrade(uint32_t choice)
	{
		SendAction(ActionType::ChooseUpgrade, choice);
	}

	void GameClient::DepositItem(uint32_t packSlot, uint32_t stashSlot)
	{
		SendAction(ActionType::StashDeposit, packSlot, stashSlot);
	}

	void GameClient::WithdrawItem(uint32_t stashSlot, uint32_t packSlot)
	{
		SendAction(ActionType::StashWithdraw, stashSlot, packSlot);
	}

	void GameClient::SalvageItem(ItemRef item)
	{
		SendAction(ActionType::Salvage, EncodeItemRef(item));
	}

	void GameClient::ForgeItem(ItemRef item, ForgeAction action, uint32_t affix, StatCategory category)
	{
		SendAction(ActionType::Forge, uint32_t(action), EncodeItemRef(item), (affix & 0xFFu) | (uint32_t(category) << 8));
	}

	void GameClient::CraftGem(ItemRef gem, GemAction action)
	{
		SendAction(ActionType::GemCraft, uint32_t(action), EncodeItemRef(gem));
	}

	void GameClient::StartMission(MapKind map, uint8_t tier, ModifierMask modifiers)
	{
		SendAction(ActionType::StartMission, uint32_t(map), tier | (uint32_t(GameMode::Mission) << 8), modifiers);
	}

	void GameClient::StartHorde(MapKind map)
	{
		SendAction(ActionType::StartMission, uint32_t(map), 1u | (uint32_t(GameMode::Horde) << 8));
	}

	void GameClient::StartArena(MapKind map, const MatchRules& rules)
	{
		SendAction(ActionType::StartMatch, uint32_t(GameMode::Arena), uint32_t(map), EncodeRules(rules));
	}

	void GameClient::StartRoyale(MapKind map)
	{
		SendAction(ActionType::StartMatch, uint32_t(GameMode::Royale), uint32_t(map));
	}

	void GameClient::StartBrawl(MapKind map, const MatchRules& rules)
	{
		SendAction(ActionType::StartMatch, uint32_t(GameMode::Brawl), uint32_t(map), EncodeRules(rules));
	}

	void GameClient::PickBrawlSpell(uint32_t slot, AbilityId spell)
	{
		SendAction(ActionType::BrawlSpell, slot, uint32_t(spell));
	}

	void GameClient::ChooseBoon(uint32_t choice)
	{
		SendAction(ActionType::BrawlBoon, choice);
	}

	void GameClient::SetBrawlReady(bool ready)
	{
		SendAction(ActionType::BrawlReady, ready ? 1u : 0u);
	}

	void GameClient::Spectate(int32_t direction)
	{
		SendAction(ActionType::Spectate, uint16_t(int16_t(direction)));
	}

	uint8_t GameClient::GetTeam(uint32_t playerId) const
	{
		const PlayerInfo* player = FindPlayer(playerId);
		return player ? player->Team : NoTeam;
	}

	bool GameClient::IsHostile(const EntityState& entity) const
	{
		if (!IsPvpMode(m_Session.Mode) || entity.Kind != EntityKind::Player || entity.Owner == m_Welcome.PlayerId || entity.Has(EntityFlags::Dead))
			return false;
		if (m_Session.Mode == GameMode::Brawl)
		{
			const BrawlPhase phase = m_Status.Brawl.Phase;
			return m_Status.Match != MatchPhase::Ended && (phase == BrawlPhase::Draft || phase == BrawlPhase::Shop || phase == BrawlPhase::Fight);
		}
		if (m_Status.Match != MatchPhase::Fight)
			return false;
		const uint8_t team = GetTeam(m_Welcome.PlayerId);
		return !m_Session.Rules.Teams || team == NoTeam || team != GetTeam(entity.Owner);
	}

	void GameClient::ReturnToHub()
	{
		SendAction(ActionType::ReturnToHub, 0);
	}

	void GameClient::OpenChest()
	{
		SendAction(ActionType::OpenChest, 0);
	}

	void GameClient::UseGateway()
	{
		SendAction(ActionType::UseGateway, 0);
	}

	void GameClient::SendAction(ActionType action, uint32_t value, uint32_t target, uint32_t extra)
	{
		if (m_State == ClientState::InGame)
			Send(ClientAction{ .Action = action, .Value = uint16_t(std::min(value, 0xFFFFu)), .Target = uint16_t(std::min(target, 0xFFFFu)), .Extra = extra }, SendMode::Reliable);
	}

	void GameClient::ReleaseHits()
	{
		while (!m_PendingHits.empty() && double(m_PendingHits.front().Tick) <= m_RenderTick + 1.0)
		{
			m_NewHits.push_back(m_PendingHits.front().Hit);
			m_PendingHits.pop_front();
		}
		while (m_PendingHits.size() > 512)
			m_PendingHits.pop_front();
	}

	const EntityState* GameClient::FindEntity(uint32_t netId) const
	{
		const auto it = std::find_if(m_View.begin(), m_View.end(), [&](const EntityState& entity) { return entity.NetId == netId; });
		return it != m_View.end() ? &*it : nullptr;
	}

	const PlayerInfo* GameClient::FindPlayer(uint32_t playerId) const
	{
		const auto it = std::find_if(m_Players.begin(), m_Players.end(), [&](const PlayerInfo& player) { return player.PlayerId == playerId; });
		return it != m_Players.end() ? &*it : nullptr;
	}

	void GameClient::HandleEvent(const NetEvent& event)
	{
		if (event.Type == NetEventType::Connected)
		{
			m_State = ClientState::Joining;
			Send(ClientHello{ .Name = m_Profile.Name, .Kit = uint8_t(m_Profile.Kit), .HasGear = true, .Gear = m_Profile.Gear }, SendMode::Reliable);
			return;
		}
		std::string reason = m_DisconnectReason.empty() ? event.Reason : m_DisconnectReason;
		if (m_DisconnectReason.empty() && event.TimedOut)
			reason = m_State == ClientState::InGame ? "Lost the connection to the server" : "The server didn't answer. Check the address, and that the host's UDP port is open";
		Reset(reason);
	}

	void GameClient::HandleMessage(const NetMessage& message)
	{
		const std::optional<MessageType> type = PeekType(message.Data);
		if (!type)
			return;
		switch (*type)
		{
		case MessageType::ServerWelcome:
			if (ServerWelcome welcome; m_State == ClientState::Joining && Decode(message.Data, welcome))
			{
				if (welcome.PropHash != GetPropLibraryHash())
				{
					const std::string reason = "The host's prop definitions (res/Data/Props.json) don't match yours";
					m_Net.Disconnect(reason.c_str());
					Reset(reason);
					return;
				}
				m_Welcome = welcome;
				m_Session = welcome.Session;
				m_Status.Mode = m_Session.Mode;
				m_Status.Map = m_Session.Map;
				m_Status.Tier = m_Session.Tier;
				m_Status.Modifiers = m_Session.Modifiers;
				m_Level = std::make_unique<Level>(BuildLevel(m_Session.Map, m_Session.MapSeed));
				m_LocalPlayer.Traits = ComputeTraits(m_Profile.Gear.Gear, m_Profile.Gear.Gems, {}, {});
				m_InterpolationDelay = double(welcome.SnapshotInterval) + StartDelayMargin;
				m_State = ClientState::InGame;
				RDN_LOG("Joined as player {} in {} ({} Hz, a snapshot every {} ticks, map seed {})", welcome.PlayerId + 1, GetMapDef(m_Session.Map).Name, welcome.TickRate, welcome.SnapshotInterval, m_Session.MapSeed);
			}
			break;
		case MessageType::ServerTravel:
			if (ServerTravel travel; m_State == ClientState::InGame && Decode(message.Data, travel))
				OnTravel(travel);
			break;
		case MessageType::ServerStash:
			if (ServerStash stash; m_State == ClientState::InGame && Decode(message.Data, stash))
			{
				m_Profile.Gear.Stash = stash.Stash;
				m_StashVersion++;
			}
			break;
		case MessageType::ServerNotice:
			if (ServerNotice notice; m_State == ClientState::InGame && Decode(message.Data, notice))
			{
				m_Notices.push_back(std::move(notice));
				if (m_Notices.size() > 16)
					m_Notices.erase(m_Notices.begin());
			}
			break;
		case MessageType::ServerKillFeed:
			if (ServerKillFeed feed; m_State == ClientState::InGame && Decode(message.Data, feed))
			{
				m_Kills.insert(m_Kills.end(), feed.Kills.begin(), feed.Kills.end());
				if (m_Kills.size() > 32)
					m_Kills.erase(m_Kills.begin(), m_Kills.end() - 32);
			}
			break;
		case MessageType::ServerReject:
			if (ServerReject reject; Decode(message.Data, reject))
			{
				RDN_LOG_WARNING("The server rejected us: {}", reject.Reason);
				m_Net.Disconnect(reject.Reason.c_str());
				Reset(reject.Reason);
			}
			break;
		case MessageType::ServerPlayerList:
			if (ServerPlayerList list; Decode(message.Data, list))
				m_Players = std::move(list.Players);
			break;
		case MessageType::ServerSnapshot:
			if (ServerSnapshot snapshot; m_State == ClientState::InGame && Decode(message.Data, snapshot))
				OnSnapshot(std::move(snapshot));
			break;
		case MessageType::ServerInventory:
			if (ServerInventory inventory; m_State == ClientState::InGame && Decode(message.Data, inventory))
			{
				m_Inventory = inventory;
				m_HasInventory = true;
				if (m_Session.Mode == GameMode::Royale)
					break;
				m_InventoryVersion++;
				m_Profile.Gear.Gear = inventory.Gear;
				m_Profile.Gear.Gems = inventory.Gems;
				m_Profile.Gear.Pack = inventory.Pack;
			}
			break;
		default:
			break;
		}
	}

	void GameClient::OnSnapshot(ServerSnapshot&& snapshot)
	{
		if (snapshot.Epoch != m_Session.Epoch || (!m_Snapshots.empty() && snapshot.Tick <= m_Snapshots.back().Tick))
			return;
		if (snapshot.LocalPlayer.Valid && m_Session.Mode != GameMode::Royale)
			m_Profile.Gear.Materials = snapshot.LocalPlayer.Materials;
		m_SnapshotAge = 0.0f;
		std::sort(snapshot.Entities.begin(), snapshot.Entities.end(), [](const EntityState& a, const EntityState& b) { return a.NetId < b.NetId; });
		for (const HitEvent& hit : snapshot.Hits)
			m_PendingHits.push_back({ snapshot.Tick, hit });
		m_Status = snapshot.Status;
		m_Snapshots.push_back(std::move(snapshot));
		while (m_Snapshots.size() > MaxSnapshots)
			m_Snapshots.pop_front();
		Reconcile(m_Snapshots.back());
	}

	void GameClient::Reconcile(const ServerSnapshot& snapshot)
	{
		const PlayerTraits knownTraits = m_LocalPlayer.Traits;
		m_LocalPlayer = snapshot.LocalPlayer;
		if (!m_LocalPlayer.HasTraits)
			m_LocalPlayer.Traits = knownTraits;
		while (!m_PendingInputs.empty() && m_PendingInputs.front().Sequence <= snapshot.LastProcessedInput)
			m_PendingInputs.pop_front();
		if (!m_LocalPlayer.Valid)
		{
			m_HasPrediction = false;
			return;
		}

		const PlayerTraits& traits = GetTraits();
		Transform motion = m_LocalPlayer.Motion;
		PlayerState state = m_LocalPlayer.State;
		uint32_t ahead = 0;
		for (const PlayerInput& input : m_PendingInputs)
		{
			CollectBodies(ahead++, m_Bodies);
			SimulatePlayer(*m_Level, motion, state, traits, input, GetTickSeconds(), m_Bodies);
		}

		const Vec2 error = m_PredictedMotion.Position - motion.Position;
		const float errorLength = Length(error);
		if (!m_HasPrediction || errorLength > TeleportDistance)
		{
			m_CorrectionOffset = {};
			m_PreviousMotion = motion;
		}
		else if (errorLength > CorrectionThreshold)
		{
			m_CorrectionOffset += error;
			m_PreviousMotion.Position -= error;
			m_Corrections++;
			m_LastCorrection = errorLength;
		}
		m_PredictedMotion = motion;
		m_PredictedState = state;
		m_HasPrediction = true;
	}

	void GameClient::PredictTick(PlayerInput input)
	{
		input.Sequence = ++m_InputSequence;
		input.AimDistance = QuantizeAimDistance(input.AimDistance);
		const double view = std::max(m_RenderTick, 0.0);
		input.ViewTick = uint32_t(view);
		input.ViewFraction = uint8_t(std::min(255.0, (view - std::floor(view)) * 256.0));
		m_PendingInputs.push_back(input);
		while (m_PendingInputs.size() > MaxPendingInputs)
			m_PendingInputs.pop_front();
		const size_t redundancy = std::min<size_t>(m_PendingInputs.size(), InputRedundancy);
		Send(ClientInput{ .Inputs = { m_PendingInputs.end() - ptrdiff_t(redundancy), m_PendingInputs.end() } }, SendMode::Unreliable);

		const float dt = GetTickSeconds();
		m_PreviousMotion = m_PredictedMotion;
		if (m_HasPrediction)
		{
			const PlayerTraits& traits = GetTraits();
			CollectBodies(uint32_t(m_PendingInputs.size() - 1), m_Bodies);
			const PlayerActions actions = SimulatePlayer(*m_Level, m_PredictedMotion, m_PredictedState, traits, input, dt, m_Bodies);
			if (actions.Teleported())
			{
				m_PreviousMotion = m_PredictedMotion;
				m_Teleports++;
			}
			const uint16_t tag = uint16_t(input.Sequence);
			for (uint32_t slot = 0; slot < AbilitySlots; slot++)
				if (actions.HasReleased(slot))
					PredictAbility(traits.Skills[slot], tag, actions, traits);
		}
		StepPredictedEffects(dt);
	}

	void GameClient::PredictAbility(const SkillTraits& skill, uint16_t tag, const PlayerActions& actions, const PlayerTraits& traits)
	{
		const AbilityDef& ability = GetAbility(skill.Ability);
		const Vec2 origin = m_PredictedMotion.Position;
		const uint8_t scale = uint8_t(std::clamp(std::lround(traits.AreaScale * skill.AreaScale * 16.0f), 1l, 255l));
		switch (ability.Kind)
		{
		case AbilityKind::Projectile:
		{
			const ProjectilePattern pattern = GetProjectilePattern(ability, uint8_t(traits.ExtraProjectiles + skill.ExtraProjectiles), traits.ExtraPierce, traits.ProjectileSpeedScale);
			for (uint32_t i = 0; i < pattern.Count && i < 16; i++)
			{
				const Vec2 heading = ProjectileDirection(pattern, actions.Direction, i);
				const Vec2 start = origin + heading * ability.SpawnOffset;
				if (CastSegment(*m_Level, origin, start, Block::Projectiles) < 1.0f)
					continue;
				m_PredictedProjectiles.push_back({
					.Tag = tag,
					.Index = uint8_t(i),
					.Ability = ability.Id,
					.Position = start,
					.Previous = start,
					.Velocity = heading * pattern.Speed,
					.Lifetime = GetProjectileLifetime(ability, pattern.Speed),
					.Radius = ability.ProjectileRadius,
					.Pierce = pattern.Pierce,
					.Explodes = ability.ImpactRadius > 0.0f,
				});
			}
			break;
		}
		case AbilityKind::Ground:
		{
			const Vec2 target = GetGroundTarget(*m_Level, origin, actions.Direction, actions.AimDistance, ability.Range);
			m_PredictedEffects.push_back({ .Tag = tag, .Ability = ability.Id, .Position = target, .Facing = AngleOf(actions.Direction), .Remaining = ability.ImpactDelay, .Scale = scale, .Pending = true });
			break;
		}
		case AbilityKind::Melee:
		case AbilityKind::Nova:
		case AbilityKind::Heal:
		case AbilityKind::Dash:
			m_PredictedEffects.push_back({ .Tag = tag, .Ability = ability.Id, .Position = origin, .Facing = AngleOf(actions.Direction), .Remaining = ability.EffectDuration, .Scale = scale });
			break;
		}
		RememberTag(tag);
	}

	void GameClient::StepPredictedEffects(float dt)
	{
		const uint8_t areaScale = uint8_t(std::clamp(std::lround(GetTraits().AreaScale * 16.0f), 1l, 255l));
		for (PredictedProjectile& projectile : m_PredictedProjectiles)
		{
			projectile.Previous = projectile.Position;
			const Vec2 end = projectile.Position + projectile.Velocity * dt;
			const float travel = CastSegment(*m_Level, projectile.Position, end, Block::Projectiles);
			projectile.Position = Lerp(projectile.Position, end, travel);
			const bool clear = travel >= 1.0f;
			projectile.Lifetime -= dt;
			if (!clear)
				projectile.Lifetime = 0.0f;
			for (const EntityState& entity : m_Entities)
			{
				const EnemyDef* def = FindEnemyDef(entity.Kind);
				if (!def && (!IsHostile(entity) || entity.Has(EntityFlags::Dashing)))
					continue;
				const float radius = def ? def->Radius * entity.GetScale() : PlayerRadius;
				float parameter = 0.0f;
				if (entity.NetId == projectile.LastHit || !SegmentTouchesCircle(projectile.Previous, projectile.Position, entity.Position, radius + projectile.Radius, parameter))
					continue;
				if (entity.Has(EntityFlags::Warded))
				{
					projectile.Position = Lerp(projectile.Previous, projectile.Position, parameter);
					projectile.Lifetime = 0.0f;
					projectile.Explodes = false;
					break;
				}
				if (projectile.Explodes)
				{
					projectile.Position = Lerp(projectile.Previous, projectile.Position, parameter);
					projectile.Lifetime = 0.0f;
					break;
				}
				if (projectile.Pierce == 0)
				{
					projectile.Lifetime = 0.0f;
					break;
				}
				projectile.Pierce--;
				projectile.LastHit = entity.NetId;
			}
			if (projectile.Lifetime <= 0.0f && projectile.Explodes)
				m_PredictedEffects.push_back({ .Tag = projectile.Tag, .Index = projectile.Index, .Ability = projectile.Ability, .Position = projectile.Position, .Remaining = GetAbility(projectile.Ability).EffectDuration, .Scale = areaScale });
		}
		std::erase_if(m_PredictedProjectiles, [](const PredictedProjectile& projectile) { return projectile.Lifetime <= 0.0f; });
		for (PredictedEffect& effect : m_PredictedEffects)
		{
			effect.Remaining -= dt;
			if (effect.Pending && effect.Remaining <= 0.0f)
			{
				effect.Pending = false;
				effect.Remaining = GetAbility(effect.Ability).EffectDuration;
			}
		}
		std::erase_if(m_PredictedEffects, [](const PredictedEffect& effect) { return effect.Remaining <= 0.0f; });
	}

	void GameClient::RememberTag(uint16_t tag)
	{
		const size_t count = std::min<size_t>(m_PredictedTagCount, m_PredictedTags.size());
		if (count > 0 && m_PredictedTags[(m_PredictedTagCount - 1) % m_PredictedTags.size()] == tag)
			return;
		m_PredictedTags[m_PredictedTagCount++ % m_PredictedTags.size()] = tag;
	}

	bool GameClient::IsPredictedTag(uint16_t tag) const
	{
		const size_t count = std::min<size_t>(m_PredictedTagCount, m_PredictedTags.size());
		return tag != 0 && std::find(m_PredictedTags.begin(), m_PredictedTags.begin() + count, tag) != m_PredictedTags.begin() + count;
	}

	void GameClient::CollectBodies(uint32_t ticksAhead, std::vector<BodyCircle>& bodies) const
	{
		bodies.clear();
		if (m_Snapshots.empty())
			return;
		const ServerSnapshot& latest = m_Snapshots.back();
		const ServerSnapshot* previous = m_Snapshots.size() >= 2 ? &m_Snapshots[m_Snapshots.size() - 2] : nullptr;
		const float lead = previous ? float(std::min(ticksAhead, MaxBodyLeadTicks)) / float(latest.Tick - previous->Tick) : 0.0f;
		for (const EntityState& entity : latest.Entities)
		{
			const EnemyDef* def = FindEnemyDef(entity.Kind);
			if (!def || entity.Health == 0 || (def->Boss && entity.Variant != 0))
				continue;
			Vec2 position = entity.Position;
			if (lead > 0.0f)
			{
				const auto before = std::lower_bound(previous->Entities.begin(), previous->Entities.end(), entity.NetId, [](const EntityState& state, uint32_t netId) { return state.NetId < netId; });
				if (before != previous->Entities.end() && before->NetId == entity.NetId && LengthSquared(entity.Position - before->Position) < WarpDistance * WarpDistance)
					position += (entity.Position - before->Position) * lead;
			}
			bodies.push_back({ position, def->Radius * (entity.Has(EntityFlags::Elite) ? EliteSize : 1.0f) });
		}
	}

	void GameClient::AdvanceClock(float dt)
	{
		if (m_Snapshots.empty())
			return;
		if (m_TickLocked && m_Welcome.SnapshotInterval == 1)
		{
			m_RenderTick = std::max(double(m_Snapshots.back().Tick) - 1.0 + double(m_Accumulator / GetTickSeconds()), double(m_Snapshots.front().Tick));
			m_InterpolationDelay = double(m_Snapshots.back().Tick) - m_RenderTick;
			m_ClockStarted = true;
			return;
		}
		const double interval = double(m_Welcome.SnapshotInterval);
		m_GrowCooldown = std::max(m_GrowCooldown - dt, 0.0f);
		if (m_Starved)
		{
			m_StableTime = 0.0f;
			if (m_GrowCooldown <= 0.0f)
			{
				m_InterpolationDelay = std::min(m_InterpolationDelay + DelayGrowStep, 3.0 * interval + 6.0);
				m_GrowCooldown = DelayGrowCooldown;
			}
		}
		else if ((m_StableTime += dt) > DelayStableSeconds)
			m_InterpolationDelay = std::max(interval + MinDelayMargin, m_InterpolationDelay - double(dt) * DelayRelaxRate);

		const double target = double(m_Snapshots.back().Tick) - m_InterpolationDelay;
		if (!m_ClockStarted || std::abs(target - m_RenderTick) > std::max(8.0 * interval, 12.0))
		{
			m_RenderTick = target;
			m_ClockStarted = true;
			return;
		}
		m_RenderTick += dt * double(m_Welcome.TickRate);
		m_RenderTick += (target - m_RenderTick) * std::min(1.0, double(dt) * 2.0);
	}

	void GameClient::Interpolate()
	{
		m_Entities.clear();
		m_Starved = false;
		if (m_Snapshots.empty())
			return;
		if (m_RenderTick <= double(m_Snapshots.front().Tick))
		{
			m_Entities = m_Snapshots.front().Entities;
			return;
		}
		const bool starved = m_RenderTick >= double(m_Snapshots.back().Tick);
		if (starved)
		{
			m_Starved = true;
			m_StarvedFrames++;
			if (m_Snapshots.size() < 2)
			{
				m_Entities = m_Snapshots.back().Entities;
				return;
			}
		}

		size_t next = starved ? m_Snapshots.size() - 1 : 1;
		while (double(m_Snapshots[next].Tick) <= m_RenderTick && !starved)
			next++;
		const ServerSnapshot& from = m_Snapshots[next - 1];
		const ServerSnapshot& to = m_Snapshots[next];
		const double renderTick = std::min(m_RenderTick, double(to.Tick) + MaxExtrapolationTicks);
		const float alpha = float((renderTick - double(from.Tick)) / double(to.Tick - from.Tick));

		size_t i = 0;
		for (const EntityState& target : to.Entities)
		{
			while (i < from.Entities.size() && from.Entities[i].NetId < target.NetId)
				i++;
			EntityState entity = target;
			if (i < from.Entities.size() && from.Entities[i].NetId == target.NetId && LengthSquared(target.Position - from.Entities[i].Position) < WarpDistance * WarpDistance)
			{
				entity.Position = Lerp(from.Entities[i].Position, target.Position, alpha);
				if (!starved)
					entity.Facing = LerpAngle(from.Entities[i].Facing, target.Facing, alpha);
			}
			m_Entities.push_back(entity);
		}
	}

	Vec2 GameClient::ExtrapolateProjectile(const EntityState& entity, double ticks) const
	{
		const AbilityDef& ability = GetAbility(AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1)));
		const Vec2 end = entity.Position + FromAngle(entity.Facing) * (ability.ProjectileSpeed * float(ticks) * GetTickSeconds());
		return Lerp(entity.Position, end, CastSegment(*m_Level, entity.Position, end, Block::Projectiles));
	}

	void GameClient::ComposeView(float alpha)
	{
		m_View.clear();
		const uint32_t playerId = m_Welcome.PlayerId;
		m_PresentTick = double(GetLatestTick()) + double(m_PendingInputs.size()) - 1.0 + double(alpha);
		const double lead = std::clamp(m_PresentTick - m_RenderTick, 0.0, MaxProjectileLeadSeconds * double(m_Welcome.TickRate));
		for (const EntityState& entity : m_Entities)
		{
			if (m_HasPrediction && entity.NetId == m_Welcome.PlayerNetId)
				continue;
			if ((entity.Kind == EntityKind::Projectile || entity.Kind == EntityKind::Effect) && entity.Owner == playerId && IsPredictedTag(entity.Tag))
				continue;
			if (entity.Kind == EntityKind::Projectile && entity.Owner == NoPlayer)
			{
				EntityState leading = entity;
				leading.Position = ExtrapolateProjectile(entity, lead);
				m_View.push_back(leading);
				continue;
			}
			m_View.push_back(entity);
		}
		if (!m_HasPrediction)
			return;

		EntityState self;
		self.NetId = m_Welcome.PlayerNetId;
		self.Kind = EntityKind::Player;
		self.Position = Lerp(m_PreviousMotion.Position, m_PredictedMotion.Position, alpha) + m_CorrectionOffset;
		self.Facing = m_HasViewAim && m_PredictedState.Alive && !m_PredictedState.IsStunned() ? m_ViewAim : m_PredictedMotion.Facing;
		self.Owner = playerId;
		self.Variant = uint8_t(AbilityId::Count);
		self.Health = m_LocalPlayer.MaxHealth > 0 ? uint8_t(std::clamp((m_LocalPlayer.Health * 255 + m_LocalPlayer.MaxHealth - 1) / m_LocalPlayer.MaxHealth, 0, 255)) : 255;
		if (!m_PredictedState.Alive)
			self.Flags |= uint16_t(EntityFlags::Dead);
		if (m_PredictedState.Invulnerable > 0.0f || m_PredictedState.DashDistance > 0.0f)
			self.Flags |= uint16_t(EntityFlags::Dashing);
		if (m_PredictedState.Chill > 0.0f)
			self.Flags |= uint16_t(EntityFlags::Chilled);
		if (m_PredictedState.Stun > 0.0f)
			self.Flags |= uint16_t(EntityFlags::Stunned);
		if (m_PredictedState.Ward > 0.0f)
			self.Flags |= uint16_t(EntityFlags::Warded);
		if (LengthSquared(m_PredictedState.Push) > 36.0f)
			self.Flags |= uint16_t(EntityFlags::Flung);
		if (const auto server = std::find_if(m_Entities.begin(), m_Entities.end(), [&](const EntityState& entity) { return entity.NetId == m_Welcome.PlayerNetId; }); server != m_Entities.end() && server->Has(EntityFlags::Burning))
			self.Flags |= uint16_t(EntityFlags::Burning);
		if (m_PredictedState.CastSlot >= 0)
		{
			self.Flags |= uint16_t(EntityFlags::Attacking);
			self.Variant = uint8_t(GetTraits().Skills[size_t(m_PredictedState.CastSlot)].Ability);
		}
		m_View.push_back(self);

		for (const PredictedProjectile& projectile : m_PredictedProjectiles)
			m_View.push_back({ .NetId = PredictedNetIdBase + (uint32_t(projectile.Tag) << 4) + projectile.Index, .Kind = EntityKind::Projectile, .Position = Lerp(projectile.Previous, projectile.Position, alpha), .Facing = AngleOf(projectile.Velocity), .Owner = playerId, .Tag = projectile.Tag, .Variant = uint8_t(projectile.Ability) });
		for (const PredictedEffect& effect : m_PredictedEffects)
			m_View.push_back({ .NetId = (effect.Pending ? PredictedEffectNetIdBase : PredictedImpactNetIdBase) + (uint32_t(effect.Tag) << 4) + effect.Index, .Kind = EntityKind::Effect, .Position = effect.Position, .Facing = effect.Facing, .Owner = playerId, .Tag = effect.Tag, .Variant = uint8_t(effect.Ability), .Scale = effect.Scale, .Flags = uint16_t(effect.Pending ? uint16_t(EntityFlags::Pending) : 0) });
	}
}
