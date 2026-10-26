#include "GameAudio.h"
#include "Rendering/SpellVisuals.h"
#include <GameShared/Content/Brawl.h>
#include <GameShared/Content/Definitions.h>
#include <GameShared/Content/Items.h>
#include <GameShared/Content/Terrain.h>
#include <RadianceEngine/Core/Log.h>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float MusicLevel = 0.75f;
		constexpr float KillWindow = 0.5f;
		constexpr float PlayerStride = 1.4f;
		constexpr float MonsterHearing = 13.0f;
		constexpr float MovingSpeed = 0.5f;
		constexpr uint32_t MaxHitSounds = 4;
		constexpr uint32_t MaxProjectileImpacts = 3;
		constexpr uint8_t BossPhaseHealth[] = { 170, 85 };

		bool Rose(uint16_t before, uint16_t after, EntityFlags flag)
		{
			return !(before & uint16_t(flag)) && (after & uint16_t(flag));
		}

		bool Fell(uint16_t before, uint16_t after, EntityFlags flag)
		{
			return (before & uint16_t(flag)) && !(after & uint16_t(flag));
		}

		AbilityId AbilityOf(const EntityState& entity)
		{
			return AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1));
		}

		Cue StepFor(TerrainId terrain)
		{
			switch (terrain)
			{
			case TerrainId::Grass:
				return Cue::StepGrass;
			case TerrainId::Dirt:
			case TerrainId::CaveFloor:
				return Cue::StepDirt;
			case TerrainId::Sand:
			case TerrainId::Ash:
				return Cue::StepSand;
			case TerrainId::WoodFloor:
				return Cue::StepWood;
			case TerrainId::Carpet:
				return Cue::StepCarpet;
			default:
				return Cue::StepStone;
			}
		}

		AmbienceScene SceneFor(MapKind map)
		{
			switch (map)
			{
			case MapKind::Hub:
			case MapKind::Abbey:
				return AmbienceScene::Hub;
			case MapKind::Town:
				return AmbienceScene::Town;
			case MapKind::Caves:
				return AmbienceScene::Caves;
			case MapKind::Crucible:
				return AmbienceScene::Crucible;
			case MapKind::Ashlands:
				return AmbienceScene::Ashlands;
			case MapKind::Caldera:
				return AmbienceScene::Caldera;
			case MapKind::FrozenLake:
				return AmbienceScene::FrozenLake;
			case MapKind::SkyRuins:
				return AmbienceScene::SkyRuins;
			case MapKind::StormPeak:
				return AmbienceScene::StormPeak;
			default:
				return AmbienceScene::None;
			}
		}
	}

	GameAudio::GameAudio(Timbre::AudioEngine& engine)
		: m_Engine(engine)
	{
		m_Buses.Music = engine.CreateBus("Music");
		m_Buses.Ambience = engine.CreateBus("Ambience");
		m_Buses.World = engine.CreateBus("World");
		m_Buses.Interface = engine.CreateBus("Interface");
		m_Buses.Stingers = engine.CreateBus("Stingers");
		m_Buses.Reverb = engine.CreateBus("Reverb");

		m_Reverb = std::make_shared<Timbre::Reverb>(0.7f, 0.45f, 1.0f, 1.0f);
		m_Reverb->SetDry(0.0f);
		m_Buses.Reverb.AddEffect(m_Reverb);
		m_Buses.World.AddEffect(std::make_shared<Timbre::Compressor>(-16.0f, 2.5f, 6.0f, 140.0f, 1.5f));
		m_Buses.Music.SetDucking({ .Trigger = m_Buses.Stingers, .VolumeDb = -9.0f, .ThresholdDb = -38.0f, .AttackSeconds = 0.05f, .ReleaseSeconds = 1.2f });
		m_Buses.Ambience.SetDucking({ .Trigger = m_Buses.World, .VolumeDb = -4.0f, .ThresholdDb = -30.0f, .AttackSeconds = 0.3f, .ReleaseSeconds = 1.8f });

		m_Bank = std::make_unique<SoundBank>(engine, m_Buses);
		m_Music = std::make_unique<MusicDirector>(engine, m_Buses.Music);
		m_Soundscape = std::make_unique<Soundscape>(engine, m_Buses.Ambience, *m_Bank);
		SetMix(1.0f, 1.0f);
		ApplyReverb(AmbienceScene::None);
	}

	void GameAudio::SetMix(float music, float effects)
	{
		m_MusicVolume = music;
		m_EffectsVolume = effects;
		m_Buses.Music.SetVolume(MusicLevel * music, 0.15f);
		for (const Timbre::Bus& bus : { m_Buses.World, m_Buses.Ambience, m_Buses.Interface, m_Buses.Stingers })
			bus.SetVolume(effects, 0.15f);
		m_Buses.Reverb.SetVolume(m_ReverbLevel * effects, 0.15f);
	}

	void GameAudio::ApplyReverb(AmbienceScene scene)
	{
		struct Space
		{
			float Decay;
			float Damping;
			float PreDelay;
			float Level;
		};
		Space space{ 1.0f, 0.5f, 0.02f, 0.0f };
		switch (scene)
		{
		case AmbienceScene::Hub:
			space = { 1.1f, 0.55f, 0.02f, 0.45f };
			break;
		case AmbienceScene::Town:
			space = { 1.5f, 0.5f, 0.025f, 0.55f };
			break;
		case AmbienceScene::Caves:
			space = { 2.8f, 0.3f, 0.04f, 1.0f };
			break;
		case AmbienceScene::Crucible:
			space = { 1.8f, 0.45f, 0.03f, 0.7f };
			break;
		case AmbienceScene::Ashlands:
			space = { 0.9f, 0.6f, 0.015f, 0.35f };
			break;
		case AmbienceScene::Menu:
			space = { 3.6f, 0.3f, 0.05f, 0.7f };
			break;
		case AmbienceScene::Caldera:
			space = { 1.7f, 0.45f, 0.03f, 0.55f };
			break;
		case AmbienceScene::FrozenLake:
			space = { 1.3f, 0.55f, 0.02f, 0.4f };
			break;
		case AmbienceScene::SkyRuins:
			space = { 0.9f, 0.6f, 0.01f, 0.25f };
			break;
		case AmbienceScene::StormPeak:
			space = { 1.6f, 0.5f, 0.03f, 0.35f };
			break;
		default:
			break;
		}
		m_Reverb->SetDecayTime(space.Decay);
		m_Reverb->SetDamping(space.Damping);
		m_Reverb->SetPreDelay(space.PreDelay);
		m_ReverbLevel = space.Level;
		m_Buses.Reverb.SetVolume(m_ReverbLevel * m_EffectsVolume, 1.0f);
	}

	void GameAudio::EnterMenu()
	{
		Reset();
		m_Level = nullptr;
		m_Music->SetMood(MusicMood::Menu);
		m_Soundscape->SetScene(AmbienceScene::Menu, nullptr);
		ApplyReverb(AmbienceScene::Menu);
		m_Overlay.Stop(0.3f);
		m_Overlaid = false;
		m_InventoryOpen = false;
		m_StationOpen = false;
		m_PauseOpen = false;
	}

	void GameAudio::Reset()
	{
		for (auto& [netId, tracked] : m_Tracked)
			tracked.Loop.Stop(0.2f);
		m_Tracked.clear();
		m_LastSwing.clear();
		m_Buses.World.StopAll(0.4f);
		m_Hits.clear();
		m_PreviousHits.clear();
		m_Status = {};
		m_Local = {};
		m_Inventory = {};
		m_Materials = {};
		m_SceneLevel = nullptr;
		m_AnnouncedBoss = 0;
		m_Primed = false;
		m_Downed.Stop(0.6f);
		m_Danger.Stop(0.6f);
		m_Soundscape->SetHeartbeat(0.0f);
		m_Soundscape->SetStorm(0.0f);
	}

	void GameAudio::Update(float dt)
	{
		m_Time += dt;
		m_CrackCooldown = std::max(m_CrackCooldown - dt, 0.0f);
		m_ThunderCooldown = std::max(m_ThunderCooldown - dt, 0.0f);
		m_Music->Update(dt);
		m_Soundscape->Update(dt, m_Focus, m_Ground);
	}

	Timbre::Voice GameAudio::PlayAt(Cue cue, Vec2 position, float volume, float pitch, float delay, float lift)
	{
		m_Played++;
		if (m_Trace)
			RDN_LOG("Audio {:.2f}: {} at {:.1f} {:.1f} ({:.1f} away), volume {:.2f}, pitch {:.2f}, delay {:.2f}", m_Time, SoundBank::GetPath(cue), position.x, position.y, Length(position - m_Focus), volume, pitch, delay);
		return m_Bank->Play(cue, { .Volume = volume, .Pitch = pitch, .Position = Timbre::Vec3(position.x, GroundAt(position) + lift, position.y), .Delay = delay });
	}

	Timbre::Voice GameAudio::PlayFlat(Cue cue, Timbre::Bus bus, float volume, float pitch, float delay)
	{
		m_Played++;
		if (m_Trace)
			RDN_LOG("Audio {:.2f}: {} flat on {}, volume {:.2f}, pitch {:.2f}, delay {:.2f}", m_Time, SoundBank::GetPath(cue), bus.GetName(), volume, pitch, delay);
		return m_Bank->Play(cue, { .Volume = volume, .Pitch = pitch, .Bus = bus, .Delay = delay });
	}

	void GameAudio::PlayUi(Cue cue, float volume, float pitch, float delay)
	{
		PlayFlat(cue, m_Buses.Interface, volume, pitch, delay);
	}

	void GameAudio::PlayHover(int index)
	{
		static constexpr float Pentatonic[] = { 3.0f, 0.0f, -2.0f, -4.0f, -7.0f, -9.0f, -12.0f };
		PlayUi(Cue::UiHover, 1.0f, std::exp2(Pentatonic[size_t(std::max(index, 0)) % std::size(Pentatonic)] / 12.0f));
	}

	void GameAudio::PlayStinger(MusicStinger stinger)
	{
		if (m_Trace)
			RDN_LOG("Audio {:.2f}: music stinger {}", m_Time, int(stinger));
		m_Music->PlayStinger(stinger);
	}

	float GameAudio::GroundAt(Vec2 position) const
	{
		return m_Level ? m_Level->Map.GetHeight(position) : 0.0f;
	}

	TerrainId GameAudio::TerrainAt(Vec2 position) const
	{
		if (!m_Level)
			return TerrainId::Flagstone;
		const int32_t x = int32_t(std::floor(position.x));
		const int32_t y = int32_t(std::floor(position.y));
		if (x < 0 || y < 0 || x >= m_Level->Map.GetWidth() || y >= m_Level->Map.GetHeight())
			return TerrainId::Flagstone;
		return m_Level->Map.GetTerrain(x, y);
	}

	bool GameAudio::HitNear(Vec2 position, float radius) const
	{
		const auto near = [&](const HitEvent& hit) { return LengthSquared(hit.Position - position) <= radius * radius; };
		return std::any_of(m_Hits.begin(), m_Hits.end(), near) || std::any_of(m_PreviousHits.begin(), m_PreviousHits.end(), near);
	}

	void GameAudio::UpdateListener(const glm::vec3& focus, const glm::vec3& eye)
	{
		glm::vec3 forward = focus - eye;
		const float length = glm::length(forward);
		forward = length > 1e-4f ? forward / length : glm::vec3(0.0f, -1.0f, 0.0f);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f) - forward * forward.y;
		up = glm::length(up) > 1e-4f ? glm::normalize(up) : glm::vec3(0.0f, 0.0f, 1.0f);
		m_Engine.SetListener({ .Position = focus - forward * ListenerDistance, .Forward = forward, .Up = up });
		m_Focus = Vec2(focus.x, focus.z);
		m_Ground = focus.y;
	}

	void GameAudio::Observe(const GameClient& client, float dt, const glm::vec3& focus, const glm::vec3& eye)
	{
		m_Level = client.GetLevel();
		m_Self = client.GetPlayerNetId();
		UpdateListener(focus, eye);
		UpdateScene(client, dt);

		const bool primed = m_Primed;
		m_FrameVolleys.clear();
		m_FrameImpacts = 0;
		m_PreviousHits = std::move(m_Hits);
		m_Hits = client.GetNewHits();
		for (auto& [netId, tracked] : m_Tracked)
		{
			tracked.Seen = false;
			tracked.SinceHit += dt;
		}
		for (const HitEvent& hit : m_Hits)
			if (const auto it = m_Tracked.find(hit.Target); it != m_Tracked.end())
				it->second.SinceHit = 0.0f;

		for (const EntityState& entity : client.GetEntities())
		{
			const auto [it, inserted] = m_Tracked.try_emplace(entity.NetId);
			Tracked& tracked = it->second;
			if (primed && inserted)
				OnAppear(entity, tracked, client);
			else if (primed)
				OnChange(entity, tracked, dt);
			tracked.Last = entity;
			tracked.Seen = true;
		}
		for (auto it = m_Tracked.begin(); it != m_Tracked.end();)
		{
			if (it->second.Seen)
			{
				++it;
				continue;
			}
			if (primed)
				OnVanish(it->second, client);
			it->second.Loop.Stop(0.12f);
			it = m_Tracked.erase(it);
		}

		if (primed)
		{
			PlayHits();
			ObserveLocalPlayer(client);
			ObserveInventory(client);
			ObserveStatus(client);
		}
		m_Status = client.GetStatus();
		m_Local = client.GetLocalPlayer();
		m_Inventory = client.GetInventory();
		m_Materials = client.GetMaterials();
		m_Primed = true;
		m_Level = nullptr;
	}

	void GameAudio::UpdateScene(const GameClient& client, float dt)
	{
		const SessionInfo& session = client.GetSession();
		const GameStatus& status = client.GetStatus();
		const AmbienceScene scene = SceneFor(session.Map);
		if (m_Level && (scene != m_Soundscape->GetScene() || m_Level != m_SceneLevel))
		{
			m_SceneLevel = m_Level;
			if (m_Trace)
				RDN_LOG("Audio {:.2f}: ambience scene {}", m_Time, int(scene));
			m_Soundscape->SetScene(scene, m_Level);
			ApplyReverb(scene);
		}

		MusicMood mood = MusicMood::Hub;
		switch (session.Mode)
		{
		case GameMode::Mission:
			mood = status.Mission == MissionPhase::Boss ? MusicMood::Boss : session.Map == MapKind::Caves ? MusicMood::Caves : MusicMood::Town;
			break;
		case GameMode::Horde:
			mood = MusicMood::Horde;
			break;
		case GameMode::Arena:
		case GameMode::Brawl:
			mood = MusicMood::Arena;
			break;
		case GameMode::Royale:
			mood = MusicMood::Royale;
			break;
		default:
			break;
		}
		if (m_Trace && mood != m_Music->GetMood())
			RDN_LOG("Audio {:.2f}: music mood {}", m_Time, int(mood));
		m_Music->SetMood(mood);

		float storm = 0.0f;
		if (session.Mode == GameMode::Royale && status.Match == MatchPhase::Fight && status.Storm.Radius > 0.0f)
		{
			const float outside = Length(m_Focus - status.Storm.Center) - status.Storm.Radius;
			storm = std::clamp((outside + 6.0f) / 9.0f, 0.0f, 1.0f);
			m_NextThunder -= dt;
			if (outside > 0.0f && m_NextThunder <= 0.0f)
			{
				const float angle = m_Random.Float() * 6.2831853f;
				PlayAt(Cue::Thunder, m_Focus + FromAngle(angle) * 14.0f, 0.65f, 0.8f + 0.25f * m_Random.Float(), 0.0f, 10.0f);
				m_NextThunder = 5.0f + 7.0f * m_Random.Float();
			}
		}
		if (session.Mode == GameMode::Brawl && session.Map == MapKind::SkyRuins && status.Brawl.Phase == BrawlPhase::Fight)
		{
			const Gust gust = GustAt(status.Brawl.Plan, client.GetArenaClock(client.GetPresentTick()));
			storm = 0.3f * gust.Warning * (1.0f - gust.Strength) + 0.85f * gust.Strength;
		}
		if (session.Mode == GameMode::Brawl && session.Map == MapKind::StormPeak)
		{
			const bool live = status.Brawl.Phase == BrawlPhase::Fight || status.Brawl.Phase == BrawlPhase::Over;
			const float clock = live ? client.GetArenaClock(client.GetPresentTick()) : 0.0f;
			const StormRing ring = RingAt(status.Brawl.Plan, clock);
			const float outside = ring.Active ? std::clamp((Length(m_Focus - ring.Center) - ring.Radius + 2.0f) / 4.0f, 0.0f, 1.0f) : 0.0f;
			storm = std::clamp(0.12f + 0.35f * (live ? StormBuild(status.Brawl.Plan, clock) : 0.0f) + 0.45f * outside, 0.12f, 0.85f);
		}
		m_Soundscape->SetStorm(storm);
	}

	void GameAudio::SetOverlay(bool inventory, bool station, bool paused)
	{
		const bool open = inventory || station || paused;
		if (open != m_Overlaid)
		{
			m_Overlaid = open;
			if (open)
				m_Overlay = m_Engine.StartSnapshot({ .Buses = { { m_Buses.World, -4.0f, 4500.0f }, { m_Buses.Ambience, -3.0f, 5000.0f } } }, 0.25f);
			else
				m_Overlay.Stop(0.35f);
		}
		if (inventory != m_InventoryOpen)
		{
			m_InventoryOpen = inventory;
			PlayUi(Cue::Leather, 0.8f, inventory ? 1.0f : 0.85f);
			if (inventory)
				PlayUi(Cue::Belt, 0.45f, 1.0f, 0.05f);
		}
		if (m_StationOpen && !station)
			PlayUi(Cue::BookClose, 0.5f);
		m_StationOpen = station;
		if (paused != m_PauseOpen)
		{
			m_PauseOpen = paused;
			PlayUi(paused ? Cue::UiOpen : Cue::UiClose, 0.7f, paused ? 0.8409f : 1.0f);
		}
	}

	void GameAudio::PlayStation(StationKind kind)
	{
		switch (kind)
		{
		case StationKind::Stash:
			PlayUi(Cue::Creak, 0.8f);
			PlayUi(Cue::DoorOpen, 0.45f, 1.3f, 0.1f);
			break;
		case StationKind::Forge:
			PlayUi(Cue::Anvil, 0.7f);
			break;
		case StationKind::Lapidary:
			PlayUi(Cue::GlassLight, 0.6f, 1.2f);
			PlayUi(Cue::GlassLight, 0.5f, 1.5f, 0.1f);
			break;
		case StationKind::MissionBoard:
			PlayUi(Cue::BookOpen, 0.8f);
			PlayUi(Cue::BookFlip, 0.6f, 1.0f, 0.08f);
			break;
		case StationKind::ProvingGrounds:
			PlayUi(Cue::Unsheathe, 0.7f);
			break;
		case StationKind::WishingVoid:
			PlayUi(Cue::Coins, 0.7f, 1.1f);
			PlayUi(Cue::WhooshLight, 0.35f, 0.7f, 0.25f);
			break;
		default:
			break;
		}
	}

	void GameAudio::PlayNotice(bool success, std::optional<StationKind> station)
	{
		if (station == StationKind::Forge)
		{
			PlayUi(Cue::Anvil, 0.8f, success ? 1.0f : 0.75f);
			if (success)
				PlayUi(Cue::Anvil, 0.6f, 1.05f, 0.22f);
			PlayUi(success ? Cue::UiSuccess : Cue::UiFail, 0.6f, 1.0f, success ? 0.3f : 0.1f);
		}
		else if (station == StationKind::Lapidary)
		{
			PlayUi(success ? Cue::GlassLight : Cue::Glass, 0.7f, success ? 1.3f : 0.8f);
			if (success)
				PlayUi(Cue::GlassLight, 0.5f, 1.6f, 0.12f);
			PlayUi(success ? Cue::UiSuccess : Cue::UiFail, 0.6f, 1.0f, 0.2f);
		}
		else
			PlayUi(success ? Cue::UiSuccess : Cue::UiError, 0.6f);
	}

	void GameAudio::PlayEvent(AudioEvent event)
	{
		switch (event)
		{
		case AudioEvent::Travel:
			PlayUi(Cue::TeleportOut, 0.7f);
			break;
		case AudioEvent::Teleport:
			PlayUi(Cue::DoorOpen, 0.6f);
			PlayUi(Cue::TeleportIn, 0.35f, 1.1f, 0.1f);
			break;
		case AudioEvent::Kill:
			PlayUi(Cue::UiReward, 0.7f);
			PlayUi(Cue::HeavyHit, 0.45f, 1.2f);
			break;
		case AudioEvent::Countdown:
			PlayFlat(Cue::Drum, m_Buses.Stingers, 0.8f);
			break;
		case AudioEvent::FightStart:
			PlayFlat(Cue::Gong, m_Buses.Stingers, 0.9f);
			PlayFlat(Cue::Horn, m_Buses.Stingers, 0.6f, 1.0f, 0.15f);
			break;
		case AudioEvent::MatchWon:
			PlayStinger(MusicStinger::Victory);
			break;
		case AudioEvent::MatchLost:
			PlayStinger(MusicStinger::Defeat);
			break;
		case AudioEvent::StormClosing:
			PlayFlat(Cue::Thunder, m_Buses.Stingers, 0.8f);
			PlayFlat(Cue::Horn, m_Buses.Stingers, 0.4f, 0.8f, 0.3f);
			break;
		case AudioEvent::RoundWon:
			PlayFlat(Cue::Gong, m_Buses.Stingers, 0.75f, 1.15f);
			PlayUi(Cue::UiReward, 0.7f, 1.0f, 0.15f);
			break;
		case AudioEvent::RoundLost:
			PlayFlat(Cue::Gong, m_Buses.Stingers, 0.6f, 0.85f);
			break;
		case AudioEvent::ApexClaimed:
			PlayFlat(Cue::Gong, m_Buses.Stingers, 0.85f, 0.7f);
			PlayUi(Cue::UiRift, 0.6f, 1.0f, 0.08f);
			break;
		}
	}

	void GameAudio::PlayLightning(Vec2 position, float distance)
	{
		if (distance < 16.0f && m_CrackCooldown <= 0.0f)
		{
			PlayAt(Cue::ShockHit, position, 0.6f, 0.5f + 0.15f * m_Random.Float());
			m_CrackCooldown = 0.12f;
		}
		if (m_ThunderCooldown > 0.0f)
			return;
		PlayAt(Cue::Thunder, position, std::clamp(1.0f - distance / 40.0f, 0.25f, 0.8f), 0.85f + 0.25f * m_Random.Float(), std::min(distance * 0.015f, 0.4f), 6.0f);
		m_ThunderCooldown = 0.7f;
	}

	void GameAudio::PlayCrumble(Vec2 position, bool ice)
	{
		if (ice)
		{
			PlayAt(Cue::Glass, position, 0.35f, 0.55f + 0.2f * m_Random.Float());
			PlayAt(Cue::IceImpact, position, 0.25f, 0.6f + 0.15f * m_Random.Float(), 0.04f);
			return;
		}
		PlayAt(Cue::Rock, position, 0.45f, 0.65f + 0.2f * m_Random.Float());
		PlayAt(Cue::FireImpact, position, 0.2f, 0.55f + 0.15f * m_Random.Float(), 0.05f);
	}

	void GameAudio::OnAppear(const EntityState& entity, Tracked& tracked, const GameClient& client)
	{
		switch (entity.Kind)
		{
		case EntityKind::Projectile:
			if (m_FrameVolleys.insert({ entity.Owner, entity.Owner != NoPlayer ? entity.Tag : uint16_t(entity.Variant) }).second)
				PlayLaunch(entity, client);
			break;
		case EntityKind::Effect:
			if (entity.Has(EntityFlags::Pending))
				StartTelegraph(entity, tracked);
			else
				PlayStrike(entity);
			break;
		case EntityKind::Loot:
			PlayLootDrop(entity, client);
			break;
		case EntityKind::Chaser:
		case EntityKind::Shooter:
		case EntityKind::Brute:
		case EntityKind::Caster:
			PlayAt(Cue::Rattle, entity.Position, 0.55f);
			if (entity.Kind == EntityKind::Brute || entity.Has(EntityFlags::Elite))
				PlayAt(Cue::Growl, entity.Position, 0.6f, 0.75f, 0.2f);
			break;
		case EntityKind::Archivist:
		case EntityKind::Wyrm:
			if (m_Status.Mission == MissionPhase::Boss)
				AnnounceBoss(entity);
			break;
		case EntityKind::Chest:
			if (ChestKind(entity.Tag) == ChestKind::Boss)
			{
				PlayAt(Cue::UiRare, entity.Position, 1.0f);
				PlayAt(Cue::Shield, entity.Position, 0.6f, 0.9f, 0.1f);
			}
			break;
		case EntityKind::Gateway:
			PlayAt(Cue::TeleportIn, entity.Position, 0.9f, 0.8f);
			tracked.Loop = PlayAt(Cue::GatewayHum, entity.Position, 0.8f);
			break;
		default:
			break;
		}
	}

	void GameAudio::OnChange(const EntityState& entity, Tracked& tracked, float dt)
	{
		const EntityState& before = tracked.Last;
		const float moved = Length(entity.Position - before.Position);
		if (entity.Kind == EntityKind::Player)
		{
			const bool self = entity.NetId == m_Self;
			const bool dead = entity.Has(EntityFlags::Dead);
			const bool revived = Fell(before.Flags, entity.Flags, EntityFlags::Dead);
			if (Rose(before.Flags, entity.Flags, EntityFlags::Dead))
			{
				PlayAt(Cue::Soft, entity.Position, 0.85f, 0.8f);
				PlayAt(Cue::Plate, entity.Position, 0.6f, 0.9f, 0.12f);
				if (self)
					PlayFlat(Cue::Bell, m_Buses.Stingers, 0.9f, 0.5f);
			}
			else if (revived)
			{
				PlayAt(Cue::TeleportIn, entity.Position, 0.7f);
				PlayAt(Cue::Heal, entity.Position, 0.45f, 1.0f, 0.2f);
			}
			if (Rose(before.Flags, entity.Flags, EntityFlags::Burning) && !dead)
			{
				PlayAt(Cue::FireImpact, entity.Position, self ? 0.6f : 0.4f, 1.35f);
				PlayAt(Cue::Soft, entity.Position, self ? 0.5f : 0.3f, 0.7f);
			}
			if (Rose(before.Flags, entity.Flags, EntityFlags::Warded) && !dead)
				PlayAt(Cue::Shield, entity.Position, self ? 0.75f : 0.5f, 1.2f);
			if (Rose(before.Flags, entity.Flags, EntityFlags::Falling))
				PlayAt(Cue::WhooshHeavy, entity.Position, self ? 0.8f : 0.5f, 0.7f);
			if (Fell(before.Flags, entity.Flags, EntityFlags::Lifted))
			{
				PlayAt(Cue::WhooshLight, entity.Position, self ? 0.7f : 0.45f, 1.1f);
				PlayAt(Cue::Soft, entity.Position, self ? 0.6f : 0.4f, 0.8f, 0.12f);
			}
			if (Rose(before.Flags, entity.Flags, EntityFlags::Attacking) || (entity.Has(EntityFlags::Attacking) && entity.Variant != before.Variant))
				StartCast(entity, tracked, self);
			else if (Fell(before.Flags, entity.Flags, EntityFlags::Attacking))
			{
				tracked.Loop.Stop(0.08f);
				tracked.Loop = {};
			}
			if ((before.Has(EntityFlags::Attacking) && before.Variant == uint8_t(AbilityId::FrostSpear)) || (entity.Has(EntityFlags::Attacking) && entity.Variant == uint8_t(AbilityId::FrostSpear)))
				ChargeSpear(entity, before, tracked, self, dt);
			if (!dead && !revived && moved > 3.0f)
				PlayAt(Cue::TeleportIn, entity.Position, 0.6f);
			else if (!dead && !entity.Has(EntityFlags::Dashing))
				PlayFootstep(entity, tracked, moved, self ? 0.9f : 0.6f);
			return;
		}

		if (entity.Kind == EntityKind::Effect)
		{
			if (before.Has(EntityFlags::Pending) && !entity.Has(EntityFlags::Pending))
			{
				tracked.Loop.Stop(0.1f);
				tracked.Loop = {};
				PlayStrike(entity);
			}
			return;
		}

		if (entity.Kind == EntityKind::Chest)
		{
			if (before.Variant == 0 && entity.Variant != 0)
			{
				PlayAt(Cue::Creak, entity.Position, 0.9f);
				PlayAt(Cue::DoorOpen, entity.Position, 0.5f, 1.35f, 0.08f);
				PlayAt(Cue::Coins, entity.Position, 0.7f, 1.0f, 0.3f);
			}
			return;
		}

		if (entity.Kind == EntityKind::Dummy)
		{
			if (!Rose(before.Flags, entity.Flags, EntityFlags::Falling))
				return;
			if (DummyKind(entity.Variant) == DummyKind::Balloon)
			{
				PlayAt(Cue::GlassLight, entity.Position, 0.7f, 1.9f);
				PlayAt(Cue::Crow, entity.Position, 0.45f, 1.7f, 0.12f);
			}
			else
				PlayAt(Cue::WhooshLight, entity.Position, 0.6f, 0.8f);
			return;
		}
		if (!IsEnemyKind(entity.Kind))
			return;
		if (Rose(before.Flags, entity.Flags, EntityFlags::Attacking))
			StartWindup(entity, tracked);
		else if (Fell(before.Flags, entity.Flags, EntityFlags::Attacking))
		{
			tracked.Loop.Stop(0.25f);
			tracked.Loop = {};
		}
		if (Rose(before.Flags, entity.Flags, EntityFlags::Chilled))
			PlayAt(Cue::IceImpact, entity.Position, 0.3f, 1.5f);
		if (Rose(before.Flags, entity.Flags, EntityFlags::Stunned))
			PlayAt(Cue::Bell, entity.Position, 0.45f, 1.5f);
		if (IsBossKind(entity.Kind))
		{
			PlayBossChange(entity, before);
			return;
		}
		if (moved > MovingSpeed * dt)
		{
			if (tracked.Idle > 1.5f)
				PlayAggro(entity);
			tracked.Idle = 0.0f;
		}
		else
			tracked.Idle += dt;
		PlayMonsterStep(entity, tracked, moved);
	}

	void GameAudio::OnVanish(const Tracked& tracked, const GameClient& client)
	{
		const EntityState& last = tracked.Last;
		if (IsEnemyKind(last.Kind))
		{
			if (tracked.SinceHit < KillWindow)
				PlayEnemyDeath(last);
			return;
		}
		switch (last.Kind)
		{
		case EntityKind::Projectile:
			PlayProjectileImpact(last);
			break;
		case EntityKind::Loot:
			if (const EntityState* self = client.FindEntity(m_Self); self && LengthSquared(self->Position - last.Position) < 2.6f)
				PlayLootTaken(last);
			break;
		default:
			break;
		}
	}

	void GameAudio::PlayLaunch(const EntityState& entity, const GameClient& client)
	{
		const Vec2 at = entity.Position;
		const bool own = entity.Owner != NoPlayer && entity.Owner == client.GetPlayerId();
		const float loud = own ? 1.0f : 0.8f;
		switch (AbilityOf(entity))
		{
		case AbilityId::Bolt:
			PlayAt(Cue::ShockLaunch, at, 0.55f * loud, 1.3f);
			break;
		case AbilityId::Orb:
			PlayAt(Cue::IceLaunch, at, 0.65f * loud, 0.8f);
			break;
		case AbilityId::Arrow:
			PlayAt(Cue::BowShoot, at, 0.85f * loud);
			break;
		case AbilityId::Quarrel:
			PlayAt(Cue::BowShoot, at, 0.8f * loud, 0.75f);
			PlayAt(Cue::Latch, at, 0.7f * loud, 0.9f);
			break;
		case AbilityId::Volley:
			PlayAt(Cue::BowShoot, at, 0.8f * loud);
			PlayAt(Cue::BowShoot, at, 0.6f * loud, 1.08f, 0.03f);
			PlayAt(Cue::BowShoot, at, 0.6f * loud, 0.93f, 0.06f);
			break;
		case AbilityId::PiercingShot:
			PlayAt(Cue::BowShoot, at, loud, 0.8f);
			PlayAt(Cue::WhooshHeavy, at, 0.5f * loud, 1.3f);
			break;
		case AbilityId::Fireball:
			PlayAt(Cue::FireLaunch, at, 0.9f * loud);
			break;
		case AbilityId::IceSpear:
			PlayAt(Cue::IceLaunch, at, 0.8f * loud, 1.15f);
			break;
		case AbilityId::ChainLightning:
		{
			const auto caster = std::find_if(client.GetEntities().begin(), client.GetEntities().end(), [&](const EntityState& other) { return other.Kind == EntityKind::Player && other.Owner == entity.Owner; });
			if (caster != client.GetEntities().end() && LengthSquared(caster->Position - at) > 6.0f)
				PlayAt(Cue::ShockHit, at, 0.5f * loud, 1.25f);
			else
				PlayAt(Cue::ShockLaunch, at, 0.8f * loud);
			break;
		}
		case AbilityId::Spit:
			PlayAt(Cue::Spit, at, 0.6f);
			PlayAt(Cue::WhooshLight, at, 0.3f, 1.4f);
			break;
		case AbilityId::TomeVolley:
			PlayAt(Cue::ShockLaunch, at, 0.55f, 0.7f);
			PlayAt(Cue::BookFlip, at, 0.5f);
			break;
		case AbilityId::InkSpiral:
			PlayAt(Cue::ManaStart, at, 0.45f, 0.75f);
			break;
		case AbilityId::ShardBurst:
			PlayAt(Cue::IceLaunch, at, 0.5f, 1.35f);
			break;
		case AbilityId::EmberBolt:
			PlayAt(Cue::FireLaunch, at, 0.7f * loud, 1.25f);
			break;
		case AbilityId::ArcLightning:
			PlayAt(Cue::ShockLaunch, at, 0.75f * loud, 1.1f);
			break;
		case AbilityId::FrostSpear:
		{
			const float power = std::clamp((entity.GetScale() - 1.0f) / (SpearFullSize - 1.0f), 0.0f, 1.0f);
			PlayAt(Cue::IceLaunch, at, (0.6f + 0.35f * power) * loud, 1.3f - 0.4f * power);
			if (power > 0.4f)
				PlayAt(Cue::WhooshHeavy, at, 0.65f * power * loud, 1.15f);
			if (power >= 0.999f)
				PlayAt(Cue::Glass, at, 0.45f * loud, 1.25f);
			break;
		}
		case AbilityId::Swap:
			PlayAt(Cue::ManaStart, at, 0.6f * loud, 1.35f);
			PlayAt(Cue::WhooshLight, at, 0.4f * loud, 1.3f);
			break;
		case AbilityId::ChainHook:
			PlayAt(Cue::WhooshLight, at, 0.6f * loud, 0.85f);
			PlayAt(Cue::Coins, at, 0.35f * loud, 1.55f, 0.02f);
			PlayAt(Cue::Latch, at, 0.4f * loud, 1.2f);
			break;
		case AbilityId::SplittingBolt:
			PlayAt(Cue::ManaStart, at, 0.6f * loud, 0.7f);
			PlayAt(Cue::ShockLaunch, at, 0.45f * loud, 0.65f);
			break;
		case AbilityId::BoltShard:
			PlayAt(Cue::GlassLight, at, 0.6f * loud, 1.3f);
			PlayAt(Cue::ShockHit, at, 0.4f * loud, 1.45f);
			break;
		default:
			PlayAt(Cue::WhooshLight, at, 0.5f);
			break;
		}
	}

	void GameAudio::PlayProjectileImpact(const EntityState& entity)
	{
		const AbilityId ability = AbilityOf(entity);
		if (GetAbility(ability).ImpactRadius > 0.0f || m_FrameImpacts >= MaxProjectileImpacts)
			return;
		m_FrameImpacts++;
		const Vec2 at = entity.Position;
		switch (ability)
		{
		case AbilityId::Bolt:
			PlayAt(Cue::ShockHit, at, 0.45f, 1.25f);
			break;
		case AbilityId::Orb:
			PlayAt(Cue::IceImpact, at, 0.6f, 0.9f);
			break;
		case AbilityId::IceSpear:
			PlayAt(Cue::IceImpact, at, 0.75f, 1.1f);
			break;
		case AbilityId::ChainLightning:
			PlayAt(Cue::ShockHit, at, 0.6f);
			break;
		case AbilityId::SplittingBolt:
			PlayAt(Cue::ShockHit, at, 0.55f, 0.75f);
			PlayAt(Cue::Glass, at, 0.3f);
			break;
		case AbilityId::BoltShard:
			PlayAt(Cue::GlassLight, at, 0.4f, 1.4f);
			break;
		case AbilityId::ChainHook:
			break;
		case AbilityId::Arrow:
		case AbilityId::Quarrel:
		case AbilityId::Volley:
		case AbilityId::PiercingShot:
			if (!HitNear(at, 1.6f))
				PlayAt(Cue::ArrowHit, at, 0.5f);
			break;
		case AbilityId::Spit:
			PlayAt(Cue::Soft, at, 0.45f, 1.5f);
			break;
		case AbilityId::TomeVolley:
			PlayAt(Cue::ShockHit, at, 0.55f, 0.75f);
			break;
		case AbilityId::InkSpiral:
			PlayAt(Cue::Soft, at, 0.4f, 1.3f);
			break;
		case AbilityId::ShardBurst:
			PlayAt(Cue::GlassLight, at, 0.6f);
			PlayAt(Cue::IceImpact, at, 0.35f, 1.3f);
			break;
		case AbilityId::EmberBolt:
			PlayAt(Cue::FireImpact, at, 0.5f, 1.35f);
			break;
		case AbilityId::ArcLightning:
			PlayAt(Cue::ShockHit, at, 0.55f, 1.1f);
			break;
		case AbilityId::FrostSpear:
		{
			const float power = std::clamp((entity.GetScale() - 1.0f) / (SpearFullSize - 1.0f), 0.0f, 1.0f);
			PlayAt(Cue::IceImpact, at, 0.55f + 0.35f * power, 1.25f - 0.4f * power);
			PlayAt(Cue::Glass, at, 0.3f + 0.4f * power, 1.3f - 0.35f * power);
			break;
		}
		case AbilityId::Swap:
			PlayAt(Cue::TeleportIn, at, 0.4f, 1.3f);
			break;
		default:
			break;
		}
	}

	void GameAudio::PlayStrike(const EntityState& entity)
	{
		const AbilityId ability = AbilityOf(entity);
		const AbilityDef& def = GetAbility(ability);
		const Vec2 at = entity.Position;
		const Vec2 forward = FromAngle(entity.Facing);
		const float scale = entity.GetScale();
		const auto alongLine = [&](Cue cue, int points, float volume, float pitch, float spacing)
		{
			for (int i = 0; i < points; i++)
				PlayAt(cue, at + forward * (def.Reach * scale * (float(i) + 0.5f) / float(points)), volume, pitch * (1.0f + 0.04f * float(i)), spacing * float(i));
		};
		switch (ability)
		{
		case AbilityId::Slash:
		case AbilityId::Chop:
		case AbilityId::Stab:
		case AbilityId::Thrust:
		{
			const auto it = m_LastSwing.find(entity.Owner);
			if (it == m_LastSwing.end() || m_Time - it->second > 0.6f)
				PlayAt(ability == AbilityId::Chop ? Cue::HeavySwing : ability == AbilityId::Slash ? Cue::SwordSwing : Cue::DaggerSwing, at, 0.8f, ability == AbilityId::Thrust ? 0.8f : 1.0f);
			break;
		}
		case AbilityId::Bite:
			PlayAt(Cue::DaggerSwing, at, 0.5f, 0.7f);
			break;
		case AbilityId::GroundSlam:
			PlayAt(Cue::Rock, at, 0.9f, 0.8f);
			PlayAt(Cue::Rock, at + forward * 2.0f, 0.7f, 0.6f, 0.05f);
			PlayAt(Cue::Rumble, at, 0.45f);
			PlayAt(Cue::Body, at, 0.7f, 0.7f);
			break;
		case AbilityId::Slam:
			PlayAt(Cue::Rock, at, 1.0f, 0.75f);
			PlayAt(Cue::Body, at, 0.8f, 0.6f);
			PlayAt(Cue::Rumble, at, 0.45f);
			PlayAt(Cue::Bone, at, 0.4f);
			break;
		case AbilityId::FrostNova:
			PlayAt(Cue::IceImpact, at, 0.9f, 0.8f);
			PlayAt(Cue::IceImpact, at, 0.6f, 1.05f, 0.06f);
			PlayAt(Cue::WhooshHeavy, at, 0.5f, 1.4f);
			break;
		case AbilityId::Cyclone:
			PlayAt(Cue::WhooshHeavy, at, 0.75f);
			PlayAt(Cue::HeavySwing, at, 0.5f, 1.1f, 0.05f);
			PlayAt(Cue::WhooshHeavy, at, 0.55f, 1.2f, 0.12f);
			break;
		case AbilityId::Fireball:
			PlayAt(Cue::FireImpact, at, 0.95f);
			PlayAt(Cue::Explosion, at, 0.4f, 1.2f);
			break;
		case AbilityId::Firebomb:
		case AbilityId::FirebombSkip:
		case AbilityId::FirebombSkip2:
			PlayAt(Cue::Explosion, at, 0.75f, ability == AbilityId::Firebomb ? 1.05f : 1.25f);
			PlayAt(Cue::FireImpact, at, 0.7f, 1.1f);
			break;
		case AbilityId::FirebombFlame:
			PlayAt(Cue::FireImpact, at, 0.3f, 1.35f);
			break;
		case AbilityId::StormStrike:
			PlayAt(Cue::Thunder, at, 0.9f, 1.15f);
			PlayAt(Cue::ShockHit, at, 0.8f, 0.9f);
			break;
		case AbilityId::StormJump:
			PlayAt(Cue::ShockHit, at, 0.65f, 1.3f);
			break;
		case AbilityId::HookAnchor:
			PlayAt(Cue::MetalHit, at, 0.45f, 0.8f);
			PlayAt(Cue::Rock, at, 0.3f, 1.4f);
			break;
		case AbilityId::HookPull:
			PlayAt(Cue::Latch, at, 0.6f, 0.9f);
			PlayAt(Cue::Coins, at, 0.4f, 1.2f, 0.03f);
			PlayAt(Cue::WhooshHeavy, at, 0.5f, 1.2f, 0.05f);
			break;
		case AbilityId::HookYank:
			PlayAt(Cue::WhooshHeavy, at, 0.7f, 0.9f);
			PlayAt(Cue::Coins, at, 0.35f, 1.0f, 0.02f);
			break;
		case AbilityId::Meteor:
			PlayAt(Cue::Explosion, at, 1.0f, 0.85f);
			PlayAt(Cue::FireImpact, at, 1.0f, 0.8f);
			PlayAt(Cue::Rock, at, 0.8f, 0.7f, 0.04f);
			PlayAt(Cue::Thunder, at, 0.45f, 0.8f, 0.05f);
			break;
		case AbilityId::RainOfArrows:
			for (int i = 0; i < 6; i++)
			{
				const Vec2 offset = FromAngle(m_Random.Float() * 6.2831853f) * (def.ImpactRadius * scale * std::sqrt(m_Random.Float()));
				PlayAt(Cue::ArrowHit, at + offset, 0.55f, 0.9f + 0.2f * m_Random.Float(), 0.05f * float(i) + 0.05f * m_Random.Float());
			}
			break;
		case AbilityId::Firestrike:
			PlayAt(Cue::FireImpact, at, 0.85f, 1.05f);
			break;
		case AbilityId::PageStorm:
			PlayAt(Cue::ShockHit, at, 0.7f, 0.9f);
			PlayAt(Cue::BookFlip, at, 0.6f);
			PlayAt(Cue::BookFlip, at, 0.5f, 1.1f, 0.12f);
			break;
		case AbilityId::ShelfCrush:
			alongLine(Cue::Wood, 3, 0.9f, 0.9f, 0.05f);
			PlayAt(Cue::Plank, at + forward * (def.Reach * scale * 0.5f), 0.8f, 1.0f, 0.05f);
			PlayAt(Cue::Rumble, at, 0.35f);
			break;
		case AbilityId::ArcaneRing:
			for (int i = 0; i < 4; i++)
				PlayAt(Cue::ShockHit, at + FromAngle(1.5707963f * float(i)) * (def.ImpactRadius * scale), 0.55f, 0.7f, 0.03f * float(i));
			PlayAt(Cue::Thunder, at, 0.4f, 1.2f);
			break;
		case AbilityId::ArcaneBurst:
			PlayAt(Cue::ShockHit, at, 1.0f, 0.6f);
			PlayAt(Cue::Explosion, at, 0.6f, 0.7f);
			PlayAt(Cue::Thunder, at, 0.5f);
			break;
		case AbilityId::CrystalSpike:
			alongLine(Cue::Glass, 4, 0.7f, 0.85f, 0.05f);
			PlayAt(Cue::IceImpact, at, 0.6f, 0.75f);
			PlayAt(Cue::Rock, at, 0.5f, 0.8f);
			break;
		case AbilityId::TailSweep:
			PlayAt(Cue::WhooshHeavy, at, 1.0f, 0.55f);
			PlayAt(Cue::Body, at, 0.6f, 0.6f, 0.1f);
			break;
		case AbilityId::Eruption:
			PlayAt(Cue::Explosion, at, 1.0f, 0.75f);
			PlayAt(Cue::Rock, at, 1.0f, 0.6f);
			PlayAt(Cue::Rock, at, 0.8f, 0.7f, 0.08f);
			PlayAt(Cue::Rumble, at, 0.7f);
			break;
		case AbilityId::CrystalRain:
			PlayAt(Cue::IceImpact, at, 0.75f, 1.1f);
			PlayAt(Cue::GlassLight, at, 0.6f);
			break;
		case AbilityId::Rejuvenate:
			PlayAt(Cue::Heal, at, 0.8f);
			PlayAt(Cue::Shield, at, 0.35f, 1.3f);
			break;
		case AbilityId::Roll:
			PlayAt(Cue::WhooshLight, at, 0.6f);
			PlayAt(Cue::Cloth, at, 0.55f);
			break;
		case AbilityId::Blink:
		case AbilityId::Warp:
			PlayAt(Cue::TeleportOut, at, 0.75f);
			break;
		case AbilityId::Comet:
			PlayAt(Cue::Explosion, at, 0.9f, 0.95f);
			PlayAt(Cue::FireImpact, at, 0.9f, 0.85f);
			PlayAt(Cue::Rock, at, 0.7f, 0.75f, 0.04f);
			break;
		case AbilityId::Gust:
			PlayAt(Cue::WhooshHeavy, at, 0.95f, 1.05f);
			PlayAt(Cue::WhooshLight, at, 0.6f, 0.8f, 0.05f);
			break;
		case AbilityId::Shockwave:
			PlayAt(Cue::ShockHit, at, 0.9f, 0.75f);
			PlayAt(Cue::Explosion, at, 0.45f, 1.4f);
			break;
		case AbilityId::GravityWell:
			PlayAt(Cue::Rumble, at, 0.8f, 1.2f);
			PlayAt(Cue::ManaEnd, at, 0.7f, 0.6f);
			break;
		case AbilityId::Swap:
			PlayAt(Cue::TeleportIn, at, 0.65f, 1.15f);
			break;
		case AbilityId::MirrorWard:
			PlayAt(Cue::Shield, at, 0.8f, 1.3f);
			PlayAt(Cue::Bell, at, 0.3f, 1.6f, 0.05f);
			break;
		default:
			break;
		}
	}

	void GameAudio::StartTelegraph(const EntityState& entity, Tracked& tracked)
	{
		const Vec2 at = entity.Position;
		switch (AbilityOf(entity))
		{
		case AbilityId::Meteor:
		case AbilityId::Comet:
			PlayAt(Cue::FireLaunch, at, 1.0f, 0.6f);
			break;
		case AbilityId::GravityWell:
			tracked.Loop = PlayAt(Cue::ShockLoop, at, 0.5f, 0.55f);
			break;
		case AbilityId::Firebomb:
			PlayAt(Cue::FireLaunch, at, 0.7f, 0.75f);
			break;
		case AbilityId::StormStrike:
			PlayAt(Cue::ShockLaunch, at, 0.5f, 0.7f);
			break;
		case AbilityId::Firestrike:
			tracked.Loop = PlayAt(Cue::FireLoop, at, 0.5f);
			break;
		case AbilityId::RainOfArrows:
			PlayAt(Cue::WhooshLight, at, 0.5f, 1.5f, 0.15f);
			PlayAt(Cue::WhooshLight, at, 0.4f, 1.3f, 0.28f);
			break;
		case AbilityId::PageStorm:
			tracked.Loop = PlayAt(Cue::ShockLoop, at, 0.45f);
			PlayAt(Cue::BookFlip, at, 0.5f);
			break;
		case AbilityId::ShelfCrush:
			PlayAt(Cue::Creak, at, 0.9f, 0.75f);
			PlayAt(Cue::Rumble, at, 0.3f);
			break;
		case AbilityId::ArcaneRing:
		case AbilityId::ArcaneBurst:
			tracked.Loop = PlayAt(Cue::ShockLoop, at, 0.55f, 0.8f);
			break;
		case AbilityId::CrystalSpike:
			tracked.Loop = PlayAt(Cue::IceLoop, at, 0.45f, 0.8f);
			PlayAt(Cue::Rumble, at, 0.25f);
			break;
		case AbilityId::TailSweep:
			PlayAt(Cue::Growl, at, 0.7f, 0.5f);
			break;
		case AbilityId::Eruption:
			tracked.Loop = PlayAt(Cue::Rumble, at, 0.7f);
			break;
		case AbilityId::CrystalRain:
			tracked.Loop = PlayAt(Cue::IceLoop, at, 0.45f);
			break;
		default:
			break;
		}
	}

	void GameAudio::StartCast(const EntityState& entity, Tracked& tracked, bool self)
	{
		tracked.Loop.Stop(0.05f);
		tracked.Loop = {};
		if (entity.Variant >= uint8_t(AbilityId::Count))
			return;
		const AbilityId ability = AbilityId(entity.Variant);
		const float cast = GetAbility(ability).CastTime;
		const Vec2 at = entity.Position;
		const float loud = self ? 1.0f : 0.75f;
		const auto swing = [&](Cue cue, float pitch, float lead)
		{
			PlayAt(cue, at, loud, pitch, std::max(0.0f, cast - lead));
			m_LastSwing[entity.Owner] = m_Time;
		};
		switch (ability)
		{
		case AbilityId::Slash:
			swing(Cue::SwordSwing, 1.0f, 0.14f);
			break;
		case AbilityId::Chop:
			swing(Cue::HeavySwing, 1.0f, 0.18f);
			break;
		case AbilityId::Stab:
			swing(Cue::DaggerSwing, 1.0f, 0.1f);
			break;
		case AbilityId::Thrust:
			swing(Cue::DaggerSwing, 0.8f, 0.1f);
			PlayAt(Cue::WhooshLight, at, 0.45f * loud, 0.8f, std::max(0.0f, cast - 0.08f));
			break;
		case AbilityId::GroundSlam:
			PlayAt(Cue::HeavySwing, at, loud, 0.7f, 0.3f);
			break;
		case AbilityId::Arrow:
		case AbilityId::Volley:
			tracked.Loop = PlayAt(Cue::BowDraw, at, 0.8f * loud);
			break;
		case AbilityId::PiercingShot:
			tracked.Loop = PlayAt(Cue::BowDraw, at, loud, 0.9f);
			break;
		case AbilityId::Quarrel:
			PlayAt(Cue::Click, at, 0.7f * loud, 0.85f);
			break;
		case AbilityId::Fireball:
			tracked.Loop = PlayAt(Cue::FireBuildup, at, loud);
			break;
		case AbilityId::Meteor:
			tracked.Loop = PlayAt(Cue::FireBuildup, at, loud, 0.8f);
			break;
		case AbilityId::FrostNova:
			tracked.Loop = PlayAt(Cue::IceBuildup, at, loud, 1.25f);
			break;
		case AbilityId::IceSpear:
			tracked.Loop = PlayAt(Cue::IceBuildup, at, 0.8f * loud, 1.1f);
			break;
		case AbilityId::Orb:
			tracked.Loop = PlayAt(Cue::IceBuildup, at, 0.5f * loud, 0.95f);
			break;
		case AbilityId::ChainLightning:
			tracked.Loop = PlayAt(Cue::ShockBuildup, at, loud);
			break;
		case AbilityId::Bolt:
			tracked.Loop = PlayAt(Cue::ShockBuildup, at, 0.35f * loud, 1.3f);
			break;
		case AbilityId::Rejuvenate:
			tracked.Loop = PlayAt(Cue::ManaStart, at, loud);
			break;
		case AbilityId::EmberBolt:
			tracked.Loop = PlayAt(Cue::FireBuildup, at, 0.45f * loud, 1.35f);
			break;
		case AbilityId::Comet:
			tracked.Loop = PlayAt(Cue::FireBuildup, at, loud, 0.85f);
			break;
		case AbilityId::Shockwave:
		case AbilityId::ArcLightning:
			tracked.Loop = PlayAt(Cue::ShockBuildup, at, 0.8f * loud, ability == AbilityId::Shockwave ? 0.85f : 1.1f);
			break;
		case AbilityId::FrostSpear:
			PlayAt(Cue::IceBuildup, at, 0.55f * loud, 1.15f);
			tracked.Loop = PlayAt(Cue::IceLoop, at, 0.3f * loud, 0.8f);
			break;
		case AbilityId::GravityWell:
		case AbilityId::Swap:
			tracked.Loop = PlayAt(Cue::ManaStart, at, 0.8f * loud, ability == AbilityId::Swap ? 1.3f : 0.75f);
			break;
		case AbilityId::Gust:
			PlayAt(Cue::WhooshLight, at, 0.6f * loud, 0.75f);
			break;
		default:
			break;
		}
	}

	void GameAudio::ChargeSpear(const EntityState& entity, const EntityState& before, Tracked& tracked, bool self, float dt)
	{
		const float loud = self ? 1.0f : 0.75f;
		const Vec2 at = entity.Position;
		const bool held = before.Has(EntityFlags::Attacking) && before.Variant == uint8_t(AbilityId::FrostSpear);
		if (!entity.Has(EntityFlags::Attacking) || entity.Variant != uint8_t(AbilityId::FrostSpear) || entity.Has(EntityFlags::Dead))
		{
			if (before.GetOvercharge() > 0.9f)
			{
				PlayAt(Cue::Glass, at, 0.8f * loud, 0.95f);
				PlayAt(Cue::IceImpact, at, 0.6f * loud, 0.75f);
			}
			else if (entity.Has(EntityFlags::Dashing) || entity.Has(EntityFlags::Stunned) || entity.Has(EntityFlags::Falling))
				PlayAt(Cue::GlassLight, at, 0.5f * loud, 0.9f);
			tracked.Crackle = 0.0f;
			return;
		}
		const float power = entity.GetChargePower();
		const float over = entity.GetOvercharge();
		tracked.Loop.SetPitch(0.8f + 0.55f * power + 0.35f * over);
		tracked.Loop.SetVolume((0.3f + 0.45f * power) * loud, 0.05f);
		if (!held)
			return;
		const float was = before.GetChargePower();
		if (was < 1.0f && power >= 1.0f)
		{
			PlayAt(Cue::Bell, at, 0.4f * loud, 1.9f);
			PlayAt(Cue::GlassLight, at, 0.5f * loud, 1.7f);
		}
		else
			for (size_t i = 0; i < std::size(SpearCrownSteps); i++)
				if (was < SpearCrownSteps[i] && power >= SpearCrownSteps[i])
					PlayAt(Cue::GlassLight, at, 0.35f * loud, 1.15f + 0.18f * float(i));
		if (over <= 0.0f)
		{
			tracked.Crackle = 0.0f;
			return;
		}
		tracked.Crackle -= dt;
		if (tracked.Crackle <= 0.0f)
		{
			PlayAt(Cue::GlassLight, at, (0.2f + 0.35f * over) * loud, 1.5f + 0.6f * over);
			tracked.Crackle = 0.3f - 0.22f * over;
		}
	}

	void GameAudio::StartWindup(const EntityState& entity, Tracked& tracked)
	{
		const Vec2 at = entity.Position;
		const bool elite = entity.Has(EntityFlags::Elite);
		const float pitch = elite ? 0.9f : 1.0f;
		const float loud = elite ? 1.15f : 1.0f;
		switch (entity.Kind)
		{
		case EntityKind::Chaser:
			PlayAt(Cue::GhoulSnarl, at, 0.75f * loud, pitch);
			break;
		case EntityKind::Shooter:
			PlayAt(Cue::SkeletonHiss, at, 0.5f * loud, 1.15f * pitch);
			break;
		case EntityKind::Brute:
			PlayAt(Cue::OgreRoar, at, 0.85f * loud, 0.95f * pitch);
			PlayAt(Cue::HeavySwing, at, 0.6f, 0.6f, 0.7f);
			break;
		case EntityKind::Caster:
			tracked.Loop = PlayAt(Cue::Chant, at, 0.6f * loud, pitch);
			PlayAt(Cue::FireBuildup, at, 0.35f, 0.9f);
			break;
		case EntityKind::Archivist:
			if (m_Time - m_LastBossVoice > 4.0f)
			{
				PlayAt(Cue::Wraith, at, 0.7f);
				m_LastBossVoice = m_Time;
			}
			break;
		case EntityKind::Wyrm:
			if (m_Time - m_LastBossVoice > 4.0f)
			{
				PlayAt(Cue::Growl, at, 0.8f, 0.55f);
				m_LastBossVoice = m_Time;
			}
			break;
		default:
			break;
		}
	}

	void GameAudio::PlayAggro(const EntityState& entity)
	{
		if (m_Time - m_LastGrowl < 0.35f || LengthSquared(entity.Position - m_Focus) > 22.0f * 22.0f)
			return;
		m_LastGrowl = m_Time;
		switch (entity.Kind)
		{
		case EntityKind::Chaser:
			PlayAt(Cue::GhoulGrowl, entity.Position, 0.6f);
			break;
		case EntityKind::Brute:
			PlayAt(Cue::Growl, entity.Position, 0.8f, 0.75f);
			break;
		default:
			PlayAt(Cue::SkeletonHiss, entity.Position, 0.5f);
			break;
		}
	}

	void GameAudio::PlayFootstep(const EntityState& entity, Tracked& tracked, float moved, float volume)
	{
		if (moved > 1.5f)
		{
			tracked.Stride = 0.0f;
			return;
		}
		tracked.Stride += moved;
		if (tracked.Stride < PlayerStride)
			return;
		tracked.Stride -= PlayerStride;
		tracked.LeftFoot = !tracked.LeftFoot;
		PlayAt(StepFor(TerrainAt(entity.Position)), entity.Position, volume, tracked.LeftFoot ? 1.0f : 0.95f, 0.0f, 0.1f);
	}

	void GameAudio::PlayMonsterStep(const EntityState& entity, Tracked& tracked, float moved)
	{
		if (moved > 1.5f || LengthSquared(entity.Position - m_Focus) > MonsterHearing * MonsterHearing)
		{
			tracked.Stride = 0.0f;
			return;
		}
		const float stride = entity.Kind == EntityKind::Brute ? 1.6f : 1.3f;
		tracked.Stride += moved;
		if (tracked.Stride < stride)
			return;
		tracked.Stride -= stride;
		if (entity.Kind == EntityKind::Brute)
		{
			PlayAt(Cue::Rock, entity.Position, 0.35f, 0.55f, 0.0f, 0.1f);
			PlayAt(Cue::Bone, entity.Position, 0.4f, 0.8f);
		}
		else
			PlayAt(Cue::Bone, entity.Position, entity.Kind == EntityKind::Chaser ? 0.5f : 0.4f);
	}

	void GameAudio::PlayBossChange(const EntityState& entity, const EntityState& before)
	{
		const Vec2 at = entity.Position;
		for (const uint8_t threshold : BossPhaseHealth)
			if (before.Health > threshold && entity.Health <= threshold && entity.Health > 0)
			{
				if (entity.Kind == EntityKind::Wyrm)
					PlayAt(Cue::WyrmRoar, at, 0.9f, 1.12f);
				else
				{
					PlayAt(Cue::SpiritShout, at, 1.0f);
					PlayAt(Cue::Wraith, at, 0.7f, 0.9f, 0.4f);
				}
				m_LastBossVoice = m_Time;
			}
		if (entity.Kind != EntityKind::Wyrm || entity.Variant == before.Variant)
			return;
		if (entity.Variant == 1)
		{
			PlayAt(Cue::Rumble, at, 0.9f);
			PlayAt(Cue::Rock, at, 0.9f, 0.6f);
			PlayAt(Cue::Rock, at, 0.7f, 0.7f, 0.15f);
		}
		else
		{
			PlayAt(Cue::Explosion, at, 0.8f, 0.7f);
			PlayAt(Cue::Rock, at, 1.0f, 0.65f);
			PlayAt(Cue::WyrmRoar, at, 0.8f, 1.1f, 0.25f);
		}
	}

	void GameAudio::AnnounceBoss(const EntityState& boss)
	{
		if (boss.NetId == m_AnnouncedBoss)
			return;
		m_AnnouncedBoss = boss.NetId;
		m_LastBossVoice = m_Time;
		const Vec2 offset = boss.Position - m_Focus;
		const float distance = Length(offset);
		const Vec2 at = distance > 16.0f ? m_Focus + offset * (16.0f / distance) : boss.Position;
		if (boss.Kind == EntityKind::Wyrm)
		{
			PlayAt(Cue::WyrmRoar, at, 1.0f);
			PlayAt(Cue::Rumble, at, 0.6f, 1.0f, 0.3f);
		}
		else
		{
			PlayAt(Cue::DemonCall, at, 1.0f);
			PlayAt(Cue::Wraith, at, 0.8f, 1.0f, 0.6f);
		}
	}

	void GameAudio::PlayEnemyDeath(const EntityState& last)
	{
		const Vec2 at = last.Position;
		switch (last.Kind)
		{
		case EntityKind::Archivist:
			PlayAt(Cue::Wraith, at, 1.0f, 0.8f);
			PlayAt(Cue::Collapse, at, 1.0f, 0.7f, 0.3f);
			PlayAt(Cue::Explosion, at, 0.5f, 0.8f);
			PlayAt(Cue::Shield, at, 0.6f, 0.8f, 0.5f);
			return;
		case EntityKind::Wyrm:
			PlayAt(Cue::WyrmRoar, at, 1.0f, 0.8f);
			PlayAt(Cue::Rumble, at, 0.8f, 1.0f, 0.4f);
			PlayAt(Cue::Glass, at, 0.8f, 0.8f, 0.6f);
			PlayAt(Cue::Glass, at, 0.7f, 1.0f, 0.75f);
			PlayAt(Cue::Explosion, at, 0.7f, 0.7f, 0.6f);
			return;
		case EntityKind::Brute:
			PlayAt(Cue::Collapse, at, 1.0f, 0.8f);
			PlayAt(Cue::Body, at, 0.9f, 0.6f, 0.15f);
			PlayAt(Cue::Growl, at, 0.6f, 0.6f);
			break;
		case EntityKind::Chaser:
			PlayAt(Cue::Collapse, at, 0.8f);
			PlayAt(Cue::Bone, at, 0.5f);
			if (m_Random.Below(3) == 0)
				PlayAt(Cue::GhoulSnarl, at, 0.4f, 0.8f);
			break;
		default:
			PlayAt(Cue::Collapse, at, 0.8f, 1.05f);
			break;
		}
		if (last.Has(EntityFlags::Elite))
			PlayAt(Cue::BeastDeath, at, 0.5f);
	}

	void GameAudio::PlayLootDrop(const EntityState& entity, const GameClient& client)
	{
		if (LootKind(entity.Variant) != LootKind::Item || (entity.Owner != NoPlayer && entity.Owner != client.GetPlayerId()))
			return;
		const Rarity rarity = ItemTagRarity(entity.Tag);
		PlayAt(Cue::Drop, entity.Position, 0.5f, 1.0f, 0.0f, 0.2f);
		if (rarity >= Rarity::Rare)
			PlayAt(Cue::UiRare, entity.Position, rarity == Rarity::Rare ? 0.6f : 0.9f, rarity == Rarity::Mythic ? 1.2f : 1.0f, 0.05f);
		if (rarity >= Rarity::Legendary)
			PlayAt(Cue::Shield, entity.Position, 0.5f, 1.25f, 0.1f);
	}

	void GameAudio::PlayLootTaken(const EntityState& last)
	{
		switch (LootKind(last.Variant))
		{
		case LootKind::Health:
			PlayUi(Cue::Heal, 0.55f, 1.1f);
			break;
		case LootKind::Mana:
			PlayUi(Cue::ManaStart, 0.6f, 1.1f);
			break;
		case LootKind::Item:
			break;
		default:
			PlayUi(Cue::Shield, 0.6f, 1.1f);
			PlayUi(Cue::UiNotify, 0.5f, 1.0f, 0.1f);
			break;
		}
	}

	void GameAudio::PlayHits()
	{
		uint32_t played = 0;
		for (const HitEvent& hit : m_Hits)
		{
			if (played++ >= MaxHitSounds)
				break;
			const auto it = m_Tracked.find(hit.Target);
			const EntityKind kind = it != m_Tracked.end() ? it->second.Last.Kind : hit.OnPlayer ? EntityKind::Player : EntityKind::Chaser;
			const Vec2 at = hit.Position;
			if ((hit.OnPlayer || kind == EntityKind::Player) && m_Status.Mode == GameMode::Brawl)
			{
				if (hit.Damage == 0)
				{
					const bool self = hit.Target == m_Self;
					PlayAt(Cue::Body, at, self ? 0.85f : 0.6f, 1.1f);
					PlayAt(Cue::WhooshHeavy, at, self ? 0.6f : 0.4f, 1.25f);
				}
				continue;
			}
			if (hit.OnPlayer || kind == EntityKind::Player)
			{
				const bool self = hit.Target == m_Self;
				PlayAt(Cue::Body, at, self ? 0.9f : 0.55f);
				PlayAt(Cue::PlateLight, at, self ? 0.55f : 0.35f);
				if (hit.Critical)
					PlayAt(Cue::HeavyHit, at, 0.5f);
				continue;
			}
			if (kind == EntityKind::Wyrm)
			{
				PlayAt(Cue::Rock, at, 0.6f, 1.2f);
				PlayAt(Cue::GlassLight, at, 0.5f);
			}
			else
				switch (hit.Damages)
				{
				case Element::Physical:
					PlayAt(Cue::Body, at, 0.6f);
					PlayAt(Cue::Bone, at, 0.55f);
					break;
				case Element::Fire:
					PlayAt(Cue::BodyLight, at, 0.45f);
					PlayAt(Cue::Bone, at, 0.3f);
					break;
				case Element::Cold:
					PlayAt(Cue::BodyLight, at, 0.45f);
					PlayAt(Cue::GlassLight, at, 0.45f, 1.2f);
					break;
				case Element::Lightning:
					PlayAt(Cue::BodyLight, at, 0.45f);
					PlayAt(Cue::Bone, at, 0.35f, 1.2f);
					break;
				default:
					PlayAt(Cue::Soft, at, 0.45f, 1.3f);
					break;
				}
			if (hit.Critical)
				PlayAt(Cue::HeavyHit, at, 0.55f, 1.1f);
		}
	}

	void GameAudio::PlayItemGain(const Item& item, bool equipped)
	{
		switch (GetItemBase(item.Base).Slot)
		{
		case EquipSlot::Weapon:
			PlayUi(Cue::Unsheathe, equipped ? 0.7f : 0.6f);
			break;
		case EquipSlot::Head:
		case EquipSlot::Body:
		case EquipSlot::Feet:
			PlayUi(Cue::Belt, 0.6f);
			PlayUi(Cue::Cloth, equipped ? 0.5f : 0.4f, 1.0f, 0.05f);
			break;
		case EquipSlot::Amulet:
		case EquipSlot::Ring:
			PlayUi(Cue::Coins, 0.45f, 1.4f);
			PlayUi(Cue::GlassLight, 0.4f, 1.3f, 0.05f);
			break;
		default:
			PlayUi(Cue::GlassLight, 0.6f, 1.2f);
			PlayUi(equipped ? Cue::Click : Cue::GlassLight, 0.4f, 1.5f, 0.08f);
			break;
		}
	}

	void GameAudio::ObserveLocalPlayer(const GameClient& client)
	{
		const LocalPlayerState& local = client.GetLocalPlayer();
		if (!local.Valid)
		{
			m_Soundscape->SetHeartbeat(0.0f);
			return;
		}
		const bool alive = local.State.Alive;
		if (!alive && !m_Downed.IsActive())
			m_Downed = m_Engine.StartSnapshot({ .Buses = { { m_Buses.World, -6.0f, 700.0f }, { m_Buses.Ambience, -4.0f, 900.0f }, { m_Buses.Music, -6.0f, 1600.0f } } }, 0.6f);
		else if (alive && m_Downed.IsActive())
			m_Downed.Stop(1.2f);

		const float fraction = local.MaxHealth > 0 ? float(local.Health) / float(local.MaxHealth) : 1.0f;
		const float danger = alive ? std::clamp((0.35f - fraction) / 0.25f, 0.0f, 1.0f) : 0.0f;
		m_Soundscape->SetHeartbeat(danger);
		if (danger > 0.01f)
		{
			if (!m_Danger.IsActive())
				m_Danger = m_Engine.StartSnapshot({ .Buses = { { m_Buses.World, 0.0f, 3000.0f }, { m_Buses.Ambience, -6.0f, 0.0f }, { m_Buses.Music, -4.0f, 0.0f } } }, 0.5f, danger);
			else
				m_Danger.SetIntensity(danger, 0.3f);
		}
		else if (m_Danger.IsActive())
			m_Danger.Stop(0.8f);

		const MaterialStock& materials = client.GetMaterials();
		if (m_Local.Valid && m_Time - m_LastCoins > 0.3f && client.GetSession().Mode != GameMode::Royale)
			for (size_t i = 0; i < materials.size(); i++)
				if (materials[i] > m_Materials[i])
				{
					PlayUi(Cue::Coins, 0.45f, Material(i) == Material::Gold ? 1.0f : 1.15f);
					m_LastCoins = m_Time;
					break;
				}
	}

	void GameAudio::ObserveInventory(const GameClient& client)
	{
		const ServerInventory& inventory = client.GetInventory();
		const auto has = [](const auto& items, uint32_t id) { return std::any_of(items.begin(), items.end(), [id](const Item& item) { return item.Id == id; }); };
		const auto heldBefore = [&](uint32_t id) { return has(m_Inventory.Pack, id) || has(m_Inventory.Gear, id) || has(m_Inventory.Gems, id); };
		for (size_t i = 0; i < inventory.Upgrades.size(); i++)
			if (inventory.Upgrades[i] > m_Inventory.Upgrades[i])
			{
				PlayUi(Cue::UiReward, 0.7f);
				return;
			}
		for (const Item& item : inventory.Pack)
			if (item.IsValid() && !heldBefore(item.Id))
			{
				PlayItemGain(item, false);
				return;
			}
		const auto equipped = [&](const auto& now, const auto& before) -> const Item*
		{
			for (size_t i = 0; i < now.size(); i++)
				if (now[i].IsValid() && now[i].Id != before[i].Id && has(m_Inventory.Pack, now[i].Id))
					return &now[i];
			return nullptr;
		};
		if (const Item* item = equipped(inventory.Gear, m_Inventory.Gear))
			PlayItemGain(*item, true);
		else if (const Item* gem = equipped(inventory.Gems, m_Inventory.Gems))
			PlayItemGain(*gem, true);
	}

	void GameAudio::ObserveStatus(const GameClient& client)
	{
		const GameStatus& status = client.GetStatus();
		if (status.Mode == GameMode::Mission && status.Mission != m_Status.Mission)
		{
			if (status.Mission == MissionPhase::Boss)
			{
				for (const auto& [netId, tracked] : m_Tracked)
					if (IsBossKind(tracked.Last.Kind))
						AnnounceBoss(tracked.Last);
				PlayFlat(Cue::Gong, m_Buses.Stingers, 0.6f, 0.7f);
			}
			else if (status.Mission == MissionPhase::Victory)
			{
				PlayStinger(MusicStinger::Victory);
				PlayUi(Cue::UiReward, 0.8f, 1.0f, 0.2f);
			}
			else if (status.Mission == MissionPhase::Failed)
				PlayStinger(MusicStinger::Defeat);
		}
		if (status.Mode == GameMode::Horde && status.Phase != m_Status.Phase)
		{
			if (status.Phase == GamePhase::Wave)
				PlayFlat(Cue::Horn, m_Buses.Stingers, 0.8f);
			else if (status.Phase == GamePhase::Defeat)
				PlayStinger(MusicStinger::Defeat);
			else if (m_Status.Phase == GamePhase::Wave)
			{
				PlayUi(Cue::UiReward, 0.8f);
				PlayFlat(Cue::Drum, m_Buses.Stingers, 0.6f, 1.0f, 0.1f);
			}
		}
	}
}
