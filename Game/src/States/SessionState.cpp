#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/MenuBackdrop.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/VoidGaze.h"
#include "Rendering/WorldRenderer.h"
#include "States/MainMenuState.h"
#include "UI/Palette.h"
#include "UI/SettingsUi.h"
#include "UI/Ui.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Content/EmberIsland.h>
#include <GameShared/Content/Fighters.h>
#include <GameShared/Content/HubLayout.h>
#include <GameShared/World/Collision.h>
#include <RadianceEngine/Core/Log.h>
#include <RadianceEngine/Window/Input.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <random>
#include <string>
#include <utility>

namespace Game
{
	namespace
	{
		constexpr float HurtFlashSeconds = 0.08f;
		constexpr float HealthTrailHold = 0.45f;
		constexpr float HealthTrailRate = 0.9f;
		constexpr float WardRegrowSeconds = 0.6f;
		constexpr float CrystalFormSeconds = 0.45f;
		constexpr float CrystalBurstHeight = 1.0f;
		constexpr float FlashCover = 0.65f;
		constexpr float FlashBright = 1.0f;
		constexpr float FlashWhite = 0.3f;
		constexpr float TraumaDecay = 1.5f;
		constexpr float ShakeOffset = 0.3f;
		constexpr float ShakeRoll = 0.022f;
		constexpr float ShakeFrequency = 16.0f;
		constexpr float KickStiffness = 22.0f;
		constexpr float KickDamping = 0.75f;
		constexpr float KickImpulse = 7.0f;
		constexpr float RumbleReach = 16.0f;
		constexpr float DefaultBloom = 0.3f;
		constexpr float CalderaBloom = 0.14f;
		constexpr float CalderaLavaLight = 0.8f;
		constexpr float CalderaLavaCrust = 0.24f;
		constexpr float ForgeFxRange = 30.0f;
		constexpr float OwnRumble = 0.7f;
		constexpr float GuardTrauma = 0.12f;
		constexpr float HitTrauma = 0.25f;
		constexpr float HitTraumaScale = 0.4f;
		constexpr float HitStack = 0.1f;
		constexpr float HitTraumaDamage = 20.0f;
		constexpr float HitTraumaPush = 22.0f;
		constexpr int32_t HeavyHitDamage = 15;
		constexpr float HeavyHitPush = 16.0f;
		constexpr float HitStopMin = 0.04f;
		constexpr float HitStopMax = 0.06f;
		constexpr float SwordSwingStop = 0.035f;
		constexpr float SwordStageCycle = 4.0f;
		constexpr float SwordStageHold = 1.5f;
		constexpr float SwordCleaveStop = 0.065f;
		constexpr float SwordFinishLinger = 0.3f;
		constexpr float SwordSweepSide = 1.0f;
		constexpr float SwordScale = 1.3f;
		constexpr float SwordRightSign = 1.0f;
		constexpr float SwordSweepBack = 2.2f;
		constexpr float SwordSweepCreep = 0.22f;
		constexpr float SwordSweepEnd = 1.85f;
		constexpr float SwordSweepOvershoot = 0.22f;
		constexpr float SwordCleaveBack = -0.8f;
		constexpr float SwordCleaveCreep = 0.18f;
		constexpr float SwordCleaveLand = 1.62f;
		constexpr float SwordSweepRoll = 0.6f;
		constexpr float SwordOvershootSeconds = 0.16f;
		constexpr float SwordTwirl = 6.2832f;
		constexpr float SwordTwirlLift = 0.3f;
		constexpr float SwordSpringFast = 38.0f;
		constexpr float SwordSpringSlow = 16.0f;
		constexpr float SwordSmearSpeed = 8.0f;
		constexpr uint32_t SwordSmearSteps = 28;
		constexpr float SwordSmearOverlap = 0.6f;
		constexpr float SwordSmearGlow = 0.07f;
		constexpr uint32_t SwordPieceCount = 16;
		constexpr uint32_t SwordSpireCount = 8;
		constexpr float SwordClusterSize = 0.62f;
		constexpr float SwordSpireStagger = 0.12f;
		constexpr float SwordPieceStagger = 0.12f;
		constexpr uint32_t SwordHistorySize = 6;
		constexpr float SwordPieceGravity = 16.0f;
		constexpr float SwordPieceFade = 0.3f;
		constexpr float SwordSpirePop = 28.0f;
		constexpr float SwordSpireGrowth = 0.22f;
		constexpr float SwordOrbit = 0.95f;
		constexpr float SwordRecoverSeconds = 0.42f;
		constexpr float SwordConfirmWait = 0.14f;
		constexpr float SwordTrailHold = 0.1f;
		constexpr float SwordTellSelf = 0.1f;
		constexpr float SwordTellRival = 0.28f;
		constexpr float SwordCleaveKick = 1.4f;
		constexpr float SwordCleaveTrauma = 0.18f;
		constexpr float SwordShatterKick = 1.0f;
		constexpr float SwordShatterTrauma = 0.3f;
		constexpr float SwordLandStop = 0.06f;
		constexpr float SwordShatterRumble = 0.5f;
		constexpr float SwordScarLife = 2.2f;
		constexpr float SwordThrowSpin = 24.0f;
		constexpr glm::vec3 SwordShardTint{ 0.39f, 1.0f, 0.68f };
		constexpr float DangerHealth = 0.3f;
		constexpr float DangerFloor = 0.08f;
		constexpr float DangerVignette = 0.8f;
		constexpr float DangerDesaturation = 0.3f;
		constexpr float DangerFade = 4.0f;
		constexpr float DangerBeat = 1.1f;
		constexpr float DangerBeatRise = 0.9f;
		constexpr float DamageNumberSeconds = 0.9f;
		constexpr size_t MaxDamageNumbers = 160;
		constexpr float NoticeSeconds = 3.5f;
		constexpr size_t MaxNotices = 4;
		constexpr float SaveDelay = 1.5f;
		constexpr const char* SlotKeys[] = { "LMB", "RMB", "Q", "R", "F" };
		static_assert(std::size(SlotKeys) == AbilitySlots);
		constexpr glm::vec3 EliteGold{ 1.0f, 0.78f, 0.25f };
		constexpr glm::vec3 EnemyFire{ 1.0f, 0.35f, 0.15f };
		constexpr glm::vec3 EnemyDanger{ 1.0f, 0.25f, 0.12f };

		glm::vec3 FighterHue(EntityKind kind)
		{
			switch (kind)
			{
			case EntityKind::CrystalDuelist:
				return { 0.55f, 0.9f, 1.0f };
			case EntityKind::ShardArcher:
				return { 0.6f, 1.0f, 0.62f };
			case EntityKind::GeodeBrute:
				return { 1.0f, 0.72f, 0.42f };
			case EntityKind::MirrorMoth:
				return { 0.72f, 0.78f, 1.0f };
			case EntityKind::LensWisp:
				return { 0.5f, 1.0f, 0.95f };
			case EntityKind::ShardObelisk:
				return { 0.75f, 0.5f, 1.0f };
			default:
				return { 0.86f, 0.6f, 1.0f };
			}
		}
		constexpr float FlierHover = 0.4f;
		constexpr float ObeliskHeight = 2.0f;
		constexpr float PortalHoverRadius = 1.9f;
		constexpr float ProjectileSink = 3.0f;
		constexpr float ProjectileClimb = 8.0f;
		constexpr glm::vec3 SpearIce{ 0.32f, 0.72f, 1.3f };
		constexpr float SpearLaneSize = 1.5f;
		constexpr float SpearLaneSeconds = 2.8f;
		constexpr float SpearLaneStep = 0.55f;
		constexpr float SpikeRiftSeconds = 3.5f;
		constexpr float SpikeRuinSeconds = 0.7f;
		constexpr float SpikeKick = 3.2f;
		constexpr float SpikeKickReach = 12.0f;
		constexpr float SkewerPop = 4.6f;
		constexpr float SkewerStop = 0.07f;
		constexpr float SpikeStagePeriod = 3.4f;
		constexpr float SpikeHeave = 0.45f;
		constexpr glm::vec3 NovaIce{ 0.28f, 0.55f, 1.0f };
		constexpr float NovaSpreadSeconds = 0.3f;
		constexpr float NovaThawSeconds = 0.45f;
		constexpr float NovaFadeSeconds = 0.6f;
		constexpr float NovaShatterSeconds = 0.14f;
		constexpr float NovaEruptLife = 1.7f;
		constexpr float NovaFrostFade = 0.8f;
		constexpr float NovaMistInterval = 0.12f;
		constexpr float NovaStompSeconds = 0.14f;
		constexpr size_t MaxNovaSpikes = 48;
		constexpr float DeathDissolveDelay = 0.9f;
		constexpr float DeathDissolveSeconds = 1.4f;
		constexpr float DownedDissolve = 0.35f;
		constexpr float SpawnDissolveSeconds = 0.75f;
		constexpr float DissolveGlow = 7.0f;
		constexpr float BodyHeight = 1.9f;
		constexpr glm::vec3 ArcaneDeath{ 0.74f, 0.3f, 1.0f };
		constexpr glm::vec3 StormDeath{ 0.55f, 0.6f, 1.0f };
		constexpr float SpellLightGain = 14.0f;
		constexpr float ArcChest = 0.9f;
		constexpr float MeteorGlowRadius = 2.4f;
		constexpr glm::vec3 WorldUp{ 0.0f, 1.0f, 0.0f };
		constexpr glm::vec3 CometDrop{ 2.4f, 12.5f, 5.0f };
		constexpr float ArcCasterReach = 1.5f;
		constexpr float ArcAnchorReach = 1.3f;
		constexpr float WandTipFresh = 0.15f;
		constexpr glm::vec3 ArcLight{ 0.26f, 0.45f, 1.0f };
		constexpr glm::vec3 StormLight{ 0.42f, 0.3f, 1.0f };
		constexpr glm::vec3 StormTelegraph{ 0.62f, 0.5f, 1.0f };
		constexpr glm::vec3 ArcTint{ 0.5f, 0.66f, 1.0f };
		constexpr float BodyGravity = 26.0f;
		constexpr float BodyClimb = 16.0f;
		constexpr float BodySnap = 3.5f;
		constexpr float LiftHover = 1.6f;
		constexpr float LiftPull = 6.0f;
		constexpr float SkyAbyss = -60.0f;
		constexpr float StormSunlight = 0.16f;
		constexpr float StormGloom = 0.8f;
		constexpr float RainRate = 260.0f;
		constexpr float CameraGlideSeconds = 0.5f;
		constexpr float CameraGlideRate = 7.0f;
		constexpr float CameraNear = 0.5f;
		constexpr float CameraZoomMin = 0.45f;
		constexpr float CameraZoomStep = 0.88f;
		constexpr float CameraZoomRate = 12.0f;
		constexpr float CorpseSeconds = 3.0f;
		constexpr float CorpseSinkSeconds = 1.0f;
		constexpr size_t MaxCorpses = 24;
		constexpr float KillWindow = 0.5f;
		constexpr int PackPayload = 1;
		constexpr int GearPayload = 2;
		constexpr int GemPayload = 3;
		constexpr float VictoryPoseSeconds = 1.9f;
		constexpr float PlayerRim = 1.0f;
		constexpr float ContactRadius = 0.7f;
		constexpr float ContactStrength = 0.85f;

		float Perlin(float x, uint32_t seed)
		{
			const float cell = std::floor(x);
			const float f = x - cell;
			const auto gradient = [seed](int32_t i)
			{
				uint32_t h = uint32_t(i) * 0x9E3779B1u ^ seed * 0x85EBCA77u;
				h ^= h >> 15;
				h *= 0x2C1B3C6Du;
				h ^= h >> 12;
				return float(h & 0xFFFFu) / 32767.5f - 1.0f;
			};
			const float u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
			return 2.0f * std::lerp(gradient(int32_t(cell)) * f, gradient(int32_t(cell) + 1) * (f - 1.0f), u);
		}

		float ImpactRumble(AbilityId ability, float blast)
		{
			switch (ability)
			{
			case AbilityId::Comet:
				return 0.5f;
			case AbilityId::PurpleFireball:
				return 0.2f + 0.5f * blast;
			case AbilityId::EarthSpike:
				return 0.42f;
			case AbilityId::StormStrike:
				return 0.3f;
			default:
				return 0.0f;
			}
		}

		uint32_t StormSeed(Vec2 position)
		{
			return uint32_t(int32_t(std::lround(position.x * 16.0f))) * 73856093u ^ uint32_t(int32_t(std::lround(position.y * 16.0f))) * 19349663u;
		}

		glm::vec3 SpellTint(AbilityId ability)
		{
			switch (CreditOf(ability))
			{
			case AbilityId::StormStrike:
				return StormTelegraph;
			case AbilityId::ArcLightning:
				return ArcTint;
			case AbilityId::ObsidianBlades:
				return ObsidianSheen * 0.7f;
			default:
				return SkillColor(ability);
			}
		}

		UiColor CardIconTint(const CardDef& card)
		{
			return card.Kind == CardKind::Upgrade ? WithAlpha(SkillColor(card.Spell)) : card.Rarity == CardRarity::Legendary ? Hex(0xFFCF5A) : Hex(0xB58CFF);
		}

		template<typename... Arguments>
		std::string Format(const char* format, Arguments... arguments)
		{
			char text[160];
			std::snprintf(text, sizeof(text), format, arguments...);
			return text;
		}

		float HealthBarHeight(EntityKind kind)
		{
			if (IsCrawlerKind(kind))
				return (kind == EntityKind::CrawlerEgg ? 0.6f : 1.15f) * CrawlerScale(kind);
			switch (kind)
			{
			case EntityKind::Chaser:
				return 1.05f;
			case EntityKind::Shooter:
				return 1.45f;
			case EntityKind::Brute:
				return 1.75f;
			case EntityKind::Caster:
				return 1.75f;
			default:
				return 2.0f;
			}
		}

		float FlightLift(AbilityId ability)
		{
			return ability == AbilityId::FrostSpear ? SpearHoldHeight : ability == AbilityId::PurpleFireball ? FireballHoldHeight : ability == AbilityId::ObsidianBlades ? ObsidianHoldHeight : ability == AbilityId::EmberBolt ? EmberFlightHeight : ProjectileHeight;
		}

		float BladeFanAngle(float facing, uint32_t slot)
		{
			return facing + ProjectileOffset(BladePattern(), slot) * ObsidianFanAngle;
		}

		glm::vec3 BladeRest(const glm::vec3& body, float facing, uint32_t slot)
		{
			const float rank = std::abs(float(slot) - float(ObsidianVolley / 2));
			return body + Heading(facing) * 0.08f + Heading(BladeFanAngle(facing, slot)) * ObsidianFanReach + glm::vec3(0.0f, 0.03f * (float(ObsidianVolley / 2) - rank), 0.0f);
		}

		void ApplyHallDusk(SceneLighting& lighting, float dusk)
		{
			const float path = dusk * dusk * (3.0f - 2.0f * dusk);
			const float azimuth = std::lerp(0.85f, -1.45f, path);
			const float elevation = std::lerp(1.0f, 0.44f, dusk);
			lighting.SunDirection = -glm::vec3(std::sin(azimuth) * std::cos(elevation), std::sin(elevation), std::cos(azimuth) * std::cos(elevation));
			const float warm = glm::smoothstep(0.3f, 1.0f, dusk);
			const float dark = glm::smoothstep(0.15f, 1.0f, dusk);
			lighting.Dusk = dark;
			lighting.SunColor = glm::mix(glm::vec3(1.0f, 0.94f, 0.82f), glm::vec3(1.0f, 0.6f, 0.34f), warm);
			lighting.SunIntensity = std::lerp(2.5f, 2.3f, warm);
			lighting.SkyColor = glm::mix(glm::vec3(0.22f, 0.33f, 0.52f), glm::vec3(0.07f, 0.08f, 0.19f), dark);
			lighting.GroundColor = glm::mix(glm::vec3(0.22f, 0.24f, 0.28f), glm::vec3(0.09f, 0.065f, 0.09f), dark);
			lighting.Haze = glm::mix(glm::vec3(0.56f, 0.68f, 0.86f), glm::vec3(0.16f, 0.12f, 0.24f), dark);
			lighting.SkyGlow = glm::vec4(glm::mix(glm::vec3(1.0f, 0.93f, 0.8f), glm::vec3(1.0f, 0.46f, 0.2f), warm), std::lerp(2.2f, 3.0f, warm));
			lighting.Grade = {
				.Saturation = std::lerp(1.08f, 1.14f, dark),
				.MidSaturation = 0.16f,
				.Shadows = glm::mix(glm::vec3(0.92f, 0.97f, 1.08f), glm::vec3(0.86f, 0.92f, 1.2f), dark),
				.Highlights = glm::mix(glm::vec3(1.04f, 1.01f, 0.95f), glm::vec3(1.1f, 1.0f, 0.88f), dark),
			};
		}

		SceneLighting MapLighting(MapKind map)
		{
			SceneLighting lighting;
			lighting.Grade = { .MidSaturation = 0.1f, .Shadows = { 0.97f, 0.99f, 1.04f }, .Highlights = { 1.02f, 1.0f, 0.97f } };
			if (map == MapKind::Hub)
			{
				lighting.SunDirection = { 0.83f, -0.7f, 0.56f };
				lighting.SunColor = { 1.0f, 0.87f, 0.7f };
				lighting.SunIntensity = 3.7f;
				lighting.SkyColor = { 0.34f, 0.33f, 0.58f };
				lighting.GroundColor = { 0.26f, 0.15f, 0.09f };
				lighting.EmissionGi = 3.5f;
				lighting.Floating = true;
				lighting.Haze = { 0.07f, 0.055f, 0.13f };
				lighting.DepthHole = { EmberCenter.x, EmberCenter.y, EmberCavernRadii[4] + 1.6f, -0.35f };
				lighting.SheenToward = { 0.12f, 0.78f, 0.62f };
				lighting.SheenColor = { 1.0f, 0.9f, 0.8f };
				lighting.SheenIntensity = 3.0f;
				lighting.ContactShadow = 0.45f;
				lighting.Grade = { .Saturation = 1.15f, .MidSaturation = 0.3f, .Shadows = { 0.92f, 0.96f, 1.1f }, .Highlights = { 1.06f, 1.01f, 0.92f } };
			}
			else if (map == MapKind::Caldera)
			{
				lighting.SunDirection = { -0.35f, -1.0f, 0.55f };
				lighting.SunColor = { 0.86f, 0.76f, 1.0f };
				lighting.SunIntensity = 1.2f;
				lighting.SkyColor = { 0.14f, 0.1f, 0.27f };
				lighting.GroundColor = { 0.1f, 0.045f, 0.03f };
				lighting.EmissionGi = 3.0f;
				lighting.Grade = { .MidSaturation = 0.15f, .Shadows = { 0.9f, 0.97f, 1.12f }, .Highlights = { 1.04f, 1.0f, 0.95f } };
			}
			else if (map == MapKind::SkyRuins)
			{
				lighting.EmissionGi = 2.5f;
				lighting.Floating = true;
				lighting.Daylight = true;
				lighting.ContactShadow = 0.45f;
				ApplyHallDusk(lighting, 0.0f);
			}
			else if (map == MapKind::StormPeak)
			{
				lighting.SunDirection = { 0.612f, -0.659f, -0.437f };
				lighting.SunColor = { 0.84f, 0.81f, 1.0f };
				lighting.SunIntensity = 3.0f;
				lighting.ContactShadow = 0.45f;
				lighting.SkyColor = { 0.27f, 0.29f, 0.48f };
				lighting.GroundColor = { 0.15f, 0.15f, 0.17f };
				lighting.EmissionGi = 3.0f;
				lighting.Floating = true;
				lighting.Storm = true;
				lighting.Haze = { 0.3f, 0.31f, 0.48f };
				lighting.Grade = { .MidSaturation = 0.08f, .Shadows = { 0.9f, 0.96f, 1.12f }, .Highlights = { 1.05f, 1.0f, 0.94f } };
			}
			else if (map == MapKind::FrozenLake)
			{
				lighting.SunDirection = { -0.72f, -0.66f, -0.2f };
				lighting.SunColor = { 0.72f, 0.82f, 1.0f };
				lighting.SunIntensity = 2.0f;
				lighting.SkyColor = { 0.12f, 0.19f, 0.27f };
				lighting.GroundColor = { 0.05f, 0.09f, 0.11f };
				lighting.EmissionGi = 2.5f;
				lighting.ContactShadow = 0.45f;
				lighting.Lake.Active = true;
				lighting.Grade = { .Saturation = 1.1f, .MidSaturation = 0.18f, .Shadows = { 0.9f, 0.99f, 1.1f }, .Highlights = { 1.0f, 1.0f, 1.0f } };
			}
			return lighting;
		}
	}

	SessionState::SessionState(GameApp& app, SessionConfig config)
		: m_App(app), m_Config(std::move(config)), m_Extent{ app.GetWindow().Width(), app.GetWindow().Height() }
	{
		m_BrawlRules.Mode = app.GetSettings().Rules.Mode;
		m_SoloBots = 1 + std::random_device{}() % 3;
		m_RunRules = app.GetSettings().Run;
		m_Characters = std::make_unique<CharacterLibrary>(app.GetWorldRenderer().GetModels());
		m_Effects = std::make_unique<EffectSystem>(app.GetWorldRenderer().GetParticles());
		m_Cosmetics = std::make_unique<CosmeticVisuals>(app.GetWorldRenderer().GetModels());
		m_Crawlers = std::make_unique<CrawlerSystem>(app.GetWorldRenderer().GetModels());
		m_MeteorMesh = app.GetWorldRenderer().GetModels().FindMesh("Rock_3_F_Color1");
		constexpr const char* ChunkMeshes[] = { "Rock_3_A_Color1", "Rock_3_B_Color1", "Rock_3_C_Color1", "Rock_3_D_Color1" };
		for (size_t i = 0; i < m_ChunkMeshes.size(); i++)
			m_ChunkMeshes[i] = app.GetWorldRenderer().GetModels().FindMesh(ChunkMeshes[i]);
		m_ObeliskMesh = app.GetWorldRenderer().GetModels().FindMesh("PeakCrystalAmethyst0");
		m_SpellMeshes = RegisterSpellMeshes(app.GetWorldRenderer());
		constexpr const char* CritterMeshes[] = { "Critters/Cat_Body", "Critters/Cat_Head", "Critters/Cat_Tail", "Critters/Duck", "Critters/Balloon", "Critters/Dummy_Post", "Critters/Dummy_Body" };
		for (size_t i = 0; i < m_Critters.size(); i++)
			for (uint32_t extra = 0;; extra++)
			{
				const std::optional<uint32_t> mesh = app.GetWorldRenderer().GetModels().FindMesh(extra == 0 ? std::string(CritterMeshes[i]) : std::string(CritterMeshes[i]) + "." + std::to_string(extra));
				if (!mesh)
					break;
				m_Critters[i].push_back(*mesh);
			}
		constexpr const char* ChessPieces[] = { "Pawn", "Knight", "Bishop", "Rook", "Queen", "King" };
		for (size_t i = 0; i < m_ChessMeshes.size(); i++)
			for (uint32_t extra = 0;; extra++)
			{
				const std::string name = std::string(i < 6 ? "Chess/White" : "Chess/Black") + ChessPieces[i % 6];
				const std::optional<uint32_t> mesh = app.GetWorldRenderer().GetModels().FindMesh(extra == 0 ? name : name + "." + std::to_string(extra));
				if (!mesh)
					break;
				m_ChessMeshes[i].push_back(*mesh);
			}
		NetLibrary::SetSimulation(app.GetSettings().Simulation);
		app.GetGameAudio().Reset();
		SteamPlatform& steam = app.GetSteam();
		steam.Unlock(m_Config.Profile.Achievements);
		m_Config.Profile.Achievements |= steam.GetUnlocked();
		if (m_Config.Host)
		{
			const Settings& settings = app.GetSettings();
			m_Server = std::make_unique<GameServer>(ServerConfig{
				.Port = m_Config.Port,
				.Relay = steam.IsReady(),
				.StartMode = m_Config.StartMode,
				.Map = m_Config.Map,
				.Rules = settings.Rules,
				.Run = settings.Run,
			});
			if (!m_Server->Start())
			{
				m_Server.reset();
				m_Error = "Can't host on UDP port " + std::to_string(m_Config.Port) + ", it may be in use";
				return;
			}
			m_Client.ConnectLocal(m_Server->OpenLocalConnection(), m_Config.Profile);
			m_LobbyTicket = steam.OpenLobby(settings.FriendsCanJoin, m_Server->GetConfig().MaxPlayers);
		}
		else if (m_Config.RelayHost != 0)
		{
			m_LobbyTicket = m_Config.LobbyTicket;
			if (!m_Client.ConnectRelay(m_Config.RelayHost, m_Config.Profile))
				m_Error = m_Client.GetDisconnectReason();
		}
		else if (!m_Client.Connect(m_Config.Address, m_Config.Profile))
			m_Error = m_Client.GetDisconnectReason();
	}

	SessionState::~SessionState()
	{
		SaveProgress(true);
		m_Client.Disconnect();
		PostProcess& post = m_App.GetWorldRenderer().GetPost();
		post.Vignette = 0.0f;
		post.Desaturation = 0.0f;
		m_Server.reset();
		m_App.GetSteam().CloseLobby(m_LobbyTicket);
		m_App.GetWindow().SetCursorMode(Rdn::CursorMode::Normal);
	}

	void SessionState::ReturnToMenu(std::string message)
	{
		if (m_Leaving)
			return;
		m_Leaving = true;
		SaveProgress(true);
		m_App.ChangeState(std::make_unique<MainMenuState>(m_App, std::move(message)));
	}

	bool SessionState::IsProfileDirty() const
	{
		return m_Client.GetSpellsVersion() != m_SavedSpellsVersion || m_Client.GetFeatsVersion() != m_SavedFeatsVersion || m_Client.GetStyleVersion() != m_SavedStyleVersion;
	}

	void SessionState::SaveProgress(bool force)
	{
		if (m_Client.GetState() != ClientState::InGame || !IsProfileDirty() || (!force && m_SaveTimer > 0.0f))
			return;
		m_App.SaveCharacter(m_Client.GetProfile());
		m_SavedSpellsVersion = m_Client.GetSpellsVersion();
		m_SavedFeatsVersion = m_Client.GetFeatsVersion();
		m_SavedStyleVersion = m_Client.GetStyleVersion();
		m_SaveTimer = -1.0f;
	}

	void SessionState::OnTravel()
	{
		m_RunPauseSent = false;
		m_SeenOffer = {};
		m_SeenOffers = 0;
		m_RunPending.reset();
		m_RunBanishing = false;
		m_SeenTravels = m_Client.GetTravelCount();
		m_Station.reset();
		m_Fx.clear();
		m_Corpses.clear();
		m_Crawlers->Clear();
		m_Effects->Clear();
		m_App.GetWorldRenderer().GetParticles().Clear();
		m_App.GetWorldRenderer().GetTrails().Clear();
		m_DamageNumbers.clear();
		m_ProjectileHeights.clear();
		m_BodyHeights.clear();
		m_SpikeRuins.clear();
		m_SpikeShards.clear();
		m_NovaFields.clear();
		m_VoidWells.clear();
		m_FireballBursts.clear();
		m_CometChunks.clear();
		m_Arcs.clear();
		m_ArcFades.clear();
		m_Shields.clear();
		m_ShieldShards.clear();
		m_Wards.clear();
		m_WardShards.clear();
		m_LevelRenderer = LevelRenderer{};
		m_CameraPlaced = false;
		m_HoveredPortal = -1;
		m_ReachablePortal = -1;
		const SessionInfo& session = m_Client.GetSession();
		if (m_SeenMode != GameMode::Count)
		{
			m_Fade = 1.0f;
			m_App.GetGameAudio().Reset();
			m_App.GetGameAudio().PlayEvent(AudioEvent::Travel);
		}
		m_SeenMode = session.Mode;
		m_SeenMatch = MatchPhase::Countdown;
		m_SeenCountdown = -1;
		m_HasPlan = false;
		m_RestoreAge = 1e9f;
		m_Risen.clear();
		m_Fallen.clear();
		m_SeenBrawlPhase = BrawlPhase::Draft;
		m_SeenRound = 0;
		m_FoughtRound = 0;
		m_LoadoutOpen = true;
		m_PickingSlot.reset();
		m_ChosenCard.reset();
		m_CardReveal = 0.0f;
		m_SelectedSpell.reset();
		m_CardNews.clear();
		m_KnownCards.fill(0);
		m_CardsKnown = false;
		m_Feed.clear();
		m_Client.TakeKills();
		m_BannerTitle = GetMapDef(session.Map).Name;
		m_BannerColor = WithAlpha(EliteGold);
		if (session.Mode == GameMode::Hub)
			m_BannerText = "The spellbook, the Caldera Gate and the Shard Rift await";
		else if (session.Mode == GameMode::Brawl)
			m_BannerText = "Spellbrawl, " + std::to_string(session.Rules.Rounds) + " rounds. Pick three spells, then " + (session.Rules.Mode == BrawlMode::Damage ? std::string("outlast everyone") : std::string("shove everyone into the ") + TermsFor(session.Map).Hazard);
		else if (session.Mode == GameMode::Shardfall && UsesRooms(session.Run))
		{
			const RoomPlace place = FindRoom(session.Run.Seed, session.MapSeed);
			const MapRoom room = place.Act > 0 ? MakeActMap(session.Run.Seed, place.Act).At(place.Room) : MapRoom{};
			const char* act = place.Act == 1 ? "I" : place.Act == 2 ? "II" : "III";
			m_BannerTitle = room.Kind == RoomKind::Guardian ? std::string(GetMapDef(session.Map).Name) : std::string(RoomKindNames[size_t(room.Kind)]);
			if (place.Act == 1 && place.Room == 0)
				m_BannerText = "Three acts of rooms, each ending at a guardian. Clear a room, then choose your door: each one shows what waits behind it";
			else if (room.Kind == RoomKind::Fight)
				m_BannerText = Format("Act %s, %s. Clear it for: %s", act, GetMapDef(session.Map).Name, RoomRewardNames[size_t(room.Reward)]);
			else if (room.Kind == RoomKind::Elite)
				m_BannerText = Format("Act %s, %s. A champion guards an elite chest", act, GetMapDef(session.Map).Name);
			else if (room.Kind == RoomKind::Rest)
				m_BannerText = "Heal at the left altar, or train at the right one to rank up a spell";
			else if (room.Kind == RoomKind::Shop)
				m_BannerText = "A crystal merchant: cards, sigils and services for shards";
			else if (room.Kind == RoomKind::Shrine)
				m_BannerText = "An old shrine: cleanse a curse, transform a card, or trade health for a rank";
			else if (room.Kind == RoomKind::Elite)
				m_BannerText = Format("Act %s, %s. A champion waits: break its armour twice, then survive its rage", act, GetMapDef(session.Map).Name);
			else
				m_BannerText = GetMapDef(session.Map).Description;
			m_BannerTime = 2.5f;
			return;
		}
		else if (session.Mode == GameMode::Shardfall)
			m_BannerText = session.Map != MapKind::ProvingHollow ? std::string(GetMapDef(session.Map).Description) : "Three acts of crystal waves, each ending at a guardian. You start with Ember Bolt and your first spellbook spell, and every level brings a card";
		m_BannerTime = 4.0f;
	}

	void SessionState::OpenStation(StationKind kind)
	{
		if (kind == StationKind::WishingVoid)
		{
			const Level* level = m_Client.GetLevel();
			const Station* edge = level ? level->FindStation(kind) : nullptr;
			const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
			if (!edge || !self || m_WishCooldown > 0.0f)
				return;
			m_WishCooldown = WishCooldownSeconds;
			m_Client.MakeWish();
			m_App.GetGameAudio().PlayStation(kind);
			m_Effects->Burst(Fx::CoinToss, Ground(edge->Position + Vec2(1.1f, 0.0f), 0.9f));
			return;
		}
		if (m_Station == kind)
		{
			m_Station.reset();
			return;
		}
		m_Station = kind;
		m_App.GetGameAudio().PlayStation(kind);
	}

	void SessionState::UpdateInteractions()
	{
		m_NearStation = -1;
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const bool alive = self && !self->Has(EntityFlags::Dead);
		if (!level || !alive)
		{
			m_Station.reset();
			return;
		}
		float nearest = 1e9f;
		if (m_Client.GetSession().Mode == GameMode::Hub)
			for (size_t i = 0; i < level->Stations.size(); i++)
			{
				const float distance = Length(level->Stations[i].Position - self->Position);
				if (distance <= level->Stations[i].Radius && distance < nearest)
				{
					nearest = distance;
					m_NearStation = int32_t(i);
				}
			}
		if (m_Station)
		{
			const Station* station = level->FindStation(*m_Station);
			if (m_Client.GetSession().Mode != GameMode::Hub || !station || Length(station->Position - self->Position) > station->Radius + 2.0f)
				m_Station.reset();
		}
	}

	bool SessionState::Interact()
	{
		const Level* level = m_Client.GetLevel();
		if (m_NearStation >= 0 && level && size_t(m_NearStation) < level->Stations.size())
		{
			OpenStation(level->Stations[size_t(m_NearStation)].Kind);
			return true;
		}
		return false;
	}

	void SessionState::Update(float dt)
	{
		if (!m_Error.empty())
			return ReturnToMenu(m_Error);
		if (m_Leaving)
			return;
		UpdateSteam(dt);
		m_Time += dt;
		m_FpsFrames++;
		if ((m_FpsWindow += dt) >= 0.5f)
		{
			m_Fps = float(m_FpsFrames) / m_FpsWindow;
			m_FpsFrames = 0;
			m_FpsWindow = 0.0f;
		}
		if (m_Client.GetState() == ClientState::InGame)
			HandleHotkeys();
		if (const bool hold = m_Paused && m_Client.GetSession().Mode == GameMode::Shardfall; m_Client.GetState() == ClientState::InGame && hold != m_RunPauseSent)
		{
			m_Client.PauseRun(hold);
			m_RunPauseSent = hold;
		}

		LatchButtonPresses();
		UpdateAim();
		if (m_Server)
			m_Client.SetTickPhase(m_Server->GetTickPhase());
		m_Client.Predict(dt, [this] { return SampleInput(); });
		if (m_Server)
			m_Server->Update(dt);
		m_Client.Receive(dt);
		if (m_Client.GetState() == ClientState::Disconnected)
			return ReturnToMenu(m_Client.GetDisconnectReason().empty() ? "Disconnected from the server" : m_Client.GetDisconnectReason());
		if (m_Client.GetState() != ClientState::InGame)
			return;
		if (m_Client.GetTravelCount() != m_SeenTravels || m_SeenMode == GameMode::Count)
			OnTravel();
		SyncSpellbook();

		if (IsProfileDirty())
		{
			if (m_SaveTimer < 0.0f)
				m_SaveTimer = SaveDelay;
			else if ((m_SaveTimer -= dt) <= 0.0f)
				SaveProgress(true);
		}
		m_BannerTime = std::max(m_BannerTime - dt, 0.0f);
		m_WishCooldown = std::max(m_WishCooldown - dt, 0.0f);
		UpdateCamera(dt);
		m_AimPoint = MouseAimPoint();
		UpdateInteractions();
		UpdateChess();
		UpdatePortals(dt);
		UpdateArena(dt);
		m_App.GetWorldRenderer().GetParticles().Advance(dt);
		UpdateEffects(dt);
		UpdateFeedback(dt);
		UpdateRunFeel(dt);
		UpdateDanger(dt);
		UpdateWonders();
		UpdateMatchFeedback(dt);
		GameAudio& audio = m_App.GetGameAudio();
		audio.Observe(m_Client, dt, m_CameraTarget, m_Camera.Eye);
		audio.SetOverlay(m_Station.has_value(), m_Paused);
	}

	float SessionState::GroundHeight(Vec2 position) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level)
			return 0.0f;
		return m_LevelRenderer.GroundHeight(*level, m_Client.GetSession().Mode == GameMode::Brawl ? &m_Client.GetStatus().Brawl : nullptr, position, m_Client.GetArenaClock(m_Client.GetPresentTick()));
	}

	bool SessionState::IsOpenAir(Vec2 position) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level)
			return false;
		if (!level->Peak.IsEmpty() && level->Peak.RimDistance(position) < 0.0f)
			return true;
		if (level->Kind == MapKind::Caldera && m_Client.GetSession().Mode == GameMode::Brawl)
			return IsBrawlLava(*level, m_Client.GetStatus().Brawl.Plan, m_Client.GetArenaClock(m_Client.GetPresentTick()), position);
		if (!level->Hall.IsEmpty())
			return level->Hall.Outline.RimDistance(position) < 0.0f;
		return !level->Map.IsInside(int32_t(std::floor(position.x)), int32_t(std::floor(position.y))) || level->Map.IsAbyssAt(position);
	}

	bool SessionState::OverGap(Vec2 position) const
	{
		const Level* level = m_Client.GetLevel();
		return level && !level->Peak.IsEmpty() && level->Peak.RimDistance(position) < 0.0f;
	}

	bool SessionState::InStorm() const
	{
		return m_Client.GetSession().Map == MapKind::StormPeak;
	}

	Fx SessionState::BurnBurst() const
	{
		return InStorm() ? Fx::LightningBurst : m_Client.GetSession().Map == MapKind::SkyRuins ? Fx::FrostBurst : Fx::FireBurst;
	}

	float SessionState::LaunchHeight(const EntityState& entity) const
	{
		const Vec2 back = FromAngle(entity.Facing) * -0.25f;
		const float lift = FlightLift(AbilityOf(entity));
		for (int32_t step = 0; step < 10; step++)
			if (const Vec2 point = entity.Position + back * float(step); !IsOpenAir(point))
				return GroundHeight(point) + lift;
		return GroundHeight(entity.Position) + lift;
	}

	void SessionState::UsePortal(int32_t portal)
	{
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!level || !self || portal < 0 || size_t(portal) >= level->Portals.size())
			return;
		if (!IsInPortalRange(*level, self->Position, uint32_t(portal)))
		{
			m_Hint = "Get closer to use the door to the " + level->Portals[size_t(portal)].Label;
			m_HintTime = 2.0f;
			return;
		}
		m_PendingInteract = uint8_t(portal + 1);
	}

	void SessionState::UpdatePortals(float dt)
	{
		m_HintTime = std::max(m_HintTime - dt, 0.0f);
		m_Fade = std::max(m_Fade - dt / 0.35f, 0.0f);
		if (m_Client.GetTeleportCount() != m_SeenTeleports)
		{
			m_SeenTeleports = m_Client.GetTeleportCount();
			m_Fade = 1.0f;
			m_App.GetGameAudio().PlayEvent(AudioEvent::Teleport);
		}

		m_HoveredPortal = -1;
		m_ReachablePortal = -1;
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (!level || !self || m_Overview)
			return;
		float nearest = 1e9f;
		const bool mouseFree = !m_Paused && !m_App.UiWantsMouse();
		for (size_t i = 0; i < level->Portals.size(); i++)
		{
			const Portal& portal = level->Portals[i];
			const float distance = LengthSquared(portal.Position - self->Position);
			if (IsInPortalRange(*level, self->Position, uint32_t(i)) && distance < nearest)
			{
				nearest = distance;
				m_ReachablePortal = int32_t(i);
			}
			if (!mouseFree || distance > 400.0f || (portal.Hidden && m_ReachablePortal != int32_t(i)))
				continue;
			const std::optional<glm::vec3> point = MouseOnPlane(level->Map.GetHeight(portal.Position) + 1.0f);
			if (point && LengthSquared(Vec2(point->x, point->z) - portal.Position) < PortalHoverRadius * PortalHoverRadius)
				m_HoveredPortal = int32_t(i);
		}
	}

	void SessionState::HandleHotkeys()
	{
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Escape) && !m_App.UiWantsKeyboard())
		{
			const BrawlPhase brawlPhase = m_Client.GetStatus().Brawl.Phase;
			const bool choosing = m_Client.GetSession().Mode == GameMode::Brawl && m_Client.GetStatus().Match != MatchPhase::Ended && (brawlPhase == BrawlPhase::Draft || brawlPhase == BrawlPhase::Pick);
			if (m_Paused && (m_PauseSettings || m_PauseWardrobe || m_PauseInvite || m_PauseAchievements))
			{
				m_PauseSettings = false;
				m_PauseWardrobe = false;
				m_PauseInvite = false;
				m_PauseAchievements = false;
			}
			else if (m_Station && !m_Paused)
				m_Station.reset();
			else if (choosing && m_LoadoutOpen && !m_Paused)
				m_LoadoutOpen = false;
			else
			{
				m_Paused = !m_Paused;
				m_PauseSettings = false;
				m_PauseWardrobe = false;
				m_PauseInvite = false;
				m_PauseAchievements = false;
				m_PauseTime = 0.0f;
			}
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F3))
			m_ShowNetStats = !m_ShowNetStats;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F4))
			m_Overview = !m_Overview;
		if (const float scroll = Rdn::Input::GetScrollVertical(); scroll != 0.0f && !m_Paused && !m_Overview && !m_App.UiWantsMouse())
			m_ZoomTarget = std::clamp(m_ZoomTarget * std::pow(CameraZoomStep, scroll), CameraZoomMin, 1.0f);
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F6))
		{
			WorldRenderer& renderer = m_App.GetWorldRenderer();
			renderer.SetDebugView(DebugView((uint32_t(renderer.GetDebugView()) + 1) % uint32_t(DebugView::Count)));
			RDN_LOG("Debug view: {}", DebugViewNames[size_t(renderer.GetDebugView())]);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F7))
		{
			PostProcess& post = m_App.GetWorldRenderer().GetPost();
			post.Tonemap = Tonemapper((uint32_t(post.Tonemap) + 1) % uint32_t(Tonemapper::Count));
			RDN_LOG("Tonemapper: {}", TonemapperNames[size_t(post.Tonemap)]);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F8))
		{
			Settings& settings = m_App.GetSettings();
			settings.Shadows = ShadowQuality((uint32_t(settings.Shadows) + 1) % uint32_t(ShadowQuality::Count));
			m_App.GetWorldRenderer().SetShadowQuality(settings.Shadows);
			RDN_LOG("Shadows: {}", ShadowQualityNames[size_t(settings.Shadows)]);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F9))
		{
			Settings& settings = m_App.GetSettings();
			settings.Gi = GiQuality((uint32_t(settings.Gi) + 1) % uint32_t(GiQuality::Count));
			m_App.GetWorldRenderer().SetGiQuality(settings.Gi);
			RDN_LOG("Global illumination: {}", GiQualityNames[size_t(settings.Gi)]);
		}
		const GameMode sessionMode = m_Client.GetSession().Mode;
		if (!m_Paused && !m_App.UiWantsMouse() && sessionMode == GameMode::Brawl && !m_Client.GetPredictedState().Alive && m_Client.GetLocalPlayer().Watching != NoPlayer)
		{
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseLeft))
				m_Client.Spectate(1);
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseRight))
				m_Client.Spectate(-1);
		}
		UpdateEmoteWheel();
		if (m_Paused || m_App.UiWantsKeyboard())
			return;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::I) && sessionMode == GameMode::Brawl)
			m_LoadoutOpen = !m_LoadoutOpen;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::M) && sessionMode == GameMode::Shardfall)
			m_ActMapOpen = !m_ActMapOpen;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::G) && sessionMode == GameMode::Shardfall)
		{
			m_GrimoireOpen = !m_GrimoireOpen;
			m_MerchantOpen = m_MerchantOpen && !m_GrimoireOpen;
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::E) && sessionMode == GameMode::Shardfall && m_MerchantRoom != UINT32_MAX)
		{
			m_MerchantOpen = !m_MerchantOpen;
			m_GrimoireOpen = m_GrimoireOpen && !m_MerchantOpen;
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::E) && !Interact())
		{
			if (m_ReachablePortal >= 0)
				UsePortal(m_ReachablePortal);
			else if (m_HoveredPortal >= 0)
				UsePortal(m_HoveredPortal);
		}
	}

	void SessionState::UpdateAim()
	{
		const bool mouseFree = !m_Paused && !m_App.UiWantsMouse();
		if (const EntityState* player = m_Client.FindEntity(m_Client.GetPlayerNetId()); player && mouseFree)
			if (const std::optional<glm::vec3> point = MouseAimPoint())
			{
				const Vec2 offset = Vec2(point->x, point->z) - player->Position;
				m_Aim = AngleOf(offset);
				m_AimDistance = Length(offset);
			}
		if (m_App.GetSettings().EmoteStage != 0)
			m_Aim = AngleOf(Vec2(0.4f, -1.0f));
		if (m_App.GetSettings().SwordStage)
		{
			m_Aim = AngleOf(Vec2(-1.0f, 0.12f));
			m_AimDistance = 3.0f;
		}
		m_Client.SetViewAim(m_Aim);
	}

	PlayerInput SessionState::SampleInput()
	{
		PlayerInput input;
		const bool mouseFree = !m_Paused && !m_App.UiWantsMouse();
		input.Aim = m_Aim;
		input.AimDistance = m_AimDistance;
		if (m_Paused || m_App.UiWantsKeyboard())
			return input;

		Vec2 move;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::W))
			move.y += 1.0f;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::S))
			move.y -= 1.0f;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::D))
			move.x += 1.0f;
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::A))
			move.x -= 1.0f;
		input.Move = Normalize(move);
		if (!Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseLeft))
			m_SuppressPrimary = false;
		if (mouseFree && !m_EmoteWheel)
		{
			if (Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseLeft) && !m_SuppressPrimary)
				input.Buttons |= uint8_t(InputButton::Primary);
			if (Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseRight))
				input.Buttons |= uint8_t(InputButton::Skill1);
		}
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::Q))
			input.Buttons |= uint8_t(InputButton::Skill2);
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::R))
			input.Buttons |= uint8_t(InputButton::Skill3);
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::F))
			input.Buttons |= uint8_t(InputButton::Skill4);
		if (Rdn::Input::IsKeyDown(Rdn::KeyCode::Space))
			input.Buttons |= uint8_t(InputButton::Dodge);
		input.Buttons |= std::exchange(m_LatchedButtons, uint8_t(0));
		if (m_App.GetSettings().SwordStage && std::fmod(m_SwordStageClock, SwordStageCycle) < SwordStageHold)
			input.Buttons |= uint8_t(InputButton::Skill1);
		input.Interact = std::exchange(m_PendingInteract, uint8_t(0));
		return input;
	}

	void SessionState::ExplainBlockedSkill(uint32_t slot)
	{
		if (m_Client.GetState() != ClientState::InGame)
			return;
		const SkillTraits& skill = m_Client.GetTraits().Skills[slot];
		const PlayerState& state = m_Client.GetPredictedState();
		if (!state.Alive)
			return;
		if (skill.IsEmpty())
		{
			if (m_Client.GetSession().Mode != GameMode::Shardfall)
				return;
			m_Hint = std::string("No spell on ") + SlotKeys[slot] + " yet";
		}
		else if (state.Mana + 1e-3f < skill.ManaCost)
			m_Hint = std::string("Not enough mana for ") + GetAbility(skill.Ability).Name;
		else
			return;
		m_HintTime = 1.6f;
	}

	void SessionState::LatchButtonPresses()
	{
		if (m_Paused)
		{
			m_LatchedButtons = 0;
			return;
		}
		if (!m_App.UiWantsMouse() && !m_EmoteWheel)
		{
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseLeft))
			{
				if (m_HoveredPortal >= 0)
				{
					UsePortal(m_HoveredPortal);
					m_SuppressPrimary = true;
				}
				else
					m_LatchedButtons |= uint8_t(InputButton::Primary);
			}
			if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::MouseRight))
			{
				m_LatchedButtons |= uint8_t(InputButton::Skill1);
				ExplainBlockedSkill(1);
			}
		}
		if (m_App.UiWantsKeyboard())
			return;
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Q))
		{
			m_LatchedButtons |= uint8_t(InputButton::Skill2);
			ExplainBlockedSkill(2);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::R))
		{
			m_LatchedButtons |= uint8_t(InputButton::Skill3);
			ExplainBlockedSkill(3);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::F))
		{
			m_LatchedButtons |= uint8_t(InputButton::Skill4);
			ExplainBlockedSkill(4);
		}
		if (Rdn::Input::IsKeyPressed(Rdn::KeyCode::Space))
			m_LatchedButtons |= uint8_t(InputButton::Dodge);
	}

	std::optional<glm::vec3> SessionState::MouseOnPlane(float height) const
	{
		if (m_Extent.Width == 0 || m_Extent.Height == 0)
			return std::nullopt;
		const float x = 2.0f * Rdn::Input::GetMouseX() / float(m_Extent.Width) - 1.0f;
		const float y = 2.0f * Rdn::Input::GetMouseY() / float(m_Extent.Height) - 1.0f;
		const glm::mat4 inverse = glm::inverse(m_ViewProjection);
		glm::vec4 nearPoint = inverse * glm::vec4(x, y, 0.0f, 1.0f);
		glm::vec4 farPoint = inverse * glm::vec4(x, y, 1.0f, 1.0f);
		nearPoint /= nearPoint.w;
		farPoint /= farPoint.w;
		const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
		if (std::abs(direction.y) < 1e-6f)
			return std::nullopt;
		return glm::vec3(nearPoint) + direction * ((height - nearPoint.y) / direction.y);
	}

	std::optional<glm::vec3> SessionState::MouseAimPoint() const
	{
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		return MouseOnPlane((self ? (OverGap(self->Position) ? m_CameraTarget.y : GroundHeight(self->Position)) : 0.0f) + ProjectileHeight);
	}

	std::optional<glm::vec2> SessionState::ProjectToScreen(const glm::vec3& position) const
	{
		const glm::vec4 clip = m_Camera.ViewProjection * glm::vec4(position, 1.0f);
		if (clip.w <= 1e-4f)
			return std::nullopt;
		return glm::vec2((clip.x / clip.w * 0.5f + 0.5f) * float(m_Extent.Width), (clip.y / clip.w * 0.5f + 0.5f) * float(m_Extent.Height));
	}

	void SessionState::UpdateCamera(float dt)
	{
		const Level* level = m_Client.GetLevel();
		glm::vec3 target = m_CameraTarget;
		const EntityState* watched = nullptr;
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (local.Valid && !local.State.Alive && local.Watching != NoPlayer)
			for (const EntityState& entity : m_Client.GetEntities())
				if (entity.Kind == EntityKind::Player && entity.Owner == local.Watching)
					watched = &entity;
		bool falling = false;
		bool aloft = false;
		if (watched)
		{
			target = Ground(watched->Position);
			if (m_CameraPlaced && OverGap(watched->Position))
				target.y = m_CameraTarget.y;
		}
		else if (const EntityState* player = m_Client.FindEntity(m_Client.GetPlayerNetId()))
		{
			target = Ground(player->Position);
			falling = player->Has(EntityFlags::Falling);
			aloft = falling || player->Has(EntityFlags::Lifted);
			if (player->Has(EntityFlags::Lifted) && m_Client.GetPredictedState().IsFalling())
			{
				const Vec2 ledge = m_Client.GetPredictedState().Ledge;
				target.x = ledge.x;
				target.z = ledge.y;
			}
			if (m_CameraPlaced && (aloft || OverGap(player->Position)))
				target.y = m_CameraTarget.y;
		}
		else if (level)
			target = Ground(level->PlayerSpawn);
		m_CameraGlide = aloft ? CameraGlideSeconds : std::max(m_CameraGlide - dt, 0.0f);
		if (!m_CameraPlaced || m_Fade > 0.5f || (m_CameraGlide <= 0.0f && LengthSquared(Vec2(target.x - m_CameraTarget.x, target.z - m_CameraTarget.z)) > 25.0f))
			m_CameraTarget = target;
		else
		{
			const float follow = falling ? 0.0f : m_CameraGlide > 0.0f ? std::min(1.0f, dt * CameraGlideRate) : 1.0f;
			m_CameraTarget.x += (target.x - m_CameraTarget.x) * follow;
			m_CameraTarget.z += (target.z - m_CameraTarget.z) * follow;
			m_CameraTarget.y += (target.y - m_CameraTarget.y) * std::min(1.0f, dt * 10.0f);
		}
		m_CameraPlaced = true;

		if (m_App.GetSettings().FireStage != 0)
			m_ZoomTarget = m_Zoom = m_App.GetSettings().FireStage >= 2 ? 1.0f : CameraZoomMin;
		if (m_App.GetSettings().SwordStage)
		{
			m_ZoomTarget = m_Zoom = CameraZoomMin;
			m_SwordStageClock += dt;
			if (m_Client.GetTraits().Skills[1].Ability != AbilityId::CrystalSword && std::fmod(m_SwordStageClock, 1.0f) < dt)
				m_Client.PickBrawlSpell(0, AbilityId::CrystalSword);
		}
		if (const uint8_t stage = m_App.GetSettings().EmoteStage; stage != 0)
		{
			m_ZoomTarget = m_Zoom = CameraZoomMin;
			m_EmoteStageClock += dt;
			if (std::fmod(m_EmoteStageClock, 4.0f) < std::fmod(m_EmoteStageClock - dt, 4.0f))
				m_Client.PlayEmote(Emote(stage));
		}
		m_Zoom += (m_ZoomTarget - m_Zoom) * std::min(1.0f, dt * CameraZoomRate);
		const float aspect = float(std::max(m_Extent.Width, 1u)) / float(std::max(m_Extent.Height, 1u));
		glm::vec3 focus = m_CameraTarget - glm::vec3(0.0f, 0.0f, CameraFocusOffset * m_Zoom);
		glm::vec3 eye = focus + glm::vec3(0.0f, CameraHeight, -CameraBack) * m_Zoom;
		if (m_Overview && level)
		{
			const float extent = float(std::max(level->Map.GetWidth(), level->Map.GetHeight()));
			focus = { float(level->Map.GetWidth()) * 0.5f, 0.0f, float(level->Map.GetHeight()) * 0.5f };
			eye = focus + glm::vec3(0.0f, extent * 1.25f, -extent * 0.45f);
		}
		if (const auto& shot = m_App.GetSettings().ShotCamera)
		{
			eye = { (*shot)[0], (*shot)[1], (*shot)[2] };
			focus = { (*shot)[3], (*shot)[4], (*shot)[5] };
		}
		const float far = m_Overview ? 600.0f : 200.0f;
		glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(CameraFov), aspect, CameraNear, far);
		projection[1][1] *= -1.0f;
		m_ViewProjection = projection * glm::lookAtLH(eye, focus, glm::vec3(0.0f, 1.0f, 0.0f));

		m_Trauma = std::max(m_Trauma - TraumaDecay * dt, 0.0f);
		m_ShakeClock += dt;
		const float step = std::min(dt, 1.0f / 30.0f);
		m_KickSpeed -= (KickStiffness * KickStiffness * m_Kick + 2.0f * KickDamping * KickStiffness * m_KickSpeed) * step;
		m_Kick += m_KickSpeed * step;
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		if (const float strength = m_App.GetSettings().ScreenShake; !m_Overview && strength > 0.0f)
		{
			const float shake = m_Trauma * m_Trauma * strength;
			const glm::vec3 forward = glm::normalize(focus - eye);
			const glm::vec3 right = glm::normalize(glm::cross(up, forward));
			const float t = m_ShakeClock * ShakeFrequency;
			const glm::vec3 offset = (right * Perlin(t, 1u) + glm::cross(forward, right) * Perlin(t, 2u)) * (ShakeOffset * shake * m_Zoom) + glm::vec3(m_Kick.x, 0.0f, m_Kick.y) * (strength * m_Zoom);
			eye += offset;
			focus += offset;
			up = glm::vec3(glm::rotate(glm::mat4(1.0f), ShakeRoll * shake * Perlin(t, 3u), forward) * glm::vec4(up, 0.0f));
		}
		const glm::mat4 view = glm::lookAtLH(eye, focus, up);
		m_Camera = WorldCamera{ .View = view, .Projection = projection, .ViewProjection = projection * view, .Eye = eye, .Near = CameraNear, .Far = far };
	}

	LevelView SessionState::ComputeView() const
	{
		LevelView view;
		view.Time = m_Time;
		view.Focus = { m_CameraTarget.x, m_CameraTarget.z };
		view.HoveredPortal = m_HoveredPortal;
		view.ReachablePortal = m_ReachablePortal;
		view.GateFocus = m_Station == StationKind::CalderaGate;
		if (m_Client.GetSession().Mode == GameMode::Brawl)
		{
			view.Brawl = &m_Client.GetStatus().Brawl;
			view.Risen = &m_Risen;
			view.Restore = std::min(m_RestoreAge / RestoreSeconds, 1.0f);
			view.Clock = m_Client.GetArenaClock(m_Client.GetPresentTick());
			view.ForceLeviathan = m_App.GetSettings().Leviathan;
		}
		const Level* level = m_Client.GetLevel();
		if (m_Overview || !level || m_App.GetSettings().ShotCamera)
		{
			view.Min = { -1e4f, -1e4f };
			view.Max = { 1e4f, 1e4f };
			return view;
		}
		Vec2 min = { 1e9f, 1e9f };
		Vec2 max = { -1e9f, -1e9f };
		const glm::mat4 inverse = glm::inverse(m_ViewProjection);
		const float overscan = 1.0f + 2.0f * m_App.GetWorldRenderer().GetGuardBand();
		for (const glm::vec2 corner : { glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f) })
			for (const float plane : { m_CameraTarget.y - (m_Lighting.Floating ? 45.0f : 4.0f), m_CameraTarget.y + 3.5f })
			{
				glm::vec4 nearPoint = inverse * glm::vec4(corner * overscan, 0.0f, 1.0f);
				glm::vec4 farPoint = inverse * glm::vec4(corner * overscan, 1.0f, 1.0f);
				nearPoint /= nearPoint.w;
				farPoint /= farPoint.w;
				const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
				if (std::abs(direction.y) < 1e-6f)
					continue;
				const glm::vec3 point = glm::vec3(nearPoint) + direction * ((plane - nearPoint.y) / direction.y);
				min = { std::min(min.x, point.x), std::min(min.y, point.z) };
				max = { std::max(max.x, point.x), std::max(max.y, point.z) };
			}
		view.Min = min - Vec2(1.5f, 1.5f);
		view.Max = max + Vec2(1.5f, 1.5f);
		view.CameraMin = view.Min;
		view.CameraMax = view.Max;
		if (m_App.GetWorldRenderer().GetShadowQuality() != ShadowQuality::Off)
		{
			const glm::vec3 sun = glm::normalize(m_Lighting.SunDirection);
			const float reach = ShadowCasterHeight / std::max(-sun.y, 0.2f);
			view.Min -= Vec2(std::max(sun.x, 0.0f), std::max(sun.z, 0.0f)) * reach;
			view.Max += Vec2(std::max(-sun.x, 0.0f), std::max(-sun.z, 0.0f)) * reach;
		}
		return view;
	}

	void SessionState::UpdateEffects(float dt)
	{
		m_Effects->Update(dt);
		if (constexpr float Period = 40.0f; m_App.GetSettings().GrassStage && std::floor((m_Time + Period - 5.0f) / Period) != std::floor((m_Time - dt + Period - 5.0f) / Period))
		{
			const glm::vec3 right = glm::normalize(glm::vec3(m_Camera.View[0][0], 0.0f, m_Camera.View[2][0]));
			const glm::vec3 depth(-right.z, 0.0f, right.x);
			const auto spot = [&](float across, float along)
			{
				const glm::vec3 point = glm::vec3(m_CameraTarget.x, 0.0f, m_CameraTarget.z) + right * across + depth * along;
				return Ground(Vec2(point.x, point.z));
			};
			const auto height = [this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); };
			m_Effects->Impact(AbilityId::Eruption, spot(-3.4f, 1.2f), 0.0f, 1.0f, SkillColor(AbilityId::Eruption), 11u, height);
			m_Effects->Impact(AbilityId::CrystalRain, spot(3.4f, 1.2f), 0.0f, 1.0f, SkillColor(AbilityId::CrystalRain), 12u, height);
			m_Effects->Impact(AbilityId::Firebomb, spot(0.0f, -3.2f), 0.0f, 1.0f, SkillColor(AbilityId::Firebomb), 13u, height);
		}
		UpdateSpikeShards(dt);
		for (SpikeRuin& ruin : m_SpikeRuins)
			ruin.Age += dt;
		std::erase_if(m_SpikeRuins, [](const SpikeRuin& ruin) { return ruin.Age >= SpikeRuinSeconds; });
		UpdateNovaFields(dt);
		UpdateVoidWells(dt);
		for (auto& [netId, fx] : m_Fx)
		{
			fx.Seen = false;
			fx.Hurt = false;
			fx.Push = {};
			fx.SinceHit += dt;
			fx.Flash = std::max(fx.Flash - dt, 0.0f);
			fx.Age += fx.Stop > 0.0f ? 0.0f : dt;
			fx.Stop = std::max(fx.Stop - dt, 0.0f);
			const float health = float(fx.Last.Health) / 255.0f;
			fx.TrailHold = std::max(fx.TrailHold - dt, 0.0f);
			fx.BloodFlash = std::max(fx.BloodFlash - dt * 2.5f, 0.0f);
			fx.Trail = health >= fx.Trail ? health : fx.TrailHold > 0.0f ? fx.Trail : std::max(health, fx.Trail - HealthTrailRate * dt);
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			const auto [it, inserted] = m_Fx.try_emplace(entity.NetId, EntityFx{ .LastHealth = entity.Health, .Last = entity });
			EntityFx& fx = it->second;
			if (inserted && entity.Kind == EntityKind::Projectile && AbilityOf(entity) == AbilityId::ObsidianBlades)
				LaunchBlade(fx, entity);
			if (inserted)
				OnEntityAppeared(entity);
			else if (entity.Kind == EntityKind::Player && entity.Has(EntityFlags::Dashing) && !fx.Last.Has(EntityFlags::Dashing) && !entity.Has(EntityFlags::Dead))
				m_Effects->Burst(Fx::Dodge, Ground(entity.Position, 0.1f));
			if (!inserted && entity.Kind == EntityKind::Player)
			{
				const float clocked = fx.Stop > 0.0f ? 0.0f : dt;
				SpearMoments(fx, entity, clocked);
				SpikeMoments(fx, entity, clocked);
				FireballMoments(fx, entity, clocked);
				ObsidianMoments(fx, entity, clocked);
				SwordMoments(fx, entity, clocked);
			}
			if (IsFighterKind(entity.Kind))
				ShieldMoments(entity, dt);
			if (!inserted && entity.Kind == EntityKind::GeodeBrute && entity.Has(EntityFlags::Shattered) && !fx.Last.Has(EntityFlags::Shattered))
			{
				const glm::vec3 chest = Ground(entity.Position, 1.0f);
				m_Effects->Burst(Fx::ShieldShatter, chest, glm::vec3(0.0f, 1.0f, 0.0f), 1.3f);
				m_Effects->AirShockwave(chest, 1.8f, 0.38f, 0.09f, 0.22f);
			}
			if (entity.Kind == EntityKind::Player)
			{
				ShieldMoments(entity, dt);
				WardMoments(entity, dt);
				if (entity.Has(EntityFlags::Barrier) && !entity.Has(EntityFlags::Dead))
					fx.CrystalForm = std::min(fx.CrystalForm + dt / CrystalFormSeconds, 1.0f);
				else
				{
					if (!inserted && fx.Last.Has(EntityFlags::Barrier) && !entity.Has(EntityFlags::Dead) && fx.CrystalForm > 0.3f)
					{
						m_Effects->Burst(Fx::ArcaneBurst, Ground(entity.Position, CrystalBurstHeight), glm::vec3(0.0f, 1.0f, 0.0f), 1.1f, glm::vec3(0.6f, 0.88f, 1.0f));
						m_Effects->AirShockwave(Ground(entity.Position, CrystalBurstHeight), 1.4f, 0.3f, 0.06f, 0.2f);
					}
					fx.CrystalForm = 0.0f;
				}
				if (!entity.Has(EntityFlags::Dead))
				{
					if (fx.DeathAge >= 0.0f)
						fx.SpawnAge = 0.0f;
					fx.DeathAge = -1.0f;
					fx.SpawnAge += dt;
				}
				else if (fx.DeathAge < 0.0f)
				{
					fx.DeathAge = inserted ? DeathDissolveDelay + DeathDissolveSeconds : 0.0f;
					fx.DeathColor = DeathColorOf(entity, fx);
				}
				else
					fx.DeathAge += fx.Stop > 0.0f ? 0.0f : dt;
			}
			if (!inserted && entity.Kind == EntityKind::Player && !entity.Has(EntityFlags::Dead))
			{
				if (entity.Has(EntityFlags::Flung) && !fx.Last.Has(EntityFlags::Flung))
					m_Effects->Burst(Fx::Dodge, Ground(entity.Position, 0.2f), glm::vec3(0.0f, 1.0f, 0.0f), 1.4f);
				if (entity.Has(EntityFlags::Burning) && !fx.Last.Has(EntityFlags::Burning))
					m_Effects->Burst(BurnBurst(), Ground(entity.Position, InStorm() ? 0.9f : 0.1f), glm::vec3(0.0f, 1.0f, 0.0f), 0.7f);
			}
			if (!inserted && entity.Kind == EntityKind::Dummy && entity.Has(EntityFlags::Burning) && !fx.Last.Has(EntityFlags::Burning))
				m_Effects->Burst(BurnBurst(), Ground(entity.Position, InStorm() ? 0.6f : 0.1f), glm::vec3(0.0f, 1.0f, 0.0f), 0.8f);
			if (entity.Has(EntityFlags::Falling) || entity.Has(EntityFlags::Lifted))
			{
				if (fx.FallAge < 0.0f)
				{
					fx.FallAge = 0.0f;
					fx.FallFrom = GroundHeight(inserted ? entity.Position : fx.Last.Position);
					if (!inserted && entity.Kind == EntityKind::Dummy && DummyKind(entity.Variant) == DummyKind::Balloon)
						m_Effects->Burst(Fx::BalloonPop, BalloonAnchor(fx.Last) + glm::vec3(0.0f, BalloonLift, 0.0f));
					else if (!inserted)
						m_Effects->Burst(Fx::Dodge, Ground(fx.Last.Position, 0.1f), glm::vec3(0.0f, 1.0f, 0.0f), 1.3f);
				}
				else
					fx.FallAge += dt;
			}
			else
			{
				if (!inserted && entity.Kind == EntityKind::Player && fx.Last.Has(EntityFlags::Lifted))
					m_Effects->Burst(Fx::Gust, Ground(entity.Position, 0.15f));
				fx.FallAge = -1.0f;
			}
			if (!inserted && entity.Health < fx.LastHealth)
			{
				fx.Flash = HurtFlashSeconds;
				fx.TrailHold = HealthTrailHold;
				fx.FlashColor = glm::vec3(1.0f);
				fx.Hurt = true;
			}
			fx.LastHealth = entity.Health;
			fx.Last = entity;
			fx.Seen = true;
		}
		for (const HitEvent& hit : m_Client.GetNewHits())
		{
			if (hit.Ability == uint8_t(AbilityId::PerfectDodge))
				continue;
			if (hit.Warded)
			{
				WardStruck(hit);
				continue;
			}
			if (const auto shielded = m_Fx.find(hit.Target); shielded != m_Fx.end() && shielded->second.Last.Has(EntityFlags::BloodShield) && !shielded->second.Last.Has(EntityFlags::Barrier))
			{
				shielded->second.BloodFlash = 1.0f;
				if (hit.Guarded)
				{
					m_Effects->Burst(Fx::ShieldImpact, Ground(hit.Position, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.7f, glm::vec3(1.0f, 0.16f, 0.24f));
					continue;
				}
			}
			if (hit.Guarded)
			{
				ShieldStruck(hit);
				continue;
			}
			FeelHit(hit);
			const bool sword = hit.Ability < uint8_t(AbilityId::Count) && CreditOf(AbilityId(hit.Ability)) == AbilityId::CrystalSword;
			if (hit.Ability == uint8_t(AbilityId::EarthSpike))
				SkewerHit(hit);
			const auto it = m_Fx.find(hit.Target);
			if (it != m_Fx.end())
			{
				it->second.SinceHit = 0.0f;
				it->second.HitElement = hit.Damages;
				if (hit.Damage > 0)
				{
					const EntityState& struck = it->second.Last;
					Vec2 away = -FromAngle(struck.Facing);
					for (const EntityState& other : m_Client.GetEntities())
						if (other.Kind == EntityKind::Player && other.Owner == hit.Attacker && other.NetId != struck.NetId && LengthSquared(struck.Position - other.Position) > 0.01f)
							away = Normalize(struck.Position - other.Position);
					it->second.Push = away * (std::clamp(0.5f + float(hit.Damage) / 30.0f, 0.6f, 1.5f) * (hit.Critical ? 1.3f : 1.0f));
				}
				if (it->second.DeathAge >= 0.0f && it->second.DeathAge < DeathDissolveDelay)
					it->second.DeathColor = DeathColorOf(it->second.Last, it->second);
			}
			const glm::vec3 point = Ground(hit.Position, it != m_Fx.end() ? BarHeight(it->second.Last) * 0.5f : 0.8f);
			if (hit.Ability == uint8_t(AbilityId::ObsidianBlades))
				StrikeBlade(hit);
			else if (sword)
				SwordHit(hit);
			else
				m_Effects->Burst(HitFor(hit.Damages), point, glm::vec3(0.0f, 1.0f, 0.0f), hit.Critical ? 1.4f : 1.0f);
			if (hit.Critical)
				m_Effects->Burst(Fx::Critical, point);
		}
		for (auto it = m_Fx.begin(); it != m_Fx.end();)
		{
			const EntityFx& fx = it->second;
			if (fx.Seen)
			{
				++it;
				continue;
			}
			if (IsCrawlerKind(fx.Last.Kind))
				m_Crawlers->Vanish(fx.Last, fx.SinceHit < KillWindow, *m_Effects);
			else if (fx.SinceHit < KillWindow && m_Characters->ForEnemy(fx.Last.Kind))
			{
				m_Corpses.push_back({ fx.Last, fx.Animator });
				m_Corpses.back().Entity.Flags |= uint16_t(EntityFlags::Dead);
				m_Effects->Burst(Fx::Death, Ground(fx.Last.Position, 0.6f), glm::vec3(0.0f, 1.0f, 0.0f), fx.Last.GetScale());
			}
			else if (fx.Last.Kind == EntityKind::Projectile)
			{
				const AbilityId ability = AbilityOf(fx.Last);
				const bool spear = ability == AbilityId::FrostSpear;
				glm::vec3 point = ProjectilePoint(fx.Last);
				if (ability == AbilityId::ArcLightning)
				{
					m_ArcFades.push_back(EndArc(it->first, fx.Last));
					m_Arcs.erase(it->first);
					point = m_ArcFades.back().End;
				}
				const auto bodyNear = [&](const EntityState& other) { return (other.Kind == EntityKind::Player || other.Kind == EntityKind::Dummy) && other.Owner != fx.Last.Owner && !other.Has(EntityFlags::Dead) && LengthSquared(other.Position - fx.Last.Position) < 0.8f; };
				const bool struck = ability == AbilityId::ObsidianBlades && ((fx.BladeStruck >= 0.0f && fx.Age - fx.BladeStruck < 0.1f) || std::any_of(m_Client.GetEntities().begin(), m_Client.GetEntities().end(), bodyNear));
				if (ability == AbilityId::ObsidianBlades)
				{
					if (!struck)
						FadeBlade(fx, point);
				}
				else if (const std::optional<Fx> impact = ProjectileImpactFor(ability))
					m_Effects->Burst(*impact, point, glm::vec3(0.0f, 1.0f, 0.0f), spear ? 0.6f + 0.4f * fx.Last.GetScale() : 1.0f);
				if (spear)
					m_Effects->Impact(ability, Ground(fx.Last.Position), fx.Last.Facing, fx.Last.GetScale(), SpearIce, fx.Last.NetId, [this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); });
				if (const auto path = m_SpearPaths.find(it->first); path != m_SpearPaths.end())
				{
					path->second.Last = fx.Last.Position;
					m_Effects->Mark(FrostLane(path->second), SpearLaneSeconds, 0.01f, 1.0f, 1.0f, 0.25f, 1.2f);
					m_SpearPaths.erase(path);
				}
			}
			else if (fx.Last.Kind == EntityKind::Effect && !fx.Last.Has(EntityFlags::Pending) && AbilityOf(fx.Last) == AbilityId::EarthSpike)
				EndSpike(fx.Last);
			else if (fx.Last.Kind == EntityKind::Dummy && !fx.Last.Has(EntityFlags::Falling) && DummyKind(fx.Last.Variant) != DummyKind::Balloon)
			{
				const glm::vec3 splash = Ground(fx.Last.Position, 0.15f);
				const float size = DummyKind(fx.Last.Variant) == DummyKind::Golden ? 1.6f : 1.0f;
				if (fx.Last.Has(EntityFlags::Burning))
					m_Effects->Burst(BurnBurst(), splash, glm::vec3(0.0f, 1.0f, 0.0f), size);
				else
					m_Effects->Burst(Fx::Dodge, splash, glm::vec3(0.0f, 1.0f, 0.0f), 1.4f * size);
				m_Effects->Burst(Fx::Debris, splash, glm::vec3(0.0f, 1.0f, 0.0f), 0.8f * size);
			}
			it = m_Fx.erase(it);
		}
		std::erase_if(m_Shields, [](const auto& item) { return !item.second.Seen; });
		for (auto& [netId, view] : m_Shields)
			view.Seen = false;
		std::erase_if(m_Wards, [](const auto& item) { return !item.second.Seen; });
		for (auto& [netId, view] : m_Wards)
			view.Seen = false;
		UpdateHexShards(m_ShieldShards, dt);
		UpdateDiamondShards(m_WardShards, dt);
		const auto ground = [this](Vec2 point) { return GroundHeight(point); };
		m_Crawlers->Update(dt, m_Client.GetEntities(), ground);
		m_Crawlers->UpdateOrbs(dt, m_Client.GetOrbs(), m_Client.GetEntities(), *m_Effects, ground);

		const bool exact = m_LevelRenderer.GetRelief() != nullptr;
		for (auto& [netId, flight] : m_ProjectileHeights)
			flight.Seen = false;
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind != EntityKind::Projectile)
				continue;
			const auto [it, inserted] = m_ProjectileHeights.try_emplace(entity.NetId, Flight{ LaunchHeight(entity), true });
			Flight& flight = it->second;
			flight.Seen = true;
			if (inserted || IsOpenAir(entity.Position))
				continue;
			const float target = GroundHeight(entity.Position) + FlightLift(AbilityOf(entity));
			if (exact)
				flight.Height = target;
			else
				flight.Height = target >= flight.Height ? std::min(target, flight.Height + ProjectileClimb * dt) : std::max(target, flight.Height - ProjectileSink * dt);
		}
		std::erase_if(m_ProjectileHeights, [](const auto& item) { return !item.second.Seen; });
		std::erase_if(m_SpearPaths, [this](const auto& item) { return !m_Fx.contains(item.first); });
		std::erase_if(m_ChestShift, [this](const auto& item) { return !m_Fx.contains(item.first); });
		std::erase_if(m_Arcs, [this](const auto& item) { return !m_Fx.contains(item.first); });
		for (ArcFade& fade : m_ArcFades)
			fade.Age += dt;
		for (BladeSlash& slash : m_BladeSlashes)
			slash.Age += dt;
		std::erase_if(m_BladeSlashes, [](const BladeSlash& slash) { return slash.Age >= ObsidianSlashSeconds; });
		for (BladeFade& fade : m_BladeFades)
		{
			fade.Age += dt;
			fade.Position += fade.Velocity * dt - glm::vec3(0.0f, (fade.Stuck ? 0.0f : 1.4f) * fade.Age * dt, 0.0f);
			fade.Velocity *= std::exp(-10.0f * dt);
			if (fade.Age >= (fade.Stuck ? ObsidianStickSeconds : ObsidianFadeSeconds))
				m_Effects->Burst(Fx::ObsidianCrumble, fade.Position, Heading(fade.Facing), 0.55f);
		}
		std::erase_if(m_BladeFades, [](const BladeFade& fade) { return fade.Age >= (fade.Stuck ? ObsidianStickSeconds : ObsidianFadeSeconds); });
		UpdateSwordDebris(dt);
		std::erase_if(m_ArcFades, [](const ArcFade& fade) { return fade.Age >= std::max(ArcFadeSeconds, ArcSparkSeconds); });
		for (auto& [netId, body] : m_BodyHeights)
			body.Seen = false;
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind != EntityKind::Player)
				continue;
			const bool abyss = m_Client.GetSession().Mode == GameMode::Brawl && (entity.Has(EntityFlags::Falling) || entity.Has(EntityFlags::Dead)) && m_LevelRenderer.IsOverSky(&m_Client.GetStatus().Brawl, entity.Position, m_Client.GetArenaClock(m_Client.GetPresentTick()));
			float target = abyss ? SkyAbyss : GroundHeight(entity.Position);
			const auto [it, inserted] = m_BodyHeights.try_emplace(entity.NetId, Body{ target, 0.0f, true });
			Body& body = it->second;
			body.Seen = true;
			if (!abyss && OverGap(entity.Position))
				target = body.Height;
			if (entity.Has(EntityFlags::Falling))
			{
				body.Speed += FallGravity * dt;
				body.Height -= body.Speed * dt;
				body.Aloft = true;
			}
			else if (entity.Has(EntityFlags::Lifted))
			{
				body.Height += (target + LiftHover - body.Height) * std::min(dt * LiftPull, 1.0f);
				body.Speed = 0.0f;
				body.Aloft = true;
			}
			else if (body.Aloft)
			{
				body.Speed += BodyGravity * dt;
				body.Height = std::max(target, body.Height - body.Speed * dt);
				body.Aloft = body.Height > target;
				if (!body.Aloft)
					body.Speed = 0.0f;
			}
			else if (exact)
				body.Height = target;
			else if (std::abs(body.Height - target) > BodySnap)
				body = { target, 0.0f, true };
			else if (target >= body.Height)
			{
				body.Height += (target - body.Height) * std::min(dt * BodyClimb, 1.0f);
				body.Speed = 0.0f;
			}
			else
			{
				body.Speed += BodyGravity * dt;
				body.Height = std::max(target, body.Height - body.Speed * dt);
				if (body.Height <= target)
					body.Speed = 0.0f;
			}
		}
		std::erase_if(m_BodyHeights, [](const auto& item) { return !item.second.Seen; });
		UpdateAnimators(dt);
		EmitContinuous(dt);
		m_Effects->EndFrame();

		for (DamageNumber& number : m_DamageNumbers)
			number.Age += dt;
		std::erase_if(m_DamageNumbers, [](const DamageNumber& number) { return number.Age >= DamageNumberSeconds; });
		for (const HitEvent& hit : m_Client.GetNewHits())
		{
			if (hit.Damage == 0)
				continue;
			const GameMode mode = m_Client.GetSession().Mode;
			const bool hurtsUs = hit.OnPlayer && (!IsPvpMode(mode) || hit.Target == m_Client.GetPlayerNetId());
			if ((mode == GameMode::Brawl || mode == GameMode::Hub) && !hurtsUs && hit.Attacker != m_Client.GetPlayerId())
				continue;
			if ((hit.Guarded || hit.Warded) && mode == GameMode::Brawl && m_Client.GetSession().Rules.Mode == BrawlMode::Knockback)
				continue;
			const float drift = std::fmod(float(m_HitCounter++) * 0.618034f, 1.0f) - 0.5f;
			m_DamageNumbers.push_back({ .Position = hit.Position, .Drift = drift * 0.9f, .Damage = hit.Damage, .Critical = hit.Critical, .OnPlayer = hurtsUs && !hit.Guarded && !hit.Warded, .Damages = hit.Damages, .Guarded = hit.Guarded, .Warded = hit.Warded });
		}
		if (m_DamageNumbers.size() > MaxDamageNumbers)
			m_DamageNumbers.erase(m_DamageNumbers.begin(), m_DamageNumbers.end() - MaxDamageNumbers);
	}

	AbilityId SessionState::AbilityOf(const EntityState& entity) const
	{
		return AbilityId(std::min<uint32_t>(entity.Variant, uint32_t(AbilityId::Count) - 1));
	}

	glm::vec3 SessionState::BodyBase(const EntityState& entity, float lift) const
	{
		const auto it = m_BodyHeights.find(entity.NetId);
		return { entity.Position.x, (it != m_BodyHeights.end() ? it->second.Height : GroundHeight(entity.Position)) + lift, entity.Position.y };
	}

	glm::vec3 SessionState::ProjectilePoint(const EntityState& entity) const
	{
		const auto it = m_ProjectileHeights.find(entity.NetId);
		return { entity.Position.x, it != m_ProjectileHeights.end() ? it->second.Height : GroundHeight(entity.Position) + ProjectileHeight, entity.Position.y };
	}

	glm::vec3 SessionState::EffectColor(const EntityState& entity) const
	{
		const AbilityDef& def = GetAbility(AbilityOf(entity));
		return entity.Owner == NoPlayer && def.Damages == Element::Physical ? EnemyFire : SkillColor(def.Id);
	}

	glm::vec3 SessionState::MeteorPoint(const EntityState& entity, float progress) const
	{
		const float fall = 1.0f - progress * progress;
		return Ground(entity.Position, 0.45f) + CometDrop * fall;
	}

	void SessionState::DrawMeteor(WorldRenderer& renderer, const EntityState& entity, float progress) const
	{
		const glm::vec3 target = Ground(entity.Position);
		DrawCometFall(renderer, target, MeteorPoint(entity, progress), glm::normalize(target - MeteorPoint(entity, 0.0f)), entity.GetScale(), progress, entity.NetId);
	}

	void SessionState::DrawCometFall(WorldRenderer& renderer, const glm::vec3& target, const glm::vec3& position, const glm::vec3& direction, float scale, float progress, uint32_t seed) const
	{
		const float grow = std::sqrt(std::max(scale, 0.2f));
		if (m_MeteorMesh)
		{
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
				* glm::rotate(glm::mat4(1.0f), m_Time * 3.0f + float(seed % 31u), glm::normalize(glm::vec3(0.6f, 0.3f, 0.75f)))
				* glm::scale(glm::mat4(1.0f), glm::vec3(CometRockSize * grow));
			renderer.DrawModel(*m_MeteorMesh, transform, NoPalette, glm::vec4(1.6f, 0.66f, 0.38f, 0.0f), 0.55f, target.y);
		}
		else
			renderer.Draw(MeshKind::Sphere, position, glm::vec3(0.4f * grow), { 0.45f, 0.22f, 0.12f }, m_Time * 3.0f, 0.5f, target.y);
		DrawComet(renderer, { .Center = position, .Direction = direction, .Scale = grow, .Progress = progress }, m_Time, seed);
		renderer.DrawTrail(uint64_t(seed) | (25ull << 40), position, ProjectileTrail(AbilityId::Comet, 0.0f, grow));
		renderer.DrawLight({ .Position = position, .Radius = 4.5f * grow, .Color = CometLight * (SpellLightGain * 0.8f) });
		DrawGroundGlow(renderer, target, MeteorGlowRadius * scale * (0.3f + 0.35f * progress), { 1.5f, 0.45f, 0.1f }, 0.1f + 0.55f * progress * progress);
	}

	void SessionState::CometLanded(const glm::vec3& ground, float radius, float scale, uint32_t seed)
	{
		m_Effects->CometCrater(ground, radius);
		m_FireballBursts.push_back({ .Ground = ground, .Radius = radius, .Seed = seed, .Comet = true });
		const auto unit = [seed](uint32_t index)
		{
			uint32_t x = seed * 0x9E3779B1u + index * 0x85EBCA77u + 0x27D4EB2Fu;
			x ^= x >> 15;
			x *= 0x2C1B3C6Du;
			x ^= x >> 12;
			x *= 0x297A2D39u;
			x ^= x >> 15;
			return float(x & 0xFFFFu) / 65535.0f;
		};
		const float grow = std::sqrt(std::max(scale, 0.2f));
		const auto axis = [&](uint32_t index)
		{
			return glm::normalize(glm::vec3(unit(index) * 2.0f - 1.0f, 0.6f, unit(index + 1u) * 2.0f - 1.0f));
		};
		constexpr int Chunks = 12;
		constexpr int Boulders = 4;
		for (int i = 0; i < Chunks; i++)
		{
			const uint32_t k = uint32_t(i) * 16u;
			const bool boulder = i < Boulders;
			const float angle = 6.2832f * (float(i) + 0.6f * unit(k)) / float(Chunks);
			const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
			CometChunk chunk;
			chunk.Position = ground + out * (0.3f * radius * unit(k + 1u)) + WorldUp * 0.3f;
			chunk.Velocity = out * ((boulder ? 1.6f + 1.4f * unit(k + 2u) : 2.2f + 3.0f * unit(k + 2u)) * grow) + WorldUp * (boulder ? 3.0f + 2.0f * unit(k + 3u) : 4.0f + 3.5f * unit(k + 3u));
			chunk.Axis = axis(k + 4u);
			chunk.Angle = 6.2832f * unit(k + 6u);
			chunk.Spin = (6.0f + 8.0f * unit(k + 7u)) * (unit(k + 8u) < 0.5f ? -1.0f : 1.0f);
			chunk.Size = (boulder ? 0.5f + 0.2f * unit(k + 9u) : 0.2f + 0.15f * unit(k + 9u)) * grow;
			chunk.Life = 3.0f + 0.8f * unit(k + 10u);
			chunk.Key = uint64_t(m_CometChunkKeys++) | (27ull << 40);
			chunk.Mesh = uint8_t(i % int(m_ChunkMeshes.size()));
			m_CometChunks.push_back(chunk);
		}
	}

	void SessionState::UpdateCometChunks(float dt)
	{
		constexpr float Gravity = 16.0f;
		for (CometChunk& chunk : m_CometChunks)
		{
			chunk.Age += dt;
			chunk.Velocity.y -= Gravity * dt;
			chunk.Position += chunk.Velocity * dt;
			chunk.Angle += chunk.Spin * dt;
			const float floor = GroundHeight(Vec2(chunk.Position.x, chunk.Position.z)) + 0.4f * chunk.Size;
			if (chunk.Position.y < floor)
			{
				chunk.Position.y = floor;
				chunk.Velocity.y = chunk.Velocity.y < -2.0f ? -0.3f * chunk.Velocity.y : 0.0f;
				chunk.Velocity.x *= 0.45f;
				chunk.Velocity.z *= 0.45f;
				chunk.Spin *= 0.5f;
			}
			else if (chunk.Age < 0.9f && !chunk.Cold)
				m_Effects->Trail(Fx::CometDebris, chunk.Key, chunk.Position, dt, chunk.Size / 0.15f);
		}
		std::erase_if(m_CometChunks, [](const CometChunk& chunk) { return chunk.Age >= chunk.Life; });
	}

	void SessionState::DrawCometChunks(WorldRenderer& renderer) const
	{
		for (const CometChunk& chunk : m_CometChunks)
		{
			const std::optional<uint32_t> mesh = chunk.Model != UINT32_MAX ? std::optional<uint32_t>(chunk.Model) : m_ChunkMeshes[chunk.Mesh] ? m_ChunkMeshes[chunk.Mesh] : m_MeteorMesh;
			if (!mesh)
				continue;
			const float heat = chunk.Cold ? 0.0f : std::exp(-chunk.Age * 0.8f);
			const float sink = glm::smoothstep(chunk.Life - 0.6f, chunk.Life, chunk.Age);
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), chunk.Position - WorldUp * (sink * chunk.Size * 0.6f))
				* glm::rotate(glm::mat4(1.0f), chunk.Angle, chunk.Axis)
				* glm::scale(glm::mat4(1.0f), glm::vec3(chunk.Size * (1.0f - 0.4f * sink)));
			const glm::vec3 tint = chunk.Model != UINT32_MAX ? glm::vec3(1.0f) : glm::mix(glm::vec3(0.5f, 0.46f, 0.43f), glm::vec3(1.9f, 0.75f, 0.42f), heat);
			renderer.DrawModel(*mesh, transform, NoPalette, glm::vec4(tint, 0.0f), 0.55f * heat, chunk.Position.y - chunk.Size);
		}
	}

	const EntityState* SessionState::FindBody(uint32_t owner) const
	{
		if (owner == NoPlayer)
			return nullptr;
		for (const EntityState& entity : m_Client.GetEntities())
			if (entity.Kind == EntityKind::Player && entity.Owner == owner)
				return &entity;
		return nullptr;
	}

	glm::vec3 SessionState::HandOf(const EntityState& body) const
	{
		return BodyBase(body, 1.0f) + Heading(body.Facing) * 0.35f + Heading(body.Facing - HalfPi) * 0.25f;
	}

	std::optional<glm::vec3> SessionState::LobPoint(const EntityState& entity, float age) const
	{
		const AbilityId ability = AbilityOf(entity);
		const uint32_t index = ability == AbilityId::FirebombSkip ? 1u : ability == AbilityId::FirebombSkip2 ? 2u : 0u;
		const float first = GetAbility(AbilityId::Firebomb).ImpactDelay;
		const float hop = GetAbility(AbilityId::FirebombSkip).ImpactDelay - first;
		const float start = index == 0 ? 0.0f : first + hop * float(index - 1);
		const float end = index == 0 ? first : first + hop * float(index);
		if (age < start || age >= end)
			return std::nullopt;
		const float t = (age - start) / (end - start);
		const glm::vec3 target = Ground(entity.Position, 0.22f);
		glm::vec3 from = Ground(entity.Position - FromAngle(entity.Facing) * FirebombSkipDistance, 0.22f);
		float height = 0.85f;
		if (index == 0)
		{
			const auto origin = m_Lobs.find(entity.NetId);
			from = origin != m_Lobs.end() ? origin->second : target - Heading(entity.Facing) * 4.0f + glm::vec3(0.0f, 0.8f, 0.0f);
			height = 1.3f + 0.12f * glm::length(target - from);
		}
		return glm::mix(from, target, t) + glm::vec3(0.0f, height * 4.0f * t * (1.0f - t), 0.0f);
	}

	void SessionState::DrawLob(WorldRenderer& renderer, const EntityState& entity, float age) const
	{
		const std::optional<glm::vec3> position = LobPoint(entity, age);
		if (!position)
			return;
		const glm::vec3 ground = Ground(entity.Position);
		renderer.Draw(MeshKind::Sphere, *position, glm::vec3(0.2f), { 0.11f, 0.085f, 0.075f }, m_Time * 7.0f, 0.15f, ground.y);
		renderer.DrawTrail(uint64_t(entity.NetId) | (23ull << 40), *position, LobTrail());
		const glm::vec3 fuse = *position + glm::vec3(0.0f, 0.23f, 0.0f);
		DrawGlow(renderer, *position, 0.62f, { 0.55f, 0.17f, 0.04f });
		DrawGlow(renderer, fuse, 0.42f, { 1.7f, 0.65f, 0.16f });
		DrawStar(renderer, fuse, 0.2f + 0.07f * std::sin(m_Time * 41.0f + float(entity.NetId)), { 3.0f, 1.9f, 0.65f }, m_Time * 9.0f);
	}

	void SessionState::DrawThunder(WorldRenderer& renderer, const EntityState& entity, float age, bool pending) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const uint32_t seed = StormSeed(entity.Position);
		const AbilityDef& strike = GetAbility(AbilityId::StormStrike);
		const float radius = strike.ImpactRadius * entity.GetScale();
		if (pending)
		{
			const float progress = std::clamp(age / strike.ImpactDelay, 0.0f, 1.0f);
			DrawStormLeader(renderer, base, radius, seed, progress, m_Time);
			return;
		}
		if (AbilityOf(entity) == AbilityId::StormJump)
		{
			const glm::vec3 to = Ground(entity.Position, ArcChest);
			const glm::vec3 from = Ground(entity.Position - FromAngle(entity.Facing) * entity.GetScale(), 0.5f);
			DrawStormJump(renderer, from, to, seed ^ entity.NetId, age, m_Time);
			const float light = std::exp(-age / 0.08f);
			if (light > 0.02f)
				renderer.DrawLight({ .Position = to, .Radius = 3.5f, .Color = StormLight * (SpellLightGain * 1.2f * light) });
			return;
		}
		StormStrikeLook look;
		look.Ground = base;
		look.Radius = radius;
		look.Age = age;
		look.Seed = seed;
		for (uint32_t i = 0; i < StormGroundArcs; i++)
		{
			const glm::vec2 offset = StormArcOffset(seed, i, radius);
			look.Ends[i] = Ground(entity.Position + Vec2(offset.x, offset.y));
		}
		DrawStormStrike(renderer, look, m_Time);
		const float light = 1.3f * std::exp(-age / 0.06f) + 0.9f * StormStroke(age);
		if (light > 0.02f)
			renderer.DrawLight({ .Position = base + glm::vec3(0.0f, 1.4f, 0.0f), .Radius = 7.0f, .Color = StormLight * (SpellLightGain * light) });
	}

	SessionState::ArcChannel SessionState::ArcStart(const EntityState& projectile) const
	{
		ArcChannel channel;
		channel.Owner = projectile.Owner;
		channel.Origin = glm::vec3(projectile.Position.x, LaunchHeight(projectile), projectile.Position.y) - Heading(projectile.Facing) * 0.3f;
		if (const EntityState* body = FindBody(projectile.Owner); body && LengthSquared(body->Position - projectile.Position) < ArcCasterReach * ArcCasterReach)
		{
			channel.Wand = true;
			channel.Origin = HandOf(*body);
			return channel;
		}
		const Vec2 contact = projectile.Position - FromAngle(projectile.Facing) * 0.3f;
		float best = ArcAnchorReach * ArcAnchorReach;
		for (const EntityState& other : m_Client.GetEntities())
		{
			if ((other.Kind != EntityKind::Player && other.Kind != EntityKind::Dummy) || (other.Kind == EntityKind::Player && other.Owner == projectile.Owner))
				continue;
			if (const float distance = LengthSquared(other.Position - contact); distance < best)
			{
				best = distance;
				channel.Anchor = other.NetId;
			}
		}
		return channel;
	}

	glm::vec3 SessionState::ArcOrigin(const ArcChannel& channel) const
	{
		if (channel.Wand)
		{
			if (const auto tip = m_WandTips.find(channel.Owner); tip != m_WandTips.end() && m_Time - tip->second.second < WandTipFresh)
				return tip->second.first;
			if (const EntityState* body = FindBody(channel.Owner))
				return HandOf(*body);
		}
		else if (channel.Anchor != 0)
			if (const EntityState* anchor = m_Client.FindEntity(channel.Anchor))
				return BodyBase(*anchor, ArcChest);
		return channel.Origin;
	}

	glm::vec3 SessionState::ArcEnd(uint32_t target, const glm::vec3& fallback) const
	{
		if (target != 0)
			if (const EntityState* body = m_Client.FindEntity(target))
				return BodyBase(*body, ArcChest);
		return fallback;
	}

	SessionState::ArcFade SessionState::EndArc(uint32_t netId, const EntityState& last) const
	{
		ArcFade fade;
		const auto channel = m_Arcs.find(netId);
		fade.Start = channel != m_Arcs.end() ? channel->second : ArcStart(last);
		fade.Seed = netId * 2654435761u;
		const Vec2 ahead = last.Position + FromAngle(last.Facing) * 0.5f;
		float best = ArcAnchorReach * ArcAnchorReach;
		for (const EntityState& other : m_Client.GetEntities())
		{
			if ((other.Kind != EntityKind::Player && other.Kind != EntityKind::Dummy) || (other.Kind == EntityKind::Player && other.Owner == last.Owner) || other.NetId == fade.Start.Anchor)
				continue;
			if (const float distance = LengthSquared(other.Position - ahead); distance < best)
			{
				best = distance;
				fade.Target = other.NetId;
			}
		}
		fade.End = ArcEnd(fade.Target, ProjectilePoint(last) + Heading(last.Facing) * 0.3f);
		return fade;
	}

	void SessionState::DrawArc(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& head) const
	{
		const auto channel = m_Arcs.find(entity.NetId);
		const glm::vec3 from = channel != m_Arcs.end() ? ArcOrigin(channel->second) : head - Heading(entity.Facing) * 0.6f;
		const uint32_t seed = entity.NetId * 2654435761u;
		const uint32_t frame = uint32_t(m_Time * 40.0f);
		DrawArcChannel(renderer, from, head, seed, m_Time, 1.0f);
		DrawArcSparks(renderer, head, seed + frame * 40503u, 0.3f, 0.8f, 3);
		DrawGlow(renderer, head, 0.34f, ArcGlow * 1.1f);
		DrawGlow(renderer, from, 0.24f, ArcGlow * 0.9f);
		if (entity.Owner != NoPlayer)
			DrawGlow(renderer, head, 0.12f, glm::mix(glm::pow(ColorOf(entity.Owner), glm::vec3(2.2f)), glm::vec3(1.0f), 0.3f) * 1.7f);
		renderer.DrawLight({ .Position = head, .Radius = 2.8f, .Color = ArcLight * (SpellLightGain * 0.7f) });
	}

	void SessionState::DrawArcFades(WorldRenderer& renderer) const
	{
		for (const ArcFade& fade : m_ArcFades)
		{
			const glm::vec3 end = ArcEnd(fade.Target, fade.End);
			DrawArcChannel(renderer, ArcOrigin(fade.Start), end, fade.Seed, m_Time, ArcFadeIntensity(fade.Age, fade.Seed, m_Time));
			if (fade.Target == 0 || fade.Age >= ArcSparkSeconds)
				continue;
			const float t = fade.Age / ArcSparkSeconds;
			DrawArcSparks(renderer, end, fade.Seed + uint32_t(m_Time * 30.0f) * 40503u, 0.55f, 1.2f * (1.0f - t), 5);
			DrawGlow(renderer, end, 0.4f * (1.0f - t) + 0.1f, ArcGlow * (0.8f * (1.0f - t)));
			renderer.DrawLight({ .Position = end, .Radius = 3.5f, .Color = ArcLight * (SpellLightGain * 0.6f * (1.0f - t)) });
		}
	}

	void SessionState::DrawSwordStrike(WorldRenderer& renderer, const EntityState& entity, float age) const
	{
		const AbilityId ability = AbilityOf(entity);
		const AbilityDef& def = GetAbility(ability);
		const float scale = entity.GetScale();
		if (ability == AbilityId::SwordCleave)
		{
			DrawSwordCleave(renderer, { .Ground = Ground(entity.Position), .Facing = entity.Facing, .Reach = def.Reach * scale, .Width = def.Width * scale, .Age = age, .Life = def.EffectDuration, .Seed = entity.NetId });
			return;
		}
		if (ability != AbilityId::CrystalSword && ability != AbilityId::SwordSweep)
			return;
		DrawSwordSlash(renderer, { .Center = Ground(entity.Position, SwordSlashHeight), .Facing = entity.Facing, .Reach = def.Reach * scale, .HalfArc = def.HalfArc, .Age = age, .Life = def.EffectDuration, .Backhand = ability == AbilityId::SwordSweep, .Seed = entity.NetId });
	}

	namespace
	{
		struct SwordFrame
		{
			glm::vec3 Base{ 0.0f };
			glm::vec3 Forward{ 1.0f, 0.0f, 0.0f };
			glm::vec3 Right{ 0.0f, 0.0f, 1.0f };
			float Scale = 1.0f;
		};

		SwordFrame FrameOf(const glm::vec3& base, float facing, float scale)
		{
			return { base, Heading(facing), Heading(facing - HalfPi) * SwordRightSign, scale };
		}

		glm::vec3 Local(const SwordFrame& frame, const glm::vec3& v)
		{
			return frame.Right * v.x + glm::vec3(0.0f, v.y, 0.0f) + frame.Forward * v.z;
		}

		glm::vec3 Turn(const glm::vec3& v, const glm::vec3& axis, float angle)
		{
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			return v * c + glm::cross(axis, v) * s + axis * (glm::dot(axis, v) * (1.0f - c));
		}

		CrystalSwordLook Posed(const SwordFrame& frame, const glm::vec3& grip, const glm::vec3& direction, const glm::vec3& flat)
		{
			CrystalSwordLook look;
			look.Grip = frame.Base + Local(frame, grip);
			look.Forward = glm::normalize(Local(frame, direction));
			const glm::vec3 side = Local(frame, flat);
			look.Flat = glm::normalize(side - look.Forward * glm::dot(side, look.Forward) + glm::vec3(1e-4f, 0.0f, 0.0f));
			look.Scale = frame.Scale;
			return look;
		}

		float Ease(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float EaseOut(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		}

		CrystalSwordLook Blend(const CrystalSwordLook& a, const CrystalSwordLook& b, float t, float arc = 0.0f)
		{
			CrystalSwordLook look = b;
			look.Grip = glm::mix(a.Grip, b.Grip, t) + glm::vec3(0.0f, arc * std::sin(Pi * std::clamp(t, 0.0f, 1.0f)), 0.0f);
			look.Forward = glm::normalize(glm::mix(a.Forward, b.Forward, t) + glm::vec3(0.0f, 1e-4f, 0.0f));
			const glm::vec3 flat = glm::mix(a.Flat, b.Flat, t);
			look.Flat = glm::normalize(flat - look.Forward * glm::dot(flat, look.Forward) + glm::vec3(1e-4f, 0.0f, 0.0f));
			look.Scale = glm::mix(a.Scale, b.Scale, t);
			return look;
		}

		CrystalSwordLook HoverPose(const SwordFrame& frame, float time, uint32_t seed)
		{
			const float phase = float(seed % 61u) * 0.37f;
			const float bob = 0.06f * std::sin(time * 1.9f + phase);
			const float sway = 0.07f * std::sin(time * 1.3f + phase * 1.7f);
			CrystalSwordLook look = Posed(frame, glm::vec3(0.8f + sway * 0.5f, 1.3f + bob, -0.45f), glm::vec3(0.3f + sway, 1.0f, -0.3f), glm::vec3(1.0f, 0.0f, 0.0f));
			look.Flat = Turn(look.Flat, look.Forward, 0.25f * std::sin(time * 0.9f + phase));
			return look;
		}

		CrystalSwordLook SweepPose(const SwordFrame& frame, float theta, float lift, float tilt, float roll)
		{
			const glm::vec3 around(std::sin(theta), 0.0f, std::cos(theta));
			const glm::vec3 pivot(0.0f, 1.1f + lift, 0.1f);
			CrystalSwordLook look = Posed(frame, pivot + around * SwordOrbit, around + glm::vec3(0.0f, tilt, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			look.Flat = Turn(look.Flat, look.Forward, roll);
			return look;
		}

		CrystalSwordLook CleavePose(const SwordFrame& frame, float phi, float land, float lift, const glm::vec3& flat = glm::vec3(1.0f, 0.0f, 0.0f))
		{
			const glm::vec3 out(0.0f, std::cos(phi), std::sin(phi));
			const glm::vec3 pivot = glm::mix(glm::vec3(0.3f, 1.6f + lift, -0.05f), glm::vec3(0.12f, 0.16f, 0.5f), land * land);
			return Posed(frame, pivot + out * 0.5f, out, flat);
		}

		float SweepSide(AbilityId swing)
		{
			return swing == AbilityId::SwordSweep ? -1.0f : 1.0f;
		}

		struct SweepPath
		{
			float LiftFrom;
			float LiftTo;
			float TiltFrom;
			float TiltTo;
		};

		SweepPath PathOf(AbilityId swing)
		{
			return swing == AbilityId::SwordSweep ? SweepPath{ -0.1f, 0.34f, -0.2f, 0.3f } : SweepPath{ 0.34f, -0.18f, 0.3f, -0.26f };
		}

		CrystalSwordLook WindupPose(const SwordFrame& frame, AbilityId swing, float clock, float windup, const CrystalSwordLook& from, float& glow)
		{
			const float strike = std::min(SwordStrike, windup * 0.6f);
			const float draw = std::max(windup - strike, 1e-3f);
			const float u = std::clamp(clock / draw, 0.0f, 1.0f);
			const float v = std::clamp((clock - draw) / strike, 0.0f, 1.0f);
			glow = clock < draw ? 0.25f + 0.45f * u : 1.0f;
			if (swing == AbilityId::SwordCleave)
			{
				const float back = SwordCleaveBack - SwordCleaveCreep * Ease((u - 0.45f) / 0.55f);
				const float lift = 0.28f * EaseOut(u / 0.7f);
				if (clock < draw)
					return Blend(from, CleavePose(frame, back, 0.0f, lift), EaseOut(u / 0.6f), 0.35f);
				const float slam = v * v;
				return CleavePose(frame, glm::mix(back, SwordCleaveLand, slam), slam, lift * (1.0f - slam));
			}
			const float side = SweepSide(swing);
			const SweepPath path = PathOf(swing);
			const float back = side * (SwordSweepBack + SwordSweepCreep * Ease((u - 0.45f) / 0.55f));
			if (clock < draw)
				return Blend(from, SweepPose(frame, back, path.LiftFrom, path.TiltFrom, 0.0f), EaseOut(u / 0.6f), 0.25f);
			const float s = v * v * (3.0f - 2.0f * v);
			return SweepPose(frame, glm::mix(back, -side * SwordSweepEnd, s), glm::mix(path.LiftFrom, path.LiftTo, s), glm::mix(path.TiltFrom, path.TiltTo, s), side * SwordSweepRoll * s);
		}

		CrystalSwordLook FollowPose(const SwordFrame& frame, AbilityId swing, float clock, float time, uint32_t seed)
		{
			if (swing == AbilityId::SwordCleave)
				return CleavePose(frame, SwordCleaveLand, 1.0f, 0.0f);
			const float side = SweepSide(swing);
			const SweepPath path = PathOf(swing);
			const float carry = std::sin(std::min(clock / SwordOvershootSeconds, 1.0f) * HalfPi);
			const CrystalSwordLook strike = SweepPose(frame, -side * (SwordSweepEnd + SwordSweepOvershoot * carry), path.LiftTo, path.TiltTo, side * SwordSweepRoll);
			const float home = Ease((clock - SwordOvershootSeconds * 0.6f) / SwordRecoverSeconds);
			CrystalSwordLook look = Blend(strike, HoverPose(frame, time, seed), home, SwordTwirlLift);
			look.Forward = Turn(look.Forward, look.Flat, side * SwordTwirl * home);
			return look;
		}

		uint32_t SwordHash(uint32_t a, uint32_t b)
		{
			uint32_t x = a * 0x9E3779B1u ^ (b * 0x85EBCA77u + 0x7F4A7C15u);
			x ^= x >> 16;
			x *= 0x7FEB352Du;
			x ^= x >> 15;
			x *= 0x846CA68Bu;
			x ^= x >> 16;
			return x;
		}

		void Spring(glm::vec3& value, glm::vec3& velocity, const glm::vec3& target, float omega, float dt)
		{
			const uint32_t steps = std::max(1u, uint32_t(std::ceil(omega * dt / 0.35f)));
			const float h = dt / float(steps);
			for (uint32_t i = 0; i < steps; i++)
			{
				velocity += ((target - value) * (omega * omega) - velocity * (2.0f * omega)) * h;
				value += velocity * h;
			}
		}
	}

	void SessionState::DrawFloatingSword(WorldRenderer& renderer, const EntityState& entity, const EntityFx& fx) const
	{
		if (fx.SwordShow <= 0.0f || SwordPhase(fx.SwordPhase) == SwordPhase::Hidden)
			return;
		CrystalSwordLook sword = fx.SwordDrawn;
		sword.Form = fx.SwordHeld ? fx.SwordShow : 1.0f;
		sword.Fade = fx.SwordHeld ? 0.0f : 1.0f - fx.SwordShow;
		sword.Glow = fx.SwordGlow;
		sword.Ground = BodyBase(entity).y;
		if (!fx.SwordHeld)
			sword.Grip += glm::vec3(0.0f, 0.35f * (1.0f - fx.SwordShow), 0.0f);
		DrawCrystalSword(renderer, m_SpellMeshes, sword, m_Time, entity.NetId);
		for (uint32_t k = 0; k + 1 < fx.SwordHistoryCount; k++)
		{
			const auto& [gripA, tipA] = fx.SwordHistory[k];
			const auto& [gripB, tipB] = fx.SwordHistory[k + 1];
			const float speed = glm::length(tipA - tipB) / std::max(fx.SwordHistoryStep, 1e-3f);
			const float fast = std::clamp((speed - SwordSmearSpeed) / SwordSmearSpeed, 0.0f, 1.0f);
			if (fast <= 0.0f)
				continue;
			const float width = 0.1f * sword.Scale;
			const float chord = glm::length(tipA - tipB);
			const uint32_t steps = std::clamp(uint32_t(std::ceil(chord / (width * SwordSmearOverlap))), 1u, SwordSmearSteps);
			const float share = std::min(1.0f, width * SwordSmearOverlap * float(steps) / std::max(chord, 1e-3f)) / float(steps);
			for (uint32_t step = 0; step < steps; step++)
			{
				const float t = (float(step) + 0.5f) / float(steps);
				const float age = (float(k) + t) / float(SwordHistorySize);
				const glm::vec3 tip = glm::mix(tipA, tipB, t);
				const glm::vec3 grip = glm::mix(gripA, gripB, t);
				DrawStreak(renderer, glm::mix(grip, tip, 0.3f), tip, width, SwordCrystal * (SwordSmearGlow * fast * (1.0f - age) * share * float(steps) * SwordSmearOverlap));
			}
		}
		const SwordPhase phase = SwordPhase(fx.SwordPhase);
		if (phase != SwordPhase::Windup || fx.SwordClock >= fx.SwordWindup)
			return;
		const bool own = entity.NetId == m_Client.GetPlayerNetId();
		const AbilityDef& def = GetAbility(fx.SwordSwing);
		const float area = sword.Scale / SwordScale;
		const float progress = std::clamp(fx.SwordClock / std::max(fx.SwordWindup, 1e-3f), 0.0f, 1.0f);
		const float tell = (own ? SwordTellSelf : SwordTellRival) * (0.5f + 0.5f * progress);
		const glm::vec3 base = Ground(entity.Position);
		if (fx.SwordSwing == AbilityId::SwordCleave)
			DrawLaneTelegraph(renderer, base, entity.Facing, (def.Reach + def.Lunge) * area, def.Width * area, SwordCrystal, tell, progress);
		else
			DrawSectorTelegraph(renderer, base, entity.Facing, (def.Reach + def.Lunge) * area, def.HalfArc, SwordCrystal, tell, progress);
	}

	float SessionState::SwordAreaOf(const EntityState& entity) const
	{
		const PlayerInfo* info = m_Client.FindPlayer(entity.Owner);
		return info && HasCard(info->Cards, CardId::ColossusBlade) ? ColossusArea : 1.0f;
	}

	void SessionState::SwordMoments(EntityFx& fx, const EntityState& entity, float dt)
	{
		const AbilityId variant = entity.Has(EntityFlags::Attacking) && entity.Variant < uint8_t(AbilityId::Count) ? AbilityId(entity.Variant) : AbilityId::Count;
		const bool swinging = variant != AbilityId::Count && CreditOf(variant) == AbilityId::CrystalSword;
		const bool local = entity.NetId == m_Client.GetPlayerNetId();
		const PlayerState& self = m_Client.GetPredictedState();
		const bool combo = local && self.RecastSlot >= 0 && self.RecastTimer > 0.0f && AbilityId(self.RecastAbility) == AbilityId::CrystalSword;
		const bool down = entity.Has(EntityFlags::Dead) || entity.Has(EntityFlags::Falling) || entity.Has(EntityFlags::Lifted) || entity.Has(EntityFlags::Stunned);
		const float area = SwordAreaOf(entity);
		SwordPhase phase = SwordPhase(fx.SwordPhase);
		if (!((phase == SwordPhase::Windup && fx.SwordClock >= fx.SwordWindup - SwordStrike) || phase == SwordPhase::Release))
			fx.SwordFacing = entity.Facing;
		const SwordFrame frame = FrameOf(BodyBase(entity), fx.SwordFacing, SwordScale * area);
		bool snap = phase == SwordPhase::Hidden;
		fx.SwordClock += dt;
		fx.SwordIdle = swinging ? 0.0f : fx.SwordIdle + dt;
		const auto enter = [&](SwordPhase next)
		{
			fx.SwordFrom = fx.SwordDrawn;
			fx.SwordClock = 0.0f;
			phase = next;
		};
		if (swinging && (phase != SwordPhase::Windup || variant != fx.SwordSwing))
		{
			if (phase == SwordPhase::Hidden || fx.SwordShow <= 0.0f || !fx.SwordHeld)
			{
				fx.SwordDrawn = HoverPose(frame, m_Time, entity.NetId);
				fx.SwordShow = 0.0f;
				fx.SwordForming = true;
				fx.SwordHistoryCount = 0;
				snap = true;
			}
			enter(SwordPhase::Windup);
			fx.SwordSwing = variant;
			fx.SwordConfirmed = false;
			const int8_t slot = self.CastSlot;
			fx.SwordWindup = local && slot >= 0 ? SwingWindup(m_Client.GetTraits().Skills[size_t(slot)], variant) : GetAbility(variant).CastTime * (area > 1.0f ? ColossusPace : 1.0f);
		}
		else if (!swinging && phase == SwordPhase::Windup)
		{
			const bool broken = entity.Has(EntityFlags::Dashing) || down;
			const PlayerInfo* info = m_Client.FindPlayer(entity.Owner);
			if (!broken && fx.SwordSwing == AbilityId::SwordCleave && info && HasCard(info->Cards, CardId::HurledEdge))
			{
				fx.SwordShow = 0.0f;
				fx.SwordHeld = false;
				fx.SwordClock = 0.0f;
				phase = SwordPhase::Hidden;
			}
			else
				enter(broken ? SwordPhase::Recover : SwordPhase::Release);
		}
		if (phase == SwordPhase::Release && !fx.SwordConfirmed && fx.SwordClock > SwordConfirmWait)
			enter(SwordPhase::Recover);
		const bool live = swinging || combo || (phase == SwordPhase::Release && fx.SwordClock < SwordFinishLinger) || (!local && fx.SwordIdle < SwordLinger);
		if (phase == SwordPhase::Recover && fx.SwordClock > SwordRecoverSeconds)
			phase = SwordPhase::Hover;
		if (phase == SwordPhase::Release && fx.SwordSwing != AbilityId::SwordCleave && fx.SwordClock > SwordOvershootSeconds * 0.6f + SwordRecoverSeconds)
			phase = SwordPhase::Hover;
		if (phase != SwordPhase::Hidden)
		{
			float glow = 0.0f;
			bool striking = false;
			CrystalSwordLook target;
			switch (phase)
			{
			case SwordPhase::Windup:
				target = WindupPose(frame, fx.SwordSwing, fx.SwordClock, fx.SwordWindup, fx.SwordFrom, glow);
				striking = fx.SwordClock > fx.SwordWindup - SwordStrike * 1.3f;
				break;
			case SwordPhase::Release:
				target = FollowPose(frame, fx.SwordSwing, fx.SwordClock, m_Time, entity.NetId);
				striking = fx.SwordClock < SwordOvershootSeconds;
				break;
			case SwordPhase::Recover:
				target = Blend(fx.SwordFrom, HoverPose(frame, m_Time, entity.NetId), Ease(fx.SwordClock / SwordRecoverSeconds), 0.2f);
				break;
			default:
				target = HoverPose(frame, m_Time, entity.NetId);
				break;
			}
			fx.SwordNow = target;
			if (snap)
			{
				fx.SwordDrawn = target;
				fx.SwordGripSpeed = {};
				fx.SwordAimSpeed = {};
				fx.SwordFlatSpeed = {};
			}
			else
			{
				const float omega = striking ? SwordSpringFast : SwordSpringSlow;
				Spring(fx.SwordDrawn.Grip, fx.SwordGripSpeed, target.Grip, omega, dt);
				Spring(fx.SwordDrawn.Forward, fx.SwordAimSpeed, target.Forward, omega, dt);
				Spring(fx.SwordDrawn.Flat, fx.SwordFlatSpeed, target.Flat, omega, dt);
				fx.SwordDrawn.Forward = glm::normalize(fx.SwordDrawn.Forward + glm::vec3(0.0f, 1e-5f, 0.0f));
				const glm::vec3 flat = fx.SwordDrawn.Flat - fx.SwordDrawn.Forward * glm::dot(fx.SwordDrawn.Flat, fx.SwordDrawn.Forward);
				fx.SwordDrawn.Flat = glm::normalize(flat + glm::vec3(1e-5f, 0.0f, 0.0f));
				fx.SwordDrawn.Scale = target.Scale;
			}
			const glm::vec3 tip = SwordTip(fx.SwordDrawn);
			for (uint32_t k = std::min(fx.SwordHistoryCount, SwordHistorySize - 1); k > 0; k--)
				fx.SwordHistory[k] = fx.SwordHistory[k - 1];
			fx.SwordHistory[0] = { fx.SwordDrawn.Grip, tip };
			fx.SwordHistoryCount = std::min(fx.SwordHistoryCount + 1, SwordHistorySize);
			fx.SwordHistoryStep = std::max(dt, 1e-3f);
			if (phase == SwordPhase::Release && fx.SwordConfirmed && fx.SwordSwing == AbilityId::SwordCleave)
			{
				fx.SwordDrawn = target;
				ShatterSword(fx, entity, area);
				fx.SwordPhase = uint8_t(SwordPhase::Hidden);
				return;
			}
			fx.SwordGlow = std::max(glow, fx.SwordGlow - dt * 4.0f);
			const float before = fx.SwordShow;
			if ((live || fx.SwordForming) && !down)
			{
				fx.SwordHeld = true;
				fx.SwordShow = std::min(fx.SwordShow + dt / SwordFormSeconds, 1.0f);
				const uint64_t key = uint64_t(entity.NetId) << 4;
				m_Effects->Trail(Fx::SwordAura, key | 1u, tip, dt, area);
				m_Effects->Trail(Fx::SwordAura, key | 2u, glm::mix(fx.SwordDrawn.Grip, tip, 0.55f), dt, 0.6f * area);
				if (fx.SwordForming)
				{
					const glm::vec3 center = glm::mix(fx.SwordDrawn.Grip, tip, 0.45f);
					m_Effects->Converge(uint64_t(entity.NetId) | (26ull << 40), center, 1.1f * area, 160.0f, 0.12f, dt, 0.035f, ParticleKind::DiamondShard, SwordShardTint, 1.5f, 0.1f);
					if (fx.SwordShow >= 1.0f)
					{
						fx.SwordForming = false;
						fx.SwordGlow = 1.0f;
						m_Effects->Burst(Fx::SwordConjure, center, glm::vec3(0.0f, 1.0f, 0.0f), area);
					}
				}
			}
			else
			{
				if (fx.SwordHeld && before > 0.0f)
					for (uint32_t k = 0; k < 3; k++)
						m_Effects->Burst(Fx::SwordFade, glm::mix(fx.SwordDrawn.Grip, tip, 0.25f + 0.33f * float(k)), glm::vec3(0.0f, 1.0f, 0.0f), down ? 1.4f : 1.0f);
				fx.SwordHeld = false;
				fx.SwordShow = std::max(fx.SwordShow - dt / SwordFadeSeconds, 0.0f);
				if (fx.SwordShow <= 0.0f)
					phase = SwordPhase::Hidden;
			}
		}
		fx.SwordPhase = uint8_t(phase);
	}

	void SessionState::ShatterSword(EntityFx& fx, const EntityState& entity, float area)
	{
		const glm::vec3 grip = fx.SwordDrawn.Grip;
		const glm::vec3 tip = SwordTip(fx.SwordDrawn);
		const glm::vec3 center = glm::mix(grip, tip, 0.5f);
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		const float facing = fx.SwordStrikeReach > 0.0f ? fx.SwordStrikeFacing : entity.Facing;
		const glm::vec3 forward = Heading(facing);
		const glm::vec3 across = glm::normalize(glm::cross(forward, up));
		const Vec2 origin = fx.SwordStrikeReach > 0.0f ? fx.SwordStrikePoint : entity.Position;
		const float reach = fx.SwordStrikeReach > 0.0f ? fx.SwordStrikeReach : GetAbility(AbilityId::SwordCleave).Reach * area;
		const uint32_t base = SwordHash(entity.NetId, uint32_t(m_Time * 240.0f));
		const auto random = [&](uint32_t index) { return float(SwordHash(base, index) & 0xFFFFu) / 65535.0f; };
		const auto lane = [&](float t) { return origin + FromAngle(facing) * (reach * t); };
		for (uint32_t k = 0; k < 5; k++)
			m_Effects->Burst(Fx::SwordShatter, glm::mix(grip, tip, 0.1f + 0.2f * float(k)), up, (k == 2 ? 0.9f : 0.6f) * area);
		m_Effects->Burst(Fx::SwordCleave, center, up, 1.3f * area);
		m_Effects->Shockwave(Ground(lane(0.5f)), reach * 0.65f, 0.5f, 0.16f, 0.32f);
		m_Effects->AirShockwave(center + up * 0.3f, 2.2f * area, 0.38f, 0.1f, 0.26f);
		GroundDecal scar;
		scar.Center = Ground(lane(0.55f));
		scar.Yaw = facing;
		scar.Extent = glm::vec2(reach * 0.47f, 0.4f * area);
		scar.Depth = 0.9f;
		scar.Shape = DecalShape::Fissure;
		scar.Color = glm::vec4(SwordCrystal * 0.55f, 1.0f);
		scar.Tint = glm::vec4(0.04f, 0.07f, 0.07f, 0.65f);
		scar.Roughness = 0.5f;
		scar.Params = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		scar.Seed = entity.NetId;
		m_Effects->Mark(scar, SwordScarLife, 0.02f, 0.85f, 1.0f, 0.06f, 1.6f);
		for (uint32_t k = 0; k < SwordPieceCount; k++)
		{
			const uint32_t key = k * 16u;
			const float along = random(key);
			const float azimuth = random(key + 1u) * 6.2832f;
			const float spread = 0.25f * area * std::sqrt(random(key + 2u));
			const float speed = 1.0f + 3.8f * random(key + 3u);
			SwordPiece piece;
			piece.Position = glm::mix(grip, tip, along) + Heading(azimuth) * spread + up * (0.05f + 0.25f * random(key + 4u));
			piece.Velocity = Heading(azimuth) * speed + forward * (0.4f + 1.2f * random(key + 5u)) + up * (1.2f + 5.5f * random(key + 6u) * random(key + 7u) * 1.6f);
			piece.Axis = glm::normalize(glm::vec3(random(key + 8u) - 0.5f, random(key + 9u) - 0.5f, random(key + 10u) - 0.5f) + glm::vec3(0.0f, 1e-3f, 0.0f));
			piece.Spin = (6.0f + 16.0f * random(key + 11u)) * (random(key + 12u) < 0.5f ? -1.0f : 1.0f);
			piece.Heading = glm::normalize(fx.SwordDrawn.Forward + glm::vec3(random(key + 13u) - 0.5f, 0.0f, random(key + 14u) - 0.5f));
			piece.Size = (0.5f + 1.1f * random(key + 15u) * random(key + 3u)) * area;
			piece.Life = 0.7f + 0.8f * random(key + 7u);
			piece.Age = -SwordPieceStagger * random(key + 2u) * random(key + 9u);
			piece.Variant = k;
			piece.Seed = base + k;
			m_SwordPieces.push_back(piece);
		}
		const float first = 0.35f / reach;
		for (uint32_t k = 0; k < SwordSpireCount; k++)
		{
			const uint32_t key = 4096u + k * 32u;
			const float t = first + (1.0f - first) * (float(k) + 0.5f * random(key)) / float(SwordSpireCount - 1);
			const Vec2 spot = lane(std::min(t, 1.0f)) + FromAngle(facing + HalfPi) * ((random(key + 1u) - 0.5f) * 0.5f * area);
			const float scale = (k == SwordSpireCount - 1 ? 1.2f : 1.0f) * (0.75f + 0.45f * random(key + 2u));
			SwordSpire spire;
			spire.Base = Ground(spot);
			spire.Axis = glm::normalize(up + forward * (0.1f + 0.25f * random(key + 3u)) + across * ((random(key + 4u) - 0.5f) * 0.4f));
			spire.Yaw = random(key + 5u) * 6.2832f;
			spire.Size = SwordClusterSize * scale * area;
			spire.Delay = SwordSpireStagger * t + 0.03f * random(key + 6u);
			spire.Life = 0.85f + 0.35f * random(key + 7u);
			spire.Variant = uint32_t(random(key + 8u) * 3.0f);
			spire.Cluster = true;
			spire.Seed = base + 97u * k;
			m_SwordSpires.push_back(spire);
			for (uint32_t c = 0; c < 2; c++)
			{
				const float yaw = random(key + 10u + c) * 6.2832f;
				SwordSpire shard;
				shard.Base = Ground(spot + FromAngle(yaw) * (0.25f + 0.2f * random(key + 12u + c)) * area);
				shard.Axis = glm::normalize(up + Heading(yaw) * (0.5f + 0.4f * random(key + 14u + c)));
				shard.Yaw = yaw;
				shard.Size = (1.4f + 1.0f * random(key + 16u + c)) * area;
				shard.Delay = spire.Delay + 0.02f + 0.04f * random(key + 18u + c);
				shard.Life = spire.Life * (0.7f + 0.2f * random(key + 20u + c));
				shard.Variant = k * 2u + c;
				shard.Seed = spire.Seed + 1u + c;
				m_SwordSpires.push_back(shard);
			}
			m_Effects->Burst(Fx::SwordDust, Ground(spot, 0.15f), up, area);
		}
		const bool local = entity.NetId == m_Client.GetPlayerNetId();
		Rumble(Ground(lane(0.5f)), SwordShatterRumble, entity.Owner);
		if (local)
		{
			m_KickSpeed += glm::vec2(forward.x, forward.z) * SwordShatterKick;
			AddTrauma(SwordShatterTrauma);
		}
		m_App.GetGameAudio().PlaySwordShatter(lane(0.5f), local);
		fx.SwordShow = 0.0f;
		fx.SwordHeld = false;
		fx.SwordStrikeReach = 0.0f;
	}

	void SessionState::UpdateSwordDebris(float dt)
	{
		for (SwordPiece& piece : m_SwordPieces)
		{
			if (piece.Age < 0.0f)
			{
				piece.Age += dt;
				continue;
			}
			piece.Age += dt;
			piece.Velocity.y -= SwordPieceGravity * dt;
			piece.Position += piece.Velocity * dt;
			piece.Angle += piece.Spin * dt;
			const float floor = GroundHeight(Vec2(piece.Position.x, piece.Position.z)) + 0.04f * piece.Size;
			if (piece.Position.y < floor)
			{
				piece.Position.y = floor;
				if (piece.Velocity.y < -0.6f)
				{
					piece.Velocity.y *= -0.35f;
					piece.Velocity.x *= 0.6f;
					piece.Velocity.z *= 0.6f;
					piece.Spin *= 0.6f;
				}
				else
				{
					piece.Velocity = glm::vec3(piece.Velocity.x * 0.85f, 0.0f, piece.Velocity.z * 0.85f);
					piece.Spin *= 0.85f;
				}
			}
			if (piece.Age >= piece.Life && !piece.Burst)
			{
				piece.Burst = true;
				m_Effects->Burst(Fx::SwordFade, piece.Position, glm::vec3(0.0f, 1.0f, 0.0f), 0.6f);
			}
		}
		std::erase_if(m_SwordPieces, [](const SwordPiece& piece) { return piece.Age >= piece.Life + SwordPieceFade; });
		for (SwordSpire& spire : m_SwordSpires)
		{
			const float before = spire.Age - spire.Delay;
			spire.Age += dt;
			if (before < 0.0f && spire.Age - spire.Delay >= 0.0f)
				m_Effects->Burst(Fx::SwordSpark, spire.Base + spire.Axis * (0.1f * spire.Size), spire.Axis, 0.5f);
		}
		std::erase_if(m_SwordSpires, [](const SwordSpire& spire) { return spire.Age >= spire.Delay + spire.Life; });
	}

	void SessionState::DrawSwordDebris(WorldRenderer& renderer) const
	{
		for (const SwordPiece& piece : m_SwordPieces)
		{
			if (piece.Age < 0.0f)
				continue;
			const glm::vec3 axis = Turn(piece.Heading, piece.Axis, piece.Angle);
			const glm::vec3 helper = std::abs(axis.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			const float fade = std::clamp((piece.Age - piece.Life) / SwordPieceFade, 0.0f, 1.0f);
			CrystalShardLook look;
			look.Center = piece.Position;
			look.Axis = axis;
			look.Up = glm::normalize(glm::cross(axis, helper));
			look.Size = piece.Size;
			look.Glow = std::exp(-piece.Age * 3.0f);
			look.Fade = fade;
			look.Ground = GroundHeight(Vec2(piece.Position.x, piece.Position.z));
			look.Variant = piece.Variant;
			DrawCrystalShard(renderer, m_SpellMeshes, look, piece.Seed);
		}
		for (const SwordSpire& spire : m_SwordSpires)
		{
			const float age = spire.Age - spire.Delay;
			if (age <= 0.0f)
				continue;
			const float pop = 1.0f - std::exp(-age * SwordSpirePop);
			const float growth = 1.0f + SwordSpireGrowth * age / spire.Life;
			const glm::vec3 helper = std::abs(spire.Axis.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			CrystalShardLook look;
			look.Center = spire.Base - spire.Axis * (0.05f * spire.Size);
			look.Axis = spire.Axis;
			look.Up = Turn(glm::normalize(glm::cross(spire.Axis, helper)), spire.Axis, spire.Yaw);
			look.Cluster = spire.Cluster;
			look.Size = spire.Size * pop * growth;
			look.Glow = 0.25f + 0.75f * std::exp(-age * 5.0f);
			look.Fade = glm::smoothstep(0.45f, 1.0f, age / spire.Life);
			look.Ground = spire.Base.y;
			look.Variant = spire.Variant;
			DrawCrystalShard(renderer, m_SpellMeshes, look, spire.Seed);
		}
	}

	void SessionState::SwordStruck(const EntityState& strike)
	{
		const AbilityId ability = AbilityOf(strike);
		const AbilityDef& def = GetAbility(ability);
		const float scale = strike.GetScale();
		EntityFx* owner = nullptr;
		if (const EntityState* body = FindBody(strike.Owner))
			if (const auto it = m_Fx.find(body->NetId); it != m_Fx.end())
				owner = &it->second;
		if (owner)
		{
			owner->SwordGlow = 1.0f;
			if (ability == AbilityId::SwordShatter)
				owner->SwordGlow = 1.0f;
			else if (owner->SwordSwing == ability || ability == AbilityId::SwordThrow)
				owner->SwordConfirmed = true;
			if (ability == AbilityId::SwordCleave)
			{
				owner->SwordStrikePoint = strike.Position;
				owner->SwordStrikeFacing = strike.Facing;
				owner->SwordStrikeReach = def.Reach * scale;
			}
			if (ability != AbilityId::SwordShatter)
				owner->SwordFacing = strike.Facing;
			if (ability == AbilityId::SwordCleave)
				owner->Stop = std::max(owner->Stop, SwordLandStop);
		}
		const glm::vec3 forward = Heading(strike.Facing);
		if (ability == AbilityId::SwordCleave)
		{
			m_Effects->Burst(Fx::SwordCleave, Ground(strike.Position + FromAngle(strike.Facing) * (def.Reach * scale * 0.92f), 0.1f), glm::vec3(0.0f, 1.0f, 0.0f), scale);
			m_Effects->Burst(Fx::SwordSpark, Ground(strike.Position + FromAngle(strike.Facing) * (def.Reach * scale * 0.5f), 0.2f), forward, 1.0f);
			if (strike.Owner == m_Client.GetPlayerId())
			{
				m_KickSpeed += glm::vec2(forward.x, forward.z) * SwordCleaveKick;
				AddTrauma(SwordCleaveTrauma);
			}
			return;
		}
		if (ability != AbilityId::CrystalSword && ability != AbilityId::SwordSweep)
			return;
		const float side = (ability == AbilityId::SwordSweep ? -1.0f : 1.0f) * SwordSweepSide;
		for (uint32_t k = 0; k < 3; k++)
		{
			const float angle = strike.Facing + side * def.HalfArc * (float(k) - 0.5f) * 0.8f;
			const glm::vec3 point = Ground(strike.Position + FromAngle(angle) * (def.Reach * scale * 0.92f), SwordSlashHeight);
			m_Effects->Burst(Fx::SwordSpark, point, Heading(angle + side * HalfPi), k == 2 ? 1.0f : 0.6f);
		}
	}

	void SessionState::DrawThrownSword(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& center) const
	{
		const float spin = entity.Facing + AgeOf(entity.NetId) * SwordThrowSpin;
		CrystalSwordLook sword;
		sword.Scale = SwordScale * SwordAreaOf(entity);
		sword.Forward = Heading(spin);
		sword.Flat = glm::vec3(0.0f, 1.0f, 0.0f);
		sword.Grip = center + glm::vec3(0.0f, 0.35f, 0.0f) - sword.Forward * ((SwordGuard + SwordBlade) * sword.Scale * 0.45f);
		sword.Glow = 0.7f;
		sword.Ground = GroundHeight(entity.Position);
		DrawCrystalSword(renderer, m_SpellMeshes, sword, m_Time, entity.NetId);
		renderer.DrawTrail(uint64_t(entity.NetId) | (20ull << 40), SwordTip(sword), SwordTrail(), 0.8f);
		renderer.DrawLight({ .Position = center + glm::vec3(0.0f, 0.35f, 0.0f), .Radius = 3.0f, .Color = SwordCrystal * 1.2f });
	}

	void SessionState::SwordHit(const HitEvent& hit)
	{
		const auto victim = m_Fx.find(hit.Target);
		const bool cleave = hit.Ability == uint8_t(AbilityId::SwordCleave) || hit.Ability == uint8_t(AbilityId::SwordShatter);
		const float stop = cleave ? SwordCleaveStop : SwordSwingStop;
		const EntityState* attacker = FindBody(hit.Attacker);
		if (victim != m_Fx.end())
		{
			victim->second.Stop = std::max(victim->second.Stop, stop);
			victim->second.FlashColor = glm::mix(SwordCrystal, glm::vec3(1.0f), 0.55f);
		}
		if (attacker && attacker->NetId != hit.Target)
			if (const auto it = m_Fx.find(attacker->NetId); it != m_Fx.end())
				it->second.Stop = std::max(it->second.Stop, stop);
		const glm::vec3 point = Ground(hit.Position, victim != m_Fx.end() ? BarHeight(victim->second.Last) * 0.5f : 0.8f);
		Vec2 away = attacker ? hit.Position - attacker->Position : Vec2(1.0f, 0.0f);
		away = LengthSquared(away) > 1e-4f ? Normalize(away) : Vec2(1.0f, 0.0f);
		m_Effects->Burst(Fx::SwordHit, point, glm::vec3(away.x, 0.25f, away.y), cleave ? 1.35f : 1.0f);
	}

	void SessionState::OnEntityAppeared(const EntityState& entity)
	{
		if (entity.Kind == EntityKind::Projectile && AbilityOf(entity) == AbilityId::ArcLightning)
			m_Arcs[entity.NetId] = ArcStart(entity);
		if (entity.Kind == EntityKind::Effect && CreditOf(AbilityOf(entity)) == AbilityId::GlacialNova)
		{
			NovaAppeared(entity);
			return;
		}
		if (entity.Kind == EntityKind::Effect && !entity.Has(EntityFlags::Pending) && AbilityOf(entity) == AbilityId::EarthSpike)
			EruptSpike(entity);
		if (entity.Kind == EntityKind::Effect && entity.Has(EntityFlags::Pending) && AbilityOf(entity) == AbilityId::Firebomb)
			if (const EntityState* body = FindBody(entity.Owner))
				m_Lobs[entity.NetId] = HandOf(*body);
		if (entity.Kind == EntityKind::Effect && !entity.Has(EntityFlags::Pending) && CreditOf(AbilityOf(entity)) == AbilityId::Firebomb && AbilityOf(entity) != AbilityId::FirebombFlame)
			if (const PlayerInfo* info = m_Client.FindPlayer(entity.Owner); info && HasCard(info->Cards, CardId::StickyFire))
				m_Flames.push_back({ Ground(entity.Position, 0.05f), StickyFirstDelay + StickyInterval * float(StickyPulses - 1) + 0.25f, m_FlameKeys++ });
		if (entity.Kind != EntityKind::Effect || entity.Has(EntityFlags::Pending))
			return;
		const AbilityId ability = AbilityOf(entity);
		const AbilityDef& def = GetAbility(ability);
		const float scale = ability == AbilityId::StormJump ? 0.7f : entity.GetScale();
		const bool fireball = ability == AbilityId::PurpleFireball;
		const float blast = fireball ? entity.GetChargePower() : 0.0f;
		const glm::vec3 tint = fireball ? FireballColor(blast) : CreditOf(ability) == AbilityId::StormStrike ? StormTelegraph : CreditOf(ability) == AbilityId::CrystalSword ? SwordCrystal : EffectColor(entity);
		m_Effects->Impact(ability, Ground(entity.Position), entity.Facing, scale, tint, entity.NetId,[this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); }, blast);
		Rumble(Ground(entity.Position), ImpactRumble(ability, blast), entity.Owner);
		if (CreditOf(ability) == AbilityId::CrystalSword)
			SwordStruck(entity);
		const std::optional<Fx> fx = EffectBurstFor(ability);
		if (!fx)
			return;
		const glm::vec3 center = Ground(entity.Position, fireball ? FireballBlastHeight : 0.5f);
		if (fireball)
		{
			m_Effects->FlameRing(Ground(entity.Position), def.ImpactRadius * scale, blast, FireballFlameHue(blast), FireballBlastSpin);
			m_FireballBursts.push_back({ .Ground = Ground(entity.Position), .Radius = def.ImpactRadius * scale, .Power = blast, .Seed = entity.NetId });
			return;
		}
		if (ability == AbilityId::Comet)
		{
			CometLanded(Ground(entity.Position), def.ImpactRadius * scale, scale, entity.NetId);
			return;
		}
		const glm::vec3 forward = Heading(entity.Facing);
		switch (def.Shape)
		{
		case StrikeShape::Line:
		{
			const float length = def.Reach * scale;
			const int points = std::max(2, int(length / 1.5f));
			for (int i = 0; i < points; i++)
				m_Effects->Burst(*fx, center + forward * (length * (float(i) + 0.5f) / float(points)), forward, 0.6f);
			return;
		}
		case StrikeShape::Ring:
		{
			const float radius = def.ImpactRadius * scale;
			for (int i = 0; i < 12; i++)
				m_Effects->Burst(*fx, center + Heading(float(i) * Pi / 6.0f) * radius, glm::vec3(0.0f, 1.0f, 0.0f), 0.6f);
			return;
		}
		case StrikeShape::Cone:
		{
			const float reach = def.Reach * scale;
			for (int i = 0; i < 5; i++)
			{
				const float angle = entity.Facing + def.HalfArc * (float(i) * 0.5f - 1.0f);
				m_Effects->Burst(*fx, center + Heading(angle) * (reach * 0.6f), Heading(angle), 0.7f);
			}
			return;
		}
		default:
			break;
		}
		if (def.Kind == AbilityKind::Melee)
			m_Effects->Burst(*fx, center + forward * (def.Reach * scale * 0.4f), forward, scale);
		else
			m_Effects->Burst(*fx, center, forward, scale);
	}

	void SessionState::EmitContinuous(float dt)
	{
		for (Flame& flame : m_Flames)
		{
			flame.Life -= dt;
			m_Effects->Trail(Fx::Campfire, (6ull << 40) | flame.Key, flame.Position, dt, 0.85f);
		}
		std::erase_if(m_Flames, [](const Flame& flame) { return flame.Life <= 0.0f; });
		for (FireballBurst& burst : m_FireballBursts)
			burst.Age += dt;
		std::erase_if(m_FireballBursts, [](const FireballBurst& burst) { return burst.Age >= (burst.Comet ? CometBlastSmoke : FireballBlastLife(burst.Power)); });
		UpdateCometChunks(dt);
		if (const Level* level = m_Client.GetLevel(); level && level->Kind == MapKind::FrozenLake)
			m_Effects->Trail(Fx::Snowfall, 7ull << 40, m_CameraTarget + glm::vec3(0.0f, 4.5f, 1.5f), dt, 1.0f);
		if (const Level* level = m_Client.GetLevel(); level && level->Kind == MapKind::FrozenLake && m_Client.GetSession().Mode == GameMode::Brawl)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const uint32_t epoch = uint32_t(m_Time / 3.0f);
			for (uint32_t i = 0; i < 26; i++)
			{
				const uint32_t salt = (epoch + i * 11u) * 2654435761u + i * 40503u;
				const Vec2 spot = Vec2(m_CameraTarget.x, m_CameraTarget.z) + Vec2(float(salt & 0xFFFFu) / 65535.0f - 0.5f, float((salt >> 16) & 0xFFFFu) / 65535.0f - 0.5f) * 30.0f;
				const Vec2 inward = Normalize(brawl.Plan.Center - spot);
				if (IsBrawlLava(*level, brawl, spot) && !IsBrawlLava(*level, brawl, spot + inward * 1.7f))
					m_Effects->Trail(Fx::SeaSmoke, (8ull << 40) | (uint64_t(epoch) << 8) | i, { spot.x, LakeWaterLine + 0.04f, spot.y }, dt, 1.0f);
			}
			const LeviathanPose& beast = m_LevelRenderer.GetLake().GetLeviathan();
			if (beast.Active)
			{
				m_Trauma = std::max(m_Trauma, 0.36f * beast.Near);
				if (!m_LeviathanCalled && beast.Progress > 0.2f)
				{
					m_LeviathanCalled = true;
					m_App.GetGameAudio().PlayLeviathan(Vec2(beast.Head.x, beast.Head.z));
				}
			}
			else
				m_LeviathanCalled = false;
		}
		std::erase_if(m_Lobs, [&](const auto& lob) { return !m_Client.FindEntity(lob.first); });
		if (const Level* level = m_Client.GetLevel(); level && level->Kind == MapKind::StormPeak)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const bool live = brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over;
			const float clock = live ? m_Client.GetArenaClock(m_Client.GetPresentTick()) : 0.0f;
			const StormRing ring = RingAt(brawl.Plan, clock);
			const float outside = ring.Active ? glm::smoothstep(ring.Radius - 1.0f, ring.Radius + 2.0f, Length(Vec2(m_CameraTarget.x, m_CameraTarget.z) - ring.Center)) : 0.0f;
			const float build = live ? StormBuild(brawl.Plan, clock) : 0.2f;
			m_Effects->Stream(Fx::Rain, m_CameraTarget + glm::vec3(-1.6f, 7.5f, -0.8f), glm::normalize(glm::vec3(0.22f, -1.0f, 0.1f)), RainRate * (0.08f + 0.5f * build * build + 0.7f * outside), dt);
			const Vec2 wind = FromAngle(0.35f);
			m_Effects->Stream(Fx::Spindrift, m_CameraTarget + glm::vec3(-wind.x * 7.0f, 0.15f + 1.4f * (0.5f + 0.5f * std::sin(m_Time * 41.3f)), -wind.y * 7.0f), glm::vec3(wind.x, -0.08f, wind.y), 30.0f + 60.0f * build + 80.0f * outside, dt);
		}
		else if (const Level* level = m_Client.GetLevel(); level && level->Skyborne && m_Client.GetSession().Mode == GameMode::Brawl)
		{
			if (!level->Hall.IsEmpty())
			{
				const float angle = m_Time * 29.3f;
				m_Effects->Stream(Fx::SunDust, m_CameraTarget + glm::vec3(5.0f * std::sin(angle), 1.2f + 1.0f * std::sin(angle * 1.7f), 5.0f * std::cos(angle * 0.7f)), glm::vec3(0.2f, 0.25f, 0.1f), 9.0f, dt);
			}
		}
		if (const Level* level = m_Client.GetLevel(); level && m_Client.GetSession().Mode == GameMode::Brawl && level->Kind != MapKind::FrozenLake && !level->Skyborne)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const uint32_t epoch = uint32_t(m_Time / 2.5f);
			for (uint32_t i = 0; i < 10; i++)
			{
				const uint32_t salt = (epoch + i * 7u) * 2654435761u + i * 40503u;
				const float u = float(salt & 0xFFFFu) / 65535.0f;
				const float v = float((salt >> 16) & 0xFFFFu) / 65535.0f;
				const Vec2 spot = Vec2(m_CameraTarget.x, m_CameraTarget.z) + Vec2(u - 0.5f, v - 0.5f) * 28.0f;
				if (IsBrawlLava(*level, brawl, spot))
					m_Effects->Trail(Fx::Campfire, (4ull << 40) | (uint64_t(epoch) << 8) | i, { spot.x, level->Kind == MapKind::Caldera ? Ground(spot).y + 0.3f : LavaSurface + 0.02f, spot.y }, dt, 0.4f + 0.3f * u);
			}
		}
		if (const Level* level = m_Client.GetLevel(); level && level->Kind == MapKind::Caldera)
		{
			m_Effects->Trail(Fx::AshFall, 9ull << 40, m_CameraTarget + glm::vec3(0.0f, 6.0f, 1.5f), dt, 1.0f);
			m_Effects->Trail(Fx::EmberDrift, 10ull << 40, m_CameraTarget + glm::vec3(0.0f, -0.2f, 2.0f), dt, 1.0f);
			const ForgeArt& forge = m_LevelRenderer.GetForge();
			const Vec2 camera(m_CameraTarget.x, m_CameraTarget.z);
			const auto emit = [&](const std::vector<glm::vec3>& spots, Fx fx, uint64_t channel)
			{
				for (size_t i = 0; i < spots.size(); i++)
					if (LengthSquared(Vec2(spots[i].x, spots[i].z) - camera) < ForgeFxRange * ForgeFxRange)
						m_Effects->Trail(fx, (channel << 40) | uint64_t(i), spots[i], dt, 1.0f);
			};
			emit(forge.GetSmokeSpots(), Fx::ForgeSmoke, 13ull);
			emit(forge.GetFallSpots(), Fx::FallSpray, 14ull);
		}
		const std::vector<LevelSpray>& sprays = m_LevelRenderer.GetSprays();
		for (size_t i = 0; i < sprays.size(); i++)
		{
			const LevelSpray& spray = sprays[i];
			if (LengthSquared(Vec2(spray.Position.x, spray.Position.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z)) > 28.0f * 28.0f)
				continue;
			const Fx fx = spray.Kind == SprayKind::Lip ? Fx::WaterSpray : spray.Kind == SprayKind::Splash ? Fx::WaterSplash : Fx::WaterMist;
			m_Effects->Trail(fx, (15ull << 40) | uint64_t(i), spray.Position, dt, spray.Scale);
		}
		const std::vector<LevelFire>& fires = m_LevelRenderer.GetFires();
		const std::vector<LevelFire> staged = StageFires();
		for (size_t i = 0; i < fires.size() + staged.size(); i++)
		{
			const LevelFire& fire = i < fires.size() ? fires[i] : staged[i - fires.size()];
			if (LengthSquared(Vec2(fire.Position.x, fire.Position.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z)) > 26.0f * 26.0f)
				continue;
			const Fx fx = fire.Kind == FireKind::Campfire ? Fx::Campfire : fire.Kind == FireKind::Candle ? Fx::CandleFlame : fire.Kind == FireKind::Torch ? Fx::TorchFlame : Fx::LanternFlame;
			m_Effects->Trail(fx, (2ull << 40) | uint64_t(i), fire.Position, dt, fire.Scale);
		}
		UpdateSpikeStage(dt);
		if (m_App.GetSettings().FireStage == 3)
		{
			glm::vec3 ground;
			const float t = StageComet(ground);
			const AbilityDef& comet = GetAbility(AbilityId::Comet);
			if (t < comet.ImpactDelay)
				m_Effects->Trail(Fx::MeteorTrail, 42ull << 40, ground + glm::vec3(0.0f, 0.45f, 0.0f) + CometDrop * (1.0f - (t / comet.ImpactDelay) * (t / comet.ImpactDelay)), dt);
			else if (t - comet.ImpactDelay < dt)
			{
				m_Effects->Impact(AbilityId::Comet, ground, 0.0f, 1.0f, SkillColor(AbilityId::Comet), 960u, [this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); });
				CometLanded(ground, comet.ImpactRadius, 1.0f, 960u + uint32_t(m_Time));
			}
		}
		for (int i = 0; i < 1 && (m_App.GetSettings().FireStage == 1 || m_App.GetSettings().FireStage == 2); i++)
		{
			glm::vec3 center;
			float facing = 0.0f;
			if (StageBolt(i, center, facing))
				m_Effects->Trail(Fx::EmberTrail, (40ull << 40) | uint64_t(i), center, dt);
		}
		if (m_App.GetSettings().FireStage == 1 || m_App.GetSettings().FireStage == 2)
		{
			for (int i = 0; i < StageBlastCount(); i++)
			{
				glm::vec3 ground;
				float power = 0.0f;
				if (const float age = StageBlast(i, ground, power); age < 0.0f || age >= dt)
					continue;
				const float radius = GetAbility(AbilityId::PurpleFireball).ImpactRadius + (FireballFullBlast - GetAbility(AbilityId::PurpleFireball).ImpactRadius) * power;
				m_Effects->FlameRing(ground, radius, power, FireballFlameHue(power), FireballBlastSpin);
				m_Effects->Impact(AbilityId::PurpleFireball, ground, 0.0f, radius / GetAbility(AbilityId::PurpleFireball).ImpactRadius, FireballColor(power), 950u + uint32_t(i), [this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); }, power);
			}
			glm::vec3 center;
			float facing = 0.0f;
			if (StageBolt(1, center, facing))
				m_Effects->Trail(Fx::FireballWake, 41ull << 40, center, dt, FireballFullSize, FireballFlameTint(1.0f));
			const FireballLook charge = StageCharge();
			m_Effects->Trail(Fx::FireballKindle, 42ull << 40, charge.Center, dt, 0.6f + 0.4f * charge.Size, FireballFlameTint(charge.Power));
		}
		if (const Level* level = m_Client.GetLevel())
			for (size_t i = 0; i < level->Props.size(); i++)
			{
				const PlacedProp& gate = level->Props[i];
				if (gate.Id == PropId::AutumnTree && LengthSquared(gate.Position - Vec2(m_CameraTarget.x, m_CameraTarget.z)) < 30.0f * 30.0f)
					for (uint32_t stream = 0; stream < TreeLeafStreams; stream++)
						m_Effects->Trail(Fx::FallingLeaves, (6ull << 40) | (uint64_t(i) << 8) | stream, TreeLeafSpawn(gate, stream, m_Time), dt);
				if (gate.Id != PropId::CalderaGate || LengthSquared(gate.Position - Vec2(m_CameraTarget.x, m_CameraTarget.z)) > 26.0f * 26.0f)
					continue;
				for (uint32_t stream = 0; stream < PortalRimStreams; stream++)
				{
					const float along = m_Time * 0.06f + float(stream) / float(PortalRimStreams);
					m_Effects->Trail(Fx::AuraAstralDust, (5ull << 40) | (uint64_t(i) << 8) | stream, PortalRimPoint(gate, along), dt, 2.4f);
				}
			}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			switch (entity.Kind)
			{
			case EntityKind::Projectile:
			{
				const AbilityId ability = AbilityOf(entity);
				if (const std::optional<Fx> trail = TrailFor(ability))
				{
					const glm::vec3 tint = ability == AbilityId::PurpleFireball ? FireballFlameTint(entity.GetChargePower()) : glm::vec3(1.0f);
					m_Effects->Trail(*trail, entity.NetId, ProjectilePoint(entity), dt, IsChargedShot(ability) ? entity.GetScale() : 1.0f, tint);
				}
				if (ability == AbilityId::FrostSpear && entity.GetScale() >= SpearLaneSize)
				{
					const auto [it, fresh] = m_SpearPaths.try_emplace(entity.NetId, SpearPath{ entity.Position, entity.Position, entity.Facing, entity.GetScale(), entity.NetId });
					SpearPath& path = it->second;
					if (!fresh && std::abs(AngleBetween(path.Facing, entity.Facing)) > 0.3f)
					{
						m_Effects->Mark(FrostLane(path), SpearLaneSeconds, 0.01f, 1.0f, 1.0f, 0.25f, 1.2f);
						path = { path.Last, path.Last, entity.Facing, path.Size, path.Seed * 31u + 7u };
					}
					path.Last = entity.Position;
					const Vec2 span = path.Last - path.Start;
					const float length = Length(span);
					for (; path.Grown + SpearLaneStep <= length; path.Grown += SpearLaneStep)
					{
						const uint32_t n = path.Seed * 7u + uint32_t(path.Grown / SpearLaneStep);
						const auto unit = [n](uint32_t salt)
						{
							uint32_t x = (n ^ salt * 0x9E3779B9u) * 0x85EBCA6Bu;
							x ^= x >> 13;
							x *= 0xC2B2AE35u;
							return float((x ^ x >> 16) & 0xFFFFu) / 65535.0f;
						};
						const Vec2 along = span / length;
						const float sign = n % 2u ? 1.0f : -1.0f;
						const Vec2 out = Vec2(-along.y, along.x) * sign;
						const Vec2 at = path.Start + along * (path.Grown + SpearLaneStep * 0.5f) + out * (path.Size * (0.06f + 0.1f * unit(1u)));
						m_Effects->Spike(Ground(at), path.Size * (0.1f + 0.07f * unit(2u)), AngleOf(out) + 0.6f * (unit(3u) - 0.5f), 0.35f + 0.35f * unit(4u), n, SpearLaneSeconds * (0.8f + 0.2f * unit(5u)));
					}
				}
				break;
			}
			case EntityKind::Effect:
				if (entity.Has(EntityFlags::Pending) && AbilityOf(entity) == AbilityId::Comet)
				{
					const float delay = GetAbility(AbilityOf(entity)).ImpactDelay;
					if (const float age = PendingAge(entity); age >= 0.0f)
						m_Effects->Trail(Fx::MeteorTrail, entity.NetId, MeteorPoint(entity, delay > 0.0f ? std::clamp(age / delay, 0.0f, 1.0f) : 1.0f), dt);
				}
				else if (entity.Has(EntityFlags::Pending) && CreditOf(AbilityOf(entity)) == AbilityId::Firebomb)
					if (const std::optional<glm::vec3> bomb = LobPoint(entity, AgeOf(entity.NetId)))
						m_Effects->Trail(Fx::FireTrail, uint64_t(entity.NetId) | (5ull << 40), *bomb + glm::vec3(0.0f, 0.23f, 0.0f), dt, 0.55f);
				break;
			case EntityKind::Player:
				if (entity.Has(EntityFlags::Attacking) && !entity.Has(EntityFlags::Dead) && entity.Variant < uint8_t(AbilityId::Count))
					if (const std::optional<Fx> cast = CastFor(AbilityId(entity.Variant)))
					{
						const glm::vec3 hand = Ground(entity.Position, 1.0f) + Heading(entity.Facing) * 0.35f + Heading(entity.Facing - HalfPi) * 0.3f;
						m_Effects->Trail(*cast, uint64_t(entity.NetId) | (1ull << 40), hand, dt);
					}
				if (entity.Has(EntityFlags::Attacking) && !entity.Has(EntityFlags::Dead) && entity.Variant == uint8_t(AbilityId::FrostSpear))
				{
					const auto fx = m_Fx.find(entity.NetId);
					const IceSpearLook look = ChargingSpear(entity, fx != m_Fx.end() ? &fx->second : nullptr);
					m_Effects->Trail(Fx::SpearAura, uint64_t(entity.NetId) | (6ull << 40), look.Center, dt, 0.6f + 0.4f * look.Size);
					m_Effects->Converge(uint64_t(entity.NetId) | (7ull << 40), look.Center, 1.0f + 0.35f * look.Size, 24.0f + 64.0f * look.Power, 0.32f, dt, 0.8f + 0.25f * look.Size);
				}
				if (entity.Has(EntityFlags::Attacking) && !entity.Has(EntityFlags::Dead) && entity.Variant == uint8_t(AbilityId::PurpleFireball))
				{
					const auto fx = m_Fx.find(entity.NetId);
					const FireballLook look = ChargingFireball(entity, fx != m_Fx.end() ? &fx->second : nullptr);
					const float violet = FireballViolet(look.Power);
					const float size = 0.6f + 0.4f * look.Size;
					m_Effects->Trail(Fx::FireballKindle, uint64_t(entity.NetId) | (12ull << 40), look.Center, dt, size, FireballFlameTint(look.Power));
					const glm::vec3 streak = glm::mix(glm::vec3(0.75f, 0.32f, 0.08f), glm::vec3(0.45f, 0.18f, 0.8f), violet);
					const float reach = 0.9f + 0.45f * look.Size;
					const float surface = 1.4f * FireballBallRadius * look.Size * look.Grow;
					m_Effects->Converge(uint64_t(entity.NetId) | (14ull << 40), look.Center, reach, 26.0f + 70.0f * look.Power, 0.3f, dt, 0.9f + 0.3f * look.Size, ParticleKind::Streak, streak, 0.35f, std::clamp(1.0f - surface / reach, 0.3f, 1.0f));
				}
				if (entity.Has(EntityFlags::Attacking) && !entity.Has(EntityFlags::Dead) && entity.Variant == uint8_t(AbilityId::ObsidianBlades))
				{
					const auto fx = m_Fx.find(entity.NetId);
					if (fx != m_Fx.end() && fx->second.ObsidianAge < 0.08f)
						m_Effects->Converge(uint64_t(entity.NetId) | (15ull << 40), BodyBase(entity, ObsidianHoldHeight) + Heading(entity.Facing) * 0.3f, 0.75f, 70.0f, 0.12f, dt, 0.7f, ParticleKind::Streak, ObsidianSheen * 0.55f, 0.35f, 0.9f);
				}
				if (entity.Has(EntityFlags::Burning) && !entity.Has(EntityFlags::Dead))
					m_Effects->Trail(InStorm() ? Fx::StormCrackle : m_Client.GetSession().Map == MapKind::SkyRuins ? Fx::FrostTrail : Fx::Campfire, uint64_t(entity.NetId) | (3ull << 40), Ground(entity.Position, InStorm() ? 0.85f : 0.05f), dt, 0.55f);
				if (entity.AimsFromPoint() && !entity.Has(EntityFlags::Dead))
				{
					const auto fx = m_Fx.find(entity.NetId);
					const float progress = std::clamp((fx != m_Fx.end() ? std::max(fx->second.SpikeAge, 0.0f) : 0.0f) / GetAbility(AbilityId::EarthSpike).CastTime, 0.0f, 1.0f);
					const Vec2 front = entity.CastPoint + SpikeAimOf(entity) * (GetAbility(AbilityId::EarthSpike).Reach * (0.3f + 0.7f * progress));
					m_Effects->Trail(Fx::SpikeTremor, uint64_t(entity.NetId) | (10ull << 40), Ground(entity.CastPoint, 0.05f), dt, 0.7f + 0.6f * progress);
					m_Effects->Trail(Fx::SpikeTremor, uint64_t(entity.NetId) | (11ull << 40), Ground(front, 0.05f), dt, 0.4f);
				}
				break;
			default:
				break;
			}
		}
	}

	void SessionState::UpdateFeedback(float dt)
	{
		for (Notice& notice : m_Notices)
			notice.Age += dt;
		std::erase_if(m_Notices, [](const Notice& notice) { return notice.Age >= NoticeSeconds; });
		for (const ServerNotice& notice : m_Client.TakeNotices())
		{
			m_Notices.push_back({ notice.Text, notice.Success ? Theme::AccentLight : Theme::Danger });
			m_App.GetGameAudio().PlayNotice(notice.Success, m_Station);
		}
		if (m_Notices.size() > MaxNotices)
			m_Notices.erase(m_Notices.begin(), m_Notices.end() - MaxNotices);
	}

	float SessionState::FlashOf(uint32_t netId) const
	{
		const auto it = m_Fx.find(netId);
		return it != m_Fx.end() ? it->second.Flash / HurtFlashSeconds : 0.0f;
	}

	glm::vec4 SessionState::HitLight(const EntityState& entity) const
	{
		const auto it = m_Fx.find(entity.NetId);
		if (it == m_Fx.end() || it->second.Flash <= 0.0f)
			return glm::vec4(0.0f);
		return glm::vec4(it->second.FlashColor * FlashBright, FlashCover * it->second.Flash / HurtFlashSeconds);
	}

	void SessionState::AddTrauma(float amount)
	{
		m_Trauma = std::min(m_Trauma + amount, 1.0f);
	}

	void SessionState::Rumble(const glm::vec3& point, float strength, uint32_t owner)
	{
		const float falloff = 1.0f - Length(Vec2(point.x, point.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z)) / RumbleReach;
		if (falloff > 0.0f)
			m_Trauma = std::max(m_Trauma, std::min(strength * falloff * (owner != NoPlayer && owner == m_Client.GetPlayerId() ? OwnRumble : 1.0f), 1.0f));
	}

	void SessionState::FeelHit(const HitEvent& hit)
	{
		const bool local = hit.Target == m_Client.GetPlayerNetId();
		if (hit.Guarded || hit.Warded)
		{
			if (local)
				AddTrauma(GuardTrauma);
			return;
		}
		const auto victim = m_Fx.find(hit.Target);
		if (victim != m_Fx.end())
		{
			victim->second.Flash = HurtFlashSeconds;
			victim->second.FlashColor = glm::mix(ElementColor(hit.Damages), glm::vec3(1.0f), FlashWhite);
		}
		const Vec2 push = m_Client.GetPredictedState().Push;
		const float shove = local ? Length(push) : 0.0f;
		const EntityState* attacker = FindBody(hit.Attacker);
		if (hit.Damage >= HeavyHitDamage || hit.Critical || shove >= HeavyHitPush)
		{
			const float stop = std::lerp(HitStopMin, HitStopMax, std::clamp(float(hit.Damage - HeavyHitDamage) / float(HeavyHitDamage), 0.0f, 1.0f));
			if (victim != m_Fx.end())
				victim->second.Stop = std::max(victim->second.Stop, stop);
			if (attacker && attacker->NetId != hit.Target)
				if (const auto it = m_Fx.find(attacker->NetId); it != m_Fx.end())
					it->second.Stop = std::max(it->second.Stop, stop);
		}
		if (!local)
			return;
		const float weight = std::min(hit.Damage > 0 ? float(hit.Damage) / HitTraumaDamage : shove / HitTraumaPush, 1.2f);
		m_Trauma = std::min(std::max(m_Trauma + HitStack, HitTrauma + HitTraumaScale * weight), 1.0f);
		Vec2 direction = shove > 0.5f ? push / shove : Vec2();
		if (direction == Vec2() && attacker && LengthSquared(hit.Position - attacker->Position) > 1e-4f)
			direction = Normalize(hit.Position - attacker->Position);
		m_KickSpeed += glm::vec2(direction.x, direction.y) * (KickImpulse * std::sqrt(std::min(weight, 1.0f)));
	}

	void SessionState::UpdateDanger(float dt)
	{
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		const bool alive = local.Valid && local.State.Alive && local.MaxHealth > 0;
		const float health = alive ? float(local.Health) / float(local.MaxHealth) : 1.0f;
		const float target = health < DangerHealth ? 1.0f : 0.0f;
		m_Danger += (target - m_Danger) * std::min(dt * DangerFade, 1.0f);
		m_DangerLevel = std::clamp((DangerHealth - health) / (DangerHealth - DangerFloor), 0.0f, 1.0f);
		m_Heartbeat = std::fmod(m_Heartbeat + dt * (DangerBeat + DangerBeatRise * m_DangerLevel), 1.0f);
	}

	float SessionState::AgeOf(uint32_t netId) const
	{
		const auto it = m_Fx.find(netId);
		return it != m_Fx.end() ? it->second.Age : 0.0f;
	}

	float SessionState::LocalArea(const EntityState& caster, AbilityId ability) const
	{
		if (caster.NetId != m_Client.GetPlayerNetId())
			return 1.0f;
		const PlayerTraits& traits = m_Client.GetTraits();
		for (const SkillTraits& skill : traits.Skills)
			if (skill.Ability == ability)
				return traits.AreaScale * skill.AreaScale;
		return traits.AreaScale;
	}

	float SessionState::PendingAge(const EntityState& entity) const
	{
		return AgeOf(entity.NetId) - (entity.Has(EntityFlags::Pending) ? UnpackLead(entity.Charge) : 0.0f);
	}

	float SessionState::BarHeight(const EntityState& entity) const
	{
		if (entity.Kind == EntityKind::Player)
			return m_Characters->ForPlayer(LookOf(entity)).Height + 0.1f;
		if (entity.Kind == EntityKind::ShardObelisk)
			return ObeliskHeight * entity.GetScale() + 0.4f;
		if (const CharacterLook* look = m_Characters->ForEnemy(entity.Kind))
			return look->Height * entity.GetScale() + 0.1f + (entity.Kind == EntityKind::MirrorMoth || entity.Kind == EntityKind::LensWisp ? FlierHover : 0.0f);
		return HealthBarHeight(entity.Kind) * (IsEnemyKind(entity.Kind) ? entity.GetScale() : 1.0f);
	}

	const CharacterAnimator* SessionState::AnimatorOf(uint32_t netId) const
	{
		const auto it = m_Fx.find(netId);
		return it != m_Fx.end() ? &it->second.Animator : nullptr;
	}

	CharacterAnimator::Input SessionState::AnimationInput(const EntityState& entity) const
	{
		CharacterAnimator::Input input;
		input.Position = entity.Position;
		input.Height = entity.Kind == EntityKind::Player ? BodyBase(entity).y : GroundHeight(entity.Position);
		input.Facing = entity.Facing;
		input.Dead = entity.Has(EntityFlags::Dead);
		input.Dashing = entity.Has(EntityFlags::Dashing);
		input.Stunned = entity.Has(EntityFlags::Stunned) || entity.Has(EntityFlags::Falling) || entity.Has(EntityFlags::Lifted);
		input.Attacking = entity.Has(EntityFlags::Attacking);
		if (entity.Kind == EntityKind::Player)
		{
			const CharacterLook& look = m_Characters->ForPlayer(LookOf(entity));
			input.Scale = look.Scale;
			input.Skeleton = look.Skeleton;
			input.Pose = look.Pose;
			input.FreeHand = look.FreeHand;
			if (input.Attacking && entity.Variant < uint8_t(AbilityId::Count))
			{
				const AbilityId ability = AbilityId(entity.Variant);
				input.Attack = PlayerAttackAnim(ability);
				input.Windup = GetAbility(ability).CastTime;
				const int8_t slot = m_Client.GetPredictedState().CastSlot;
				if (entity.NetId == m_Client.GetPlayerNetId() && slot >= 0)
					input.Windup = SwingWindup(m_Client.GetTraits().Skills[size_t(slot)], ability);
			}
			const bool stomping = std::any_of(m_NovaFields.begin(), m_NovaFields.end(), [&](const NovaField& field) { return field.Owner == entity.Owner && field.Erupt >= 0.0f && field.Erupt < NovaStompSeconds; });
			if (!input.Attacking && !input.Dead && stomping)
			{
				input.Attacking = true;
				input.Attack = { .Windup = Anim::CastErupt };
				input.Windup = NovaStompSeconds;
			}
			const GameStatus& status = m_Client.GetStatus();
			input.Alert = status.Mode != GameMode::Hub;
			if (!input.Attacking && (entity.Variant & EmoteFlag) != 0)
				input.Emote = entity.Variant;
			if (!input.Attacking && !input.Dead && status.Mode == GameMode::Brawl && status.Brawl.Phase == BrawlPhase::Over && WonBrawlRound(entity.Owner) && status.Brawl.Timer > BrawlOverSeconds - VictoryPoseSeconds)
				input.Emote = PackEmote(Emote::Victory, 0);
			return input;
		}
		const CharacterLook* look = m_Characters->ForEnemy(entity.Kind);
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		input.Scale = (look ? look->Scale : 1.0f) * entity.GetScale();
		input.Skeleton = look ? look->Skeleton : m_Characters->GetModels().GetSharedSkeleton();
		input.Pose = look ? look->Pose : Stance::Skeleton;
		input.Undead = true;
		input.Attack = EnemyAttackAnim(entity.Kind);
		input.Windup = def ? def->Windup : 0.5f;
		if (IsFighterKind(entity.Kind) && input.Attacking && entity.Variant < uint8_t(AbilityId::Count))
		{
			input.Attack = FighterAttackAnim(AbilityId(entity.Variant));
			if (const FighterMove* move = FindFighterMove(entity.Kind, AbilityId(entity.Variant)))
				input.Windup = move->Windup;
		}
		return input;
	}

	void SessionState::UpdateAnimators(float dt)
	{
		const GroundSampler ground = [this](Vec2 point) { return GroundHeight(point); };
		for (auto& [netId, fx] : m_Fx)
		{
			if (fx.Last.Kind != EntityKind::Player && !m_Characters->ForEnemy(fx.Last.Kind))
				continue;
			CharacterAnimator::Input input = AnimationInput(fx.Last);
			input.Hurt = fx.Hurt;
			input.Push = fx.Push;
			if (fx.Last.Kind == EntityKind::Player)
			{
				const auto body = m_BodyHeights.find(netId);
				input.Grounded = body == m_BodyHeights.end() || !body->second.Aloft;
				input.Ground = &ground;
			}
			if (fx.Last.Kind == EntityKind::Player && fx.Last.Has(EntityFlags::Attacking) && fx.Last.Variant < uint8_t(AbilityId::Count))
				fx.Cast = AbilityId(fx.Last.Variant);
			fx.Animator.Update(*m_Characters, input, fx.Stop > 0.0f ? 0.0f : dt);
		}
		for (Corpse& corpse : m_Corpses)
		{
			corpse.Age += dt;
			corpse.Animator.Update(*m_Characters, AnimationInput(corpse.Entity), dt);
		}
		std::erase_if(m_Corpses, [](const Corpse& corpse) { return corpse.Age > CorpseSeconds; });
		if (m_Corpses.size() > MaxCorpses)
			m_Corpses.erase(m_Corpses.begin(), m_Corpses.end() - MaxCorpses);
	}

	glm::vec3 SessionState::DeathColorOf(const EntityState& entity, const EntityFx& fx) const
	{
		if (entity.Has(EntityFlags::Burning))
		{
			const MapKind map = m_Client.GetSession().Map;
			return map == MapKind::FrozenLake ? ElementColor(Element::Cold) : map == MapKind::StormPeak ? StormDeath : map == MapKind::SkyRuins ? glm::vec3(0.2f, 0.85f, 0.72f) : ElementColor(Element::Fire);
		}
		if (fx.SinceHit < KillWindow && fx.HitElement != Element::Physical)
			return ElementColor(fx.HitElement);
		return ArcaneDeath;
	}

	ModelDissolve SessionState::BodyDissolve(const EntityState& entity) const
	{
		const auto it = m_Fx.find(entity.NetId);
		if (it == m_Fx.end())
			return {};
		const EntityFx& fx = it->second;
		ModelDissolve dissolve{ .Scale = 4.0f, .Sweep = -BodyHeight, .Seed = float(entity.NetId % 251u) };
		if (entity.Has(EntityFlags::Dead) && fx.DeathAge >= 0.0f)
		{
			const float t = std::clamp((fx.DeathAge - DeathDissolveDelay) / DeathDissolveSeconds, 0.0f, 1.0f);
			dissolve.Amount = t * (2.0f - t);
			if (m_Client.GetSession().Mode == GameMode::Shardfall && entity.Kind == EntityKind::Player)
				dissolve.Amount = std::min(dissolve.Amount, DownedDissolve);
			dissolve.Edge = fx.DeathColor * DissolveGlow;
		}
		else if (fx.SpawnAge < SpawnDissolveSeconds)
		{
			const float t = fx.SpawnAge / SpawnDissolveSeconds;
			dissolve.Amount = 1.0f - t * (2.0f - t);
			dissolve.Edge = glm::pow(ColorOf(entity.Owner), glm::vec3(2.2f)) * DissolveGlow;
		}
		return dissolve;
	}

	glm::vec4 SessionState::CharacterTint(const EntityState& entity) const
	{
		glm::vec3 color(1.0f);
		if (entity.Has(EntityFlags::Elite))
			color = glm::mix(color, glm::vec3(1.0f, 0.82f, 0.45f), 0.4f);
		if (entity.Has(EntityFlags::Chilled))
			color = glm::mix(color, glm::vec3(0.6f, 0.85f, 1.0f), 0.5f);
		if (entity.Has(EntityFlags::Burning))
			color = InStorm() ? glm::mix(color, glm::vec3(0.62f, 0.76f, 1.0f), 0.35f + (std::fmod(std::floor(m_Time * 14.0f) * 0.618f + float(entity.NetId) * 0.37f, 1.0f) > 0.5f ? 0.3f : 0.0f)) : glm::mix(color, m_Client.GetSession().Map == MapKind::SkyRuins ? glm::vec3(0.45f, 1.0f, 0.88f) : glm::vec3(1.0f, 0.5f, 0.28f), 0.45f + 0.15f * std::sin(m_Time * 11.0f + float(entity.NetId)));
		return glm::vec4(color, 0.0f);
	}

	void SessionState::DrawCorpses(WorldRenderer& renderer) const
	{
		for (const Corpse& corpse : m_Corpses)
		{
			const CharacterLook* look = m_Characters->ForEnemy(corpse.Entity.Kind);
			if (!look)
				continue;
			const float sink = std::max(corpse.Age - CorpseSeconds + CorpseSinkSeconds, 0.0f) / CorpseSinkSeconds;
			const glm::vec3 base = Ground(corpse.Entity.Position) - glm::vec3(0.0f, sink * 0.6f, 0.0f);
			DrawCharacterModel(renderer, *m_Characters, *look, corpse.Animator, base, corpse.Entity.Facing, look->Scale * corpse.Entity.GetScale(), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f);
		}
	}

	PlayerLook SessionState::LookOf(const EntityState& player) const
	{
		if (player.Owner == m_Client.GetPlayerId())
			return { .Style = m_Client.GetCosmetics() };
		if (const PlayerInfo* info = m_Client.FindPlayer(player.Owner))
			return info->Look;
		return {};
	}

	bool SessionState::Render(Rdn::RenderGraph& graph, Rdn::RGImage target, Rdn::Extent2D extent, uint32_t frameIndex)
	{
		const Level* level = m_Client.GetLevel();
		WorldRenderer& renderer = m_App.GetWorldRenderer();
		if (!level || !renderer.IsReady())
		{
			if (m_Client.GetState() == ClientState::InGame)
				return false;
			BackdropView view;
			view.Time = m_App.GetUi().GetTime() + 30.0f;
			view.Mood = m_Config.Host ? BackdropMood::Kindle : BackdropMood::Seek;
			view.Dive = m_Time;
			view.UiScale = m_App.GetUi().GetScale();
			return m_App.GetMenuBackdrop().Render(graph, target, extent, frameIndex, view);
		}
		if (extent != m_Extent)
		{
			m_Extent = extent;
			UpdateCamera(0.0f);
		}

		m_Lighting = MapLighting(level->Kind);
		m_Lighting.Time = m_Time;
		if (m_Lighting.Daylight)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const bool live = brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over;
			const float target = live ? HallDusk(brawl.Plan, m_Client.GetArenaClock(m_Client.GetPresentTick())) : 0.0f;
			m_Dusk += (target - m_Dusk) * (1.0f - std::exp(-2.0f * std::max(m_Time - m_DuskTime, 0.0f)));
			m_DuskTime = m_Time;
			ApplyHallDusk(m_Lighting, m_Dusk);
		}
		if (m_Lighting.Storm)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const bool live = brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over;
			const float clock = live ? m_Client.GetArenaClock(m_Client.GetPresentTick()) : 0.0f;
			const StormRing ring = RingAt(brawl.Plan, clock);
			const float storm = std::lerp(StormGloom, 1.0f, live ? StormBuild(brawl.Plan, clock) : StormBuild(brawl.Plan, 0.0f));
			m_Lighting.StormBuild = storm;
			m_Lighting.Eye = ring.Active ? glm::vec4(ring.Center.x, ring.Center.y, ring.Radius, StormSunlight) : glm::vec4(0.0f);
			m_Lighting.SunIntensity *= 1.0f - 0.5f * storm * storm;
			m_Lighting.SunColor = glm::mix(m_Lighting.SunColor, glm::vec3(0.82f, 0.86f, 1.0f), storm * storm);
			m_Lighting.SkyColor = glm::mix(m_Lighting.SkyColor, glm::vec3(0.085f, 0.085f, 0.19f), storm);
			m_Lighting.GroundColor = glm::mix(m_Lighting.GroundColor, glm::vec3(0.045f, 0.045f, 0.08f), storm);
			m_Lighting.Haze = glm::mix(m_Lighting.Haze, glm::vec3(0.055f, 0.052f, 0.11f), storm);
			m_Lighting.SkyColor += glm::vec3(0.5f, 0.6f, 0.9f) * (1.4f * m_Flash);
			m_Lighting.GroundColor += glm::vec3(0.25f, 0.3f, 0.45f) * m_Flash;
			m_Lighting.SkyGlow = glm::vec4(glm::mix(glm::vec3(0.78f, 0.82f, 1.0f), glm::vec3(0.62f, 0.76f, 1.0f), m_Flash), 2.2f + 8.0f * m_Flash);
			m_Lighting.Rain = 0.6f + 0.4f * (live ? StormBuild(brawl.Plan, clock) : 0.0f);
		}
		if (level->Kind == MapKind::Caldera)
		{
			const BrawlStatus& brawl = m_Client.GetStatus().Brawl;
			const bool live = m_Client.GetSession().Mode == GameMode::Brawl && (brawl.Phase == BrawlPhase::Fight || brawl.Phase == BrawlPhase::Over);
			m_Lighting.Fury = live ? CalderaFury(brawl.Plan, m_Client.GetArenaClock(m_Client.GetPresentTick())) : 0.15f;
			const float beat = m_HeartPhase - std::floor(m_HeartPhase);
			const auto lobe = [](float x, float width) { return std::exp(-(x * x) / (width * width)); };
			m_Lighting.Pulse = std::min(lobe(beat, 0.06f) + 0.65f * lobe(beat - 0.17f, 0.065f) + lobe(beat - 1.0f, 0.06f), 1.0f);
			m_Lighting.GroundColor *= 0.85f + 0.3f * m_Lighting.Pulse + 0.4f * m_Lighting.Fury;
			m_Lighting.LavaLight = CalderaLavaLight;
			m_Lighting.LavaCrust = CalderaLavaCrust;
			m_Lighting.LavaLine = ForgePitSurface;
		}
		if (m_Lighting.Floating)
			m_Lighting.Leviathan = LeviathanPass();
		if (m_Lighting.Lake.Active)
		{
			const float round = m_Client.GetSession().Mode == GameMode::Brawl ? m_Client.GetArenaClock(m_Client.GetPresentTick()) : 0.0f;
			const float swell = glm::smoothstep(0.0f, 75.0f, round);
			const float breath = 0.5f + 0.5f * std::sin(m_Time * 0.13f) * std::sin(m_Time * 0.07f + 1.3f);
			m_Lighting.Lake.Aurora = std::lerp(0.4f, 0.74f, swell) * (0.82f + 0.3f * breath);
			m_Lighting.SkyColor += glm::vec3(0.0f, 0.034f, 0.018f) * (m_Lighting.Lake.Aurora * 0.6f);
		}
		PostProcess& post = renderer.GetPost();
		const float beat = std::pow(std::max(std::cos(2.0f * Pi * m_Heartbeat), 0.0f), 6.0f);
		post.Vignette = m_Danger * DangerVignette * (0.5f + 0.5f * m_DangerLevel) * (0.6f + 0.4f * beat);
		post.Desaturation = m_Danger * DangerDesaturation * (0.4f + 0.6f * m_DangerLevel);
		post.BloomStrength = level->Kind == MapKind::Caldera ? CalderaBloom : DefaultBloom;
		renderer.Begin(m_Camera, m_Lighting, m_Overview ? 0.0f : m_CameraTarget.y, !m_Overview && !m_Lighting.Floating && level->Kind != MapKind::Caldera);
		renderer.SetGroundRelief(m_LevelRenderer.GetRelief());
		renderer.SetSight(m_CameraTarget, !m_Overview);
		m_LevelRenderer.Draw(renderer, *level, ComputeView());
		if (level->Kind == MapKind::Hub)
			for (const EntityState& entity : m_Client.GetEntities())
			{
				if (entity.Kind != EntityKind::Player || entity.Has(EntityFlags::Dead))
					continue;
				const float depth = HubRiverDepth(entity.Position - EmberCenter);
				if (depth > 0.02f)
					renderer.DrawRipple({ entity.Position.x, entity.Position.y, std::min(depth * 5.0f, 1.0f), float(entity.NetId % 97u) });
			}
		const std::vector<LevelFire>& fires = m_LevelRenderer.GetFires();
		const std::vector<LevelFire> staged = StageFires();
		for (size_t i = 0; i < fires.size() + staged.size(); i++)
		{
			const LevelFire& fire = i < fires.size() ? fires[i] : staged[i - fires.size()];
			if (LengthSquared(Vec2(fire.Position.x, fire.Position.z) - Vec2(m_CameraTarget.x, m_CameraTarget.z)) > 26.0f * 26.0f)
				continue;
			const float phase = float(i) * 1.7f;
			const float flicker = 0.85f + 0.1f * std::sin(m_Time * 13.0f + phase) + 0.05f * std::sin(m_Time * 29.0f + phase * 2.3f);
			const bool campfire = fire.Kind == FireKind::Campfire;
			const float size = campfire ? 0.9f : fire.Kind == FireKind::Torch ? 0.45f : fire.Kind == FireKind::Lantern ? 0.32f : 0.16f;
			renderer.DrawSprite({ .Position = fire.Position + glm::vec3(0.0f, campfire ? 0.25f : 0.02f, 0.0f), .Size = size * fire.Scale, .Color = glm::vec4(glm::vec3(1.0f, 0.55f, 0.22f) * flicker, 0.35f), .Shape = ParticleShape::Glow, .Emission = 0.3f, .Softness = 1.0f });
			const float s = fire.Scale;
			const uint32_t seed = uint32_t(i) * 7u + 3u;
			const glm::vec3 lean(0.07f * std::sin(m_Time * 1.3f + phase), 1.0f, 0.05f * std::sin(m_Time * 1.7f + phase * 0.7f));
			switch (fire.Kind)
			{
			case FireKind::Campfire:
				DrawHeatHaze(renderer, fire.Position + glm::vec3(0.0f, 0.9f * s, 0.0f), 0.55f * s, 0.02f, 3.0f, 1.4f, seed);
				break;
			case FireKind::Torch:
				DrawHeatHaze(renderer, fire.Position + glm::vec3(0.0f, 0.4f * s, 0.0f), 0.25f * s, 0.015f, 3.5f, 1.4f, seed);
				break;
			case FireKind::Lantern:
				DrawFlame(renderer, { .Base = fire.Position, .Axis = lean * glm::vec3(0.4f, 1.0f, 0.4f), .Width = 0.055f * s, .Height = 0.14f * s, .Wild = 0.3f, .Speed = 1.8f, .Seed = seed });
				break;
			case FireKind::Candle:
				DrawFlame(renderer, { .Base = fire.Position, .Axis = lean * glm::vec3(0.3f, 1.0f, 0.3f), .Width = 0.024f * s, .Height = 0.065f * s, .Wild = 0.15f, .Speed = 1.6f, .Seed = seed });
				break;
			}
		}
		m_Effects->Draw(renderer, m_SpellMeshes);
		DrawArcFades(renderer);
		for (const BladeSlash& slash : m_BladeSlashes)
			DrawBladeSlash(renderer, slash);
		DrawSwordDebris(renderer);
		for (const BladeFade& fade : m_BladeFades)
		{
			const float t = fade.Age / (fade.Stuck ? ObsidianStickSeconds : ObsidianFadeSeconds);
			const float pitch = fade.Stuck ? 0.0f : 0.9f * t;
			ObsidianLook look;
			look.Center = fade.Position;
			look.Forward = Heading(fade.Facing) * std::cos(pitch) - glm::vec3(0.0f, std::sin(pitch), 0.0f);
			look.Roll = fade.Roll + (fade.Stuck ? 0.18f * std::sin(fade.Age * 70.0f) * std::exp(-fade.Age * 12.0f) : 5.0f * fade.Age);
			look.Glint = 0.35f * (1.0f - t);
			look.Dissolve = glm::smoothstep(0.15f, 1.0f, t);
			look.Ground = fade.Ground;
			look.Variant = fade.Slot;
			DrawObsidianBlade(renderer, m_SpellMeshes, look, m_Time, fade.Seed);
		}
		DrawSpikeRuins(renderer);
		DrawSpikeShards(renderer);
		DrawNovaFields(renderer);
		DrawVoidWells(renderer);
		DrawShields(renderer);
		DrawWards(renderer);
		for (const EntityState& entity : m_Client.GetEntities())
			DrawEntity(renderer, entity);
		DrawCorpses(renderer);
		m_Crawlers->DrawRemains(renderer, m_Time);
		m_Crawlers->DrawTethers(renderer, m_Client.GetEntities(), m_Time);
		m_Crawlers->DrawOrbs(renderer, m_Time);
		if (const Level* level = m_Client.GetLevel(); level && m_Client.GetSession().Mode == GameMode::Hub)
			if (const Station* rift = level->FindStation(StationKind::ShardRift))
				m_Crawlers->DrawRift(renderer, Ground(rift->Position), m_Time, m_Station == StationKind::ShardRift);
		if (const Level* level = m_Client.GetLevel(); level && m_Client.GetSession().Mode == GameMode::Shardfall && level->BossArenaRadius > 0.0f)
		{
			const RunPhase phase = m_Client.GetStatus().Run.Phase;
			if (phase == RunPhase::Gate || phase == RunPhase::Respite)
			{
				const glm::vec3 stone = Ground(level->BossArena);
				const bool rift = phase == RunPhase::Respite;
				m_Crawlers->DrawRift(renderer, stone, m_Time, rift);
				DrawGroundRing(renderer, stone, GateRadius, rift ? glm::vec3(0.55f, 0.85f, 1.0f) : glm::vec3(0.75f, 0.5f, 1.0f), 1.2f + 0.4f * std::sin(m_Time * 2.5f), 0.04f);
			}
		}
		if (const Level* level = m_Client.GetLevel(); level && m_Client.GetSession().Mode == GameMode::Shardfall && level->Doors.size() == DoorSlots)
		{
			const RunStatus& run = m_Client.GetStatus().Run;
			const bool open = run.Cleared && run.Phase == RunPhase::Act;
			const ActMap map = MakeActMap(m_Client.GetSession().Run.Seed, run.Act);
			for (uint8_t slot = 0; slot < DoorSlots; slot++)
			{
				if (!(level->DoorMask & (1u << slot)))
					continue;
				const glm::vec3 base = Ground(level->Doors[slot]);
				const uint8_t target = run.Room != NoRoom ? DoorTarget(map, run.Room, slot) : NoRoom;
				const glm::vec3 tone = RoomTone(map.At(target).Kind);
				const float pulse = std::sin(m_Time * 2.5f + float(slot) * 1.7f);
				if (open)
				{
					m_Crawlers->DrawRift(renderer, base, m_Time + float(slot) * 1.3f, run.Votes[slot] > 0);
					DrawGroundRing(renderer, base, DoorRadius, tone, 1.1f + 0.3f * pulse + (run.Votes[slot] > 0 ? 0.3f : 0.0f), 0.05f);
				}
				else
					DrawGroundRing(renderer, base, DoorRadius, tone * 0.5f, 0.45f, 0.03f, 14.0f, 0.15f);
			}
			const RoomKind calm = CurrentRoomKind();
			if ((calm == RoomKind::Shop || calm == RoomKind::Shrine) && level->Altars.size() == 2)
			{
				const glm::vec3 counter = Ground((level->Altars[0] + level->Altars[1]) * 0.5f);
				m_Crawlers->DrawRift(renderer, counter, m_Time, true);
				DrawGroundRing(renderer, counter, 2.2f, RoomTone(calm), 1.1f + 0.3f * std::sin(m_Time * 2.0f), 0.05f);
			}
			const uint8_t rested = calm == RoomKind::Rest ? m_Client.GetLocalPlayer().Run.Rested : uint8_t(0);
			for (size_t i = 0; i < level->Altars.size() && i < 2 && calm == RoomKind::Rest; i++)
			{
				const glm::vec3 tone = i == 0 ? HealAltarTone : TrainAltarTone;
				const bool spent = rested != 0;
				const bool chosen = rested == i + 1;
				DrawGroundRing(renderer, Ground(level->Altars[i]), AltarRadius, spent && !chosen ? tone * 0.3f : tone, spent ? 0.5f : 1.1f + 0.3f * std::sin(m_Time * 2.0f + float(i)), 0.06f, spent ? 0.0f : 10.0f, spent ? 0.0f : 0.3f);
			}
		}
		DrawHallRing(renderer);
		DrawPeak(renderer);
		DrawWonders(renderer);
		DrawChessTable(renderer);
		if (m_App.GetSettings().DistortionStage != 0)
			DrawDistortionStage(renderer);
		if (m_App.GetSettings().FireStage)
			DrawFireStage(renderer);
		for (const FireballBurst& burst : m_FireballBursts)
		{
			if (burst.Comet)
				DrawCometBlast(renderer, { .Ground = burst.Ground, .Radius = burst.Radius, .Age = burst.Age }, burst.Seed);
			else
				DrawFireballBlast(renderer, { .Center = burst.Ground + glm::vec3(0.0f, FireballBlastHeight, 0.0f), .Ground = burst.Ground, .Radius = burst.Radius, .Power = burst.Power, .Age = burst.Age }, burst.Seed);
		}
		DrawCometChunks(renderer);
		const PlayerState& state = m_Client.GetPredictedState();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		if (m_AimPoint && self && !m_Paused && !m_Overview && m_HoveredPortal < 0)
		{
			if (state.CastSlot >= 0)
			{
				const SkillTraits& skill = m_Client.GetTraits().Skills[size_t(state.CastSlot)];
				const AbilityDef& ability = GetAbility(skill.Ability);
				if (ability.Kind == AbilityKind::Ground && !AimsFromPoint(ability.Id))
				{
					const Vec2 point = GetGroundTarget(*level, self->Position, FromAngle(m_Aim), m_AimDistance, ability.Range);
					const float radius = ability.ImpactRadius * m_Client.GetTraits().AreaScale * skill.AreaScale;
					if (!DrawSpellMarkFor(renderer, ability.Id, Ground(point), radius, -1.0f, 1.0f, 7u))
						DrawRuneCircle(renderer, { .Center = Ground(point), .Radius = radius, .Color = SpellTint(ability.Id), .Intensity = 0.6f, .Spin = 0.6f, .Seed = 7u });
				}
			}
		}
		renderer.Render(graph, target, extent, frameIndex, { 0.05f, 0.055f, 0.07f, 1.0f });
		if (m_Paused)
			m_App.GetMenuBackdrop().RenderVeil(graph, extent, m_App.GetUi().GetTime());
		return true;
	}

	float SessionState::LeviathanPass() const
	{
		constexpr double Cycle = 720.0;
		constexpr double Pass = 75.0;
		if (m_App.GetSettings().Leviathan)
			return float(std::fmod(double(m_Time) / Pass + 0.45, 1.0));
		const double seconds = m_Client.GetRenderTick() / double(std::max(m_Client.GetTickRate(), 1u));
		const double cycle = std::floor(seconds / Cycle);
		uint32_t hash = uint32_t(cycle) * 2654435761u + 0x9E3779B9u;
		hash = (hash ^ (hash >> 15)) * 2246822519u;
		hash ^= hash >> 13;
		const double start = 120.0 + 420.0 * double(hash & 0xFFFFu) / 65535.0;
		const double progress = (seconds - cycle * Cycle - start) / Pass;
		return progress >= 0.0 && progress <= 1.0 ? float(progress) : -1.0f;
	}

	void SessionState::DrawEntity(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		switch (entity.Kind)
		{
		case EntityKind::Player:
			DrawPlayer(renderer, entity);
			break;
		case EntityKind::Chaser:
		case EntityKind::Shooter:
		case EntityKind::Brute:
		case EntityKind::Caster:
		case EntityKind::CrystalDuelist:
		case EntityKind::ShardArcher:
		case EntityKind::PrismMage:
		case EntityKind::GeodeBrute:
		case EntityKind::MirrorMoth:
		case EntityKind::LensWisp:
			DrawEnemy(renderer, entity);
			break;
		case EntityKind::ShardObelisk:
			DrawObelisk(renderer, entity);
			break;
		case EntityKind::Archivist:
		case EntityKind::Wyrm:
			DrawBoss(renderer, entity);
			break;
		case EntityKind::QuartzCrawler:
		case EntityKind::JadeCrawler:
		case EntityKind::SapphireCrawler:
		case EntityKind::RubyCrawler:
		case EntityKind::AmethystCrawler:
		case EntityKind::PrismaticCrawler:
		case EntityKind::CrawlerEgg:
			m_Crawlers->Draw(renderer, entity, m_Time, HitLight(entity));
			break;
		case EntityKind::Dummy:
			DrawDummy(renderer, entity);
			break;
		case EntityKind::Projectile:
			DrawProjectile(renderer, entity);
			break;
		case EntityKind::Effect:
			DrawEffect(renderer, entity);
			break;
		case EntityKind::Loot:
		{
			const float phase = float(entity.NetId % 97);
			const glm::vec3 color = LootColors[std::min<size_t>(entity.Variant, std::size(LootColors) - 1)];
			const float bob = 0.38f + 0.08f * std::sin(m_Time * 3.0f + phase);
			if (entity.Variant >= uint8_t(LootKind::Magnet))
			{
				const bool chest = LootKind(entity.Variant) == LootKind::Chest;
				const float pulse = 0.75f + 0.25f * std::sin(m_Time * 4.0f + phase);
				const glm::vec3 lift = base + glm::vec3(0.0f, 0.25f + bob, 0.0f);
				if (chest)
					for (uint32_t i = 0; i < 3; i++)
						m_Crawlers->DrawShard(renderer, lift + Heading(m_Time * 0.6f + float(i) * 2.094f) * 0.22f, m_Time * 0.6f + float(i) * 2.094f, color, m_Time + float(i));
				else
					m_Crawlers->DrawShard(renderer, lift, m_Time * 1.4f + phase, color, m_Time);
				DrawGlow(renderer, lift + glm::vec3(0.0f, 0.2f, 0.0f), (chest ? 0.7f : 0.45f) * pulse, color);
				DrawGroundGlow(renderer, base, chest ? 0.9f : 0.6f, color, 0.8f * pulse);
				DrawGroundRing(renderer, base, chest ? 0.9f : 0.6f, color, 0.9f * pulse, 0.03f);
				renderer.DrawLight({ .Position = lift + glm::vec3(0.0f, 0.4f, 0.0f), .Radius = chest ? 4.0f : 3.0f, .Color = color * (1.6f * pulse) });
				break;
			}
			const MeshKind mesh = LootKind(entity.Variant) == LootKind::Health || LootKind(entity.Variant) == LootKind::Mana ? MeshKind::Sphere : MeshKind::Box;
			renderer.Draw(mesh, base + glm::vec3(0.0f, bob, 0.0f), { 0.14f, 0.14f, 0.14f }, color, m_Time * 2.0f + phase, 0.75f, base.y);
			DrawGlow(renderer, base + glm::vec3(0.0f, bob, 0.0f), 0.32f, color * 0.55f);
			DrawGroundGlow(renderer, base, 0.4f, color, 0.6f);
			break;
		}
		default:
			break;
		}
	}

	void SessionState::DrawPlayer(WorldRenderer& renderer, const EntityState& entity) const
	{
		const CharacterAnimator* animator = AnimatorOf(entity.NetId);
		if (!animator)
			return;
		if (const PlayerInfo* info = m_Client.FindPlayer(entity.Owner); info && info->Spectator && entity.Has(EntityFlags::Dead))
			return;
		const glm::vec3 base = BodyBase(entity);
		const CharacterLook& look = m_Characters->ForPlayer(LookOf(entity));
		const bool dead = entity.Has(EntityFlags::Dead);
		const glm::vec3 color = ColorOf(entity.Owner);
		const glm::vec4 accent(glm::pow(color, glm::vec3(2.2f)), PlayerRim);
		const CharacterPose pose = DrawCharacterModel(renderer, *m_Characters, look, *animator, base, entity.Facing, look.Scale, CharacterTint(entity), 0.0f, accent, HitLight(entity), BodyDissolve(entity), dead ? 0u : MeshHero);
		if (dead)
		{
			if (m_Client.GetSession().Mode == GameMode::Shardfall && !IsRunOver(m_Client.GetStatus().Run.Phase))
			{
				const float revive = entity.NetId == m_Client.GetPlayerNetId() ? float(m_Client.GetLocalPlayer().Revive) / 255.0f : entity.GetChargePower();
				const float pulse = 0.5f + 0.5f * std::sin(m_Time * (3.0f + 6.0f * revive));
				DrawGroundRing(renderer, base, ReviveRange, color, 0.7f + 0.6f * pulse, 0.03f);
				if (revive > 0.0f)
					DrawGroundRing(renderer, base, ReviveRange * revive, glm::mix(color, glm::vec3(1.0f), 0.5f), 1.6f, 0.05f);
			}
			return;
		}
		const auto lifted = m_BodyHeights.find(entity.NetId);
		m_Cosmetics->Forget(m_Time);
		m_Cosmetics->Draw(renderer, *m_Effects, { .Key = entity.NetId, .Style = LookOf(entity).Style, .Base = base, .Facing = entity.Facing, .Scale = look.Scale, .Grounded = lifted == m_BodyHeights.end() || !lifted->second.Aloft, .Pose = &pose, .Rig = &m_Characters->GetRig(look.Skeleton) }, m_Time);
		if (const auto sworded = m_Fx.find(entity.NetId); sworded != m_Fx.end())
			DrawFloatingSword(renderer, entity, sworded->second);
		if (entity.Has(EntityFlags::Dashing))
			renderer.DrawTrail(uint64_t(entity.NetId) | (22ull << 40), base + glm::vec3(0.0f, 0.95f * look.Scale, 0.0f), DashTrail(color));
		if (look.Tip)
		{
			const glm::vec3 tip = pose.JointPoint(look.TipJoint, *look.Tip);
			m_WandTips[entity.Owner] = { tip, m_Time };
			if (const CharacterRig& rig = m_Characters->GetRig(look.Skeleton); rig.Chest)
				m_ChestShift[entity.NetId] = pose.JointPosition(*rig.Chest) - glm::vec3(pose.Model * glm::vec4(rig.ChestRest, 1.0f));
			if (const float strike = animator->StrikeWeight(*m_Characters); strike > 0.02f)
			{
				const auto fx = m_Fx.find(entity.NetId);
				const AbilityId cast = fx != m_Fx.end() ? fx->second.Cast : AbilityId::Count;
				const glm::vec3 tint = cast == AbilityId::PurpleFireball ? FireballColor(1.0f) : cast != AbilityId::Count ? SpellTint(cast) : color;
				renderer.DrawTrail(uint64_t(entity.NetId) | (21ull << 40), tip, WandTrail(tint), strike);
			}
		}
		const bool brawl = m_Client.GetSession().Mode == GameMode::Brawl;
		if (brawl)
			DrawBrawlPlayer(renderer, entity, base);
		const auto body = m_BodyHeights.find(entity.NetId);
		if (body != m_BodyHeights.end() && body->second.Aloft)
			return;
		DrawContactShadow(renderer, base, ContactRadius * look.Scale, ContactStrength);
		DrawGroundRing(renderer, base, 0.42f, color, 1.2f, 0.022f, 0.0f, 0.0f, PlayerPattern(entity.Owner));
		if (m_Client.IsHostile(entity) && !brawl)
			DrawGroundRing(renderer, base, 0.56f, { 1.0f, 0.25f, 0.18f }, 1.6f, 0.03f, 12.0f, 0.15f);
		if (entity.Has(EntityFlags::Stunned))
			DrawStunStars(renderer, pose.JointPosition(m_Characters->GetRig(look.Skeleton).Head) + glm::vec3(0.0f, 0.75f * look.Scale, 0.0f));
		if (entity.NetId == m_Client.GetPlayerNetId())
			DrawBarrageReticle(renderer, entity);
		if (!entity.Has(EntityFlags::Attacking) || entity.Variant >= uint8_t(AbilityId::Count))
			return;
		const AbilityId casting = AbilityId(entity.Variant);
		if (casting == AbilityId::FrostSpear)
		{
			DrawSpearCharge(renderer, entity, base);
			return;
		}
		if (casting == AbilityId::EarthSpike)
		{
			DrawSpikeAim(renderer, entity);
			return;
		}
		if (casting == AbilityId::PurpleFireball)
		{
			DrawFireballCharge(renderer, entity, base);
			return;
		}
		if (casting == AbilityId::ObsidianBlades)
		{
			DrawObsidianCharge(renderer, entity);
			return;
		}
		const glm::vec3 glow = SpellTint(casting);
		const float pulse = 0.6f + 0.4f * std::sin(m_Time * 14.0f);
	}

	IceSpearLook SessionState::ChargingSpear(const EntityState& caster, const EntityFx* fx) const
	{
		const float power = caster.GetChargePower();
		const float age = fx ? std::max(fx->SpearAge, 0.0f) : 0.0f;
		const float phase = float(caster.NetId % 97u);
		const glm::vec3 base = BodyBase(caster);
		IceSpearLook look;
		look.Facing = caster.Facing;
		look.Center = base + glm::vec3(0.0f, SpearHoldHeight + 0.05f * std::sin(m_Time * 3.1f + phase), 0.0f) + Heading(caster.Facing - HalfPi) * SpearHoldSide;
		look.Pitch = -0.05f;
		look.Roll = 0.4f * std::sin(m_Time * 1.7f + phase);
		look.Size = SpearShot(power).Size;
		look.Grow = 1.0f - std::pow(1.0f - std::clamp(age / 0.2f, 0.0f, 1.0f), 3.0f);
		look.Power = power;
		look.Flash = fx && fx->SpearReady >= 0.0f ? std::exp(-fx->SpearReady * 7.0f) : 0.0f;
		look.Ground = base.y;
		return look;
	}

	GroundDecal SessionState::FrostLane(const SpearPath& path) const
	{
		const Vec2 span = path.Last - path.Start;
		const float length = std::max(Length(span), 0.05f);
		const float width = 0.3f * path.Size;
		GroundDecal lane;
		lane.Center = Ground((path.Start + path.Last) * 0.5f);
		lane.Yaw = length > 0.06f ? AngleOf(span) : path.Facing;
		lane.Extent = glm::vec2(length * 0.5f + width, width);
		lane.Depth = std::min(0.75f + 0.08f * length, 1.4f);
		lane.Shape = DecalShape::FrostLane;
		lane.Color = glm::vec4(0.35f, 0.65f, 1.0f, 0.6f);
		lane.Tint = glm::vec4(0.78f, 0.88f, 1.0f, 0.9f);
		lane.Roughness = 0.25f;
		lane.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		lane.Seed = path.Seed;
		return lane;
	}

	void SessionState::SpearMoments(EntityFx& fx, const EntityState& entity, float dt)
	{
		const auto charging = [](const EntityState& state) { return state.Has(EntityFlags::Attacking) && state.Variant == uint8_t(AbilityId::FrostSpear) && !state.Has(EntityFlags::Dead); };
		const bool was = charging(fx.Last);
		if (charging(entity))
		{
			fx.SpearAge = was && fx.SpearAge >= 0.0f ? fx.SpearAge + dt : 0.0f;
			fx.SpearReady = entity.GetChargePower() < 1.0f ? -1.0f : fx.SpearReady >= 0.0f ? fx.SpearReady + dt : 0.0f;
			if (!was)
				return;
			const float before = fx.Last.GetChargePower();
			const float after = entity.GetChargePower();
			const IceSpearLook look = ChargingSpear(entity, &fx);
			if (before < 1.0f && after >= 1.0f)
				m_Effects->Burst(Fx::SpearReady, look.Center, glm::vec3(0.0f, 1.0f, 0.0f), 0.5f + 0.3f * look.Size);
			else
				for (const float step : SpearCrownSteps)
					if (before < step && after >= step)
						m_Effects->Burst(Fx::SpearReady, look.Center - Heading(entity.Facing) * (0.2f * SpearLength * look.Size), glm::vec3(0.0f, 1.0f, 0.0f), 0.35f);
			return;
		}
		if (was)
		{
			const IceSpearLook look = ChargingSpear(fx.Last, &fx);
			if (entity.Has(EntityFlags::Dashing) || entity.Has(EntityFlags::Stunned) || entity.Has(EntityFlags::Falling))
				m_Effects->Burst(Fx::SpearCrack, look.Center, glm::vec3(0.0f, 1.0f, 0.0f), 0.3f + 0.2f * look.Size);
			else
				m_Effects->Burst(Fx::SpearRelease, look.Center, Heading(fx.Last.Facing), 0.6f + 0.4f * look.Size);
		}
		fx.SpearAge = -1.0f;
		fx.SpearReady = -1.0f;
	}

	void SessionState::DrawSpearCharge(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& base) const
	{
		const auto fx = m_Fx.find(entity.NetId);
		const IceSpearLook look = ChargingSpear(entity, fx != m_Fx.end() ? &fx->second : nullptr);
		DrawIceSpear(renderer, m_SpellMeshes, look, m_Time, entity.NetId);
		DrawRuneCircle(renderer, { .Center = base, .Radius = (0.75f + 0.35f * look.Power) * look.Grow, .Color = SpearIce, .Intensity = 0.3f + 0.45f * look.Power + 0.4f * look.Flash, .Spin = 1.6f, .Progress = look.Power, .Appear = look.Grow, .Seed = entity.NetId });
		GroundDecal frost;
		frost.Center = base;
		frost.Extent = glm::vec2((0.5f + 1.0f * look.Power) * look.Grow);
		frost.Depth = 0.6f;
		frost.Shape = DecalShape::Frost;
		frost.Color = glm::vec4(0.35f, 0.6f, 0.95f, 0.35f);
		frost.Tint = glm::vec4(0.78f, 0.88f, 1.0f, 0.45f + 0.35f * look.Power);
		frost.Roughness = 0.3f;
		frost.Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		frost.Seed = entity.NetId;
		renderer.DrawDecal(frost);
		DrawGroundGlow(renderer, base, 1.1f + 0.4f * look.Power, SpearIce * (0.07f + 0.15f * look.Power + 0.15f * look.Flash), 1.0f);
		if (entity.NetId == m_Client.GetPlayerNetId())
		{
			const AbilityDef& spear = GetAbility(AbilityId::FrostSpear);
			const ChargedShot shot = SpearShot(look.Power);
			DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * spear.SpawnOffset, entity.Facing, shot.Range, 2.0f * shot.Radius + 0.12f, SpearIce, 0.28f + 0.3f * look.Power, -1.0f);
		}
	}

	FireballLook SessionState::ChargingFireball(const EntityState& caster, const EntityFx* fx) const
	{
		const float power = caster.GetChargePower();
		const float age = fx ? std::max(fx->FireballAge, 0.0f) : 0.0f;
		const float phase = float(caster.NetId % 97u);
		FireballLook look;
		look.Size = FireballShot(power).Size;
		look.Grow = 1.0f - std::pow(1.0f - std::clamp(age / 0.25f, 0.0f, 1.0f), 3.0f);
		look.Forward = Heading(caster.Facing);
		look.Center = BodyBase(caster) + glm::vec3(0.0f, FireballHoldHeight + 0.04f * std::sin(m_Time * 2.7f + phase), 0.0f) + look.Forward * (FireballHoldReach + 0.6f * FireballBallRadius * look.Size * look.Grow);
		if (const auto shift = m_ChestShift.find(caster.NetId); shift != m_ChestShift.end())
			look.Center += shift->second;
		look.Power = power;
		look.Flash = fx && fx->FireballReady >= 0.0f ? std::exp(-fx->FireballReady * 7.0f) : 0.0f;
		return look;
	}

	void SessionState::FireballMoments(EntityFx& fx, const EntityState& entity, float dt)
	{
		const auto charging = [](const EntityState& state) { return state.Has(EntityFlags::Attacking) && state.Variant == uint8_t(AbilityId::PurpleFireball) && !state.Has(EntityFlags::Dead); };
		const bool was = charging(fx.Last);
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		if (charging(entity))
		{
			fx.FireballAge = was && fx.FireballAge >= 0.0f ? fx.FireballAge + dt : 0.0f;
			fx.FireballReady = entity.GetChargePower() < 1.0f ? -1.0f : fx.FireballReady >= 0.0f ? fx.FireballReady + dt : 0.0f;
			if (!was)
				return;
			const float before = fx.Last.GetChargePower();
			const float after = entity.GetChargePower();
			const FireballLook look = ChargingFireball(entity, &fx);
			if (before < 1.0f && after >= 1.0f)
			{
				m_Effects->Burst(Fx::FireballReady, look.Center, up, 0.8f + 0.2f * look.Size);
				m_Effects->Burst(Fx::FireballRing, Ground(entity.Position, 0.05f), up, 1.0f);
			}
			else if (before < FireballSteps[0] && after >= FireballSteps[0])
				m_Effects->Burst(Fx::FireBurst, look.Center, up, 0.4f);
			else if (before < FireballSteps[1] && after >= FireballSteps[1])
				m_Effects->Burst(Fx::FireballReady, look.Center, up, 0.4f);
			return;
		}
		if (was)
		{
			const FireballLook look = ChargingFireball(fx.Last, &fx);
			const float scale = 0.6f + 0.4f * look.Size;
			if (entity.Has(EntityFlags::Dashing) || entity.Has(EntityFlags::Stunned) || entity.Has(EntityFlags::Falling))
				m_Effects->Burst(Fx::FireballSputter, look.Center, up, 0.5f * scale);
			else if (look.Power < 0.35f)
				m_Effects->Burst(Fx::FireBurst, look.Center, look.Forward, 0.5f);
			else
			{
				m_Effects->Burst(Fx::FireballRelease, look.Center, look.Forward, scale);
				if (look.Power > 0.8f)
					m_Effects->Burst(Fx::FireballRing, Ground(fx.Last.Position, 0.05f), up, 0.8f);
			}
		}
		fx.FireballAge = -1.0f;
		fx.FireballReady = -1.0f;
	}

	void SessionState::DrawFireballCharge(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& base) const
	{
		const auto fx = m_Fx.find(entity.NetId);
		FireballLook look = ChargingFireball(entity, fx != m_Fx.end() ? &fx->second : nullptr);
		look.Ground = base.y;
		look.Grounded = true;
		DrawPurpleFireball(renderer, look, m_Time, entity.NetId);
		const glm::vec3 color = FireballColor(look.Power);
		DrawRuneCircle(renderer, { .Center = base, .Radius = (0.75f + 0.4f * look.Power) * look.Grow, .Color = color, .Intensity = 0.2f + 0.28f * look.Power + 0.12f * look.Flash, .Spin = 1.2f + 1.0f * look.Power, .Progress = look.Power, .Appear = look.Grow, .Seed = entity.NetId });
		DrawGroundGlow(renderer, Ground(Vec2(look.Center.x, look.Center.z)), 0.8f + 0.9f * look.Power, color * (0.08f + 0.14f * look.Power + 0.1f * look.Flash), 1.0f);
		if (entity.NetId != m_Client.GetPlayerNetId())
			return;
		const AbilityDef& def = GetAbility(AbilityId::PurpleFireball);
		const ChargedShot shot = FireballShot(look.Power);
		const Vec2 aim = FromAngle(entity.Facing);
		DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * def.SpawnOffset, entity.Facing, shot.Range, 2.0f * shot.Radius + 0.12f, color, 0.24f + 0.26f * look.Power, -1.0f);
		DrawGroundRing(renderer, Ground(entity.Position + aim * (def.SpawnOffset + shot.Range)), shot.Blast, color, 0.45f + 0.4f * look.Power, 0.025f, 18.0f, 0.4f);
	}

	uint32_t SessionState::FormingBlades(const EntityState& caster, float age, std::array<ObsidianLook, ObsidianVolley>& blades) const
	{
		const glm::vec3 body = BodyBase(caster, ObsidianHoldHeight);
		const glm::vec3 hand = body + Heading(caster.Facing) * 0.3f;
		const float middle = float(ObsidianVolley / 2);
		uint32_t formed = 0;
		for (uint32_t k = 0; k < ObsidianVolley; k++)
		{
			const float rank = std::abs(float(k) - middle);
			const float side = float(k) < middle ? -1.0f : 1.0f;
			const float t = age - rank * ObsidianFormStagger;
			const float form = std::clamp(t / ObsidianFormSeconds, 0.0f, 1.0f);
			const float back = form - 1.0f;
			const float ease = 1.0f + 2.70158f * back * back * back + 1.70158f * back * back;
			const float sweep = (age - ObsidianSweepStart - 0.006f * rank) / ObsidianSweepSeconds;
			ObsidianLook& look = blades[k];
			look.Center = glm::mix(hand, BladeRest(body, caster.Facing, k), ease);
			look.Forward = Heading(BladeFanAngle(caster.Facing, k) + side * 2.4f * (1.0f - ease));
			look.Roll = 0.08f * side * rank + 3.5f * (1.0f - ease);
			look.Grow = form > 0.0f ? 0.35f + 0.65f * ease : 0.0f;
			look.Flash = t > ObsidianFormSeconds ? std::exp(-(t - ObsidianFormSeconds) * 28.0f) : 0.0f;
			look.Sweep = sweep >= 0.0f && sweep <= 1.0f ? sweep : -1.0f;
			look.Glint = 0.15f + (look.Sweep >= 0.0f ? 0.55f * std::sin(sweep * Pi) : 0.0f) + 0.4f * look.Flash;
			look.Ground = body.y - ObsidianHoldHeight;
			look.Variant = k;
			formed += form > 0.0f ? 1u : 0u;
		}
		return formed;
	}

	void SessionState::ObsidianMoments(EntityFx& fx, const EntityState& entity, float dt)
	{
		const auto casting = [](const EntityState& state) { return state.Has(EntityFlags::Attacking) && state.Variant == uint8_t(AbilityId::ObsidianBlades) && !state.Has(EntityFlags::Dead); };
		const bool was = casting(fx.Last);
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		std::array<ObsidianLook, ObsidianVolley> blades;
		if (casting(entity))
		{
			fx.ObsidianAge = was && fx.ObsidianAge >= 0.0f ? fx.ObsidianAge + dt : 0.0f;
			if (!was)
				m_Effects->Burst(Fx::ObsidianSummon, BodyBase(entity, ObsidianHoldHeight) + Heading(entity.Facing) * 0.3f, Heading(entity.Facing));
			std::array<ObsidianLook, ObsidianVolley> previous;
			if (was)
				FormingBlades(fx.Last, std::max(fx.ObsidianAge - dt, 0.0f), previous);
			FormingBlades(entity, fx.ObsidianAge, blades);
			for (uint32_t k = 0; k < ObsidianVolley; k++)
				if (blades[k].Flash > 0.0f && previous[k].Flash <= 0.0f)
					m_Effects->Burst(Fx::ObsidianForm, blades[k].Center, up, 0.6f);
			return;
		}
		if (was)
		{
			FormingBlades(fx.Last, std::max(fx.ObsidianAge, 0.0f), blades);
			if (entity.Has(EntityFlags::Dashing) || entity.Has(EntityFlags::Stunned) || entity.Has(EntityFlags::Falling) || entity.Has(EntityFlags::Dead))
			{
				for (const ObsidianLook& blade : blades)
					if (blade.Grow > 0.0f)
						m_Effects->Burst(Fx::ObsidianCrumble, blade.Center, up, 0.45f);
			}
			else
			{
				const glm::vec3 forward = Heading(fx.Last.Facing);
				m_Effects->Burst(Fx::ObsidianRelease, BodyBase(fx.Last, ObsidianHoldHeight) + forward * 0.55f, forward);
			}
		}
		fx.ObsidianAge = -1.0f;
	}

	void SessionState::DrawObsidianCharge(WorldRenderer& renderer, const EntityState& entity) const
	{
		const auto fx = m_Fx.find(entity.NetId);
		const float age = fx != m_Fx.end() ? std::max(fx->second.ObsidianAge, 0.0f) : 0.0f;
		std::array<ObsidianLook, ObsidianVolley> blades;
		FormingBlades(entity, age, blades);
		for (uint32_t k = 0; k < ObsidianVolley; k++)
			if (blades[k].Grow > 0.0f)
				DrawObsidianBlade(renderer, m_SpellMeshes, blades[k], m_Time, entity.NetId + k);
		const glm::vec3 hand = BodyBase(entity, ObsidianHoldHeight) + Heading(entity.Facing) * 0.3f;
		const float tear = 1.0f - std::clamp(age / 0.12f, 0.0f, 1.0f);
		DrawImplosion(renderer, hand, 0.6f, 0.07f * tear, m_Time * 1.5f, 2);
		ParticleSprite rift;
		rift.Position = hand;
		rift.Size = 0.3f + 0.15f * tear;
		rift.Color = glm::vec4(0.04f, 0.0f, 0.08f, 0.75f * tear);
		rift.Additive = 0.0f;
		rift.Emission = 0.0f;
		rift.Softness = 0.5f;
		renderer.DrawSprite(rift);
		DrawGlow(renderer, hand, 0.34f, ObsidianSheen * (0.1f + 0.25f * tear), 0.4f);
	}

	void SessionState::LaunchBlade(EntityFx& fx, const EntityState& entity)
	{
		Vec2 sum;
		for (const EntityState& other : m_Client.GetEntities())
			if (other.Kind == EntityKind::Projectile && other.Owner == entity.Owner && other.Tag == entity.Tag && other.Variant == entity.Variant)
				sum += FromAngle(other.Facing);
		const float aim = LengthSquared(sum) > 1e-6f ? AngleOf(sum) : entity.Facing;
		const float offset = AngleBetween(aim, entity.Facing);
		const ProjectilePattern fan = BladePattern();
		uint32_t slot = 0;
		for (uint32_t k = 1; k < fan.Count; k++)
			if (std::abs(ProjectileOffset(fan, k) - offset) < std::abs(ProjectileOffset(fan, slot) - offset))
				slot = k;
		fx.BladeSlot = slot;
		const EntityState* caster = FindBody(entity.Owner);
		if (!caster || Length(entity.Position - caster->Position) > GetAbility(AbilityId::ObsidianBlades).SpawnOffset + 1.0f)
			return;
		fx.BladeOffset = BladeRest(BodyBase(*caster, ObsidianHoldHeight), aim, slot) - glm::vec3(entity.Position.x, LaunchHeight(entity), entity.Position.y);
		fx.BladeTurn = AngleBetween(entity.Facing, BladeFanAngle(aim, slot));
	}

	void SessionState::StrikeBlade(const HitEvent& hit)
	{
		EntityFx* blade = nullptr;
		float nearest = 2.5f;
		for (auto& [netId, fx] : m_Fx)
		{
			if (fx.Last.Kind != EntityKind::Projectile || AbilityOf(fx.Last) != AbilityId::ObsidianBlades || fx.BladeStruck >= 0.0f || (hit.Attacker != NoPlayer && fx.Last.Owner != hit.Attacker))
				continue;
			const float distance = LengthSquared(fx.Last.Position - hit.Position);
			if (distance < nearest)
			{
				nearest = distance;
				blade = &fx;
			}
		}
		Vec2 heading(1.0f, 0.0f);
		if (blade)
		{
			blade->BladeStruck = blade->Age;
			heading = FromAngle(blade->Last.Facing);
		}
		else if (const EntityState* attacker = FindBody(hit.Attacker); attacker && LengthSquared(hit.Position - attacker->Position) > 1e-4f)
			heading = Normalize(hit.Position - attacker->Position);
		const auto victim = m_Fx.find(hit.Target);
		const float height = victim != m_Fx.end() ? BarHeight(victim->second.Last) * 0.5f : 0.8f;
		const float facing = AngleOf(heading);
		const glm::vec3 forward = Heading(facing);
		const glm::vec3 point = Ground(hit.Position, height) - forward * 0.22f;
		const bool kill = victim != m_Fx.end() && (victim->second.Last.Has(EntityFlags::Dead) || victim->second.Last.Health == 0);
		const uint32_t order = uint32_t(std::count_if(m_BladeSlashes.begin(), m_BladeSlashes.end(), [&](const BladeSlash& slash) { return slash.Age <= 0.0f && glm::distance(slash.Center, point) < 1.0f; }));
		const float swing = order % 2u ? -1.0f : 1.0f;
		const float lift = swing * (0.45f + 0.12f * float(order % 3u));
		const glm::vec3 across = Heading(facing + HalfPi + swing * 0.55f);
		m_BladeSlashes.push_back({ .Center = point, .Axis = glm::normalize(across * std::cos(lift) + glm::vec3(0.0f, std::sin(lift), 0.0f)), .Age = -0.035f * float(order), .Size = kill ? 1.3f : 1.0f, .Kill = kill ? 1.0f : 0.0f });
		m_Effects->Burst(Fx::ObsidianImpact, point, forward, order == 0u ? 1.0f : 0.6f);
		if (victim != m_Fx.end())
			victim->second.FlashColor = ObsidianSheen * 0.75f;
		if (kill && order == 0u)
		{
			m_Effects->Burst(Fx::ObsidianKill, point, glm::vec3(0.0f, 1.0f, 0.0f));
			const glm::vec3 cross = Heading(facing + HalfPi - swing * 0.55f);
			m_BladeSlashes.push_back({ .Center = point, .Axis = glm::normalize(cross * std::cos(lift) - glm::vec3(0.0f, std::sin(lift), 0.0f)), .Age = -0.06f, .Size = 1.4f, .Kill = 1.0f });
		}
	}

	void SessionState::FadeBlade(const EntityFx& fx, const glm::vec3& point)
	{
		const AbilityDef& def = GetAbility(AbilityId::ObsidianBlades);
		const bool stuck = fx.Age < def.ProjectileRange / def.ProjectileSpeed - 0.05f;
		const glm::vec3 forward = Heading(fx.Last.Facing);
		const float phase = float(fx.BladeSlot) * 1.7f + float(fx.Last.NetId % 13u);
		BladeFade fade;
		fade.Position = point + (stuck ? forward * 0.18f : glm::vec3(0.0f));
		fade.Velocity = stuck ? glm::vec3(0.0f) : forward * (0.45f * def.ProjectileSpeed);
		fade.Facing = fx.Last.Facing;
		fade.Roll = (float(ObsidianVolley / 2) - float(fx.BladeSlot)) * 0.06f + 0.32f * std::sin(fx.Age * 13.0f + phase);
		fade.Ground = GroundHeight(fx.Last.Position);
		fade.Slot = fx.BladeSlot;
		fade.Seed = fx.Last.NetId;
		fade.Stuck = stuck;
		m_BladeFades.push_back(fade);
		if (stuck)
			m_Effects->Burst(Fx::ObsidianImpact, point + forward * 0.3f, -forward, 0.5f);
	}

	void SessionState::DrawBlade(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& center, const glm::vec3& base) const
	{
		const auto fx = m_Fx.find(entity.NetId);
		const bool known = fx != m_Fx.end();
		const float age = known ? fx->second.Age : 1.0f;
		const float merge = std::pow(1.0f - std::clamp(age / ObsidianMergeSeconds, 0.0f, 1.0f), 2.0f);
		const uint32_t slot = known ? fx->second.BladeSlot : entity.NetId % ObsidianVolley;
		const float phase = float(slot) * 1.7f + float(entity.NetId % 13u);
		const float spin = std::sin(age * 13.0f + phase);
		ObsidianLook look;
		look.Center = center + (known ? fx->second.BladeOffset : glm::vec3(0.0f)) * merge;
		look.Forward = Heading(entity.Facing + (known ? fx->second.BladeTurn : 0.0f) * merge);
		look.Roll = (float(ObsidianVolley / 2) - float(slot)) * 0.06f + 0.32f * spin;
		look.Glint = 0.1f + 0.35f * std::exp(-age * 16.0f) + 0.3f * std::pow(std::max(spin, 0.0f), 12.0f);
		look.Ground = base.y;
		look.Variant = slot;
		DrawObsidianBlade(renderer, m_SpellMeshes, look, m_Time, entity.NetId);
		renderer.DrawTrail(uint64_t(entity.NetId) | (20ull << 40), look.Center - look.Forward * (0.15f * ObsidianLength), ProjectileTrail(AbilityId::ObsidianBlades), 0.55f + 0.45f * std::exp(-age * 9.0f));
	}

	Vec2 SessionState::SpikeAimOf(const EntityState& caster) const
	{
		const Vec2 aim = FromAngle(caster.CastAim);
		if (caster.NetId != m_Client.GetPlayerNetId() || m_Paused)
			return aim;
		return SteerSpike(caster.CastPoint, aim, caster.Position + FromAngle(m_Aim) * m_AimDistance);
	}

	void SessionState::SpikeMoments(EntityFx& fx, const EntityState& entity, float dt)
	{
		if (!entity.AimsFromPoint() || entity.Has(EntityFlags::Dead))
		{
			fx.SpikeAge = -1.0f;
			return;
		}
		if (fx.SpikeAge >= 0.0f && fx.Last.AimsFromPoint())
		{
			fx.SpikeAge += dt;
			return;
		}
		fx.SpikeAge = 0.0f;
		m_Effects->Burst(Fx::SpikeTremor, Ground(entity.CastPoint, 0.05f), glm::vec3(0.0f, 1.0f, 0.0f), 1.5f);
	}

	void SessionState::EruptSpike(const EntityState& spike)
	{
		const glm::vec3 base = Ground(spike.Position);
		const glm::vec3 thrust = glm::normalize(Heading(spike.Facing) * std::sin(SpikeTilt) + glm::vec3(0.0f, std::cos(SpikeTilt), 0.0f));
		m_Effects->Burst(Fx::SpikeEruption, base + glm::vec3(0.0f, 0.1f, 0.0f), thrust);
		m_Effects->Shockwave(base, 1.9f, 0.38f, 0.15f, 0.24f);
		EarthSpikeLook placed;
		placed.Facing = spike.Facing;
		placed.Scale = spike.GetScale();
		placed.Seed = spike.NetId;
		PlaceSatellites(placed, spike.Position);
		for (size_t i = 0; i < std::size(SpikeSatellites); i++)
			m_Effects->Burst(Fx::SpikeEruption, placed.SatelliteBase[i] + glm::vec3(0.0f, 0.05f, 0.0f), thrust, SpikeSatellites[i].Size);
		SpikeChunks(base, spike.Facing, spike.NetId, false);
		const Vec2 heading = FromAngle(spike.Facing);
		if (const float near = 1.0f - Length(Vec2(m_CameraTarget.x, m_CameraTarget.z) - spike.Position) / SpikeKickReach; near > 0.0f)
			m_KickSpeed += glm::vec2(heading.x, heading.y) * (SpikeKick * near * (spike.Owner != NoPlayer && spike.Owner == m_Client.GetPlayerId() ? OwnRumble : 1.0f));
		GroundDecal soil;
		soil.Center = base;
		soil.Yaw = spike.Facing;
		soil.Extent = glm::vec2(1.15f);
		soil.Depth = 0.75f;
		soil.Shape = DecalShape::Scorch;
		soil.Color = glm::vec4(0.0f);
		soil.Tint = glm::vec4(0.11f, 0.085f, 0.065f, 0.55f);
		soil.Roughness = 0.95f;
		soil.Seed = spike.NetId;
		m_Effects->Mark(soil, SpikeRiftSeconds, 0.02f, 0.55f, 1.0f, 0.12f);
		GroundDecal cracks = soil;
		cracks.Extent = glm::vec2(1.6f);
		cracks.Shape = DecalShape::Cracks;
		cracks.Color = glm::vec4(SkillColor(AbilityId::EarthSpike) * 1.1f, 1.0f);
		cracks.Tint = glm::vec4(0.04f, 0.035f, 0.03f, 0.85f);
		cracks.Params = glm::vec4(0.12f, 0.0f, 0.0f, 0.0f);
		m_Effects->Mark(cracks, SpikeRiftSeconds, 0.02f, 0.45f, 1.0f, 0.1f, 3.0f);
		m_Effects->Mark(SpikeRift(spike, 0.25f), SpikeRiftSeconds, 0.02f, 0.3f, 1.0f, 0.08f, 2.5f);
	}

	void SessionState::EndSpike(const EntityState& spike, float age)
	{
		if (age < 0.0f)
			age = AgeOf(spike.NetId);
		const glm::vec3 base = Ground(spike.Position);
		const glm::vec3 axis = glm::normalize(Heading(spike.Facing) * std::sin(SpikeTilt) + glm::vec3(0.0f, std::cos(SpikeTilt), 0.0f));
		m_Effects->Burst(Fx::SpikeCrumble, base + glm::vec3(0.0f, 0.2f, 0.0f));
		m_Effects->Burst(Fx::SpikeCrumble, base + axis * (SpikeLength * 0.55f), glm::vec3(0.0f, 1.0f, 0.0f), 0.7f);
		const bool shatter = m_SpellMeshes.SpikeModel;
		m_SpikeRuins.push_back({ spike.Position, spike.Facing, 0.0f, spike.NetId, SpikeThrust(age), shatter });
		const bool burst = (spike.Charge & SpikeShrapnelFlag) != 0;
		if (shatter)
			ShatterSpike(SpikeLook(spike, age), burst);
		if (burst)
		{
			const glm::vec3 center = Ground(spike.Position + FromAngle(spike.Facing) * (GetAbility(AbilityId::EarthSpike).Reach * spike.GetScale() * SpikeShrapnelCenter));
			m_Effects->Burst(Fx::SpikeEruption, center + glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.2f);
			m_Effects->Shockwave(center, SpikeShrapnelRadius * spike.GetScale(), 0.32f, 0.12f, 0.2f);
			SpikeChunks(center, spike.Facing + 1.7f, spike.NetId * 5u + 2u, false);
			Rumble(center, 0.3f, spike.Owner);
		}
		else
			SpikeChunks(base, spike.Facing, spike.NetId * 3u + 1u, true);
		GroundDecal cracks;
		cracks.Center = base;
		cracks.Yaw = spike.Facing;
		cracks.Extent = glm::vec2(0.95f);
		cracks.Depth = 0.75f;
		cracks.Shape = DecalShape::Cracks;
		cracks.Color = glm::vec4(SkillColor(AbilityId::EarthSpike), 1.0f);
		cracks.Tint = glm::vec4(0.04f, 0.035f, 0.03f, 0.8f);
		cracks.Roughness = 0.95f;
		cracks.Params = glm::vec4(0.08f, 0.0f, 0.0f, 0.0f);
		cracks.Seed = spike.NetId * 7u + 3u;
		m_Effects->Mark(cracks, SpikeRiftSeconds * 0.8f, 0.02f, 0.5f, 1.0f, 0.1f, 3.0f);
	}

	void SessionState::ShatterSpike(const EarthSpikeLook& look, bool burst)
	{
		const auto unit = [seed = look.Seed](uint32_t index)
		{
			uint32_t x = seed * 0x9E3779B1u + index * 0x85EBCA77u + 0x5BD1E995u;
			x ^= x >> 15;
			x *= 0x2C1B3C6Du;
			x ^= x >> 12;
			x *= 0x297A2D39u;
			x ^= x >> 15;
			return float(x & 0xFFFFu) / 65535.0f;
		};
		const glm::mat4 frame = EarthSpikeFrame(look);
		glm::mat4 basis = frame;
		basis[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const size_t variant = look.Seed % SpikeVariants;
		for (size_t piece = 0; piece < SpikePieces; piece++)
		{
			const uint32_t k = uint32_t(piece) * 8u;
			SpikeShard shard;
			shard.Mesh = m_SpellMeshes.SpikeParts[variant][piece];
			shard.Basis = basis;
			shard.Pivot = m_SpellMeshes.SpikePivots[variant][piece];
			shard.Center = glm::vec3(frame * glm::vec4(shard.Pivot, 1.0f));
			shard.Radius = m_SpellMeshes.SpikeRadii[variant][piece] * look.Scale;
			shard.Seed = look.Seed * 7u + uint32_t(piece);
			const float ground = GroundHeight(Vec2(shard.Center.x, shard.Center.z));
			shard.Buried = shard.Center.y < ground + 0.15f;
			if (shard.Buried)
			{
				shard.Velocity = glm::vec3(0.0f, -1.4f, 0.0f);
				shard.Life = 0.6f;
			}
			else
			{
				glm::vec3 out(shard.Center.x - look.Base.x, 0.0f, shard.Center.z - look.Base.z);
				out = glm::length(out) > 0.05f ? glm::normalize(out) : Heading(look.Facing);
				const glm::vec3 side(-out.z, 0.0f, out.x);
				out = glm::normalize(out + side * (unit(k) - 0.5f));
				shard.Velocity = burst ? out * (3.0f + 2.5f * unit(k + 1u)) + WorldUp * (2.0f + 1.6f * unit(k + 2u)) : out * (0.5f + 1.3f * unit(k + 1u)) + WorldUp * (0.4f + 1.4f * unit(k + 2u));
				shard.Axis = glm::normalize(glm::cross(WorldUp, out) + glm::vec3(unit(k + 3u) - 0.5f, unit(k + 4u) - 0.5f, unit(k + 5u) - 0.5f) * 0.8f);
				shard.Spin = (burst ? 2.0f : 1.0f) * (2.0f + 3.5f * unit(k + 6u));
				shard.Life = 1.5f + 0.4f * unit(k + 7u);
			}
			m_SpikeShards.push_back(shard);
		}
	}

	void SessionState::UpdateSpikeShards(float dt)
	{
		constexpr float Gravity = 16.0f;
		for (SpikeShard& shard : m_SpikeShards)
		{
			shard.Age += dt;
			if (!shard.Buried)
				shard.Velocity.y -= Gravity * dt;
			shard.Center += shard.Velocity * dt;
			shard.Angle += shard.Spin * dt;
			if (shard.Buried)
				continue;
			const float floor = GroundHeight(Vec2(shard.Center.x, shard.Center.z)) + shard.Radius;
			if (shard.Center.y < floor)
			{
				shard.Center.y = floor;
				shard.Velocity.y = shard.Velocity.y < -2.0f ? -0.25f * shard.Velocity.y : 0.0f;
				shard.Velocity.x *= 0.5f;
				shard.Velocity.z *= 0.5f;
				shard.Spin *= 0.45f;
			}
		}
		std::erase_if(m_SpikeShards, [](const SpikeShard& shard) { return shard.Age >= shard.Life; });
	}

	void SessionState::DrawSpikeShards(WorldRenderer& renderer) const
	{
		for (const SpikeShard& shard : m_SpikeShards)
		{
			const float fade = glm::smoothstep(shard.Life - 0.55f, shard.Life, shard.Age);
			const glm::vec3 sink = WorldUp * (fade * shard.Radius * 1.2f);
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), shard.Center - sink)
				* glm::rotate(glm::mat4(1.0f), shard.Angle, shard.Axis)
				* shard.Basis
				* glm::translate(glm::mat4(1.0f), -shard.Pivot);
			DrawSpikeShard(renderer, shard.Mesh, transform, fade, GroundHeight(Vec2(shard.Center.x, shard.Center.z)), shard.Seed);
		}
	}

	EarthSpikeLook SessionState::SpikeLook(const EntityState& spike, float age) const
	{
		if (age < 0.0f)
			age = AgeOf(spike.NetId);
		const float phase = float(spike.NetId % 97u);
		EarthSpikeLook look;
		look.Base = Ground(spike.Position);
		look.Facing = spike.Facing;
		look.Extend = SpikeThrust(age);
		look.Collar = SpikeHeave + (1.0f - SpikeHeave) * SpikeThrust(age * 1.4f);
		look.Satellite = SpikeThrust(age - SpikeSatelliteLag);
		look.Tilt = SpikeTilt + 0.05f * std::exp(-age * 9.0f) * std::sin(age * 50.0f);
		look.Sway = 0.05f * std::exp(-age * 8.0f) * std::sin(age * 61.0f + phase);
		look.Scale = spike.GetScale();
		look.Seed = spike.NetId;
		PlaceSatellites(look, spike.Position);
		return look;
	}

	void SessionState::PlaceSatellites(EarthSpikeLook& look, Vec2 position) const
	{
		const Vec2 heading = FromAngle(look.Facing);
		const Vec2 side(-heading.y, heading.x);
		const float mirror = look.Seed % 2u == 0u ? 1.0f : -1.0f;
		for (size_t i = 0; i < std::size(SpikeSatellites); i++)
			look.SatelliteBase[i] = Ground(position + (heading * SpikeSatellites[i].Along + side * (SpikeSatellites[i].Side * mirror)) * look.Scale);
	}

	void SessionState::SpikeChunks(const glm::vec3& base, float facing, uint32_t seed, bool crumble)
	{
		const auto unit = [seed](uint32_t index)
		{
			uint32_t x = seed * 0x9E3779B1u + index * 0x85EBCA77u + 0x165667B1u;
			x ^= x >> 15;
			x *= 0x2C1B3C6Du;
			x ^= x >> 12;
			x *= 0x297A2D39u;
			x ^= x >> 15;
			return float(x & 0xFFFFu) / 65535.0f;
		};
		const glm::vec3 heading = Heading(facing);
		const glm::vec3 axis = glm::normalize(heading * std::sin(SpikeTilt) + WorldUp * std::cos(SpikeTilt));
		const int count = crumble ? 4 : 7;
		for (int i = 0; i < count; i++)
		{
			const uint32_t k = uint32_t(i) * 16u;
			const float angle = 6.2832f * (float(i) + 0.7f * unit(k)) / float(count);
			const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
			CometChunk chunk;
			if (crumble)
			{
				chunk.Position = base + axis * (SpikeLength * (0.25f + 0.5f * unit(k + 1u))) + out * 0.2f;
				chunk.Velocity = out * (0.6f + 1.2f * unit(k + 2u)) + WorldUp * (0.5f + 1.5f * unit(k + 3u));
				chunk.Size = 0.3f + 0.14f * unit(k + 9u);
				chunk.Life = 1.6f + 0.6f * unit(k + 10u);
			}
			else
			{
				const bool big = i < 2;
				chunk.Position = base + out * (0.35f + 0.25f * unit(k + 1u)) + WorldUp * 0.25f;
				chunk.Velocity = out * (big ? 1.2f + 1.0f * unit(k + 2u) : 1.8f + 2.0f * unit(k + 2u)) + axis * (big ? 2.5f + 1.5f * unit(k + 3u) : 4.0f + 3.0f * unit(k + 3u)) + WorldUp * (1.5f + 1.5f * unit(k + 5u));
				chunk.Size = big ? 0.42f + 0.12f * unit(k + 9u) : 0.3f + 0.08f * unit(k + 9u);
				chunk.Life = 2.2f + 0.6f * unit(k + 10u);
			}
			chunk.Axis = glm::normalize(glm::vec3(unit(k + 4u) * 2.0f - 1.0f, 0.6f, unit(k + 5u) * 2.0f - 1.0f));
			chunk.Angle = 6.2832f * unit(k + 6u);
			chunk.Spin = (5.0f + 7.0f * unit(k + 7u)) * (unit(k + 8u) < 0.5f ? -1.0f : 1.0f);
			chunk.Key = uint64_t(m_CometChunkKeys++) | (27ull << 40);
			chunk.Mesh = uint8_t((seed + uint32_t(i)) % uint32_t(m_ChunkMeshes.size()));
			chunk.Cold = true;
			chunk.Model = m_SpellMeshes.SpikeFrags[(seed + uint32_t(i)) % m_SpellMeshes.SpikeFrags.size()];
			m_CometChunks.push_back(chunk);
		}
	}

	void SessionState::SkewerHit(const HitEvent& hit)
	{
		const auto victim = m_Fx.find(hit.Target);
		if (victim == m_Fx.end())
			return;
		victim->second.Stop = std::max(victim->second.Stop, SkewerStop);
		if (const EntityState* attacker = FindBody(hit.Attacker); attacker && attacker->NetId != hit.Target)
			if (const auto it = m_Fx.find(attacker->NetId); it != m_Fx.end())
				it->second.Stop = std::max(it->second.Stop, SkewerStop);
		const EntityState& struck = victim->second.Last;
		if (struck.Kind == EntityKind::Player && !struck.Has(EntityFlags::Falling) && !struck.Has(EntityFlags::Lifted) && !struck.Has(EntityFlags::Dead))
			if (const auto body = m_BodyHeights.find(hit.Target); body != m_BodyHeights.end())
			{
				body->second.Speed = std::min(body->second.Speed, -SkewerPop);
				body->second.Aloft = true;
			}
		m_Effects->Burst(Fx::SpikeCrumble, Ground(hit.Position, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), 0.55f);
	}

	GroundDecal SessionState::SpikeRift(const EntityState& spike, float glow) const
	{
		const float length = GetAbility(AbilityId::EarthSpike).Reach * spike.GetScale();
		GroundDecal rift;
		rift.Center = Ground(spike.Position + FromAngle(spike.Facing) * (length * 0.5f));
		rift.Yaw = spike.Facing;
		rift.Extent = glm::vec2(length * 0.5f + 0.15f, 0.55f);
		rift.Depth = 1.2f;
		rift.Shape = DecalShape::Fissure;
		rift.Color = glm::vec4(SkillColor(AbilityId::EarthSpike) * 0.55f, 1.0f);
		rift.Tint = glm::vec4(0.07f, 0.055f, 0.045f, 0.85f);
		rift.Roughness = 0.95f;
		rift.Params = glm::vec4(glow, 1.0f, 1.0f, 0.0f);
		rift.Seed = spike.NetId;
		return rift;
	}


	void SessionState::DrawSpikeAim(WorldRenderer& renderer, const EntityState& caster) const
	{
		const AbilityDef& def = GetAbility(AbilityId::EarthSpike);
		const auto fx = m_Fx.find(caster.NetId);
		const float progress = std::clamp((fx != m_Fx.end() ? std::max(fx->second.SpikeAge, 0.0f) : 0.0f) / def.CastTime, 0.0f, 1.0f);
		const float facing = AngleOf(SpikeAimOf(caster));
		DrawSpikeTelegraph(renderer, Ground(caster.CastPoint), facing, progress, LocalArea(caster, AbilityId::EarthSpike), caster.NetId);
	}

	void SessionState::DrawBarrageReticle(WorldRenderer& renderer, const EntityState& self) const
	{
		const PlayerState& state = m_Client.GetPredictedState();
		if (state.RecastSlot < 0 || state.RecastTimer <= 0.0f || AbilityId(state.RecastAbility) != AbilityId::EarthSpike || m_Paused)
			return;
		const SkillTraits& skill = m_Client.GetTraits().Skills[size_t(state.RecastSlot)];
		const Level* level = m_Client.GetLevel();
		if (!level)
			return;
		const Vec2 point = BarragePoint(*level, m_Client.GetPredictedArena(), state.RecastOrigin, self.Position, FromAngle(m_Aim), m_AimDistance, GetAbility(AbilityId::EarthSpike).Range * skill.RangeScale);
		const glm::vec3 color = SkillColor(AbilityId::EarthSpike);
		const float pulse = 0.5f + 0.5f * std::sin(m_Time * 9.0f);
		DrawGroundRing(renderer, Ground(point), 0.42f + 0.05f * pulse, color, 0.55f + 0.3f * pulse, 0.025f, 10.0f, 0.5f);
		DrawGroundRing(renderer, Ground(state.RecastOrigin), BarrageSpacing, color, 0.18f, 0.015f, 24.0f, 0.4f);
	}

	void SessionState::DrawSpikeTelegraph(WorldRenderer& renderer, const glm::vec3& origin, float facing, float progress, float area, uint32_t seed) const
	{
		const AbilityDef& def = GetAbility(AbilityId::EarthSpike);
		const glm::vec3 color = SkillColor(AbilityId::EarthSpike);
		const float pulse = 0.5f + 0.5f * std::sin(m_Time * 22.0f);
		const Vec2 point(origin.x, origin.z);
		EarthSpikeLook heave;
		heave.Base = origin + glm::vec3(0.025f * std::sin(m_Time * 71.0f), 0.0f, 0.025f * std::sin(m_Time * 83.0f + 1.3f)) * progress;
		heave.Facing = facing;
		heave.Extend = 0.0f;
		heave.Satellite = 0.0f;
		heave.Collar = SpikeHeave * progress * progress;
		heave.Scale = area;
		heave.Seed = seed;
		DrawEarthSpike(renderer, m_SpellMeshes, heave);
		GroundDecal soil;
		soil.Center = origin;
		soil.Extent = glm::vec2(0.7f + 0.35f * progress);
		soil.Depth = 0.75f;
		soil.Shape = DecalShape::Scorch;
		soil.Color = glm::vec4(0.0f);
		soil.Tint = glm::vec4(0.11f, 0.085f, 0.065f, 0.3f + 0.25f * progress);
		soil.Roughness = 0.95f;
		soil.Seed = seed;
		renderer.DrawDecal(soil);
		GroundDecal cracks = soil;
		cracks.Extent = glm::vec2(0.55f + 0.75f * progress);
		cracks.Shape = DecalShape::Cracks;
		cracks.Color = glm::vec4(color * (0.7f + 0.3f * progress), 1.0f);
		cracks.Tint = glm::vec4(0.04f, 0.035f, 0.03f, 0.8f);
		cracks.Params = glm::vec4(0.06f + 0.12f * progress + 0.03f * pulse, 0.0f, 0.0f, 0.0f);
		renderer.DrawDecal(cracks);
		DrawGroundRing(renderer, origin, def.Width * 0.5f + 0.15f, color, 0.3f + 0.35f * progress, 0.02f, 10.0f, 0.6f);
		GroundDecal lane;
		lane.Center = Ground(point + FromAngle(facing) * (def.Reach * area * 0.5f));
		lane.Yaw = facing;
		lane.Extent = glm::vec2(def.Reach * area * 0.5f, def.Width * area * 0.5f);
		lane.Depth = 1.4f;
		lane.Shape = DecalShape::Fissure;
		lane.Color = glm::vec4(color * (0.35f + 0.3f * progress), 1.0f);
		lane.Tint = glm::vec4(0.06f, 0.05f, 0.04f, 0.75f);
		lane.Roughness = 0.95f;
		lane.Params = glm::vec4(1.0f, 0.3f + 0.7f * progress, 0.0f, 0.0f);
		lane.Seed = seed;
		renderer.DrawDecal(lane);
	}

	void SessionState::DrawSpikeRuins(WorldRenderer& renderer) const
	{
		for (const SpikeRuin& ruin : m_SpikeRuins)
		{
			const float t = std::clamp(ruin.Age / SpikeRuinSeconds, 0.0f, 1.0f);
			const float sink = std::clamp((t - 0.3f) / 0.7f, 0.0f, 1.0f);
			const float phase = float(ruin.Seed % 97u);
			EarthSpikeLook look;
			look.Base = Ground(ruin.Position);
			look.Facing = ruin.Facing;
			look.Extend = ruin.Shattered ? 0.0f : ruin.Extend * (1.0f - 0.4f * sink * sink);
			look.Dissolve = std::pow(t, 1.3f);
			look.Collar = 1.0f - sink;
			look.Tilt = SpikeTilt + 0.12f * sink;
			look.Sway = 0.06f * (1.0f - t) * std::sin(m_Time * 53.0f + phase);
			look.Scale = 1.0f - 0.1f * sink;
			look.Seed = ruin.Seed;
			look.Satellite = ruin.Extend * std::max(1.0f - 1.5f * sink, 0.0f);
			PlaceSatellites(look, ruin.Position);
			DrawEarthSpike(renderer, m_SpellMeshes, look);
		}
	}

	void SessionState::NovaAppeared(const EntityState& entity)
	{
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		if (AbilityOf(entity) == AbilityId::GlacialNova)
		{
			m_NovaFields.push_back({ .Position = entity.Position, .Owner = entity.Owner, .Tag = entity.Tag, .Seed = StormSeed(entity.Position) ^ (entity.Owner * 0x9E3779B1u), .Scale = entity.GetScale() });
			m_Effects->Burst(Fx::NovaSpread, Ground(entity.Position, 0.05f), up, entity.GetScale());
			Rumble(Ground(entity.Position), 0.12f, entity.Owner);
			return;
		}
		NovaField* field = nullptr;
		float nearest = 1.0f;
		for (NovaField& candidate : m_NovaFields)
		{
			if (candidate.Owner != entity.Owner || candidate.Erupt >= 0.0f)
				continue;
			const float distance = candidate.Tag == entity.Tag ? -1.0f : Length(candidate.Position - entity.Position);
			if (distance < nearest)
			{
				nearest = distance;
				field = &candidate;
			}
		}
		if (!field)
		{
			m_NovaFields.push_back({ .Position = entity.Position, .Owner = entity.Owner, .Tag = entity.Tag, .Seed = StormSeed(entity.Position) ^ (entity.Owner * 0x9E3779B1u), .Scale = entity.GetScale(), .Age = NovaSpreadSeconds + 0.5f });
			field = &m_NovaFields.back();
		}
		field->Erupt = 0.0f;
		field->Strength = NovaEruptionPush(field->Age);
		field->Shattered = 0;
		const glm::vec3 ground = Ground(field->Position);
		const float reach = GetAbility(AbilityId::GlacialNova).Reach * field->Scale;
		m_Effects->Burst(Fx::NovaBurst, ground + up * 0.1f, up, field->Scale * (0.6f + 0.4f * field->Strength));
		m_Effects->Shockwave(ground, reach * (0.95f + 0.25f * field->Strength), 0.4f, 0.13f * field->Strength, 0.3f);
		std::array<NovaSpike, MaxNovaSpikes> spikes;
		const uint32_t count = NovaSpikes(*field, spikes);
		for (uint32_t i = 0; i < count; i++)
			if (spikes[i].Main)
				m_Effects->Burst(Fx::NovaErupt, Ground(spikes[i].Point, 0.1f), glm::normalize(Heading(spikes[i].Angle) * 0.8f + up * 0.7f), std::min(0.6f + 0.4f * field->Strength, 1.1f));
		Rumble(ground, 0.35f * field->Strength, entity.Owner);
	}

	uint32_t SessionState::NovaSpikes(const NovaField& field, std::span<NovaSpike> spikes) const
	{
		constexpr uint32_t Clusters[] = { 0u, 1u, 4u };
		const float reach = GetAbility(AbilityId::GlacialNova).Reach * field.Scale;
		const auto unit = [&](uint32_t index)
		{
			uint32_t x = field.Seed * 0x9E3779B1u + index * 0x85EBCA77u + 0x165667B1u;
			x ^= x >> 15;
			x *= 0x2C1B3C6Du;
			x ^= x >> 12;
			x *= 0x297A2D39u;
			x ^= x >> 15;
			return float(x & 0xFFFFu) / 65535.0f;
		};
		uint32_t count = 0;
		const auto add = [&](uint32_t key, float angle, float radius, float small, float big, float lean, float stand, uint32_t variant, bool fang, bool main)
		{
			if (count >= spikes.size())
				return;
			NovaSpike& spike = spikes[count++];
			spike.Angle = angle;
			spike.Point = field.Position + FromAngle(angle) * radius;
			spike.Small = small * field.Scale;
			spike.Big = big * field.Scale;
			spike.Lean = lean;
			spike.Stand = stand;
			spike.Appear = NovaSpreadSeconds * (1.0f - std::pow(std::max(1.0f - radius / reach, 0.0f), 1.0f / 2.2f)) + 0.02f * unit(key + 1u);
			spike.Burst = 0.01f + 0.05f * radius / reach + 0.02f * unit(key + 2u);
			spike.Break = 0.46f + 0.1f * radius / reach + 0.08f * unit(key + 3u);
			spike.Variant = variant;
			spike.Fang = fang;
			spike.Main = main;
		};
		const uint32_t arms = NovaArmCount(field.Seed);
		for (uint32_t arm = 0; arm < arms; arm++)
		{
			const uint32_t key = arm * 32u;
			const float angle = NovaArmAngle(field.Seed, arm);
			const float between = angle + Pi / float(arms);
			add(key, angle + 0.06f * (unit(key) - 0.5f), reach * (0.47f + 0.15f * unit(key + 4u)), 0.7f + 0.2f * unit(key + 5u), 1.5f + 0.45f * unit(key + 6u), 0.88f + 0.14f * unit(key + 7u), 0.6f + 0.16f * unit(key + 8u), uint32_t(unit(key + 9u) * 2.99f), true, true);
			add(key + 10u, between + 0.15f * (unit(key + 10u) - 0.5f), reach * (0.3f + 0.08f * unit(key + 11u)), 0.3f + 0.08f * unit(key + 12u), 0.95f + 0.3f * unit(key + 13u), 0.75f, 0.5f + 0.15f * unit(key + 14u), Clusters[uint32_t(unit(key + 15u) * 2.99f)], false, false);
			if (arm % 2 == 0)
				add(key + 20u, between + 0.2f * (unit(key + 20u) - 0.5f), reach * (0.72f + 0.08f * unit(key + 21u)), 0.26f + 0.08f * unit(key + 22u), 0.8f + 0.25f * unit(key + 23u), 1.0f, 0.8f + 0.1f * unit(key + 24u), uint32_t(unit(key + 25u) * 2.99f), true, false);
		}
		return count;
	}

	void SessionState::UpdateNovaFields(float dt)
	{
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		std::array<NovaSpike, MaxNovaSpikes> spikes;
		for (NovaField& field : m_NovaFields)
		{
			field.Age += dt;
			if (field.Erupt >= 0.0f)
				field.Erupt += dt;
			const uint32_t count = NovaSpikes(field, spikes);
			for (uint32_t i = 0; i < count; i++)
			{
				const NovaSpike& spike = spikes[i];
				const uint64_t bit = 1ull << i;
				if (field.Erupt < 0.0f && spike.Main && (field.Popped & bit) == 0 && field.Age >= spike.Appear)
				{
					field.Popped |= bit;
					m_Effects->Burst(Fx::IceCrumble, Ground(spike.Point, 0.1f), up, 0.7f);
				}
				if (field.Erupt >= spike.Break && (field.Shattered & bit) == 0)
				{
					field.Shattered |= bit;
					const glm::vec3 out = Heading(spike.Angle);
					const glm::vec3 axis = glm::normalize(up * std::cos(spike.Stand) + out * std::sin(spike.Stand));
					const glm::vec3 middle = Ground(spike.Point + FromAngle(spike.Angle) * (0.12f * spike.Big)) + axis * (0.45f * spike.Big);
					if (spike.Fang)
						m_Effects->Burst(Fx::NovaShatter, middle, glm::normalize(out * 0.6f + up * 0.8f), std::max(spike.Big / 1.6f, 0.5f));
					else
						m_Effects->Burst(Fx::IceCrumble, middle, up, spike.Big);
				}
			}
			if (field.Erupt < 0.0f && !field.Ripe && IsNovaBrittle(field.Age))
			{
				field.Ripe = true;
				const glm::vec3 ground = Ground(field.Position);
				m_Effects->Shockwave(ground, GetAbility(AbilityId::GlacialNova).Reach * field.Scale * 0.9f, 0.25f, 0.035f, 0.18f);
				for (uint32_t i = 0; i < count; i++)
					if (spikes[i].Main)
						m_Effects->Burst(Fx::IceCrumble, Ground(spikes[i].Point, 0.25f), up, 0.6f, glm::vec3(0.8f, 0.95f, 1.3f));
				m_App.GetGameAudio().PlayNovaRipe(field.Position, field.Owner == m_Client.GetPlayerId());
			}
			if (field.Erupt < 0.0f && !field.Thawed && field.Age >= FrostNovaWindow)
			{
				field.Thawed = true;
				for (uint32_t i = 0; i < count; i++)
					if (spikes[i].Main)
						m_Effects->Burst(Fx::IceCrumble, Ground(spikes[i].Point, 0.15f), up, 0.9f);
			}
			if (field.Erupt < 0.0f && field.Age < FrostNovaWindow)
			{
				field.Mist -= dt;
				if (field.Mist <= 0.0f)
				{
					field.Mist += NovaMistInterval;
					const uint32_t draw = StormSeed(Vec2(field.Age * 37.0f, float(field.Seed & 1023u)));
					const float angle = float(draw & 0xFFFFu) / 65535.0f * 2.0f * Pi;
					const float radius = GetAbility(AbilityId::GlacialNova).Reach * field.Scale * (0.3f + 0.6f * float((draw >> 16) & 0xFFFFu) / 65535.0f);
					m_Effects->Burst(Fx::NovaMist, Ground(field.Position + FromAngle(angle) * radius, 0.05f), up);
				}
			}
		}
		std::erase_if(m_NovaFields, [](const NovaField& field) { return field.Erupt >= 0.0f ? field.Erupt >= NovaEruptLife : field.Age >= FrostNovaWindow + NovaFadeSeconds + 0.1f; });
	}

	void SessionState::DrawNovaFields(WorldRenderer& renderer) const
	{
		const auto smooth = [](float edge0, float edge1, float x)
		{
			const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-4f), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		};
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		std::array<NovaSpike, MaxNovaSpikes> spikes;
		for (const NovaField& field : m_NovaFields)
		{
			const float t = field.Age;
			const float e = field.Erupt;
			const bool erupted = e >= 0.0f;
			const float reach = GetAbility(AbilityId::GlacialNova).Reach * field.Scale;
			const glm::vec3 ground = Ground(field.Position);
			const float front = 1.0f - std::pow(1.0f - std::clamp(t / NovaSpreadSeconds, 0.0f, 1.0f), 2.2f);
			const bool brittle = IsNovaBrittle(t);
			const float ready = erupted ? 0.0f : NovaRipeness(t) * (1.0f - smooth(FrostNovaWindow - 0.3f, FrostNovaWindow, t));
			const float snap = !erupted && brittle ? std::exp(-(t - FrostNovaRipe) * 7.0f) : 0.0f;
			const float pulse = 0.5f + 0.5f * std::sin(m_Time * (brittle ? 11.0f : 5.0f) + float(field.Seed % 61u)) + 2.0f * snap;
			const float fade = erupted ? 1.0f - smooth(NovaEruptLife - NovaFrostFade, NovaEruptLife, e) : 1.0f - smooth(FrostNovaWindow, FrostNovaWindow + NovaFadeSeconds, t);
			const float flare = erupted ? std::exp(-e * 4.0f) : std::exp(-t * 3.5f);
			GroundDecal frost;
			frost.Center = ground;
			frost.Extent = glm::vec2(reach * NovaDecalBox);
			frost.Depth = 0.9f;
			frost.Shape = DecalShape::Nova;
			frost.Seed = field.Seed;
			frost.Color = glm::vec4(NovaIce, fade);
			frost.Tint = glm::vec4(0.6f, 0.76f, 1.0f, 0.85f * fade);
			frost.Roughness = 0.3f;
			frost.Params = glm::vec4(0.22f + 0.55f * flare + 0.1f * ready * pulse, front, erupted ? smooth(0.0f, 0.12f, e) : 0.0f, reach);
			renderer.DrawDecal(frost);
			GroundDecal light;
			light.Center = ground;
			light.Extent = glm::vec2(reach * 0.9f);
			light.Shape = DecalShape::Glow;
			light.Color = glm::vec4(NovaIce * ((0.06f + 0.04f * ready * pulse) * fade + (erupted ? 0.25f : 0.15f) * flare), 1.0f);
			renderer.DrawDecal(light);
			renderer.DrawLight({ .Position = ground + up * 0.7f, .Radius = reach * 1.4f, .Color = NovaIce * (SpellLightGain * ((0.04f + 0.02f * ready * pulse) * fade + (erupted ? 0.3f : 0.15f) * flare)) });

			const uint32_t count = NovaSpikes(field, spikes);
			for (uint32_t i = 0; i < count; i++)
			{
				const NovaSpike& spike = spikes[i];
				const float since = std::max(t - spike.Appear, 0.0f);
				const float rise = t >= spike.Appear ? std::max(SpikeThrust(since), 0.0f) : 0.0f;
				IceClusterLook look;
				look.Yaw = spike.Angle;
				look.Variant = spike.Variant;
				look.Fang = spike.Fang;
				look.Lean = spike.Lean;
				look.Scale = spike.Small * rise;
				look.Sink = spike.Small * 0.35f * (1.0f - std::min(rise, 1.0f));
				look.Frost = 0.35f;
				look.Glow = 0.9f * std::exp(-since * 6.0f);
				Vec2 point = spike.Point;
				if (!erupted)
				{
					const float thaw = smooth(FrostNovaWindow, FrostNovaWindow + NovaThawSeconds, t);
					look.Glow += ((brittle ? 0.6f : 0.3f) + 0.2f * std::sin(m_Time * (brittle ? 11.0f : 5.0f) + spike.Angle * 3.0f) + 1.2f * snap) * ready * (spike.Main ? 1.0f : 0.6f);
					look.Melt = thaw;
					look.Sink += spike.Small * 0.3f * thaw;
					look.Frost += (brittle ? 0.2f : 0.0f) + 0.3f * thaw;
				}
				else
				{
					const float lift = std::max(SpikeThrust(std::max(e - spike.Burst, 0.0f) * 0.9f), 0.0f);
					const float stood = std::min(lift, 1.0f);
					const float shatter = smooth(spike.Break, spike.Break + NovaShatterSeconds, e);
					look.Scale = (look.Scale + (spike.Big * std::min(0.6f + 0.4f * field.Strength, 1.08f) - look.Scale) * lift) * (1.0f - 0.1f * shatter);
					look.Lean = spike.Lean + (spike.Stand - spike.Lean) * stood;
					look.Sink *= 1.0f - stood;
					look.Glow = 0.25f + 0.6f * std::exp(-std::max(e - spike.Burst, 0.0f) * 5.0f) + 0.8f * smooth(spike.Break - 0.1f, spike.Break, e);
					look.Frost = 0.15f + 0.35f * smooth(spike.Break - 0.15f, spike.Break, e);
					look.Melt = shatter;
					point += FromAngle(spike.Angle) * (0.12f * spike.Big * stood);
				}
				if (look.Melt >= 1.0f)
					continue;
				look.Base = Ground(point);
				DrawIceCluster(renderer, m_SpellMeshes, look);
			}
		}
	}

	SessionState::VoidWell* SessionState::FindVoidWell(uint32_t owner, uint16_t tag, Vec2 position)
	{
		const auto it = std::find_if(m_VoidWells.begin(), m_VoidWells.end(), [&](const VoidWell& well) { return well.Owner == owner && well.Tag == tag && (tag != 0 || LengthSquared(well.Position - position) < 0.25f * 0.25f); });
		return it != m_VoidWells.end() ? &*it : nullptr;
	}

	void SessionState::UpdateVoidWells(float dt)
	{
		const AbilityDef& def = GetAbility(AbilityId::GravityWell);
		for (VoidWell& well : m_VoidWells)
		{
			well.Seen = false;
			well.Age += dt;
			if (well.Pull >= 0.0f)
				well.Pull += dt;
		}
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (entity.Kind != EntityKind::Effect || AbilityOf(entity) != AbilityId::GravityWell)
				continue;
			const bool pending = entity.Has(EntityFlags::Pending);
			VoidWell* well = FindVoidWell(entity.Owner, entity.Tag, entity.Position);
			if (!well)
			{
				if (!pending && AgeOf(entity.NetId) > 0.3f)
					continue;
				m_VoidWells.push_back({ .Position = entity.Position, .Owner = entity.Owner, .Tag = entity.Tag, .Seed = StormSeed(entity.Position) ^ (entity.Owner * 0x9E3779B1u) ^ (uint32_t(entity.Tag) * 0x85EBCA77u), .Scale = entity.GetScale(), .Age = pending ? AgeOf(entity.NetId) : def.ImpactDelay });
				well = &m_VoidWells.back();
			}
			well->Seen = true;
			well->Orphan = 0.0f;
			if (pending)
			{
				well->Position = entity.Position;
				well->Scale = entity.GetScale();
				continue;
			}
			if (well->Pull >= 0.0f)
				continue;
			well->Pull = 0.0f;
			well->PullAge = well->Age;
			well->Position = entity.Position;
			const PlayerInfo* info = m_Client.FindPlayer(entity.Owner);
			well->Tempo = info && HasCard(info->Cards, CardId::Implosion) ? ImplosionDelay / VoidDetonate : 1.0f;
			const float reach = def.ImpactRadius * well->Scale + 0.4f;
			for (const EntityState& body : m_Client.GetEntities())
				if ((body.Kind == EntityKind::Player || body.Kind == EntityKind::Dummy) && body.Owner != entity.Owner && !body.Has(EntityFlags::Dead) && !body.Warded() && LengthSquared(body.Position - entity.Position) <= reach * reach)
					well->Bodies.push_back(body.NetId);
			Rumble(Ground(entity.Position), 0.22f, entity.Owner);
		}
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		for (VoidWell& well : m_VoidWells)
		{
			if (!well.Seen && well.Pull < 0.0f)
				well.Orphan += dt;
			if (well.Detonated || well.Pull < VoidDetonate * well.Tempo)
				continue;
			well.Detonated = true;
			const glm::vec3 ground = Ground(well.Position);
			const float radius = def.ImpactRadius * well.Scale;
			m_Effects->Burst(Fx::VoidBurst, ground + up * (VoidCoreHeight * std::sqrt(well.Scale)), up, std::sqrt(well.Scale));
			GroundDecal scar;
			scar.Center = ground;
			scar.Extent = glm::vec2(radius * VoidFieldBox * 0.85f);
			scar.Depth = 1.0f;
			scar.Shape = DecalShape::Void;
			scar.Color = glm::vec4(VoidViolet, 0.4f);
			scar.Tint = glm::vec4(0.025f, 0.008f, 0.05f, 0.7f);
			scar.Params = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
			scar.Seed = well.Seed;
			m_Effects->Mark(scar, 1.2f, 0.05f, 0.7f, 1.0f, 0.3f, 0.0f);
			Rumble(ground, 0.32f, well.Owner);
		}
		std::erase_if(m_VoidWells, [](const VoidWell& well) { return well.Pull >= VoidLifetime(well.Tempo) || well.Orphan > 0.4f; });
	}

	void SessionState::DrawVoidWells(WorldRenderer& renderer) const
	{
		const AbilityDef& def = GetAbility(AbilityId::GravityWell);
		for (const VoidWell& well : m_VoidWells)
		{
			VoidGazeLook look;
			look.Ground = Ground(well.Position);
			look.Eye = m_Camera.Eye;
			look.Radius = def.ImpactRadius * well.Scale;
			look.Scale = well.Scale;
			look.Age = well.Age;
			look.PullAge = well.PullAge;
			look.Pull = well.Pull;
			look.Tempo = well.Tempo;
			look.Fade = 1.0f - std::clamp((well.Orphan - 0.15f) / 0.25f, 0.0f, 1.0f);
			look.Seed = well.Seed;
			look.Time = m_Time;
			float nearest = look.Radius * 1.4f;
			for (const EntityState& entity : m_Client.GetEntities())
			{
				if (entity.Kind != EntityKind::Player || entity.Owner == well.Owner || entity.Has(EntityFlags::Dead))
					continue;
				const float distance = Length(entity.Position - well.Position);
				if (distance < nearest)
				{
					nearest = distance;
					look.Gaze = Ground(entity.Position, 1.1f);
					look.Gazing = 1.0f;
				}
			}
			DrawVoidGaze(renderer, look, SpellLightGain);
			const float vanish = std::clamp(std::max(well.Pull, 0.0f) / 0.5f, 0.0f, 1.0f);
			if (vanish < 1.0f)
			{
				const float ease = vanish * vanish * (3.0f - 2.0f * vanish);
				const float progress = well.Pull < 0.0f ? std::clamp(well.Age / def.ImpactDelay, 0.0f, 1.0f) : 1.0f;
				DrawSpellMarkFor(renderer, AbilityId::GravityWell, look.Ground, look.Radius * (1.0f - 0.45f * ease), progress, well.Age, well.Seed, (1.0f - ease) * look.Fade);
			}
			if (well.Pull < 0.0f || well.Pull > 0.55f)
				continue;
			const float strength = 1.0f - well.Pull / 0.55f;
			for (const uint32_t netId : well.Bodies)
				if (const EntityState* body = m_Client.FindEntity(netId))
					renderer.DrawTrail(uint64_t(netId) | (uint64_t(well.Seed & 0xFFFFu) << 32) | (31ull << 48), Ground(body->Position, 0.9f), VoidWispTrail(), strength);
		}
	}

	void SessionState::ShieldMoments(const EntityState& entity, float dt)
	{
		const bool raised = entity.Guarding();
		const bool broken = entity.Has(EntityFlags::Shattered);
		if (!raised && !m_Shields.contains(entity.NetId))
			return;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		ShieldView& view = m_Shields[entity.NetId];
		HexShieldLook& look = view.Look;
		view.Seen = true;
		if (raised)
		{
			look.Base = BodyBase(entity);
			look.Facing = entity.Facing;
			const float guard = entity.GetGuard();
			if (view.Up)
			{
				for (const float step : HexShieldCracks)
					if (look.Guard > step && guard <= step)
						m_Effects->Burst(Fx::ShieldCrack, HexShieldPoint(look.Base, look.Facing, glm::vec2(0.0f)), up, 1.0f);
			}
			else
			{
				look.Seed = entity.NetId * 2654435761u;
				look.Flash = std::max(look.Flash, 0.7f);
				look.Shatter = 0.0f;
				look.Popped.reset();
				if (guard >= 0.98f)
					look.ScarCount = 0;
				m_Effects->Burst(Fx::ShieldRaise, HexShieldPoint(look.Base, look.Facing, glm::vec2(0.0f)), Heading(look.Facing), 1.0f);
			}
			look.Guard = guard;
		}
		if (broken && !view.Broken && look.Raise > 0.05f)
		{
			ShatterHexShield(look, m_ShieldShards);
			glm::vec3 normal;
			const glm::vec3 middle = HexShieldPoint(look.Base, look.Facing, glm::vec2(0.0f, -0.2f), &normal);
			m_Effects->Burst(Fx::ShieldShatter, middle, normal, 1.0f);
			m_Effects->AirShockwave(middle, 1.7f, 0.38f, 0.09f, 0.22f);
			look.Shatter = 1.0f;
			look.Raise = std::min(look.Raise, 0.8f);
		}
		look.Raise = raised ? std::min(look.Raise + dt / HexShieldRaiseSeconds, 1.0f) : std::max(look.Raise - dt / (look.Shatter > 0.0f ? HexShieldShatterSeconds : HexShieldLowerSeconds), 0.0f);
		look.Flash = std::max(look.Flash - dt * 4.0f, 0.0f);
		look.Shatter = std::max(look.Shatter - dt / HexShieldShatterSeconds, 0.0f);
		for (HexShieldImpact& impact : look.Impacts)
			impact.Age += dt;
		view.Up = raised;
		view.Broken = broken;
	}

	void SessionState::ShieldStruck(const HitEvent& hit)
	{
		const bool shove = m_Client.GetSession().Mode == GameMode::Brawl && m_Client.GetSession().Rules.Mode == BrawlMode::Knockback;
		const float strength = std::clamp(0.45f + float(hit.Damage) / (shove ? 30.0f : 18.0f), 0.45f, 1.3f);
		const auto found = m_Shields.find(hit.Target);
		if (found == m_Shields.end())
		{
			m_Effects->Burst(Fx::ShieldImpact, Ground(hit.Position, 1.1f), glm::vec3(0.0f, 1.0f, 0.0f), 0.6f + 0.4f * strength);
			return;
		}
		HexShieldLook& look = found->second.Look;
		const uint32_t key = uint32_t(QuantizePosition(hit.Position.x)) * 73856093u ^ uint32_t(QuantizePosition(hit.Position.y)) * 19349663u;
		const float height = HexShieldLift + 0.1f + 0.5f * (float(key % 1024u) / 1023.0f - 0.5f);
		glm::vec2 point = HexShieldLocal(look.Base, look.Facing, glm::vec3(hit.Position.x, look.Base.y + height, hit.Position.y));
		const float rim = HexShieldRim(point);
		if (rim > 0.9f)
			point *= 0.9f / rim;
		look.Impacts[look.ImpactHead++ % HexShieldImpactSlots] = { point, 0.0f, strength };
		if (look.ScarCount < HexShieldScarSlots)
			look.Scars[look.ScarCount++] = point;
		glm::vec3 normal;
		const glm::vec3 at = HexShieldPoint(look.Base, look.Facing, point, &normal);
		m_Effects->Burst(Fx::ShieldImpact, at, normal, 0.7f + 0.4f * strength);
	}

	void SessionState::DrawShields(WorldRenderer& renderer) const
	{
		for (const auto& [netId, view] : m_Shields)
		{
			if (view.Look.Raise <= 0.01f)
				continue;
			DrawHexShield(renderer, view.Look, m_Time);
			const Vec2 front = Vec2(view.Look.Base.x, view.Look.Base.z) + FromAngle(view.Look.Facing) * 0.7f;
			DrawGroundGlow(renderer, Ground(front), 1.0f, HexShieldColor * (0.12f * view.Look.Raise + 0.1f * view.Look.Flash), 1.0f);
		}
		DrawHexShards(renderer, m_ShieldShards);
	}

	void SessionState::WardMoments(const EntityState& entity, float dt)
	{
		const bool warded = entity.Warded();
		if (!warded && !m_Wards.contains(entity.NetId))
			return;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		const glm::vec3 chest(0.0f, DiamondImpactHeight, 0.0f);
		WardView& view = m_Wards[entity.NetId];
		DiamondWardLook& look = view.Look;
		view.Seen = true;
		const glm::vec3 base = BodyBase(entity);
		if (warded)
		{
			const float ward = entity.GetWard();
			if (!view.Up)
			{
				look = DiamondWardLook{};
				look.Seed = entity.NetId * 2654435761u ^ uint32_t(m_Time * 997.0f);
				look.Spin = float(look.Seed % 628u) * 0.01f;
				look.Flash = 1.0f;
				view.Dropped = 1e3f;
				m_Effects->Burst(Fx::WardForm, base + chest, up, 1.0f);
			}
			else
				for (const float step : DiamondCracks)
					if (look.Ward > step && ward <= step)
						m_Effects->Burst(Fx::WardCrack, base + chest, up, 1.0f);
			look.Ward = ward;
			look.Fade = 1.0f;
			look.Age += dt;
		}
		else
		{
			if (view.Up)
				view.Dropped = 0.0f;
			else
				view.Dropped += dt;
			if (entity.Has(EntityFlags::WardShattered) && view.Dropped < 0.2f && look.Fade > 0.3f && !entity.Has(EntityFlags::Dead))
			{
				ShatterDiamondWard(look, m_WardShards);
				m_Effects->Burst(Fx::WardShatter, look.Base + chest, up, 1.0f);
				look.Fade = 0.0f;
			}
			if (look.Fade > 0.0f)
				look.Dissolve = std::min(look.Dissolve + dt / DiamondDissolveSeconds, 1.0f);
			if (look.Dissolve >= 1.0f)
				look.Fade = 0.0f;
		}
		look.Base = base;
		look.Spin += dt * (0.45f + 6.0f * std::pow(1.0f - std::clamp(look.Age / DiamondFormSeconds, 0.0f, 1.0f), 2.0f));
		look.Flash = std::max(look.Flash - dt * 3.5f, 0.0f);
		for (DiamondImpact& impact : look.Impacts)
			impact.Age += dt;
		view.Up = warded;
	}

	void SessionState::WardStruck(const HitEvent& hit)
	{
		const bool shove = m_Client.GetSession().Mode == GameMode::Brawl && m_Client.GetSession().Rules.Mode == BrawlMode::Knockback;
		const float strength = std::clamp(0.5f + float(hit.Damage) / (shove ? 30.0f : 16.0f), 0.5f, 1.4f);
		const auto found = m_Wards.find(hit.Target);
		const glm::vec3 center = found != m_Wards.end() ? found->second.Look.Base : Ground(hit.Position);
		const uint32_t key = uint32_t(QuantizePosition(hit.Position.x)) * 73856093u ^ uint32_t(QuantizePosition(hit.Position.y)) * 19349663u;
		const float height = DiamondImpactHeight + 0.35f * (float(key % 1024u) / 1023.0f - 0.5f);
		glm::vec3 normal;
		const glm::vec3 at = DiamondPoint(center, glm::vec3(hit.Position.x, center.y, hit.Position.y), height, &normal);
		if (found != m_Wards.end())
		{
			DiamondWardLook& look = found->second.Look;
			const glm::vec3 local = DiamondLocal(look, at);
			*std::max_element(look.Impacts.begin(), look.Impacts.end(), [](const DiamondImpact& a, const DiamondImpact& b) { return a.Age < b.Age; }) = { local, 0.0f, strength };
			look.Scars[look.ScarCount++ % DiamondScarSlots] = local;
			look.Flash = std::max(look.Flash, 0.35f * strength);
		}
		m_Effects->Burst(Fx::WardDeflect, at, normal, 0.7f + 0.4f * strength);
	}

	void SessionState::DrawWards(WorldRenderer& renderer) const
	{
		for (const auto& [netId, view] : m_Wards)
		{
			if (view.Look.Fade <= 0.01f)
				continue;
			DrawDiamondWard(renderer, view.Look, m_Time);
			DrawDiamondCaustics(renderer, view.Look, m_Lighting.SunDirection);
		}
		DrawDiamondShards(renderer, m_WardShards);
	}

	void SessionState::DrawStunStars(WorldRenderer& renderer, const glm::vec3& center) const
	{
		for (uint32_t i = 0; i < 3; i++)
		{
			const float angle = m_Time * 5.0f + float(i) * 2.094f;
			DrawStar(renderer, center + Heading(angle) * 0.3f, 0.14f, { 2.4f, 2.1f, 0.6f }, angle);
		}
	}

	void SessionState::DrawEnemy(WorldRenderer& renderer, const EntityState& entity) const
	{
		const CharacterLook* look = m_Characters->ForEnemy(entity.Kind);
		const CharacterAnimator* animator = AnimatorOf(entity.NetId);
		if (!look || !animator)
			return;
		const float size = entity.GetScale();
		const bool elite = entity.Has(EntityFlags::Elite);
		const glm::vec3 base = Ground(entity.Position);
		const bool attacking = entity.Has(EntityFlags::Attacking);
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 16.0f);
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		const AbilityDef& attack = GetAbility(def ? def->Attack : AbilityId::Bite);
		const float reach = attack.Reach * (elite ? EliteAreaScale : 1.0f);
		const bool fighter = IsFighterKind(entity.Kind);
		glm::vec4 tint = CharacterTint(entity);
		if (fighter)
			tint = glm::vec4(glm::vec3(tint) * FighterHue(entity.Kind), tint.a);
		if (entity.Has(EntityFlags::Enraged))
			tint = glm::vec4(glm::vec3(tint) * glm::vec3(1.35f, 0.6f, 0.5f), tint.a);
		const bool flier = entity.Kind == EntityKind::MirrorMoth || entity.Kind == EntityKind::LensWisp;
		const glm::vec3 body = base + glm::vec3(0.0f, flier ? FlierHover + 0.08f * std::sin(m_Time * 3.1f + float(entity.NetId)) : 0.0f, 0.0f);
		const CharacterPose pose = DrawCharacterModel(renderer, *m_Characters, *look, *animator, body, entity.Facing, look->Scale * size, tint, (elite ? 0.12f : 0.0f) + (fighter ? 0.06f : 0.0f), glm::vec4(0.0f), HitLight(entity));
		if (fighter)
			DrawFighterAura(renderer, entity, body, size);
		if (fighter && attacking && entity.Variant < uint8_t(AbilityId::Count))
		{
			const AbilityId move = AbilityId(entity.Variant);
			const FighterMove* plan = FindFighterMove(entity.Kind, move);
			const AbilityDef& ability = GetAbility(move);
			const float progress = entity.GetChargePower();
			const float heat = 0.7f + 0.6f * progress;
			const float lunge = plan ? plan->Lunge : 0.0f;
			const float area = elite ? 1.15f : 1.0f;
			const glm::vec3 hue = FighterHue(entity.Kind);
			const glm::vec3 tip = pose.JointPosition(m_Characters->GetRig(look->Skeleton).RightHand) + glm::vec3(0.0f, 0.9f * look->Scale * size, 0.0f);
			switch (move)
			{
			case AbilityId::DuelistSlash:
			case AbilityId::DuelistCleave:
				DrawSectorTelegraph(renderer, base, entity.Facing, (ability.Reach + lunge) * area, ability.HalfArc, EnemyDanger, heat, progress);
				break;
			case AbilityId::DuelistThrust:
			case AbilityId::DuelistLunge:
				DrawLaneTelegraph(renderer, base, entity.Facing, (ability.Reach + lunge) * area, ability.Width * area, EnemyDanger, heat, progress);
				break;
			case AbilityId::ArcherShot:
				DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * ability.SpawnOffset, entity.Facing, std::min(ability.ProjectileRange, 11.0f), 0.32f, hue, heat * 0.8f, progress);
				break;
			case AbilityId::ArcherVolley:
				for (uint32_t i = 0; i < ability.ProjectileCount; i++)
				{
					const float angle = entity.Facing + ability.ProjectileSpread * (float(i) / float(ability.ProjectileCount - 1) - 0.5f);
					DrawLaneTelegraph(renderer, base + Heading(angle) * ability.SpawnOffset, angle, std::min(ability.ProjectileRange, 10.0f), 0.26f, hue, heat * 0.7f, progress);
				}
				break;
			case AbilityId::Comet:
				DrawSpellMarkFor(renderer, AbilityId::Comet, Ground(entity.CastPoint), ability.ImpactRadius * area, -1.0f, 0.0f, entity.NetId, 0.35f + 0.65f * progress);
				DrawGlow(renderer, tip, 0.3f + 0.25f * progress, glm::vec3(2.6f, 1.2f, 0.4f));
				break;
			case AbilityId::ArcLightning:
				DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * 0.5f, entity.Facing, ability.ProjectileRange, 0.28f, glm::vec3(0.55f, 0.45f, 1.0f), heat * 0.8f, progress);
				DrawGlow(renderer, tip, 0.25f + 0.25f * progress, glm::vec3(1.4f, 1.2f, 3.0f));
				break;
			case AbilityId::GlacialEruption:
				DrawSectorTelegraph(renderer, base, entity.Facing, ability.Reach * area, Pi, glm::vec3(0.5f, 0.85f, 1.0f), heat, progress);
				DrawGlow(renderer, tip, 0.25f + 0.25f * progress, glm::vec3(1.2f, 2.2f, 3.0f));
				break;
			case AbilityId::BruteSlam:
				DrawSectorTelegraph(renderer, base, entity.Facing, ability.Reach * area, Pi, EnemyDanger, heat, progress);
				break;
			case AbilityId::BruteQuake:
				DrawLaneTelegraph(renderer, base, entity.Facing, ability.Reach * area, ability.Width * area, EnemyDanger, heat, progress);
				break;
			case AbilityId::GeodeToss:
				DrawSpellMarkFor(renderer, AbilityId::GeodeToss, Ground(entity.CastPoint), ability.ImpactRadius * area, -1.0f, 0.0f, entity.NetId, 0.35f + 0.65f * progress);
				DrawGlow(renderer, tip, 0.18f + 0.15f * progress, hue * 1.2f);
				break;
			case AbilityId::MothDust:
				for (uint32_t i = 0; i < ability.ProjectileCount; i++)
				{
					const float angle = entity.Facing + ability.ProjectileSpread * (float(i) / float(ability.ProjectileCount - 1) - 0.5f);
					DrawLaneTelegraph(renderer, base + Heading(angle) * ability.SpawnOffset, angle, std::min(ability.ProjectileRange, 8.0f), 0.3f, hue, heat * 0.4f, progress);
				}
				break;
			case AbilityId::MothDive:
				DrawLaneTelegraph(renderer, base, entity.Facing, (ability.Reach + lunge) * area, ability.Width * area, EnemyDanger, heat, progress);
				break;
			case AbilityId::LensBeam:
			case AbilityId::LensSweep:
			{
				const float locked = plan && plan->Windup > 0.0f ? 1.0f - plan->Lock / plan->Windup : 1.0f;
				const float charge = progress >= locked ? 1.0f : 0.55f;
				DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * 0.4f, entity.Facing, ability.Reach * area, ability.Width * area, hue, heat * charge, progress);
				DrawGlow(renderer, tip, (0.25f + 0.35f * progress) * charge, hue * (1.5f + 2.5f * charge));
				break;
			}
			case AbilityId::LensFlare:
				DrawSectorTelegraph(renderer, base, entity.Facing, ability.Reach * area, Pi, hue, heat, progress);
				DrawGlow(renderer, tip, 0.3f + 0.3f * progress, hue * 2.5f);
				break;
			case AbilityId::ObeliskPulse:
				if (entity.Has(EntityFlags::Champion))
				{
					DrawSectorTelegraph(renderer, base, entity.Facing, ability.Reach * ChampionRoarArea, Pi, EnemyDanger, heat * 1.2f, progress);
					DrawGlow(renderer, body + glm::vec3(0.0f, 1.2f * size, 0.0f), 0.4f + 0.5f * progress, glm::vec3(2.4f, 1.0f, 0.5f));
				}
				break;
			default:
				break;
			}
		}
		switch (entity.Kind)
		{
		case EntityKind::Chaser:
			if (attacking)
				DrawSectorTelegraph(renderer, base, entity.Facing, reach, attack.HalfArc, { 1.0f, 0.22f, 0.12f }, 0.8f + 0.4f * pulse, -1.0f);
			break;
		case EntityKind::Shooter:
			if (attacking)
				DrawLaneTelegraph(renderer, base + Heading(entity.Facing) * attack.SpawnOffset, entity.Facing, attack.ProjectileRange, 0.24f, { 0.55f, 1.0f, 0.3f }, 0.7f + 0.35f * pulse, -1.0f);
			break;
		case EntityKind::Brute:
			if (attacking)
				DrawRuneCircle(renderer, { .Center = base, .Radius = reach, .Color = { 1.0f, 0.25f, 0.1f }, .Intensity = 0.8f + 0.4f * pulse, .Spin = 1.5f, .Seed = entity.NetId });
			break;
		case EntityKind::Caster:
		{
			const glm::vec3 tip = pose.JointPosition(m_Characters->GetRig(look->Skeleton).RightHand) + glm::vec3(0.0f, 0.95f * look->Scale * size, 0.0f);
			DrawGlow(renderer, tip, (attacking ? 0.45f + 0.12f * pulse : 0.25f) * size, EnemyFire * (attacking ? 2.2f : 1.2f));
			DrawGlow(renderer, tip, 0.1f * size, { 2.5f, 1.8f, 1.0f });
			if (attacking)
				DrawRuneCircle(renderer, { .Center = base, .Radius = 0.8f * size, .Color = EnemyFire, .Intensity = 0.9f + 0.4f * pulse, .Spin = 2.0f, .Seed = entity.NetId });
			break;
		}
		default:
			break;
		}
		const float top = BarHeight(entity);
		if (entity.Has(EntityFlags::Stunned))
			DrawStunStars(renderer, base + glm::vec3(0.0f, top + 0.15f, 0.0f));
		if (!elite)
			return;
		const float shimmer = 0.6f + 0.3f * std::sin(m_Time * 5.0f + float(entity.NetId));
		DrawGroundRing(renderer, base, (def ? def->Radius : 0.4f) * size + 0.22f, EliteGold, 1.0f + 0.5f * shimmer, 0.035f, 10.0f, 0.08f);
		DrawStar(renderer, base + glm::vec3(0.0f, top - 0.12f, 0.0f), 0.2f * size, EliteGold * 2.2f, m_Time * 1.5f);
	}

	void SessionState::DrawFighterAura(WorldRenderer& renderer, const EntityState& entity, const glm::vec3& body, float size) const
	{
		const float pulse = 0.5f + 0.5f * std::sin(m_Time * 6.0f + float(entity.NetId));
		const EnemyDef* def = FindEnemyDef(entity.Kind);
		const float radius = (def ? def->Radius : 0.4f) * size;
		const glm::vec3 chest = body + glm::vec3(0.0f, (BarHeight(entity) - (body.y - Ground(entity.Position).y)) * 0.55f, 0.0f);
		if (entity.Has(EntityFlags::Mirror))
		{
			const glm::vec3 silver(0.55f, 0.6f, 0.8f);
			DrawFloatingRing(renderer, chest, radius + 0.38f + 0.05f * pulse, silver * (0.7f + 0.4f * pulse), 0.04f);
			DrawFloatingRing(renderer, chest - glm::vec3(0.0f, 0.25f, 0.0f), radius + 0.28f, silver * 0.45f, 0.03f);
		}
		if (entity.Has(EntityFlags::Shell))
			DrawGroundRing(renderer, Ground(entity.Position), radius + 0.18f, { 1.0f, 0.62f, 0.25f }, 0.7f + 0.3f * pulse, 0.05f, 6.0f, 0.06f);
		if (entity.Has(EntityFlags::Warded))
		{
			const glm::vec3 ward(0.55f, 0.8f, 1.6f);
			DrawFloatingRing(renderer, chest, radius + 0.5f, ward * (0.9f + 0.4f * pulse), 0.03f);
			DrawFloatingRing(renderer, chest + glm::vec3(0.0f, 0.35f, 0.0f), radius + 0.35f, ward * 0.6f, 0.025f);
		}
		if (entity.Has(EntityFlags::Empowered))
			DrawGlow(renderer, Ground(entity.Position, 0.15f), 0.55f * size, FighterHue(EntityKind::ShardObelisk) * (0.9f + 0.5f * pulse));
	}

	void SessionState::DrawObelisk(WorldRenderer& renderer, const EntityState& entity) const
	{
		const FighterKit* kit = FindFighterKit(EntityKind::ShardObelisk);
		const glm::vec3 base = Ground(entity.Position);
		const float size = entity.GetScale();
		const glm::vec3 hue = FighterHue(EntityKind::ShardObelisk);
		const bool stunned = entity.Has(EntityFlags::Stunned);
		const bool elite = entity.Has(EntityFlags::Elite);
		const float pulse = 0.5f + 0.5f * std::sin(m_Time * 2.4f + float(entity.NetId));
		const glm::vec4 flash = HitLight(entity);
		const float glow = stunned ? 0.02f : 0.08f + 0.06f * pulse;
		if (m_ObeliskMesh)
		{
			const auto& mesh = m_Characters->GetModels().GetMesh(*m_ObeliskMesh);
			const float scale = ObeliskHeight * size / std::max(mesh.Max.y - mesh.Min.y, 0.1f);
			renderer.DrawModel(*m_ObeliskMesh, ModelTransform(base - glm::vec3(0.0f, mesh.Min.y * scale, 0.0f), entity.Facing, scale), NoPalette, glm::vec4(glm::mix(glm::vec3(1.0f), hue, 0.6f) * (elite ? 1.15f : 1.0f), 0.0f), glow, base.y, glm::vec4(0.0f), 0, flash);
		}
		else
			renderer.DrawBox(base + glm::vec3(0.0f, ObeliskHeight * 0.5f * size, 0.0f), glm::vec3(0.3f, ObeliskHeight * 0.5f, 0.3f) * size, hue, entity.Facing, glow, base.y);
		DrawGlow(renderer, base + glm::vec3(0.0f, ObeliskHeight * 1.05f * size, 0.0f), (stunned ? 0.15f : 0.3f + 0.1f * pulse) * size, hue * 1.4f);
		if (kit && !stunned && !entity.Has(EntityFlags::Pending))
		{
			DrawGroundRing(renderer, base, kit->Aura, hue, 0.16f + 0.06f * pulse, 0.025f);
			for (const EntityState& other : m_Client.GetEntities())
			{
				const Vec2 gap = other.Position - entity.Position;
				if (other.NetId == entity.NetId || !other.Has(EntityFlags::Empowered) || !IsEnemyKind(other.Kind) || LengthSquared(gap) > (kit->Aura + 0.5f) * (kit->Aura + 0.5f))
					continue;
				DrawLaneTelegraph(renderer, base, AngleOf(gap), Length(gap), 0.12f, hue, 0.45f + 0.3f * pulse, -1.0f);
			}
		}
		if (entity.Has(EntityFlags::Attacking) && entity.Variant == uint8_t(AbilityId::ObeliskPulse))
		{
			const float progress = entity.GetChargePower();
			DrawSectorTelegraph(renderer, base, entity.Facing, GetAbility(AbilityId::ObeliskPulse).Reach * (elite ? 1.15f : 1.0f), Pi, hue, 0.7f + 0.6f * progress, progress);
		}
		DrawFighterAura(renderer, entity, base, size);
		if (stunned)
			DrawStunStars(renderer, base + glm::vec3(0.0f, ObeliskHeight * size + 0.3f, 0.0f));
		if (elite)
			DrawGroundRing(renderer, base, (kit ? FindEnemyDef(EntityKind::ShardObelisk)->Radius : 0.55f) * size + 0.22f, EliteGold, 1.2f, 0.035f, 10.0f, 0.08f);
	}

	bool SessionState::DrawSpellMarkFor(WorldRenderer& renderer, AbilityId ability, const glm::vec3& base, float radius, float progress, float age, uint32_t seed, float fade) const
	{
		SpellMark mark{ .Center = base, .Radius = radius, .Progress = progress, .Fade = fade, .Seed = seed };
		const bool aiming = progress < 0.0f;
		switch (ability)
		{
		case AbilityId::Comet:
			mark.Kind = SpellMarkKind::Comet;
			mark.Color = { 1.0f, 0.42f, 0.12f };
			mark.Intensity = aiming ? 0.45f : 0.55f + 0.25f * progress;
			mark.Appear = aiming ? 1.0f : std::clamp(age / 0.18f, 0.0f, 1.0f);
			break;
		case AbilityId::StormStrike:
			mark.Kind = SpellMarkKind::Storm;
			mark.Color = { 0.48f, 0.4f, 1.0f };
			mark.Intensity = aiming ? 0.4f : 0.5f + 0.2f * progress;
			mark.Appear = aiming ? 1.0f : std::clamp(age / 0.05f, 0.0f, 1.0f);
			break;
		case AbilityId::GravityWell:
			mark.Kind = SpellMarkKind::Void;
			mark.Color = { 0.55f, 0.22f, 1.0f };
			mark.Intensity = aiming ? 0.4f : 0.45f + 0.2f * progress;
			mark.Appear = aiming ? 1.0f : std::clamp(age / 0.12f, 0.0f, 1.0f);
			break;
		case AbilityId::Firebomb:
		case AbilityId::FirebombSkip:
		case AbilityId::FirebombSkip2:
			mark.Kind = SpellMarkKind::Firebomb;
			mark.Color = { 1.0f, 0.5f, 0.14f };
			mark.Intensity = (aiming ? 0.45f : 0.5f + 0.25f * progress) * (ability == AbilityId::Firebomb ? 1.0f : 0.75f);
			mark.Appear = aiming ? 1.0f : std::clamp(age / 0.15f, 0.0f, 1.0f);
			break;
		case AbilityId::Aftershock:
			mark.Kind = SpellMarkKind::Aftershock;
			mark.Color = { 1.0f, 0.32f, 0.08f };
			mark.Intensity = 0.5f + 0.25f * progress;
			mark.Appear = std::clamp(age / 0.12f, 0.0f, 1.0f);
			break;
		case AbilityId::GeodeToss:
			mark.Kind = SpellMarkKind::Comet;
			mark.Color = { 1.0f, 0.6f, 0.22f };
			mark.Intensity = aiming ? 0.4f : 0.5f + 0.3f * progress;
			mark.Appear = aiming ? 1.0f : std::clamp(age / 0.12f, 0.0f, 1.0f);
			break;
		case AbilityId::VolatileBlast:
			mark.Kind = SpellMarkKind::Firebomb;
			mark.Color = { 1.0f, 0.3f, 0.12f };
			mark.Intensity = 0.55f + 0.35f * progress;
			mark.Appear = std::clamp(age / 0.15f, 0.0f, 1.0f);
			break;
		case AbilityId::Resonance:
			mark.Kind = SpellMarkKind::Resonance;
			mark.Color = { 1.0f, 0.72f, 0.28f };
			mark.Intensity = 0.42f + 0.3f * progress;
			mark.Appear = std::clamp(age / 0.25f, 0.0f, 1.0f);
			break;
		default:
			return false;
		}
		DrawSpellMark(renderer, mark);
		return true;
	}

	void SessionState::DrawTelegraph(WorldRenderer& renderer, const EntityState& entity, float progress) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const AbilityDef& def = GetAbility(AbilityOf(entity));
		const AbilityId ability = AbilityOf(entity);
		if (DrawSpellMarkFor(renderer, ability, base, def.ImpactRadius * entity.GetScale(), progress, PendingAge(entity), entity.NetId))
			return;
		const bool storm = ability == AbilityId::StormStrike;
		const glm::vec3 color = storm ? StormTelegraph : EffectColor(entity);
		const float scale = entity.GetScale();
		const float appear = std::clamp(PendingAge(entity) / 0.3f, 0.0f, 1.0f);
		const float dim = ability == AbilityId::Firebomb || storm ? 0.7f : ability == AbilityId::FirebombSkip || ability == AbilityId::FirebombSkip2 ? 0.5f : 1.0f;
		const float surge = storm ? 1.0f + 0.5f * glm::smoothstep(0.7f, 1.0f, progress) : 1.0f;
		const float intensity = (0.75f + 0.35f * progress) * dim * surge;
		switch (def.Shape)
		{
		case StrikeShape::Line:
			DrawLaneTelegraph(renderer, base, entity.Facing, def.Reach * scale, def.Width * scale, color, intensity, progress, appear);
			break;
		case StrikeShape::Cone:
			DrawSectorTelegraph(renderer, base, entity.Facing, def.Reach * scale, def.HalfArc, color, intensity, progress, appear);
			break;
		case StrikeShape::Ring:
			DrawRuneCircle(renderer, { .Center = base, .Radius = def.ImpactRadius * scale, .Color = color, .Intensity = intensity, .Spin = 0.25f, .Progress = progress, .Appear = appear, .Inner = def.InnerRadius / std::max(def.ImpactRadius, 0.01f), .Seed = entity.NetId });
			break;
		default:
			DrawRuneCircle(renderer, { .Center = base, .Radius = def.ImpactRadius * scale, .Color = color, .Intensity = intensity, .Spin = 0.5f, .Progress = progress, .Appear = appear, .Seed = entity.NetId });
			break;
		}
	}

	void SessionState::DrawBoss(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const float flash = FlashOf(entity.NetId);
		const bool attacking = entity.Has(EntityFlags::Attacking);
		const bool chilled = entity.Has(EntityFlags::Chilled);
		const float pulse = 0.55f + 0.45f * std::sin(m_Time * 9.0f);
		const glm::vec3 forward = Heading(entity.Facing);
		const glm::vec3 side = Heading(entity.Facing + HalfPi);
		const auto shade = [&](const glm::vec3& color)
		{
			const glm::vec3 tone = chilled ? glm::mix(color, glm::vec3(0.55f, 0.85f, 1.0f), 0.35f) : color;
			return glm::mix(tone, glm::vec3(1.0f), flash * 0.7f);
		};
		if (entity.Kind == EntityKind::Archivist)
		{
			const glm::vec3 body = base;
			const glm::vec3 arcane(0.75f, 0.55f, 1.0f);
			const CharacterLook* look = m_Characters->ForEnemy(entity.Kind);
			if (const CharacterAnimator* animator = AnimatorOf(entity.NetId); look && animator)
				DrawCharacterModel(renderer, *m_Characters, *look, *animator, base, entity.Facing, look->Scale, CharacterTint(entity), 0.0f, glm::vec4(0.0f), HitLight(entity));
			const uint32_t books = 6;
			for (uint32_t i = 0; i < books; i++)
			{
				const float angle = m_Time * (attacking ? 2.6f : 1.1f) + float(i) * 2.0f * Pi / float(books);
				const glm::vec3 position = body + Heading(angle) * 1.25f + glm::vec3(0.0f, 1.5f + 0.25f * std::sin(m_Time * 2.0f + float(i)), 0.0f);
				renderer.DrawBox(position, { 0.2f, 0.06f, 0.15f }, i % 2 ? glm::vec3(0.5f, 0.2f, 0.15f) : glm::vec3(0.2f, 0.25f, 0.45f), angle + HalfPi, attacking ? 0.6f + 0.4f * pulse : 0.15f);
				renderer.DrawBox(position + glm::vec3(0.0f, 0.065f, 0.0f), { 0.17f, 0.01f, 0.13f }, glm::vec3(0.95f, 0.9f, 0.75f), angle + HalfPi, 0.4f);
			}
			DrawRuneCircle(renderer, { .Center = base, .Radius = 1.25f + (attacking ? 0.15f * pulse : 0.0f), .Color = arcane, .Intensity = attacking ? 1.2f : 0.6f, .Spin = attacking ? 1.2f : 0.3f, .Seed = entity.NetId });
			return;
		}

		const glm::vec3 crystal(0.45f, 0.9f, 1.0f);
		const glm::vec3 hide(0.22f, 0.26f, 0.34f);
		if (entity.Variant == 1)
		{
			renderer.Draw(MeshKind::Sphere, base + glm::vec3(0.0f, 0.05f, 0.0f), { 1.1f, 0.3f, 1.1f }, glm::vec3(0.32f, 0.26f, 0.2f), m_Time, 0.0f, base.y);
			for (uint32_t i = 0; i < 5; i++)
			{
				const float angle = float(i) * 1.257f + m_Time * 0.5f;
				renderer.Draw(MeshKind::Cone, base + Heading(angle) * 0.55f + glm::vec3(0.0f, 0.35f, 0.0f), { 0.12f, 0.35f, 0.12f }, crystal, angle, 0.7f, base.y);
			}
			DrawGroundRing(renderer, base, 1.3f + 0.15f * pulse, glm::vec3(0.55f, 0.45f, 0.35f), 0.6f, 0.05f);
			return;
		}
		const uint32_t segments = 6;
		for (uint32_t i = segments; i-- > 0;)
		{
			const float along = float(i) * 0.75f;
			const float sway = std::sin(m_Time * 3.0f - float(i) * 0.8f) * 0.25f * float(i) / float(segments);
			const float size = 0.62f - 0.06f * float(i);
			const glm::vec3 center = base - forward * along + side * sway + glm::vec3(0.0f, size + 0.05f, 0.0f);
			renderer.Draw(MeshKind::Sphere, center, glm::vec3(size), shade(i == 0 ? hide * 1.2f : hide), entity.Facing, flash, base.y);
			renderer.Draw(MeshKind::Cone, center + glm::vec3(0.0f, size + 0.15f, 0.0f), { 0.13f, 0.32f + 0.05f * float(i % 2), 0.13f }, crystal, entity.Facing + float(i), attacking ? 0.6f + 0.4f * pulse : 0.45f, base.y);
		}
		const glm::vec3 head = base + forward * 0.55f + glm::vec3(0.0f, 0.75f, 0.0f);
		renderer.DrawBox(head, { 0.5f, 0.3f, 0.38f }, shade(hide * 1.3f), entity.Facing, flash);
		renderer.DrawBox(head + forward * 0.45f - glm::vec3(0.0f, 0.1f, 0.0f), { 0.25f, 0.14f, 0.28f }, shade(hide * 1.1f), entity.Facing, flash);
		for (const float sign : { -1.0f, 1.0f })
		{
			renderer.DrawBox(head + forward * 0.25f + side * (0.24f * sign) + glm::vec3(0.0f, 0.12f, 0.0f), { 0.06f, 0.05f, 0.05f }, crystal, entity.Facing, 1.0f);
			renderer.Draw(MeshKind::Cone, head - forward * 0.15f + side * (0.22f * sign) + glm::vec3(0.0f, 0.42f, 0.0f), { 0.08f, 0.3f, 0.08f }, crystal, entity.Facing, 0.7f, base.y);
		}
		if (attacking)
			DrawRuneCircle(renderer, { .Center = base, .Radius = 1.6f + 0.2f * pulse, .Color = crystal, .Intensity = 1.0f, .Spin = 1.0f, .Seed = entity.NetId });
	}

	void SessionState::DrawProjectile(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const glm::vec3 center = ProjectilePoint(entity);
		const AbilityId ability = AbilityOf(entity);
		if (ability == AbilityId::FrostSpear && m_SpellMeshes.Spear != UINT32_MAX)
		{
			const float size = entity.GetScale();
			const float power = std::clamp((size - 1.0f) / (SpearFullSize - 1.0f), 0.0f, 1.0f);
			IceSpearLook look;
			look.Center = center;
			look.Facing = entity.Facing;
			look.Roll = m_Time * 5.0f + float(entity.NetId % 97u);
			look.Size = size;
			look.Power = power;
			look.Ground = base.y;
			look.Flying = true;
			DrawIceSpear(renderer, m_SpellMeshes, look, m_Time, entity.NetId);
			renderer.DrawTrail(uint64_t(entity.NetId) | (20ull << 40), center, ProjectileTrail(ability, power, size));
			if (const auto path = m_SpearPaths.find(entity.NetId); path != m_SpearPaths.end())
			{
				SpearPath live = path->second;
				live.Last = entity.Position;
				renderer.DrawDecal(FrostLane(live));
			}
			DrawGroundGlow(renderer, base, 0.6f + 0.5f * power, SpearIce * (0.22f + 0.25f * power), 1.0f);
			renderer.DrawLight({ .Position = center, .Radius = 3.0f + power, .Color = SpearIce * (SpellLightGain * (0.6f + 0.6f * power)) });
			return;
		}
		if (ability == AbilityId::PurpleFireball)
		{
			const float power = entity.GetChargePower();
			FireballLook look;
			look.Center = center;
			look.Forward = Heading(entity.Facing);
			look.Size = entity.GetScale();
			look.Power = power;
			look.Ground = base.y;
			look.Grounded = true;
			look.Flying = true;
			DrawPurpleFireball(renderer, look, m_Time, entity.NetId);
			renderer.DrawTrail(uint64_t(entity.NetId) | (20ull << 40), center, ProjectileTrail(ability, power, look.Size));
			DrawGroundGlow(renderer, base, 0.7f + 0.8f * power, FireballColor(power) * (0.25f + 0.25f * power), 1.0f);
			renderer.DrawLight({ .Position = center, .Radius = 3.5f + 1.5f * power, .Color = FireballColor(power) * (SpellLightGain * (1.0f + power)) });
			return;
		}
		if (ability == AbilityId::ArcLightning)
		{
			DrawArc(renderer, entity, center);
			return;
		}
		if (ability == AbilityId::ShardSpit || ability == AbilityId::RubyShard || ability == AbilityId::ArcherShot || ability == AbilityId::ArcherVolley)
		{
			const bool arrow = ability == AbilityId::ArcherShot || ability == AbilityId::ArcherVolley;
			const glm::vec3 tone = arrow ? FighterHue(EntityKind::ShardArcher) * 1.6f : CrawlerTone(ability == AbilityId::RubyShard ? EntityKind::RubyCrawler : EntityKind::SapphireCrawler, m_Time);
			if (arrow)
				renderer.DrawTrail(uint64_t(entity.NetId) | (20ull << 40), center, ProjectileTrail(ability));
			m_Crawlers->DrawShard(renderer, center, entity.Facing, tone, m_Time);
			DrawGroundGlow(renderer, base, 0.35f, tone, 0.35f);
			return;
		}
		if (ability == AbilityId::ObsidianBlades)
		{
			DrawBlade(renderer, entity, center, base);
			return;
		}
		if (ability == AbilityId::SwordThrow)
		{
			DrawThrownSword(renderer, entity, center);
			return;
		}
		const glm::vec3 color = entity.Owner == NoPlayer ? SkillColor(ability) : glm::mix(ColorOf(entity.Owner), SkillColor(ability), 0.6f);
		const glm::vec3 owner = entity.Owner == NoPlayer ? glm::vec3(0.0f) : glm::pow(ColorOf(entity.Owner), glm::vec3(2.2f));
		DrawProjectileVisual(renderer, ability, center, entity.Facing, color, m_Time, entity.NetId, owner);
		if (GetAbility(ability).Damages != Element::Physical)
			renderer.DrawLight({ .Position = center, .Radius = 2.8f, .Color = glm::pow(color, glm::vec3(2.2f)) * (SpellLightGain * 0.7f) });
		renderer.DrawTrail(uint64_t(entity.NetId) | (20ull << 40), center, ProjectileTrail(ability));
	}

	void SessionState::DrawEffect(WorldRenderer& renderer, const EntityState& entity) const
	{
		const glm::vec3 base = Ground(entity.Position);
		const AbilityId ability = AbilityOf(entity);
		const AbilityDef& def = GetAbility(ability);
		const glm::vec3 owner = entity.Owner == NoPlayer ? EnemyFire : ColorOf(entity.Owner);
		const glm::vec3 color = EffectColor(entity);
		const glm::vec3 bright = glm::mix(owner, glm::vec3(1.0f), 0.6f);
		const float age = AgeOf(entity.NetId);
		const float scale = entity.GetScale();
		if (ability == AbilityId::GravityWell)
			return;
		if (entity.Has(EntityFlags::Pending) && ability == AbilityId::EarthSpike)
		{
			DrawSpikeTelegraph(renderer, base, entity.Facing, std::clamp(age / BloomDelay, 0.0f, 1.0f), scale, entity.NetId);
			return;
		}
		if (entity.Has(EntityFlags::Pending))
		{
			const float pending = PendingAge(entity);
			if (pending < 0.0f)
				return;
			const float progress = def.ImpactDelay > 0.0f ? std::clamp(pending / def.ImpactDelay, 0.0f, 1.0f) : 1.0f;
			DrawTelegraph(renderer, entity, progress);
			if (ability == AbilityId::Comet)
				DrawMeteor(renderer, entity, progress);
			if (CreditOf(ability) == AbilityId::Firebomb)
				DrawLob(renderer, entity, pending);
			if (ability == AbilityId::StormStrike)
				DrawThunder(renderer, entity, pending, true);
			return;
		}

		if (ability == AbilityId::StormStrike || ability == AbilityId::StormJump)
		{
			DrawThunder(renderer, entity, age, false);
			return;
		}
		if (ability == AbilityId::EarthSpike)
		{
			DrawEarthSpike(renderer, m_SpellMeshes, SpikeLook(entity));
			return;
		}
		if (CreditOf(ability) == AbilityId::GlacialNova)
			return;
		if (ability == AbilityId::PurpleFireball)
			return;
		if (CreditOf(ability) == AbilityId::CrystalSword)
		{
			DrawSwordStrike(renderer, entity, age);
			return;
		}
		const float t = std::clamp(age / std::max(def.EffectDuration, 0.05f), 0.0f, 1.0f);
		const float swing = 1.0f - (1.0f - t) * (1.0f - t);
		const glm::vec3 forward = Heading(entity.Facing);
		const glm::vec3 chest = base + glm::vec3(0.0f, 0.45f, 0.0f);
		const bool mirrored = (entity.NetId & 1u) != 0;
		switch (def.Kind)
		{
		case AbilityKind::Nova:
			if (ability == AbilityId::Ascension || ability == AbilityId::ShardBomb || ability == AbilityId::FrostClock)
			{
				const glm::vec3 tone = ability == AbilityId::Ascension ? glm::vec3(1.0f, 0.84f, 0.45f) : ability == AbilityId::ShardBomb ? glm::vec3(1.0f, 0.5f, 0.22f) : glm::vec3(0.55f, 0.85f, 1.0f);
				const float reach = def.Reach * scale * (ability == AbilityId::Ascension ? 1.0f : 0.65f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.3f, 0.0f), reach * swing, tone * (3.0f * (1.0f - t)), 0.09f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.15f, 0.0f), reach * swing * 0.65f, glm::mix(tone, glm::vec3(1.0f), 0.5f) * (2.0f * (1.0f - t)), 0.05f);
				DrawGlow(renderer, base + glm::vec3(0.0f, 0.8f, 0.0f), 1.2f * (1.0f - t), tone * 2.0f);
				renderer.DrawLight({ .Position = base + glm::vec3(0.0f, 1.2f, 0.0f), .Radius = reach * 1.2f, .Color = tone * (6.0f * (1.0f - t)) });
				break;
			}
			if (ability == AbilityId::BruteSlam || ability == AbilityId::LensFlare || ability == AbilityId::ObeliskPulse)
			{
				const glm::vec3 tone = ability == AbilityId::BruteSlam ? glm::vec3(1.0f, 0.55f, 0.2f) : FighterHue(ability == AbilityId::LensFlare ? EntityKind::LensWisp : EntityKind::ShardObelisk);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.25f, 0.0f), def.Reach * scale * swing, tone * (2.8f * (1.0f - t)), 0.08f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.12f, 0.0f), def.Reach * scale * swing * 0.6f, glm::mix(tone, glm::vec3(1.0f), 0.4f) * (1.8f * (1.0f - t)), 0.05f);
				break;
			}
			if (ability == AbilityId::ShatterBurst)
			{
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.35f, 0.0f), def.Reach * scale * swing, color * (2.6f * (1.0f - t)), 0.07f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.2f, 0.0f), def.Reach * scale * swing * 0.7f, glm::mix(color, glm::vec3(1.0f), 0.5f) * (1.8f * (1.0f - t)), 0.04f);
			}
			break;
		case AbilityKind::Ward:
			break;
		case AbilityKind::Melee:
		{
			const float reach = def.Reach * scale;
			switch (ability)
			{
			case AbilityId::Bite:
				for (const bool jaw : { false, true })
					DrawSlash(renderer, chest, entity.Facing, reach * 0.75f, def.HalfArc, swing, { 1.5f, 0.22f, 0.12f }, jaw);
				break;
			case AbilityId::Gust:
				for (uint32_t i = 0; i < 3; i++)
					DrawSlash(renderer, chest + glm::vec3(0.0f, -0.15f + 0.15f * float(i), 0.0f) + forward * (0.3f * float(i)), entity.Facing, reach * (0.55f + 0.2f * float(i)) * (0.6f + 0.4f * swing), def.HalfArc, swing, color * (1.6f - 0.35f * float(i)), i % 2 == 0, 0.6f);
				break;
			case AbilityId::LensBeam:
			case AbilityId::LensSweep:
			{
				const glm::vec3 hue = FighterHue(EntityKind::LensWisp);
				const float fade = 1.0f - t;
				const glm::vec3 from = base + glm::vec3(0.0f, 1.0f, 0.0f) + forward * 0.4f;
				constexpr uint32_t Beads = 28;
				for (uint32_t i = 0; i <= Beads; i++)
					DrawGlow(renderer, from + forward * (reach * float(i) / float(Beads)), (0.32f + 0.08f * std::sin(m_Time * 40.0f + float(i))) * fade, hue * (3.2f * fade));
				DrawLaneTelegraph(renderer, base, entity.Facing, reach, def.Width * scale, hue, 1.6f * fade, 1.0f);
				renderer.DrawLight({ .Position = from + forward * (reach * 0.3f), .Radius = 5.0f, .Color = hue * (5.0f * fade) });
				break;
			}
			case AbilityId::BruteQuake:
				DrawLaneTelegraph(renderer, base, entity.Facing, reach, def.Width * scale, glm::vec3(1.0f, 0.55f, 0.2f), 1.8f * (1.0f - t), 1.0f);
				break;
			default:
				DrawSlash(renderer, chest, entity.Facing, reach * 0.85f, def.HalfArc, swing, glm::mix(owner, glm::vec3(1.0f, 0.9f, 0.75f), 0.45f) * 1.25f, mirrored);
				break;
			}
			break;
		}
		case AbilityKind::Projectile:
		case AbilityKind::Ground:
			break;
		case AbilityKind::Heal:
			for (uint32_t i = 0; i < 3; i++)
			{
				const float rise = std::fmod(t + float(i) * 0.33f, 1.0f);
				DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.2f + 1.3f * rise, 0.0f), 0.55f - 0.15f * rise, color * (2.2f * (1.0f - rise)), 0.035f);
			}
			break;
		case AbilityKind::Dash:
			DrawFloatingRing(renderer, base + glm::vec3(0.0f, 0.5f, 0.0f), 0.3f + 0.6f * t, color * (1.0f * (1.0f - t)), 0.04f);
			break;
		}
	}

	void SessionState::DrawUi()
	{
		if (m_Leaving)
			return;
		Ui& ui = m_App.GetUi();
		if (m_Client.GetState() != ClientState::InGame)
		{
			m_App.GetWindow().SetCursorMode(Rdn::CursorMode::Normal);
			DrawConnecting();
			return;
		}
		ui.SetLayer(UiLayer::World);
		const GameStatus& status = m_Client.GetStatus();
		DrawLavaWarning();
		DrawSkyWarnings();
		DrawWorldUi();
		ui.SetLayer(UiLayer::Hud);
		if (!(status.Mode == GameMode::Shardfall && IsRunOver(status.Run.Phase) && status.Run.PhaseClock >= 1.0f && !m_Paused))
		{
			DrawOverlay();
			DrawStatus();
			DrawHud();
			DrawRunStreak();
			DrawPerfectDodge();
		}
		DrawConnectionWarning();
		DrawBanner();
		ui.SetLayer(UiLayer::Windows);
		DrawStationWindow();
		DrawBrawlLoadout();
		DrawMatchResults();
		DrawRunOffer();
		DrawRunRecap();
		if (m_Paused)
		{
			ui.SetLayer(UiLayer::Popup);
			DrawPauseMenu();
		}
		if (m_ShowNetStats)
			DrawNetworkWindow();

		ui.SetLayer(UiLayer::Overlay);
		DrawApexNews();
		DrawAchievementToasts();
		UiDrawList& draw = ui.Draw();
		if (m_HintTime > 0.0f && !m_Hint.empty())
		{
			const float alpha = std::clamp(m_HintTime / 0.4f, 0.0f, 1.0f);
			const TextStyle style = Theme::Bold(17.0f, WithAlpha(Theme::AccentLight, alpha));
			const float width = draw.MeasureWidth(m_Hint, style) + 40.0f;
			const UiRect pill = UiRect::Centered({ ui.GetSize().x * 0.5f, ui.GetSize().y * 0.64f }, { width, 36.0f });
			draw.Rect(pill.Translate({ 0.0f, 4.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f * alpha), .Softness = 10.0f, .Chamfer = 10.0f });
			draw.Rect(pill, { .Fill = Rgba(0.06f, 0.045f, 0.11f, 0.9f * alpha), .FillBottom = Rgba(0.03f, 0.024f, 0.06f, 0.9f * alpha), .Border = WithAlpha(Theme::Edge, alpha), .BorderWidth = 1.2f, .Chamfer = 9.0f });
			draw.TextAligned(pill, m_Hint, style, { 0.5f, 0.5f });
		}
		if (m_Fade > 0.0f)
			draw.Rect(ui.GetScreen(), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.9f * m_Fade) });
		DrawEmoteWheel();
		DrawCursor();
	}

	void SessionState::UpdateEmoteWheel()
	{
		const bool able = !m_Paused && !m_App.UiWantsKeyboard() && m_Client.GetState() == ClientState::InGame && m_Client.GetPredictedState().Alive;
		const bool held = able && Rdn::Input::IsKeyDown(Rdn::KeyCode::T);
		if (held && !m_EmoteWheel)
		{
			m_EmoteWheel = true;
			m_EmoteCenter = m_App.GetUi().GetMouse();
			m_EmoteChoice = Emote::None;
		}
		else if (!held && m_EmoteWheel)
		{
			m_EmoteWheel = false;
			if (able && m_EmoteChoice != Emote::None)
				m_Client.PlayEmote(m_EmoteChoice);
		}
	}

	void SessionState::DrawEmoteWheel()
	{
		if (!m_EmoteWheel)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		constexpr float Ring = 132.0f;
		constexpr float Slot = 38.0f;
		constexpr float Dead = 30.0f;
		constexpr uint32_t Count = uint32_t(Emote::Count) - 1;
		const glm::vec2 margin(Ring + Slot + 16.0f);
		const glm::vec2 center = glm::clamp(m_EmoteCenter, margin, glm::max(ui.GetSize() - margin, margin));
		const glm::vec2 pointer = ui.GetMouse() - center;
		const float step = 2.0f * Pi / float(Count);
		m_EmoteChoice = Emote::None;
		if (glm::length(pointer) > Dead)
		{
			const float angle = std::atan2(pointer.y, pointer.x) + 0.5f * Pi;
			const int32_t index = int32_t(std::lround(angle / step)) % int32_t(Count);
			m_EmoteChoice = Emote(uint32_t(index < 0 ? index + int32_t(Count) : index) + 1);
		}
		draw.Circle(center, Ring + Slot + 10.0f, Rgba(0.02f, 0.02f, 0.05f, 0.55f), WithAlpha(Theme::Edge, 0.5f), 1.2f);
		for (uint32_t i = 0; i < Count; i++)
		{
			const Emote emote = Emote(i + 1);
			const bool chosen = emote == m_EmoteChoice;
			const float angle = float(i) * step - 0.5f * Pi;
			const glm::vec2 spot = center + Ring * glm::vec2(std::cos(angle), std::sin(angle));
			const float radius = chosen ? Slot + 4.0f : Slot;
			draw.Circle(spot, radius, chosen ? Rgba(0.2f, 0.16f, 0.09f, 0.95f) : Rgba(0.06f, 0.06f, 0.1f, 0.9f), chosen ? Theme::Gold : WithAlpha(Theme::Edge, 0.8f), chosen ? 2.0f : 1.2f);
			draw.TextAligned(UiRect::Centered(spot, { 2.0f * radius, 20.0f }), GetEmote(emote).Name, Theme::Bold(13.5f, chosen ? Theme::GoldLight : Theme::Text), { 0.5f, 0.5f });
		}
		draw.Circle(center, Dead + 14.0f, Rgba(0.05f, 0.045f, 0.09f, 0.92f), WithAlpha(Theme::Edge, 0.8f), 1.2f);
		if (m_EmoteChoice != Emote::None)
		{
			const float angle = std::atan2(pointer.y, pointer.x);
			draw.Triangle(center + (Dead + 22.0f) * glm::vec2(std::cos(angle), std::sin(angle)), { 12.0f, 10.0f }, angle, Theme::Gold);
		}
		draw.TextAligned(UiRect::Centered(center, { 2.0f * Dead + 20.0f, 20.0f }), "Emote", Theme::Caps(11.0f, Theme::Muted), { 0.5f, 0.5f });
	}

	void SessionState::DrawCursor()
	{
		Ui& ui = m_App.GetUi();
		const bool show = !m_Paused && !m_Overview && !m_Station && !m_App.UiWantsMouse() && m_Extent.Width > 0;
		m_App.GetWindow().SetCursorMode(show ? Rdn::CursorMode::Hidden : Rdn::CursorMode::Normal);
		if (!show)
			return;

		UiDrawList& draw = ui.Draw(UiLayer::Overlay);
		const glm::vec2 mouse = ui.ToLogical({ Rdn::Input::GetMouseX(), Rdn::Input::GetMouseY() });
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const bool alive = self && !self->Has(EntityFlags::Dead);

		float direction = -0.5f * glm::pi<float>();
		float pointer = 0.0f;
		if (alive)
			if (const std::optional<glm::vec2> origin = ProjectToScreen(Ground(self->Position) + glm::vec3(0.0f, ProjectileHeight, 0.0f)))
			{
				const glm::vec2 offset = mouse - ui.ToLogical(*origin);
				direction = std::atan2(offset.y, offset.x);
				pointer = glm::smoothstep(16.0f, 42.0f, glm::length(offset));
			}

		bool hostile = false;
		if (alive)
			for (const EntityState& entity : m_Client.GetEntities())
			{
				if (entity.Has(EntityFlags::Dead) || !(m_Client.IsHostile(entity) || IsEnemyKind(entity.Kind) || entity.Kind == EntityKind::Dummy))
					continue;
				const std::optional<glm::vec3> point = MouseOnPlane(GroundHeight(entity.Position) + 0.9f);
				if (point && LengthSquared(Vec2(point->x, point->z) - entity.Position) < 0.7f * 0.7f)
				{
					hostile = true;
					break;
				}
			}

		float charge = 0.0f;
		glm::vec3 spell(1.0f);
		bool outOfRange = false;
		if (alive && self->Has(EntityFlags::Attacking) && self->Variant < uint8_t(AbilityId::Count))
		{
			const AbilityId casting = AbilityId(self->Variant);
			spell = casting == AbilityId::PurpleFireball ? FireballColor(self->GetChargePower()) : SpellTint(casting);
			spell /= std::max({ spell.r, spell.g, spell.b, 1e-3f });
			if (IsChargedShot(casting))
				charge = self->GetChargePower();
			const AbilityDef& ability = GetAbility(casting);
			outOfRange = ability.Kind == AbilityKind::Ground && !AimsFromPoint(casting) && m_AimDistance > ability.Range;
		}

		const float hover = ui.Animate(Ui::MakeId("cursor.hostile"), hostile, 18.0f);
		const float portal = ui.Animate(Ui::MakeId("cursor.portal"), m_HoveredPortal >= 0, 14.0f);
		const float dim = ui.Animate(Ui::MakeId("cursor.range"), outOfRange, 14.0f);
		const float charging = ui.Animate(Ui::MakeId("cursor.charge"), charge > 0.0f, 20.0f);
		pointer *= 1.0f - portal;

		const UiColor ink = WithAlpha(Theme::Ink, 0.78f);
		UiColor light = Hex(0xF4F0FF);
		light = glm::mix(light, Hex(0xFF7A7A), hover);
		light = glm::mix(light, Theme::Astral, portal);
		light = glm::mix(light, WithAlpha(Theme::Muted, 0.7f), dim);
		UiColor accent = glm::mix(Theme::AccentLight, light, std::max(hover, portal));

		const float radius = 11.0f * (1.0f - 0.2f * hover + 0.18f * portal);
		const float quarter = 0.5f * glm::pi<float>();
		const float gap = 0.34f;
		const auto arc = [&](float center, float span, float ringRadius, float width, UiColor color)
		{
			const float sweep = 1.0f - span / glm::two_pi<float>();
			draw.Rect(UiRect::Centered(mouse, glm::vec2(2.0f * ringRadius)), { .Radius = ringRadius, .Border = color, .BorderWidth = width, .Sweep = sweep, .Rotation = center + quarter + 0.5f * span });
		};
		const auto at = [&](float angle, float distance) { return mouse + glm::vec2(std::cos(angle), std::sin(angle)) * distance; };

		const float spin = portal * 0.25f * glm::pi<float>();
		for (int i = 0; i < 4; i++)
			arc(direction + spin + quarter * (float(i) + 0.5f), quarter - gap, radius + 1.3f, 4.1f, ink);
		for (int i = 0; i < 4; i++)
			arc(direction + spin + quarter * (float(i) + 0.5f), quarter - gap - 0.08f, radius, 1.5f, light);

		if (charging > 0.0f)
		{
			const UiColor fill = Lighten(WithAlpha(spell, 1.0f), 0.3f);
			const float span = glm::two_pi<float>() * std::max(charge, 0.02f);
			arc(direction, std::min(span + 0.1f, glm::two_pi<float>()), radius + 1.8f, 5.0f, WithAlpha(ink, charging));
			arc(direction, span, radius + 0.8f, 3.0f, WithAlpha(fill, charging));
		}

		const auto tick = [&](float angle, float alpha)
		{
			draw.Rect(UiRect::Centered(at(angle, radius), glm::vec2(4.0f)), { .Fill = WithAlpha(accent, alpha), .Border = WithAlpha(ink, alpha), .BorderWidth = 1.2f, .Rotation = angle + 0.25f * glm::pi<float>() });
		};
		for (int i = 1; i < 4; i++)
			tick(direction + spin + quarter * float(i), 1.0f - 0.5f * dim);
		if (portal > 0.0f)
			tick(direction + spin, portal);

		if (pointer > 0.0f)
		{
			const UiColor tip = charge >= 1.0f ? Lighten(WithAlpha(spell, 1.0f), 0.45f) : light;
			draw.Triangle(at(direction, radius + 5.5f), { 11.0f, 9.0f }, direction + quarter, WithAlpha(tip, pointer), WithAlpha(ink, pointer), 1.4f);
		}

		draw.Circle(mouse, 2.6f, ink);
		draw.Circle(mouse, 1.5f, light);
	}

	void SessionState::DrawConnecting()
	{
		Ui& ui = m_App.GetUi();
		ui.SetMotion(1.0f);
		ui.SetLayer(UiLayer::Windows);
		UiDrawList& draw = ui.Draw();
		const glm::vec2 center = ui.GetSize() * 0.5f + glm::vec2(0.0f, -40.0f);
		const float time = ui.GetTime();
		draw.Rect(UiRect::Centered(center, glm::vec2(330.0f)), { .Fill = Rgba(0.01f, 0.008f, 0.03f, 0.6f), .Radius = 165.0f, .Softness = 60.0f });
		draw.Rect(UiRect::Centered(center, glm::vec2(208.0f)), { .Radius = 104.0f, .Border = WithAlpha(Theme::Accent, 0.45f), .BorderWidth = 1.0f, .Glow = WithAlpha(Theme::Astral, 0.08f), .GlowSize = 18.0f });
		draw.Rect(UiRect::Centered(center, glm::vec2(136.0f)), { .Radius = 68.0f, .Border = WithAlpha(Theme::Edge, 0.35f), .BorderWidth = 1.0f });
		const float pulse = 0.75f + 0.25f * std::sin(time * 2.2f);
		draw.Rect(UiRect::Centered(center, glm::vec2(90.0f * pulse)), { .Fill = WithAlpha(Theme::Astral, 0.22f), .Radius = 45.0f * pulse, .Softness = 30.0f * pulse, .Additive = 1.0f });
		draw.Rect(UiRect::Centered(center, glm::vec2(10.0f)), { .Fill = Rgba(0.95f, 1.0f, 1.0f, 0.95f), .Radius = 5.0f, .Softness = 2.0f, .Additive = 1.0f });
		for (int i = 0; i < 3; i++)
		{
			const float angle = time * 1.7f + float(i) * 2.0943951f;
			const glm::vec2 spark = center + glm::vec2(std::cos(angle), std::sin(angle)) * 104.0f;
			draw.Rect(UiRect::Centered(spark, glm::vec2(22.0f)), { .Fill = WithAlpha(Theme::Astral, 0.5f), .Radius = 11.0f, .Softness = 8.0f, .Additive = 1.0f });
			draw.Rect(UiRect::Centered(spark, glm::vec2(4.0f)), { .Fill = Rgba(0.9f, 1.0f, 1.0f, 1.0f), .Radius = 2.0f, .Additive = 1.0f });
		}
		TextStyle title = Theme::Title(30.0f);
		title.Tracking = 4.0f;
		draw.TextAligned({ 0.0f, center.y + 136.0f, ui.GetSize().x, 40.0f }, m_Config.Host ? "Opening the Rift" : "Following the Rift", title, { 0.5f, 0.5f });
		const std::string detail = m_Config.Host ? "Binding UDP port " + std::to_string(m_Config.Port)
			: m_Config.RelayHost != 0 ? "Reaching " + (m_Config.HostName.empty() ? std::string("your friend") : m_Config.HostName) + " through Steam" : "Reaching " + m_Config.Address;
		draw.TextAligned({ 0.0f, center.y + 178.0f, ui.GetSize().x, 24.0f }, detail, Theme::Body(16.0f, Theme::Muted), { 0.5f, 0.5f });
		if (ui.MenuEntry(Ui::MakeId("cancel"), UiRect::Centered({ center.x, center.y + 238.0f }, { 320.0f, 44.0f }), "Turn Back", {}, false, true))
			ReturnToMenu({});
	}

	void SessionState::DrawWorldUi()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		const auto screen = [&](const glm::vec3& position) -> std::optional<glm::vec2>
		{
			const std::optional<glm::vec2> pixels = ProjectToScreen(position);
			return pixels ? std::optional<glm::vec2>(ui.ToLogical(*pixels)) : std::nullopt;
		};
		const auto onScreen = [&](const glm::vec2& point) { return point.x > -60.0f && point.y > -60.0f && point.x < size.x + 60.0f && point.y < size.y + 60.0f; };
		const auto healthBar = [&](const UiRect& bar, float fraction, float trail, UiColor color, UiColor edge, float shield, bool shielded)
		{
			constexpr float Cut = 1.0f;
			const UiRect frame = bar.Expand(1.5f);
			draw.Rect(frame.Translate({ 0.0f, 1.5f }), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.4f), .Softness = 3.0f });
			draw.Rect(frame, { .Fill = Rgba(0.02f, 0.015f, 0.04f, 0.92f), .Border = shielded ? Hex(0xFF3355, 0.95f) : edge, .BorderWidth = 1.0f, .Chamfer = Cut });
			draw.Rect(bar, { .Fill = WithAlpha(Darken(color, 0.78f), 0.95f) });
			if (trail > fraction + 0.002f)
				draw.Rect({ bar.X + bar.W * fraction, bar.Y, bar.W * (trail - fraction), bar.H }, { .Fill = Rgba(1.0f, 0.94f, 0.86f, 0.92f) });
			if (fraction > 0.0f)
			{
				const float filled = bar.W * fraction;
				draw.Rect({ bar.X, bar.Y, filled, bar.H }, { .Fill = Lighten(color, 0.35f), .FillBottom = Darken(color, 0.22f) });
				if (filled > 3.0f)
					draw.Rect({ bar.X + 1.5f, bar.Y, filled - 3.0f, 1.0f }, { .Fill = Rgba(1.0f, 1.0f, 1.0f, 0.38f) });
			}
			for (int i = 1; i < 4; i++)
				draw.Rect({ bar.X + bar.W * float(i) * 0.25f - 0.5f, bar.Y, 1.0f, bar.H }, { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.5f) });
			if (shield > 0.0f)
			{
				const UiRect line{ bar.X, frame.Y - 3.5f, bar.W * std::min(shield, 1.0f), 2.0f };
				draw.Rect(line, { .Fill = Hex(0xFF5C78), .Glow = Hex(0xFF2E4D, 0.6f), .GlowSize = 4.0f });
			}
		};
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		const bool fighting = m_Client.GetSession().Mode != GameMode::Hub;
		for (const EntityState& entity : m_Client.GetEntities())
		{
			if (IsBossKind(entity.Kind))
				continue;
			const bool enemy = IsEnemyKind(entity.Kind);
			const bool elite = entity.Has(EntityFlags::Elite);
			const bool teammate = entity.Kind == EntityKind::Player && entity.NetId != m_Client.GetPlayerNetId() && !entity.Has(EntityFlags::Dead);
			const bool mine = entity.Kind == EntityKind::Player && entity.NetId == m_Client.GetPlayerNetId() && !entity.Has(EntityFlags::Dead) && fighting;
			const bool dummy = entity.Kind == EntityKind::Dummy && entity.Health < 255;
			if (!(enemy && (entity.Health < 255 || elite)) && !teammate && !dummy && !mine)
				continue;
			const std::optional<glm::vec2> point = screen(entity.Kind == EntityKind::Player ? BodyBase(entity, BarHeight(entity)) : Ground(entity.Position, BarHeight(entity)));
			if (!point || !onScreen(*point))
				continue;
			const bool rival = m_Client.IsHostile(entity);
			const bool marked = rival && m_Client.GetSession().Mode != GameMode::Brawl;
			const bool body = entity.Kind == EntityKind::Player;
			const float width = (entity.Kind == EntityKind::Brute ? 70.0f : body ? (mine ? 70.0f : 64.0f) : rival ? 58.0f : 46.0f) * (elite ? 1.25f : 1.0f);
			const float height = body ? 7.0f : elite || rival ? 6.0f : 5.0f;
			const UiRect bar = UiRect::Centered(*point - glm::vec2(0.0f, height * 0.5f), { width, height });
			const float fraction = float(entity.Health) / 255.0f;
			const auto fx = m_Fx.find(entity.NetId);
			const float trail = fx != m_Fx.end() ? std::max(fx->second.Trail, fraction) : fraction;
			const UiColor color = mine ? Hex(0xF0485E) : marked ? Hex(0xE04A3C) : dummy ? Hex(0xFF8A3A) : !enemy ? WithAlpha(ColorOf(entity.Owner)) : elite ? WithAlpha(EliteGold) : Hex(0xE04A3C);
			const float low = body && fraction < 0.3f ? 0.6f + 0.4f * std::sin(m_Time * 8.0f) : 0.0f;
			UiColor edge = elite ? WithAlpha(EliteGold, 0.9f) : mine ? WithAlpha(Theme::GoldLight, 0.75f) : WithAlpha(Lighten(color, 0.2f), body ? 0.55f : 0.3f);
			if (low > 0.0f)
				edge = glm::mix(edge, Hex(0xFF3B3B), low);
			const float cap = float(std::max(local.MaxHealth, 1)) / (1.0f + BloodMagicHealth) * BloodMagicOverheal;
			const float shield = mine && local.BloodShield > 0 ? float(local.BloodShield) / std::max(cap, 1.0f) : 0.0f;
			healthBar(bar, fraction, trail, color, edge, shield, entity.Has(EntityFlags::BloodShield));
			if (const PlayerTraits& traits = m_Client.GetTraits(); mine && traits.MaxMana > 0.0f)
			{
				const float mana = std::clamp(m_Client.GetPredictedState().Mana / traits.MaxMana, 0.0f, 1.0f);
				const UiRect well{ bar.X, bar.Bottom() + 2.5f, bar.W, 3.0f };
				draw.Rect(well.Expand(1.0f), { .Fill = Rgba(0.02f, 0.015f, 0.04f, 0.92f) });
				draw.Rect(well, { .Fill = Hex(0x10163A, 0.95f) });
				if (mana > 0.0f)
					draw.Rect({ well.X, well.Y, well.W * mana, well.H }, { .Fill = Hex(0x8DB4FF), .FillBottom = Hex(0x3D5BD6) });
			}
			if (elite)
				draw.Icon(UiIcon::Crown, { bar.X - 19.0f, bar.Center().y - 8.0f, 16.0f, 16.0f }, { .Color = WithAlpha(EliteGold), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
			if (enemy && elite && entity.Tag != 0)
			{
				std::string affixes;
				for (uint32_t affix = 0; affix < uint32_t(EliteAffix::Count); affix++)
					if (HasAffix(uint8_t(entity.Tag), EliteAffix(affix)))
						affixes += std::string(affixes.empty() ? "" : " \xC2\xB7 ") + EliteAffixNames[affix];
				draw.TextAligned({ point->x - 140.0f, bar.Y - 20.0f, 280.0f, 16.0f }, affixes, Theme::Floating(13.0f, Lighten(WithAlpha(EliteGold), 0.2f)), { 0.5f, 1.0f });
			}
			if (!teammate)
				continue;
			if (const PlayerInfo* player = m_Client.FindPlayer(entity.Owner))
			{
				draw.TextAligned({ point->x - 120.0f, bar.Y - 22.0f, 240.0f, 18.0f }, player->Name, Theme::Floating(14.0f, marked ? Hex(0xFF9A8A) : Lighten(WithAlpha(ColorOf(entity.Owner)), 0.3f)), { 0.5f, 1.0f });
				if (const uint32_t count = CountCards(player->Cards); count > 0 && m_Client.GetSession().Mode == GameMode::Brawl)
				{
					constexpr float Icon = 13.0f;
					constexpr float Mark = 19.0f;
					const CardId apex = HeldApex(player->Cards);
					const uint32_t plain = count - (apex != CardId::Count ? 1u : 0u);
					float x = point->x - (float(plain) * (Icon + 2.0f) - 2.0f + (apex != CardId::Count ? Mark + 5.0f : 0.0f)) * 0.5f;
					if (apex != CardId::Count)
					{
						const UiColor glow = WithAlpha(ApexColor(m_Time));
						const UiRect mark{ x, bar.Bottom() + 2.0f, Mark, Mark };
						draw.Shadow(mark, 7.0f, 9.0f, WithAlpha(glow, 0.5f), { 0.0f, 0.0f });
						draw.Diamond(mark.Center(), Mark + 7.0f, Rgba(0.06f, 0.03f, 0.1f, 0.92f), glow, 1.3f);
						draw.Icon(IconFor(apex), mark.Shrink(3.0f), { .Color = Lighten(glow, 0.5f), .ColorBottom = glow, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.85f) });
						x += Mark + 5.0f;
					}
					for (const CardDef& card : Cards)
						if (HasCard(player->Cards, card.Id) && card.Rarity != CardRarity::Apex)
						{
							const UiColor tint = CardIconTint(card);
							draw.Icon(IconFor(card.Id), { x, bar.Bottom() + 4.0f, Icon, Icon }, { .Color = Lighten(tint, 0.45f), .ColorBottom = tint, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.85f) });
							x += Icon + 2.0f;
						}
				}
			}
		}

		const GameStatus& brawlStatus = m_Client.GetStatus();
		if (brawlStatus.Mode == GameMode::Brawl && (brawlStatus.Brawl.RoundWinner != NoPlayer || brawlStatus.Brawl.RoundTeam < MaxTeams) && (brawlStatus.Brawl.Phase == BrawlPhase::Over || brawlStatus.Brawl.Phase == BrawlPhase::Pick))
			for (const EntityState& entity : m_Client.GetEntities())
			{
				if (entity.Kind != EntityKind::Player || !WonBrawlRound(entity.Owner) || entity.Has(EntityFlags::Dead))
					continue;
				if (const std::optional<glm::vec2> point = screen(BodyBase(entity, BarHeight(entity) + 0.55f)); point && onScreen(*point))
				{
					const UiRect crown = UiRect::Centered(*point - glm::vec2(0.0f, 6.0f + 3.0f * std::sin(m_Time * 2.5f)), { 30.0f, 30.0f });
					draw.Shadow(crown, 15.0f, 16.0f, WithAlpha(Theme::Gold, 0.45f), { 0.0f, 0.0f });
					draw.Icon(UiIcon::Crown, crown, { .Color = Theme::GoldLight, .ColorBottom = Theme::Gold, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
				}
			}

		if (const Level* level = m_Client.GetLevel(); level && self && !m_Overview)
			for (size_t i = 0; i < level->Portals.size(); i++)
			{
				const Portal& portal = level->Portals[i];
				const bool hovered = int32_t(i) == m_HoveredPortal;
				const bool reachable = int32_t(i) == m_ReachablePortal;
				if ((!hovered && !reachable && LengthSquared(portal.Position - self->Position) > 100.0f) || (portal.Hidden && !reachable))
					continue;
				const std::optional<glm::vec2> point = screen(Ground(portal.Position, 2.4f));
				if (!point || !onScreen(*point))
					continue;
				const std::string title = "To the " + portal.Label;
				const char* action = reachable ? "Click or press E" : hovered ? "Get closer to use" : nullptr;
				const TextStyle titleStyle = Theme::Bold(15.0f, Theme::AccentLight);
				const TextStyle actionStyle = Theme::Body(13.0f, Theme::Text);
				const float width = std::max(draw.MeasureWidth(title, titleStyle), action ? draw.MeasureWidth(action, actionStyle) : 0.0f) + 54.0f;
				const float height = action ? 46.0f : 30.0f;
				const UiRect box{ point->x - width * 0.5f, point->y - height, width, height };
				const float strength = hovered || reachable ? 1.0f : 0.7f;
				draw.Rect(box.Translate({ 0.0f, 3.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.5f), .Softness = 8.0f, .Chamfer = 8.0f });
				draw.Rect(box, { .Fill = Rgba(0.06f, 0.05f, 0.11f, 0.92f), .FillBottom = Rgba(0.03f, 0.025f, 0.06f, 0.92f), .Border = WithAlpha(Theme::Accent, strength), .BorderWidth = hovered ? 1.8f : 1.2f, .Glow = WithAlpha(Theme::Accent, hovered ? 0.12f : 0.0f), .GlowSize = 10.0f, .Chamfer = 8.0f});
				draw.Icon(UiIcon::Door, { box.X + 9.0f, box.Y + 6.0f, 18.0f, 18.0f }, { .Color = Theme::AccentLight, .ColorBottom = Theme::Accent });
				draw.Text({ box.X + 34.0f, box.Y + 5.0f }, title, titleStyle);
				if (action)
					draw.Text({ box.X + 34.0f, box.Y + 25.0f }, action, actionStyle);
			}

		const PlayerState& state = m_Client.GetPredictedState();
		if (self && state.CastSlot >= 0)
			if (const std::optional<glm::vec2> point = screen(Ground(self->Position)))
			{
				const SkillTraits& skill = m_Client.GetTraits().Skills[size_t(state.CastSlot)];
				const float progress = skill.CastTime > 0.0f ? 1.0f - std::clamp(state.CastTimer / skill.CastTime, 0.0f, 1.0f) : 1.0f;
				const UiColor color = WithAlpha(SkillColor(skill.Ability));
				const UiRect bar = UiRect::Centered({ point->x + 11.0f, point->y + 30.0f }, { 116.0f, 9.0f });
				draw.Rect(bar.Expand(1.5f), { .Fill = Rgba(0.0f, 0.0f, 0.0f, 0.8f), .Radius = 5.0f, .Border = WithAlpha(color, 0.6f), .BorderWidth = 1.0f });
				draw.Rect({ bar.X, bar.Y, bar.W * progress, bar.H }, { .Fill = Lighten(color, 0.45f), .FillBottom = color, .Radius = 4.0f });
				draw.Shadow({ bar.X + bar.W * progress - 3.0f, bar.Y, 6.0f, bar.H }, 3.0f, 5.0f, WithAlpha(Lighten(color, 0.6f), 0.8f), { 0.0f, 0.0f });
				draw.Icon(IconFor(skill.Ability), { bar.X - 26.0f, bar.Center().y - 11.0f, 22.0f, 22.0f }, { .Color = Lighten(color, 0.5f), .ColorBottom = color, .Outline = 1.0f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.85f) });
				const std::string label = std::string(GetAbility(skill.Ability).Name) + (skill.MoveScale <= 0.01f ? "  (rooted)" : "");
				draw.TextAligned({ bar.X - 60.0f, bar.Bottom() + 3.0f, bar.W + 120.0f, 18.0f }, label, Theme::Floating(13.5f, Theme::Text), { 0.5f, 0.0f });
			}

		for (const DamageNumber& number : m_DamageNumbers)
		{
			const float t = number.Age / DamageNumberSeconds;
			const std::optional<glm::vec2> point = screen(Ground(number.Position + Vec2(number.Drift, 0.0f), 1.3f + 1.4f * t));
			if (!point || !onScreen(*point))
				continue;
			const float pop = number.Critical ? 1.0f + 0.7f * std::max(0.0f, 1.0f - number.Age / 0.16f) : 1.0f + 0.25f * std::max(0.0f, 1.0f - number.Age / 0.1f);
			const float fontSize = (number.Critical ? 27.0f : number.OnPlayer ? 21.0f : number.Guarded || number.Warded ? 16.0f : 18.0f) * pop;
			const float alpha = std::clamp((1.0f - t) / 0.35f, 0.0f, 1.0f);
			UiColor color = number.Warded ? Hex(0xFFE3A6) : number.Guarded ? Hex(0x9FE6FF) : number.OnPlayer ? Hex(0xFF5A4E) : number.Critical ? Hex(0xFFE27A) : number.Damages != Element::Physical ? Lighten(WithAlpha(ElementColor(number.Damages)), 0.2f) : Hex(0xF4F1EA);
			TextStyle style = Theme::Floating(fontSize, WithAlpha(color, alpha));
			style.OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.9f * alpha);
			style.ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.5f * alpha);
			style.Outline = number.Critical ? 2.2f : 1.7f;
			if (number.Critical)
				style.ColorBottom = WithAlpha(Hex(0xFF9A1F), alpha);
			char text[16];
			std::snprintf(text, sizeof(text), number.Critical ? "%u!" : "%u", unsigned(number.Damage));
			draw.TextAligned(UiRect::Centered(*point, { 200.0f, fontSize }), text, style, { 0.5f, 0.5f });
		}

		DrawInteractionLabels();
		DrawRunMarker();
		DrawActMap();
		DrawChampionBar();
		DrawRunItems();
		DrawMerchant();
		DrawGrimoire();
	}

	std::string SessionState::PlayerName(uint32_t playerId) const
	{
		const PlayerInfo* player = m_Client.FindPlayer(playerId);
		return player ? player->Name : "Someone";
	}

	glm::vec3 SessionState::ColorOf(uint32_t playerId) const
	{
		const SessionInfo& session = m_Client.GetSession();
		if (session.Mode == GameMode::Brawl && session.Rules.Teams >= 2)
			if (const uint8_t team = m_Client.GetTeam(playerId); team != NoTeam)
				return TeamColor(team);
		return PlayerColor(playerId);
	}

	void SessionState::SortStandings(std::vector<PlayerInfo>& players) const
	{
		const GameMode mode = m_Client.GetSession().Mode;
		if (mode == GameMode::Brawl)
			std::stable_sort(players.begin(), players.end(), [this](const PlayerInfo& a, const PlayerInfo& b) { return BrawlRanksAbove(a, b); });
	}

	std::vector<uint8_t> SessionState::TeamOrder() const
	{
		const SessionInfo& session = m_Client.GetSession();
		const uint8_t teams = session.Mode == GameMode::Brawl && session.Rules.Teams >= 2 ? session.Rules.Teams : uint8_t(0);
		std::vector<uint8_t> order;
		for (uint8_t team = 0; team < teams; team++)
			order.push_back(team);
		const GameStatus& status = m_Client.GetStatus();
		std::stable_sort(order.begin(), order.end(), [&](uint8_t a, uint8_t b) { return status.TeamScores[a] > status.TeamScores[b]; });
		return order;
	}

	void SessionState::DrawOverlay()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const SessionInfo& session = m_Client.GetSession();
		const GameStatus& status = m_Client.GetStatus();
		const GameMode mode = session.Mode;
		const bool pvp = IsPvpMode(mode);
		const uint32_t self = m_Client.GetPlayerId();
		std::vector<PlayerInfo> players;
		for (const PlayerInfo& player : m_Client.GetPlayers())
			if (!player.Spectator)
				players.push_back(player);
		SortStandings(players);

		struct Line
		{
			std::string Name;
			std::string Value;
			UiColor Color;
			uint32_t Rank = 0;
			bool Mine = false;
			bool Out = false;
		};
		std::vector<Line> lines;
		const std::vector<uint8_t> teams = TeamOrder();
		if (!teams.empty())
		{
			const uint8_t mine = m_Client.GetTeam(self);
			for (const uint8_t team : teams)
				lines.push_back({ GetTeamName(team), std::to_string(status.TeamScores[team]), WithAlpha(TeamColor(team)), 0, team == mine });
		}
		else if (pvp)
			for (size_t i = 0; i < players.size(); i++)
			{
				const PlayerInfo& player = players[i];
				const bool you = player.PlayerId == self;
				if (i >= 3 && !you)
					continue;
				const uint32_t value = mode == GameMode::Brawl ? player.Score : player.Kills;
				lines.push_back({ player.Name, std::to_string(value), WithAlpha(ColorOf(player.PlayerId)), uint32_t(i + 1), you, false });
			}

		constexpr float Width = 264.0f;
		constexpr float RowHeight = 22.0f;
		const float listHeight = lines.empty() ? (pvp ? 0.0f : 20.0f) : float(lines.size()) * RowHeight + 4.0f;
		const UiRect frame{ 14.0f, 14.0f, Width, 30.0f + listHeight + 20.0f };
		draw.Rect(frame.Translate({ 0.0f, 4.0f }), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.35f), .Softness = 10.0f, .Chamfer = 9.0f });
		draw.Rect(frame, { .Fill = Hex(0x120E24, 0.72f), .FillBottom = Hex(0x07060E, 0.74f), .Border = WithAlpha(Theme::Edge, 0.6f), .BorderWidth = 1.0f, .Chamfer = 9.0f, .Aether = 0.3f, .Seed = 0.13f });
		char line[96];
		if (m_Server)
			std::snprintf(line, sizeof(line), m_Server->IsRelayListening() ? "Hosting on port %u and Steam" : "Hosting on port %u", unsigned(m_Server->GetConfig().Port));
		else if (m_Client.IsRelayed())
			std::snprintf(line, sizeof(line), "%s's game, through Steam", m_Config.HostName.empty() ? "A friend" : m_Config.HostName.c_str());
		else
			std::snprintf(line, sizeof(line), "Connected to %s", m_Config.Address.c_str());
		const ConnectionStats connection = m_Client.GetConnectionStats();
		char stats[48];
		std::snprintf(stats, sizeof(stats), "%d ms   %.0f fps", connection.Ping, double(m_Fps));
		const TextStyle statsStyle = Theme::Body(12.5f, Theme::Faint);
		const float statsWidth = draw.MeasureWidth(stats, statsStyle);
		draw.TextFit({ frame.X + 11.0f, frame.Y + 6.0f, frame.W - statsWidth - 30.0f, 18.0f }, line, Theme::Body(12.5f, Theme::Muted));
		draw.TextAligned({ frame.X, frame.Y + 6.0f, frame.W - 11.0f, 18.0f }, stats, statsStyle, { 1.0f, 0.5f });
		float y = frame.Y + 28.0f;
		if (lines.empty() && !pvp)
		{
			const size_t count = players.size();
			draw.TextAligned({ frame.X + 11.0f, y, frame.W - 22.0f, 20.0f }, count <= 1 ? std::string("Only you here") : Format("%u mages here", unsigned(count)), Theme::Body(13.5f, Theme::Text), { 0.0f, 0.5f });
			y += 20.0f;
		}
		for (const Line& entry : lines)
		{
			const UiRect row{ frame.X + 5.0f, y, frame.W - 10.0f, RowHeight };
			if (entry.Mine)
				draw.Rect(row, { .Fill = Rgba(0.75f, 0.65f, 1.0f, 0.08f), .Border = WithAlpha(Theme::Edge, 0.5f), .BorderWidth = 1.0f, .Chamfer = 5.0f });
			float x = row.X + 6.0f;
			if (entry.Rank > 0)
			{
				draw.TextAligned({ x, row.Y, 18.0f, row.H }, std::to_string(entry.Rank), Theme::Body(12.5f, Theme::Faint), { 0.5f, 0.5f });
				x += 20.0f;
			}
			draw.Circle({ x + 5.0f, row.Center().y }, 4.5f, entry.Color, Rgba(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			x += 15.0f;
			const TextStyle valueStyle = Theme::Bold(14.0f, entry.Mine ? Theme::GoldLight : Theme::Text);
			const float valueWidth = draw.MeasureWidth(entry.Value, valueStyle);
			draw.TextFit({ x, row.Y, row.Right() - x - valueWidth - 14.0f, row.H }, entry.Name, Theme::Bold(14.0f, entry.Out ? Theme::Faint : Lighten(entry.Color, 0.45f)));
			draw.TextAligned({ row.X, row.Y, row.W - 6.0f, row.H }, entry.Value, valueStyle, { 1.0f, 0.5f });
			y += RowHeight;
		}
		if (!lines.empty())
			y += 4.0f;
		const bool board = Rdn::Input::IsKeyDown(Rdn::KeyCode::Tab) && !m_Paused && !m_App.UiWantsKeyboard();
		draw.TextAligned({ frame.X + 11.0f, y, frame.W - 22.0f, 16.0f }, board ? (mode == GameMode::Brawl ? "Hover a card to read it, release Tab to close" : "Release Tab to close") : "Hold Tab for the scoreboard", Theme::Body(12.0f, Theme::Faint), { 0.0f, 0.5f });
		if (board)
			DrawScoreboard();
		if (!m_App.GetSettings().ShowControls)
			return;
		draw.Text({ 16.0f, ui.GetSize().y - 46.0f }, mode == GameMode::Shardfall ? "WASD move   LMB attack   RMB / Q / R / F spells   Space dodge" : "WASD move   LMB attack   RMB / Q / R spells   Space dodge", Theme::Floating(13.0f, Theme::Muted, UiFont::Body));
		draw.Text({ 16.0f, ui.GetSize().y - 26.0f }, mode == GameMode::Brawl ? "Tab scoreboard   I spellbook   F4 map   F3 network   Esc menu" : (mode == GameMode::Shardfall ? "E interact   Tab scoreboard   M route   F4 map   F3 network   Esc menu" : "E interact   Tab scoreboard   F4 map   F3 network   Esc menu"), Theme::Floating(13.0f, Theme::Muted, UiFont::Body));
	}

	void SessionState::DrawScoreboard()
	{
		Ui& ui = m_App.GetUi();
		const UiLayer previous = ui.GetLayer();
		ui.SetLayer(UiLayer::Popup);
		UiDrawList& draw = ui.Draw();
		const SessionInfo& session = m_Client.GetSession();
		const GameStatus& status = m_Client.GetStatus();
		const GameMode mode = session.Mode;
		const bool brawl = mode == GameMode::Brawl;
		const bool damage = brawl && session.Rules.Mode == BrawlMode::Damage;
		const bool pvp = IsPvpMode(mode);
		const uint32_t self = m_Client.GetPlayerId();
		std::vector<PlayerInfo> players;
		std::vector<std::string> watchers;
		for (const PlayerInfo& player : m_Client.GetPlayers())
		{
			if (player.Spectator)
				watchers.push_back(player.Name);
			else
				players.push_back(player);
		}
		SortStandings(players);
		const std::vector<uint8_t> teams = TeamOrder();
		if (!teams.empty())
			std::stable_sort(players.begin(), players.end(), [&](const PlayerInfo& a, const PlayerInfo& b)
			{
				const auto place = [&](uint8_t team) { return size_t(std::find(teams.begin(), teams.end(), team) - teams.begin()); };
				return place(a.Team) < place(b.Team);
			});

		std::vector<const char*> labels;
		if (brawl)
			labels = damage ? std::vector<const char*>{ "Points", "Rounds", "Kills", "Assists", "Damage" } : std::vector<const char*>{ "Points", "Rounds", "Kills" };
		const auto cells = [&](const PlayerInfo& player)
		{
			if (brawl)
				return std::vector<std::string>{ std::to_string(player.Score), std::to_string(player.Rounds), std::to_string(player.Kills), std::to_string(player.Assists), std::to_string(player.Damage) };
			return std::vector<std::string>{ std::to_string(player.Kills), std::to_string(player.Deaths) };
		};

		constexpr float RowHeight = 30.0f;
		constexpr float HeaderHeight = 28.0f;
		constexpr float ColumnWidth = 70.0f;
		constexpr float PingWidth = 64.0f;
		const float nameWidth = brawl ? 370.0f : 280.0f;
		const float columns = float(labels.size()) * ColumnWidth + PingWidth;
		const float width = 44.0f + 36.0f + nameWidth + columns;
		const float height = 92.0f + 26.0f + float(players.size()) * RowHeight + float(teams.size()) * (HeaderHeight + 6.0f) + (watchers.empty() ? 0.0f : 30.0f) + 16.0f;
		const UiRect panel = UiRect::Centered({ ui.GetSize().x * 0.5f, std::max(ui.GetSize().y * 0.45f, height * 0.5f + 20.0f) }, { width, height });
		ui.Panel(panel, 14.0f, false);
		std::string title = brawl ? "Spellbrawl" : GetMapDef(session.Map).Name;
		std::string subtitle;
		if (brawl)
		{
			const BrawlStatus& match = status.Brawl;
			const uint8_t round = match.Phase == BrawlPhase::Fight || match.Phase == BrawlPhase::Over ? match.Round : uint8_t(std::min<uint32_t>(match.Round + 1u, match.Rounds));
			subtitle = Format("Round %u of %u", unsigned(round), unsigned(match.Rounds)) + (teams.empty() ? std::string() : std::string("   Most rounds won takes the match")) + (session.Rules.Mode == BrawlMode::Knockback ? "   Knockback" : "");
		}
		else
			subtitle = players.size() == 1 ? std::string("Only you here") : Format("%u mages here", unsigned(players.size()));
		TextStyle titleStyle = Theme::Title(26.0f);
		titleStyle.Tracking = 2.5f;
		draw.TextAligned({ panel.X, panel.Y + 14.0f, panel.W, 34.0f }, title, titleStyle, { 0.5f, 0.5f });
		draw.TextAligned({ panel.X, panel.Y + 50.0f, panel.W, 20.0f }, subtitle, Theme::Body(14.5f, Theme::Muted), { 0.5f, 0.5f });
		UiRect table{ panel.X + 22.0f, panel.Y + 84.0f, panel.W - 44.0f, panel.H - 100.0f };
		const UiRect header = table.CutTop(22.0f);
		const TextStyle headStyle = Theme::Label(13.0f);
		draw.TextAligned({ header.X, header.Y, 36.0f, header.H }, "#", headStyle, { 0.5f, 0.5f });
		draw.TextAligned({ header.X + 40.0f, header.Y, nameWidth, header.H }, "Mage", headStyle, { 0.0f, 0.5f });
		for (size_t i = 0; i < labels.size(); i++)
			draw.TextAligned({ header.Right() - columns + float(i) * ColumnWidth, header.Y, ColumnWidth, header.H }, labels[i], headStyle, { 0.5f, 0.5f });
		draw.TextAligned({ header.Right() - PingWidth, header.Y, PingWidth, header.H }, "Ping", headStyle, { 0.5f, 0.5f });
		ui.Separator(table.CutTop(4.0f));

		const uint8_t mine = m_Client.GetTeam(self);
		uint8_t lastTeam = NoTeam;
		uint32_t rank = 0;
		for (const PlayerInfo& player : players)
		{
			if (!teams.empty() && player.Team != lastTeam)
			{
				lastTeam = player.Team;
				rank = 0;
				table.CutTop(6.0f);
				const UiRect band = table.CutTop(HeaderHeight);
				const UiColor color = player.Team < MaxTeams ? WithAlpha(TeamColor(player.Team)) : Theme::Muted;
				draw.Rect(band, { .Fill = WithAlpha(Darken(color, 0.72f), 0.75f), .FillBottom = WithAlpha(Darken(color, 0.85f), 0.4f), .Border = WithAlpha(color, player.Team == mine ? 0.9f : 0.45f), .BorderWidth = player.Team == mine ? 1.5f : 1.0f, .Chamfer = 6.0f });
				draw.TextAligned(band.Shrink(12.0f, 0.0f), std::string(GetTeamName(player.Team)) + (player.Team == mine ? "  (your team)" : ""), Theme::Bold(15.0f, Lighten(color, 0.45f)), { 0.0f, 0.5f });
				if (player.Team < MaxTeams)
					draw.TextAligned(band.Shrink(12.0f, 0.0f), Format(brawl ? (status.TeamScores[player.Team] == 1 ? "%u round won" : "%u rounds won") : "%u kills", unsigned(status.TeamScores[player.Team])), Theme::Bold(15.0f, Theme::Text), { 1.0f, 0.5f });
			}
			rank++;
			const UiRect row = table.CutTop(RowHeight);
			const bool you = player.PlayerId == self;
			if (you)
				draw.Rect(row, { .Fill = Rgba(0.75f, 0.65f, 1.0f, 0.08f), .Border = WithAlpha(Theme::Edge, 0.6f), .BorderWidth = 1.0f, .Chamfer = 6.0f });
			draw.TextAligned({ row.X, row.Y, 36.0f, row.H }, std::to_string(rank), Theme::Bold(14.0f, rank == 1 && teams.empty() ? Theme::GoldLight : Theme::Muted), { 0.5f, 0.5f });
			const UiColor color = WithAlpha(ColorOf(player.PlayerId));
			draw.Circle({ row.X + 48.0f, row.Center().y }, 5.0f, color, Rgba(0.0f, 0.0f, 0.0f, 0.6f), 1.0f);
			UiRect name{ row.X + 60.0f, row.Y, nameWidth - 20.0f, row.H };
			if (brawl && player.Cards != 0)
			{
				const uint32_t count = CountCards(player.Cards);
				UiRect icons = name.CutRight(std::min(176.0f, float(count) * 19.0f + 6.0f));
				float x = icons.X;
				const auto hover = [&](const UiRect& rect, CardId card)
				{
					if (ui.Interact(Ui::MakeId("boardcard", uint64_t(player.PlayerId) * 256u + uint64_t(card)), rect).Hovered)
						ShowCardTooltip(card);
				};
				const CardId apex = HeldApex(player.Cards);
				if (apex != CardId::Count)
				{
					const UiColor glow = WithAlpha(ApexColor(m_Time));
					const UiRect mark{ x, row.Center().y - 10.0f, 20.0f, 20.0f };
					draw.Rect(mark, { .Fill = Rgba(0.08f, 0.04f, 0.12f, 0.95f), .Border = glow, .BorderWidth = 1.2f, .Glow = WithAlpha(glow, 0.35f), .GlowSize = 8.0f, .Chamfer = 4.0f });
					draw.Icon(IconFor(apex), mark.Shrink(2.5f), { .Color = Lighten(glow, 0.45f), .ColorBottom = glow, .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
					hover(mark, apex);
					x += 24.0f;
				}
				for (const CardDef& card : Cards)
				{
					if (!HasCard(player.Cards, card.Id) || card.Rarity == CardRarity::Apex)
						continue;
					if (x + 16.0f > icons.Right())
						break;
					const UiColor tint = CardIconTint(card);
					const UiRect mark{ x, row.Center().y - 8.0f, 16.0f, 16.0f };
					draw.Icon(IconFor(card.Id), mark, { .Color = Lighten(tint, 0.45f), .ColorBottom = tint, .Outline = 0.8f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
					hover(mark, card.Id);
					x += 19.0f;
				}
			}
			const TextStyle nameStyle = Theme::Bold(15.0f, Lighten(color, 0.45f));
			const char* tag = you ? "you" : player.Bot ? "bot" : player.Host ? "host" : "";
			const float tagWidth = *tag ? draw.MeasureWidth(tag, Theme::Body(12.5f, Theme::Faint)) + 10.0f : 0.0f;
			const float nameText = std::min(draw.MeasureWidth(player.Name, nameStyle), name.W - tagWidth);
			draw.TextFit({ name.X, name.Y, nameText, name.H }, player.Name, nameStyle);
			if (*tag)
				draw.TextAligned({ name.X + nameText + 6.0f, name.Y, tagWidth, name.H }, tag, Theme::Body(12.5f, Theme::Faint), { 0.0f, 0.5f });
			std::vector<std::string> values = cells(player);
			values.resize(labels.size());
			for (size_t column = 0; column < values.size(); column++)
				draw.TextAligned({ row.Right() - columns + float(column) * ColumnWidth, row.Y, ColumnWidth, row.H }, values[column], column == 0 ? Theme::Bold(15.0f, Theme::Text) : Theme::Body(14.5f, Theme::Muted), { 0.5f, 0.5f });
			draw.TextAligned({ row.Right() - PingWidth, row.Y, PingWidth, row.H }, player.Bot ? std::string("-") : player.Ping >= 0 ? std::to_string(player.Ping) : std::string("..."), Theme::Body(13.5f, Theme::Faint), { 0.5f, 0.5f });
		}
		if (!watchers.empty())
		{
			table.CutTop(8.0f);
			UiRect band = table.CutTop(22.0f);
			draw.Icon(UiIcon::Eye, band.CutLeft(20.0f).Shrink(0.0f, 2.0f), { .Color = Theme::Astral });
			band.CutLeft(8.0f);
			std::string names;
			for (size_t i = 0; i < watchers.size(); i++)
				names += (i == 0 ? "" : ", ") + watchers[i];
			draw.TextFit(band, "Spectating: " + names, Theme::Body(14.0f, Theme::Muted));
		}
		ui.SetLayer(previous);
	}

	void SessionState::DrawNetworkWindow()
	{
		const ConnectionStats connection = m_Client.GetConnectionStats();
		ImGui::SetNextWindowPos(ImVec2(16.0f, 220.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.85f);
		if (!ImGui::Begin("Network (F3)", &m_ShowNetStats, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::End();
			return;
		}
		const double loss = connection.Quality > 0.0f ? (1.0 - double(connection.Quality)) * 100.0 : 0.0;
		const double tickMs = 1000.0 / double(std::max(m_Client.GetTickRate(), 1u));
		const double delayMs = (double(m_Client.GetLatestTick()) - m_Client.GetRenderTick()) * tickMs;
		const double behindMs = std::max(m_Client.GetPresentTick() - m_Client.GetRenderTick(), 0.0) * tickMs;
		ImGui::Text("Ping %d ms, %.1f%% loss, interpolation %.0f ms, world drawn %.0f ms behind you", connection.Ping, loss, delayMs, behindMs);
		ImGui::Text("Down %.1f KB/s, up %.1f KB/s", double(connection.InBytesPerSecond) / 1024.0, double(connection.OutBytesPerSecond) / 1024.0);
		ImGui::Text("Interpolation target %.2f ticks, %u starved frames", m_Client.GetInterpolationDelay(), m_Client.GetStarvedFrames());
		ImGui::Text("%u unacknowledged inputs, %u corrections (last %.3f)", m_Client.GetPendingInputCount(), m_Client.GetCorrectionCount(), double(m_Client.GetLastCorrection()));
		ImGui::Text("%u sounds played, %u UI primitives", m_App.GetGameAudio().GetPlayedCount(), m_App.GetUiPrimitiveCount());
		const Timbre::AudioStats audio = m_App.GetAudio().GetStats();
		ImGui::Text("Audio: %u voices (%u audible, %u virtual), %u streams, %.1f%% CPU, %llu underruns, %.1f MB", audio.PlayingVoices, audio.AudibleVoices, audio.VirtualVoices, audio.Streams, double(audio.CpuLoad) * 100.0,
			static_cast<unsigned long long>(audio.StreamUnderruns), double(audio.SoundMemory) / (1024.0 * 1024.0));
		if (m_Server)
		{
			ImGui::Separator();
			ImGui::Text("Server: %u starved player ticks", m_Server->GetStarvedTicks());
			for (const ServerPlayerStats& player : m_Server->GetPlayerStats())
				ImGui::Text("  %s: ping %d ms, %.1f%% loss, input buffer %u of %u", player.Name.c_str(), player.Connection.Ping, player.Connection.Quality > 0.0f ? (1.0 - double(player.Connection.Quality)) * 100.0 : 0.0, player.QueuedInputs, player.InputBufferTarget);
		}
		ImGui::End();
	}

	void SessionState::DrawStatus()
	{
		if (IsPvpMode(m_Client.GetSession().Mode))
		{
			DrawMatchStatus();
			return;
		}
		if (m_Client.GetSession().Mode != GameMode::Shardfall)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const GameStatus& status = m_Client.GetStatus();
		const float center = ui.GetSize().x * 0.5f;
		const RunStatus& run = status.Run;
		const unsigned clock = unsigned(run.ActClock);
		const char* act = run.Act == 1 ? "I" : run.Act == 2 ? "II" : "III";
		std::string title = Format("Act %s", act);
		std::string subtitle = Format("%u:%02u of %u:00   %u %s", clock / 60u, clock % 60u, unsigned(ActSeconds / 60.0f), unsigned(run.Alive), run.Alive == 1 ? "enemy" : "enemies");
		if (run.Waves > 0 && run.Lull > 0.0f)
			subtitle = run.Wave == 0 ? Format("The first encounter comes in %.0f s", double(std::ceil(run.Lull))) : Format("Encounter %u of %u cleared. The next one comes in %.0f s", unsigned(run.Wave), unsigned(run.Waves), double(std::ceil(run.Lull)));
		else if (run.Waves > 0)
			subtitle = Format("Encounter %u of %u   %u %s", unsigned(std::min<uint32_t>(run.Wave + 1u, run.Waves)), unsigned(run.Waves), unsigned(run.Alive), run.Alive == 1 ? "enemy" : "enemies");
		const unsigned living = [&]
		{
			unsigned count = 0;
			for (const EntityState& entity : m_Client.GetEntities())
				count += entity.Kind == EntityKind::Player && !entity.Has(EntityFlags::Dead) ? 1u : 0u;
			return count;
		}();
		const bool rooms = UsesRooms(m_Client.GetSession().Run) && run.Room != NoRoom;
		const auto clockText = [](float seconds)
		{
			const unsigned whole = unsigned(std::max(seconds, 0.0f));
			return Format("%u:%02u", whole / 60u, whole % 60u);
		};
		if (rooms && run.Phase == RunPhase::Act)
		{
			const MapRoom room = MakeActMap(m_Client.GetSession().Run.Seed, run.Act).At(run.Room);
			title = RoomKindNames[size_t(room.Kind)];
			const std::string where = Format("Act %s, room %u of %u.  ", act, unsigned(RoomLayer(run.Room) + 1), unsigned(MapLayers));
			unsigned voters = 0;
			for (const uint8_t votes : run.Votes)
				voters = std::max<unsigned>(voters, votes);
			const std::string door = living > 1 ? Format("Everyone on one door to go on (%u of %u)", voters, living) : std::string("Stand on a door to go on");
			if (room.Kind == RoomKind::Shop)
				subtitle = where + "Spend shards at the merchant (E).  " + door;
			else if (room.Kind == RoomKind::Shrine)
				subtitle = where + (m_Client.GetLocalPlayer().Run.Rested == 0 ? std::string("Ask the shrine for one boon (E).  ") : std::string()) + door;
			else 			if (room.Kind == RoomKind::Rest)
				subtitle = where + (m_Client.GetLocalPlayer().Run.Rested == 0 ? std::string("Heal at the left altar or train at the right one.  ") : std::string()) + door;
			else if (!run.Cleared)
				subtitle = where + Format("%s of par %s   %u %s", clockText(run.RoomClock).c_str(), clockText(run.Par).c_str(), unsigned(run.Alive), run.Alive == 1 ? "enemy" : "enemies");
			else
				subtitle = where + "Cleared in " + clockText(run.RoomClock) + (run.Medal != RoomMedal::None ? std::string(", ") + RoomMedalNames[size_t(run.Medal)] : std::string()) + ".  " + door;
		}
		switch (run.Phase)
		{
		case RunPhase::Gate:
			subtitle = rooms ? "Step onto the guardian stone in the cobbled ring when you are ready" : "The waves are spent. Step onto the guardian stone in the cobbled ring";
			break;
		case RunPhase::Boss:
			title = Format("Act %s Guardian", act);
			subtitle = Format("%u of %u guardians left   %u %s", unsigned(run.Guardians), unsigned(GuardianCount), unsigned(run.Alive), run.Alive == 1 ? "enemy" : "enemies");
			break;
		case RunPhase::Respite:
			title = "Respite";
			subtitle = Format("Act %s in %.0f s. ", run.Act == 1 ? "II" : "III", double(std::max(RespiteSeconds - run.PhaseClock, 0.0f))) + (living > 1 ? Format("Everyone into the rift to go now (%u of %u)", unsigned(run.Rift), living) : std::string("Step into the rift to go now"));
			break;
		case RunPhase::Victory:
			title = "Victory";
			subtitle = "The hollow falls silent";
			break;
		case RunPhase::Defeat:
			title = "The Run Is Over";
			subtitle = "The party has fallen";
			break;
		default:
			break;
		}
		if (run.Paused)
			subtitle += "   \xC2\xB7   paused";
		TextStyle titleStyle = Theme::Title(30.0f);
		titleStyle.Tracking = 2.0f;
		const float titleWidth = draw.MeasureWidth(title, titleStyle);
		draw.Shadow(UiRect::Centered({ center, 44.0f }, { titleWidth + 160.0f, 56.0f }), 28.0f, 30.0f, Rgba(0.0f, 0.0f, 0.0f, 0.55f), { 0.0f, 0.0f });
		draw.TextAligned({ 0.0f, 16.0f, ui.GetSize().x, 36.0f }, title, titleStyle, { 0.5f, 0.5f });
		DrawTitleOrnament(center, 33.0f, titleWidth);
		draw.TextAligned({ 0.0f, 56.0f, ui.GetSize().x, 22.0f }, subtitle, Theme::Floating(16.0f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		const UiRect bar = UiRect::Centered({ center, 80.0f }, { 340.0f, 0.0f });
		const RunRules& rules = m_Client.GetSession().Run;
		float below = bar.Bottom() + 8.0f;
		if (rooms)
		{
			const UiColor surge = Hex(0xFFC46B);
			std::string line = "Run " + clockText(run.RunClock);
			if (run.Golds > 0)
				line += Format(", %u gold %s", unsigned(run.Golds), run.Golds == 1 ? "medal" : "medals");
			draw.TextAligned({ bar.X, below, bar.W * 0.5f, 18.0f }, line, Theme::Floating(13.5f, Theme::Muted, UiFont::Body), { 0.0f, 0.5f });
			const UiRect meter{ bar.Right() - 110.0f, below + 6.0f, 110.0f, 5.0f };
			draw.Rect(meter.Expand(1.5f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.7f), .Border = WithAlpha(surge, 0.3f), .BorderWidth = 1.0f, .Chamfer = 2.0f });
			if (run.Momentum > 0.0f)
				draw.Rect({ meter.X, meter.Y, meter.W * run.Momentum, meter.H }, { .Fill = Lighten(surge, 0.25f), .FillBottom = Darken(surge, 0.2f), .Glow = WithAlpha(surge, 0.35f), .GlowSize = 5.0f, .Chamfer = 2.0f });
			draw.TextAligned({ meter.X - 130.0f, below, 122.0f, 18.0f }, Format("Momentum \xC3\x97%.2f", double(1.0f + MomentumShards * run.Momentum)), Theme::Floating(13.5f, run.Momentum > 0.0f ? Lighten(surge, 0.2f) : Theme::Muted, UiFont::Body), { 1.0f, 0.5f });
			below += 20.0f;
		}
		RunRules plain = rules;
		plain.Seed = 0;
		plain.Endless = false;
		if (plain != RunRules{})
		{
			std::string test = "Test run";
			if (rules.StartMinute > 0)
				test += ", from " + std::to_string(rules.StartMinute) + ":00";
			if (rules.StartLevel > 1)
				test += ", " + std::to_string(rules.StartLevel - 1) + " cards ahead";
			if (rules.Spawns != 100)
				test += rules.Spawns == 0 ? std::string(", no spawns") : ", spawns " + std::to_string(rules.Spawns) + "%";
			if (rules.ShardRate != 100)
				test += ", shards " + std::to_string(rules.ShardRate) + "%";
			if (rules.Only > 0)
				test += std::string(", only ") + FindEnemyDef(RunOnlyKinds[rules.Only - 1])->Name + "s";
			if (rules.Immortal)
				test += ", immortal";
				if (rules.AllRunes)
					test += ", every rune";
			draw.TextAligned({ 0.0f, below, ui.GetSize().x, 18.0f }, test, Theme::Floating(13.5f, Theme::Warning, UiFont::Body), { 0.5f, 0.5f });
		}
	}

	void SessionState::DrawSpectatorBar()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const glm::vec2 size = ui.GetSize();
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		const GameStatus& status = m_Client.GetStatus();
		const UiRect plate = UiRect::Centered({ size.x * 0.5f, size.y - 64.0f }, { 580.0f, 80.0f });
		draw.Rect(plate.Translate({ 0.0f, 6.0f }).Expand(4.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.55f), .Softness = 16.0f, .Chamfer = 18.0f });
		draw.Rect(plate, { .Fill = Hex(0x130F26, 0.9f), .FillBottom = Hex(0x07060E, 0.94f), .Border = WithAlpha(Theme::Astral, 0.55f), .BorderWidth = 1.3f, .Glow = WithAlpha(Theme::Astral, 0.06f), .GlowSize = 30.0f, .Chamfer = 14.0f, .Aether = 0.7f, .Seed = 0.42f });
		draw.Diamond({ plate.Center().x, plate.Y }, 10.0f, Theme::Astral, WithAlpha(Theme::Ink, 0.9f), 1.2f);
		ui.Block(plate);
		UiRect content = plate.Shrink(18.0f, 12.0f);
		draw.Icon(UiIcon::Eye, content.CutLeft(42.0f).Shrink(0.0f, 7.0f), { .Color = Lighten(Theme::Astral, 0.3f), .ColorBottom = Theme::Astral, .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
		content.CutLeft(14.0f);
		const bool open = status.Match != MatchPhase::Ended;
		const UiRect button = open ? content.CutRight(200.0f) : UiRect{};
		const std::string watched = local.Watching != NoPlayer ? PlayerName(local.Watching) : std::string();
		draw.TextAligned(content.CutTop(18.0f), "SPECTATING", Theme::Caps(13.0f, Theme::Astral, 2.0f), { 0.0f, 0.5f });
		draw.TextFit(content.CutTop(26.0f), watched.empty() ? std::string("Nobody to watch yet") : watched, Theme::Bold(19.0f, watched.empty() ? Theme::Muted : Lighten(WithAlpha(ColorOf(local.Watching)), 0.35f)));
		if (!watched.empty())
			draw.TextAligned(content, "LMB next mage, RMB the one before", Theme::Body(12.5f, Theme::Faint), { 0.0f, 0.5f });
		const bool choosing = status.Brawl.Phase == BrawlPhase::Draft || status.Brawl.Phase == BrawlPhase::Pick;
		if (open && ui.Button(Ui::MakeId("spectatejoin"), button.Shrink(0.0f, 8.0f), choosing ? "Join the fight" : "Join next round", ButtonKind::Primary, true, UiIcon::Whirlwind))
			m_Client.JoinMatch();
		float noticeY = plate.Y - 14.0f;
		for (auto it = m_Notices.rbegin(); it != m_Notices.rend(); ++it)
		{
			const float alpha = std::clamp((NoticeSeconds - it->Age) / 0.6f, 0.0f, 1.0f) * std::clamp(it->Age / 0.15f, 0.0f, 1.0f);
			noticeY -= 24.0f;
			draw.TextAligned({ 0.0f, noticeY, size.x, 22.0f }, it->Text, Theme::Floating(16.0f, WithAlpha(it->Color, it->Color.a * alpha), UiFont::Bold), { 0.5f, 0.5f });
		}
	}

	void SessionState::DrawHud()
	{
		const LocalPlayerState& local = m_Client.GetLocalPlayer();
		if (!local.Valid)
			return;
		if (m_Client.IsSpectating())
		{
			DrawSpectatorBar();
			return;
		}
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const PlayerState& state = m_Client.GetPredictedState();
		const PlayerTraits& traits = m_Client.GetTraits();
		const glm::vec2 size = ui.GetSize();
		const float center = size.x * 0.5f;
		constexpr float SlotSize = 62.0f;
		constexpr float SlotGap = 9.0f;
		constexpr float OrbRadius = 58.0f;
		const uint32_t shown = m_Client.GetSession().Mode == GameMode::Shardfall ? AbilitySlots : BrawlSlots + 1;
		const float barWidth = float(shown + 1) * SlotSize + float(shown) * SlotGap;
		const UiRect slots{ center - barWidth * 0.5f, size.y - 22.0f - SlotSize, barWidth, SlotSize };
		const UiRect plate = slots.Expand(12.0f);
		draw.Rect(plate.Translate({ 0.0f, 6.0f }).Expand(4.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.55f), .Softness = 16.0f, .Chamfer = 18.0f });
		draw.Rect(plate, { .Fill = Hex(0x130F26, 0.9f), .FillBottom = Hex(0x07060E, 0.94f), .Border = WithAlpha(Theme::Edge, 0.95f), .BorderWidth = 1.3f, .Glow = WithAlpha(Theme::Accent, 0.06f), .GlowSize = 30.0f, .Chamfer = 14.0f, .Aether = 0.7f, .Seed = 0.42f });
		draw.Rect(plate.Shrink(4.0f), { .Border = WithAlpha(Theme::AccentLight, 0.06f), .BorderWidth = 1.0f, .Chamfer = 12.0f });
		draw.Diamond({ plate.Center().x, plate.Y }, 10.0f, Theme::AccentLight, WithAlpha(Theme::Ink, 0.9f), 1.2f);
		ui.Block(plate);
		DrawAbilityBar(slots);
		if (m_Client.GetSession().Mode == GameMode::Brawl && local.Brawl.Cards != 0)
		{
			const float count = float(CountCards(local.Brawl.Cards));
			constexpr float CardIcon = 22.0f;
			const float width = count * (CardIcon + 6.0f) - 6.0f;
			DrawCardRow({ center - width * 0.5f, plate.Y - CardIcon - 12.0f, width, CardIcon }, local.Brawl.Cards, CardIcon, nullptr);
		}

		const auto orb = [&](float x, float fill, UiColor light, UiColor deep, UiColor rune, const std::string& text, float phase, float danger)
		{
			const glm::vec2 orbCenter(x, size.y - 18.0f - OrbRadius);
			const float frame = OrbRadius + 11.0f;
			const UiRect bounds = UiRect::Centered(orbCenter, glm::vec2(frame * 2.0f));
			draw.Rect(bounds.Translate({ 0.0f, 5.0f }).Expand(2.0f), { .Fill = Rgba(0.0f, 0.0f, 0.02f, 0.6f), .Radius = frame + 2.0f, .Softness = 14.0f });
			if (danger > 0.01f)
				draw.Rect(bounds.Expand(10.0f), { .Fill = WithAlpha(rune, 0.3f * danger * (0.6f + 0.4f * std::sin(m_Time * 5.0f))), .Radius = frame + 10.0f, .Softness = 16.0f, .Additive = 1.0f });
			draw.Circle(orbCenter, frame, Hex(0x0B0918, 0.96f), WithAlpha(glm::mix(Theme::Edge, rune, danger), 0.95f), 1.3f);
			draw.Sigil(orbCenter, frame, { .Color = WithAlpha(glm::mix(Theme::AccentLight, rune, 0.5f + 0.5f * danger), 0.6f + 0.35f * danger), .Speed = 3.0f * danger, .Phase = phase, .LineWidth = 1.0f, .Glow = 0.35f + 0.6f * danger, .Layers = SigilRings });
			draw.Orb(orbCenter, OrbRadius, fill, light, deep, Rgba(0.04f, 0.03f, 0.08f, 0.95f), Rgba(1.0f, 1.0f, 1.0f, 0.3f), phase);
			draw.Circle(orbCenter, OrbRadius + 1.0f, UiColor(0.0f), WithAlpha(Theme::AccentLight, 0.55f), 1.4f);
			draw.Diamond(orbCenter - glm::vec2(0.0f, frame), 12.0f, Lighten(rune, 0.45f), WithAlpha(Theme::Ink, 0.9f), 1.2f);
			draw.TextAligned(UiRect::Centered(orbCenter, { 160.0f, 24.0f }), text, Theme::Floating(17.0f, Hex(0xFFF7FF)), { 0.5f, 0.5f });
			ui.Block(bounds);
		};
		const float healthFraction = local.MaxHealth > 0 ? float(std::max(local.Health, 0)) / float(local.MaxHealth) : 0.0f;
		const float manaFraction = traits.MaxMana > 0.0f ? state.Mana / traits.MaxMana : 0.0f;
		const float danger = state.Alive ? std::clamp((0.35f - healthFraction) / 0.25f, 0.0f, 1.0f) : 0.0f;
		orb(plate.X - OrbRadius - 8.0f, healthFraction, Hex(0xFF5F6F), Hex(0x5E0A24), Hex(0xFF7A96), std::to_string(std::max(local.Health, 0)) + " / " + std::to_string(local.MaxHealth), 0.0f, danger);
		if (local.BloodShield > 0 && state.Alive)
		{
			const glm::vec2 healthCenter(plate.X - OrbRadius - 8.0f, size.y - 18.0f - OrbRadius);
			const float cap = std::max(float(local.MaxHealth) / (1.0f + BloodMagicHealth) * BloodMagicOverheal, 1.0f);
			const float share = std::clamp(float(local.BloodShield) / cap, 0.0f, 1.0f);
			draw.OrbAura(healthCenter, OrbRadius, OrbAuraStyle::BloodShield, share, Hex(0xFF6E86), Hex(0x24020A));
			draw.TextAligned(UiRect::Centered({ healthCenter.x, healthCenter.y - OrbRadius - 26.0f }, { 220.0f, 18.0f }), "+" + std::to_string(local.BloodShield) + " blood shield", Theme::Floating(14.0f, Hex(0xFF8095), UiFont::Bold), { 0.5f, 0.5f });
		}
		const SessionInfo& session = m_Client.GetSession();
		const bool brawlDamage = session.Mode == GameMode::Brawl && session.Rules.Mode == BrawlMode::Damage;
		if (session.Mode == GameMode::Brawl && !brawlDamage)
		{
			const float missing = state.Alive ? 1.0f - healthFraction : 0.0f;
			const float rise = std::clamp((missing - 0.4f) / 0.5f, 0.0f, 1.0f);
			orb(plate.Right() + OrbRadius + 8.0f, missing, Hex(0xFFB45A), Hex(0x5E2208), Hex(0xFF9A4A), "+" + std::to_string(int(std::lround(missing * MissingHealthKnockback * 100.0f))) + "% fling", 2.1f, rise);
			if (ui.Interact(Ui::MakeId("flingorb"), UiRect::Centered({ plate.Right() + OrbRadius + 8.0f, size.y - 18.0f - OrbRadius }, glm::vec2(OrbRadius * 2.0f))).Hovered)
				ui.Tooltip().Header("Fling", Hex(0xFFB45A), std::nullopt, UiIcon::Whirlwind).Text("Spells don't hurt you, but the less health you have, the further they throw you.").Muted("Lava and its afterburn are the only things that cost health.");
		}
		else
		{
			const glm::vec2 manaCenter(plate.Right() + OrbRadius + 8.0f, size.y - 18.0f - OrbRadius);
			const CardId apex = brawlDamage ? HeldApex(local.Brawl.Cards) : CardId::Count;
			const bool blood = brawlDamage && traits.MaxMana <= 0.0f && traits.BloodRate > 0.0f;
			if (blood)
				orb(manaCenter.x, healthFraction, Hex(0xFF4D6A), Hex(0x2A0410), Hex(0xFF3355), GetCard(CardId::BloodMagic).Name, 2.1f, 0.0f);
			else if (traits.MaxMana > 0.0f)
				orb(manaCenter.x, manaFraction, Hex(0x7FA8FF), Hex(0x13135E), Theme::Astral, std::to_string(int(state.Mana)) + " / " + std::to_string(int(traits.MaxMana)), 2.1f, 0.0f);
			if (session.Mode == GameMode::Shardfall && traits.OvercastCost > 0.0f)
			{
				if (ui.Interact(Ui::MakeId("manaorb"), UiRect::Centered(manaCenter, glm::vec2(OrbRadius * 2.0f))).Hovered)
					ui.Tooltip().Header("Mana", Hex(0x7FA8FF), std::nullopt, UiIcon::Droplets).Text("Spells are free. Mana lets you overcast: press a spell that is still recharging to cast it anyway.").Pair("Price", Format("up to %.0f, less the closer the spell is to ready", double(traits.OvercastCost))).Pair("Refills", Format("%.0f per basic hit, %.0f per kill, %.0f per perfect dodge, %.0f a second", double(RunBasicMana), double(RunKillMana), double(RunPerfectMana), double(traits.ManaRegen))).Muted("Blinks, wards and shields cannot be overcast.");
			}
			else if ((brawlDamage || session.Mode == GameMode::Hub) && ui.Interact(Ui::MakeId("manaorb"), UiRect::Centered(manaCenter, glm::vec2(OrbRadius * 2.0f))).Hovered)
			{
				if (blood)
					ui.Tooltip().Header(GetCard(CardId::BloodMagic).Name, Hex(0xFF4D6A), std::nullopt, IconFor(CardId::BloodMagic)).Text(CardText(GetCard(CardId::BloodMagic), BrawlMode::Damage)).Muted("Spells never take your last point of health.");
				else
					ui.Tooltip().Header("Mana", Hex(0x7FA8FF), std::nullopt, UiIcon::Droplets).Text("Every spell costs mana, " + std::string(GetAbility(BrawlBasic).Name) + " too. Dodging is free.").Muted("You regain " + std::to_string(int(traits.ManaRegen)) + " mana per second.");
			}
			char label[48] = "";
			UiColor tone = Theme::Text;
			if (apex == CardId::ManaSingularity)
			{
				std::snprintf(label, sizeof(label), "+%d%% damage", int(ManaSingularityDamage * 100.0f * state.Mana));
				tone = Hex(0xFFD978);
			}
			else if (apex == CardId::MindOverMatter)
			{
				if (state.ManaStall > 0.0f)
					std::snprintf(label, sizeof(label), "Mana slowed for %.1f s", double(state.ManaStall));
				else
					std::snprintf(label, sizeof(label), "Your mana shields you");
				tone = Hex(0x9DBBFF);
			}
			else if (apex == CardId::Archmage)
			{
				if (state.Weave >= ArchmageStacks)
					std::snprintf(label, sizeof(label), "Weave full: next spell instant, +%d%%", int(std::lround(ArchmageDamage * float(ArchmageStacks) * 100.0f)));
				else if (state.Weave > 0)
					std::snprintf(label, sizeof(label), "Weave %u: +%d%% damage", unsigned(state.Weave), int(std::lround(ArchmageDamage * float(state.Weave) * 100.0f)));
				else
					std::snprintf(label, sizeof(label), "Weave different spells");
				tone = state.Weave >= ArchmageStacks ? Hex(0xFFE2A8) : state.Weave > 0 ? Hex(0xF2D8A6) : Theme::Muted;
			}
			const uint8_t surgeKind = state.Surge & 0x0F;
			if (state.Surge != m_SeenSurge)
			{
				m_SeenSurge = state.Surge;
				m_SurgeTime = m_Time;
			}
			if (surgeKind != 0 && m_Time - m_SurgeTime < 1.4f)
			{
				const bool arcana = apex == CardId::UnstableArcana;
				const char* surgeText = !arcana ? "Rewind: cooldown skipped" : surgeKind == uint8_t(ArcanaSurge::Power) ? "Surge: double damage" : surgeKind == uint8_t(ArcanaSurge::Area) ? "Surge: huge area" : surgeKind == uint8_t(ArcanaSurge::Copies) ? "Surge: three casts" : "Surge: no cooldown";
				std::snprintf(label, sizeof(label), "%s", surgeText);
				tone = arcana ? WithAlpha(ApexColor(m_Time)) : Hex(0xA8E8FF);
			}
			if (label[0] != '\0')
				draw.TextAligned(UiRect::Centered({ manaCenter.x, manaCenter.y - OrbRadius - (traits.OverloadEvery > 0 ? 62.0f : 26.0f) }, { 260.0f, 18.0f }), label, Theme::Floating(14.0f, tone, UiFont::Bold), { 0.5f, 0.5f });
			if (traits.OverloadEvery > 0)
			{
				const bool primed = state.Overload + 1u >= traits.OverloadEvery;
				const float spacing = 20.0f;
				const float left = manaCenter.x - spacing * float(traits.OverloadEvery - 1) * 0.5f;
				for (uint32_t i = 0; i < traits.OverloadEvery; i++)
				{
					const bool lit = i < state.Overload || (primed && i + 1 == traits.OverloadEvery);
					const UiColor pip = primed ? Hex(0xC9A6FF) : Hex(0x7FA8FF);
					draw.Diamond({ left + spacing * float(i), manaCenter.y - OrbRadius - 26.0f }, primed ? 12.0f : 10.0f, lit ? pip : WithAlpha(Theme::Faint, 0.6f), WithAlpha(Theme::Ink, 0.9f), 1.0f);
				}
				if (primed)
					draw.TextAligned(UiRect::Centered({ manaCenter.x, manaCenter.y - OrbRadius - 44.0f }, { 200.0f, 18.0f }), "Next spell is free", Theme::Floating(13.5f, Hex(0xE3D4FF), UiFont::Bold), { 0.5f, 0.5f });
			}
		}
		if (local.Shield > 0)
		{
			const glm::vec2 healthCenter(plate.X - OrbRadius - 8.0f, size.y - 18.0f - OrbRadius);
			const bool up = local.Shield == 255;
			const float reform = float(local.Shield) / 10.0f;
			if (!up)
				m_WardUpAt = -1.0f;
			else if (m_WardUpAt < 0.0f)
				m_WardUpAt = m_Time;
			if (up)
			{
				const float grow = std::clamp((m_Time - m_WardUpAt) / WardRegrowSeconds, 0.0f, 1.0f);
				draw.OrbAura(healthCenter, OrbRadius, OrbAuraStyle::CrystalWard, grow * grow * (3.0f - 2.0f * grow), Hex(0xE4F4FF), Hex(0x4F7FB0));
			}
			char ward[40];
			if (up)
				std::snprintf(ward, sizeof(ward), "Crystal Ward");
			else
				std::snprintf(ward, sizeof(ward), "Ward re-forms in %.0f s", double(std::ceil(reform)));
			draw.TextAligned(UiRect::Centered({ healthCenter.x, healthCenter.y - OrbRadius - 26.0f }, { 200.0f, 18.0f }), ward, Theme::Floating(14.0f, up ? Hex(0xCFEBFF) : Theme::Muted, UiFont::Bold), { 0.5f, 0.5f });
		}
		else
			m_WardUpAt = -1.0f;

		const float buffY = plate.Y - 40.0f;

		float noticeY = buffY - 10.0f;
		for (auto it = m_Notices.rbegin(); it != m_Notices.rend(); ++it)
		{
			const float alpha = std::clamp((NoticeSeconds - it->Age) / 0.6f, 0.0f, 1.0f) * std::clamp(it->Age / 0.15f, 0.0f, 1.0f);
			noticeY -= 24.0f;
			draw.TextAligned({ 0.0f, noticeY, size.x, 22.0f }, it->Text, Theme::Floating(16.0f, WithAlpha(it->Color, it->Color.a * alpha), UiFont::Bold), { 0.5f, 0.5f });
		}

		const GameStatus& match = m_Client.GetStatus();
		const bool brawlFight = match.Mode == GameMode::Brawl && match.Match != MatchPhase::Ended && match.Brawl.Phase == BrawlPhase::Fight;
		if (m_Client.IsSpectating())
		{
		}
		else if (!state.Alive && brawlFight && m_FoughtRound != match.Brawl.Round)
		{
			TextStyle waiting = Theme::Title(40.0f);
			waiting.Tracking = 3.0f;
			draw.TextAligned({ 0.0f, size.y * 0.3f, size.x, 48.0f }, "Next Round Soon", waiting, { 0.5f, 0.5f });
			const std::string watching = local.Watching != NoPlayer ? PlayerName(local.Watching) : std::string();
			const std::string line = watching.empty() ? std::string("This round started without you. You join the next one") : "This round started without you. Spectating " + watching + ", click to switch";
			draw.TextAligned({ 0.0f, size.y * 0.3f + 52.0f, size.x, 24.0f }, line, Theme::Floating(18.0f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		}
		else if (!state.Alive && (match.Mode != GameMode::Brawl || brawlFight))
		{
			TextStyle died = Theme::Title(56.0f);
			died.Color = Hex(0xFF7A6A);
			died.ColorBottom = Hex(0x9E2620);
			died.Tracking = 4.0f;
			died.ShadowOffset = { 0.0f, 0.0f };
			died.ShadowColor = Rgba(0.6f, 0.0f, 0.0f, 0.6f);
			died.ShadowBlur = 16.0f;
			const GameStatus& status = m_Client.GetStatus();
			const bool brawlOut = status.Mode == GameMode::Brawl;
			const std::string killer = local.KilledBy != NoPlayer ? PlayerName(local.KilledBy) : std::string();
			const bool slain = brawlDamage && !killer.empty();
			draw.TextAligned({ 0.0f, size.y * 0.3f, size.x, 64.0f }, brawlOut ? (slain ? "Slain" : TermsFor(status.Map).Out) : IsPvpMode(status.Mode) ? "Slain" : status.Mode == GameMode::Shardfall ? "Down" : "You Died", died, { 0.5f, 0.5f });
			char respawn[160];
			respawn[0] = 0;
			if (brawlOut)
			{
				const std::string watching = local.Watching != NoPlayer ? PlayerName(local.Watching) : std::string();
				const std::string cause = killer.empty() ? std::string("The ") + TermsFor(status.Map).Hazard + " got you" : slain ? killer + " took you down" : killer + " shoved you into the " + TermsFor(status.Map).Hazard;
				if (!watching.empty())
					std::snprintf(respawn, sizeof(respawn), "%s. Spectating %s, click to switch. You're back next round", cause.c_str(), watching.c_str());
				else
					std::snprintf(respawn, sizeof(respawn), "%s. You're back next round", cause.c_str());
			}
			else if (IsPvpMode(status.Mode))
			{
				if (status.Match == MatchPhase::Ended)
					std::snprintf(respawn, sizeof(respawn), "%s%s", killer.empty() ? "" : "Slain by ", killer.c_str());
				else
					std::snprintf(respawn, sizeof(respawn), "%s%s%sBack in %.1f", killer.empty() ? "" : "Slain by ", killer.c_str(), killer.empty() ? "" : ". ", double(local.RespawnTime));
			}
			else if (status.Mode == GameMode::Shardfall)
			{
				const unsigned revive = unsigned(std::lround(float(local.Revive) * 100.0f / 255.0f));
				uint32_t players = 0;
				for (const PlayerInfo& player : m_Client.GetPlayers())
					players += player.Bot ? 0u : 1u;
				if (players > 1)
				{
					if (revive > 0)
						std::snprintf(respawn, sizeof(respawn), "An ally is reviving you: %u%%", revive);
					else
						std::snprintf(respawn, sizeof(respawn), "An ally can revive you by standing close for %.0f seconds", double(ReviveSeconds));
				}
				else if (local.Run.Revives > 0)
					std::snprintf(respawn, sizeof(respawn), "Your one revive is rising: %u%%", revive);
				else
					std::snprintf(respawn, sizeof(respawn), "No revives left");
			}
			else if (local.RespawnTime > 0.0f)
				std::snprintf(respawn, sizeof(respawn), "Respawning in %.1f", double(local.RespawnTime));
			else
				std::snprintf(respawn, sizeof(respawn), "Reviving...");
			draw.TextAligned({ 0.0f, size.y * 0.3f + 66.0f, size.x, 24.0f }, respawn, Theme::Floating(18.0f, Theme::Text, UiFont::Body), { 0.5f, 0.5f });
		}

		const GameMode mode = m_Client.GetSession().Mode;
		if (IsPvpMode(mode))
			DrawKillFeed();
	}

	void SessionState::DrawAbilityBar(const UiRect& area)
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		const PlayerState& state = m_Client.GetPredictedState();
		const PlayerTraits& traits = m_Client.GetTraits();
		const float slotSize = area.H;
		const uint32_t shown = m_Client.GetSession().Mode == GameMode::Shardfall ? AbilitySlots : BrawlSlots + 1;
		const float gap = (area.W - slotSize * float(shown + 1)) / float(shown);
		const bool editing = CanEditSpells();
		const UiPayload* drag = ui.GetDrag();
		const bool dragging = drag && (drag->Type == SpellPayload || drag->Type == KeyPayload);
		const float beckon = 0.55f + 0.45f * std::sin(m_Time * 6.0f);
		const CardMask cards = m_Client.GetSession().Mode == GameMode::Brawl && m_Client.GetSession().Rules.Mode == BrawlMode::Damage ? m_Client.GetLocalPlayer().Brawl.Cards : 0;
		for (uint32_t slot = 0; slot <= shown; slot++)
		{
			const bool dodge = slot == shown;
			const SkillTraits& skill = dodge ? traits.Dodge : traits.Skills[slot];
			const UiRect rect{ area.X + float(slot) * (slotSize + gap), area.Y, slotSize, slotSize };
			const UiId abilityId = Ui::MakeId("ability", slot);
			const UiInteraction interaction = ui.Interact(abilityId, rect);
			const bool spellSlot = editing && slot >= 1 && slot <= BrawlSlots;
			if (spellSlot)
			{
				ui.OfferDrag(abilityId, KeyPayload, int(slot - 1));
				AcceptSpellDrop(rect, slot - 1, interaction.Clicked);
				if (dragging || m_SelectedSpell)
					draw.Shadow(rect, 10.0f, 14.0f, WithAlpha(Theme::Astral, (rect.Contains(ui.GetMouse()) ? 0.85f : 0.4f) * beckon), { 0.0f, 0.0f });
			}
			const bool empty = skill.IsEmpty();
			const float cooldown = dodge ? std::max(state.DodgeCooldown - float(std::max<uint8_t>(traits.DodgeCharges, 1) - 1) * skill.Cooldown, 0.0f) : state.Cooldowns[slot];
			const bool guard = !dodge && IsGuard(skill);
			const bool shattered = guard && IsGuardBroken(state, traits);
			const float span = shattered ? GuardBreakCooldown(skill) : skill.Cooldown;
			const bool recasting = !dodge && AwaitsNovaRecast(state, slot);
			const bool rearmed = !dodge && state.RecastSlot == int8_t(slot) && state.RecastTimer > 0.0f;
			const bool barrage = rearmed && AbilityId(state.RecastAbility) == AbilityId::EarthSpike;
			const float ready = span > 0.0f && !rearmed ? 1.0f - std::clamp(cooldown / span, 0.0f, 1.0f) : 1.0f;
			const bool covered = state.Mana + 1e-3f >= skill.ManaCost;
			const bool overloaded = !dodge && IsOverloaded(state, traits, slot);
			const bool affordable = covered || overloaded || traits.BloodRate > 0.0f;
			const bool casting = state.CastSlot == int8_t(slot);
			const bool active = !empty && skill.Usable && affordable;
			const UiColor color = empty ? Theme::Faint : dodge ? Hex(0xC9D4E8) : WithAlpha(SkillColor(skill.Ability));
			if (casting)
				draw.Shadow(rect, 9.0f, 9.0f, WithAlpha(Lighten(color, 0.4f), 0.75f), { 0.0f, 0.0f });
			UiColor border = WithAlpha(color, active ? 0.9f : 0.4f);
			if (casting)
				border = Hex(0xFFF6DD);
			else if (interaction.Hovered)
				border = Lighten(border, 0.35f);
			draw.Rect(rect, { .Fill = Theme::SlotTop, .FillBottom = Theme::SlotBottom, .Border = border, .BorderWidth = casting ? 2.2f : 1.4f, .Glow = WithAlpha(color, active ? 0.16f : 0.0f), .GlowSize = 14.0f, .Chamfer = 10.0f, .Seed = float(slot) * 0.21f });
			if (empty)
				draw.Icon(UiIcon::Emerald, rect.Shrink(17.0f), { .Color = Rgba(0.62f, 0.56f, 0.9f, 0.2f) });
			else
			{
				const UiColor top = active ? Lighten(color, 0.55f) : Hex(0x8A86A0);
				const UiColor bottom = active ? color : Hex(0x4A4760);
				draw.Icon(IconFor(skill.Ability), rect.Shrink(10.0f), { .Color = top, .ColorBottom = bottom, .Outline = 0.9f, .OutlineColor = Rgba(0.0f, 0.0f, 0.0f, 0.75f), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.6f) });
			}
			if (ready < 1.0f)
			{
				draw.Rect(rect.Shrink(1.5f), { .Fill = Rgba(0.02f, 0.015f, 0.05f, 0.74f), .Sweep = ready, .Chamfer = 9.0f });
				if (cooldown > 0.25f)
				{
					char text[16];
					std::snprintf(text, sizeof(text), cooldown < 3.0f ? "%.1f" : "%.0f", double(cooldown));
					draw.TextAligned(rect, text, Theme::Floating(19.0f, Hex(0xF4F0FF)), { 0.5f, 0.5f });
				}
			}
			if (recasting)
			{
				const bool brittle = IsNovaBrittle(state.RecastAge);
				const float left = brittle ? std::clamp(state.RecastTimer / (FrostNovaWindow - FrostNovaRipe), 0.0f, 1.0f) : std::clamp(state.RecastAge / FrostNovaRipe, 0.0f, 1.0f);
				const float glint = brittle ? 0.75f + 0.25f * std::sin(m_Time * 16.0f) : 0.35f + 0.15f * std::sin(m_Time * 6.0f);
				const float snap = brittle ? std::exp(-(state.RecastAge - FrostNovaRipe) * 9.0f) : 0.0f;
				const UiColor fill = brittle ? Lighten(color, 0.65f + 0.35f * snap) : WithAlpha(Lighten(color, 0.15f), 0.6f + 0.4f * NovaRipeness(state.RecastAge));
				const UiRect bar{ rect.X + 7.0f, rect.Y + 5.0f, rect.W - 14.0f, 4.0f };
				draw.Rect(rect.Expand(2.5f + 3.0f * snap), { .Border = WithAlpha(Lighten(color, 0.5f), 0.9f * glint), .BorderWidth = brittle ? 2.2f : 1.4f, .Glow = WithAlpha(color, (brittle ? 0.5f : 0.2f) * glint + 0.5f * snap), .GlowSize = 10.0f + 8.0f * snap, .Chamfer = 11.0f });
				draw.Rect(bar, { .Fill = Rgba(0.02f, 0.02f, 0.05f, 0.85f), .Chamfer = 1.5f });
				draw.Rect({ bar.X, bar.Y, bar.W * left, bar.H }, { .Fill = fill, .Glow = WithAlpha(color, brittle ? 0.55f : 0.2f), .GlowSize = 5.0f, .Chamfer = 1.5f });
			}
			if (rearmed)
			{
				const float pulse = 0.5f + 0.5f * std::sin(m_Time * 3.4f);
				const UiRect ring = rect.Expand(3.0f);
				draw.Rect(ring, { .Border = WithAlpha(Lighten(color, 0.55f), 0.3f + 0.5f * pulse), .BorderWidth = 1.6f, .Glow = WithAlpha(color, 0.15f + 0.3f * pulse), .GlowSize = 12.0f, .Chamfer = 11.0f });
				const auto perimeter = [&](float t)
				{
					const float p = (t - std::floor(t)) * 4.0f;
					const float f = p - std::floor(p);
					switch (int(p))
					{
					case 0: return glm::vec2(ring.X + ring.W * f, ring.Y);
					case 1: return glm::vec2(ring.Right(), ring.Y + ring.H * f);
					case 2: return glm::vec2(ring.Right() - ring.W * f, ring.Bottom());
					default: return glm::vec2(ring.X, ring.Bottom() - ring.H * f);
					}
				};
				const UiColor spark = Lighten(color, 0.75f);
				for (const float head : { 0.0f, 0.5f })
					for (int k = 0; k < 9; k++)
					{
						const float fade = 1.0f - float(k) / 9.0f;
						draw.Circle(perimeter(m_Time * 0.42f + head - float(k) * 0.011f), 3.4f * (0.45f + 0.55f * fade), WithAlpha(spark, fade * (0.55f + 0.45f * pulse)));
					}
			}
			if (barrage)
			{
				const UiRect bar{ rect.X + 7.0f, rect.Y + 5.0f, rect.W - 14.0f, 4.0f };
				draw.Rect(bar, { .Fill = Rgba(0.02f, 0.02f, 0.05f, 0.85f), .Chamfer = 1.5f });
				draw.Rect({ bar.X, bar.Y, bar.W * std::clamp(state.RecastTimer / BarrageWindow, 0.0f, 1.0f), bar.H }, { .Fill = Lighten(color, 0.3f), .Glow = WithAlpha(color, 0.4f), .GlowSize = 5.0f, .Chamfer = 1.5f });
				for (uint32_t pip = 0; pip < skill.Recasts; pip++)
					draw.Diamond({ rect.Center().x + (float(pip) - 0.5f * float(skill.Recasts - 1)) * 14.0f, rect.Y - 7.0f }, 10.0f, pip < state.RecastCharges ? Lighten(color, 0.5f) : Rgba(0.04f, 0.035f, 0.07f, 0.85f), WithAlpha(Theme::Edge, 0.85f), 0.8f);
			}
			if (guard && !shattered)
			{
				const float left = std::clamp(GuardLeft(state, traits) / std::max(skill.Durability, 1e-3f), 0.0f, 1.0f);
				const UiRect bar{ rect.X + 7.0f, rect.Y + 5.0f, rect.W - 14.0f, 4.0f };
				const UiColor tone = left > 0.34f ? Hex(0x8FE3FF) : Hex(0xC9A6FF);
				draw.Rect(bar, { .Fill = Rgba(0.02f, 0.02f, 0.05f, 0.85f), .Chamfer = 1.5f });
				draw.Rect({ bar.X, bar.Y, bar.W * left, bar.H }, { .Fill = tone, .Glow = WithAlpha(tone, 0.35f), .GlowSize = 5.0f, .Chamfer = 1.5f });
			}
			if (!empty && !skill.Usable)
			{
				draw.Rect(rect.Shrink(1.5f), { .Fill = Rgba(0.45f, 0.05f, 0.12f, 0.35f), .Chamfer = 9.0f });
				draw.Icon(UiIcon::Padlock, UiRect::Centered(rect.Center(), { 26.0f, 26.0f }), { .Color = Hex(0xFF8A7A), .ColorBottom = Hex(0xB83A30), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.8f) });
			}
			const char* key = dodge ? "Space" : SlotKeys[slot];
			const TextStyle keyStyle = Theme::Bold(11.5f, Theme::AccentLight);
			const float keyWidth = draw.MeasureWidth(key, keyStyle) + 10.0f;
			const UiRect badge{ rect.X + 3.0f, rect.Bottom() - 17.0f, keyWidth, 15.0f };
			draw.Rect(badge, { .Fill = Rgba(0.03f, 0.025f, 0.06f, 0.94f), .Border = WithAlpha(Theme::Edge, 0.9f), .BorderWidth = 1.0f, .Chamfer = 3.0f });
			draw.TextAligned(badge, key, keyStyle, { 0.5f, 0.5f });
			if (dodge && traits.DodgeCharges > 1)
			{
				const uint32_t ready = DodgesReady(state, traits);
				for (uint32_t pip = 0; pip < traits.DodgeCharges; pip++)
					draw.Diamond({ rect.Center().x + (float(pip) - 0.5f * float(traits.DodgeCharges - 1)) * 14.0f, rect.Y - 6.0f }, 9.0f, pip < ready ? Hex(0xE6EEFF) : Rgba(0.04f, 0.035f, 0.07f, 0.85f), WithAlpha(Theme::Edge, 0.8f), 0.8f);
			}
			if (!empty && !dodge && m_Client.GetSession().Mode == GameMode::Shardfall)
				for (uint8_t pip = 1; pip <= MaxSpellRank; pip++)
					draw.Diamond({ rect.X + 8.0f + float(pip - 1) * (rect.W - 16.0f) / float(MaxSpellRank - 1), rect.Y - 6.0f }, 7.0f, pip <= skill.Level ? WithAlpha(Lighten(color, 0.2f), active ? 1.0f : 0.5f) : Rgba(0.04f, 0.035f, 0.07f, 0.85f), WithAlpha(Theme::Edge, 0.7f), 0.8f);
			if (!empty && !dodge && m_Client.GetSession().Mode == GameMode::Shardfall)
			{
				const RunLoadout& run = m_Client.GetLocalPlayer().Run;
				for (uint32_t socket = 0; socket < OpenSockets(run, slot); socket++)
				{
					const RuneId rune = run.Sockets[slot][socket];
					const bool filled = RuneFits(rune, skill.Ability);
					const UiColor tone = filled ? Hex(GetRune(rune).Kind == RuneKind::Trigger ? 0xFFB45E : 0x6FE3C4) : UiColor(0.0f);
					draw.Circle({ rect.Right() - 7.5f, rect.Y + 9.0f + 8.5f * float(socket) }, 3.2f, filled ? tone : Rgba(0.03f, 0.025f, 0.06f, 0.9f), WithAlpha(filled ? Lighten(tone, 0.3f) : Theme::Edge, 0.9f), 0.8f);
				}
			}
			if (!empty && skill.ManaCost > 0.0f)
			{
				char cost[16];
				const bool blood = traits.MaxMana <= 0.0f && traits.BloodRate > 0.0f;
				if (blood)
					std::snprintf(cost, sizeof(cost), "%d hp", std::max(1, int(std::lround(skill.ManaCost * traits.BloodRate))));
				else
					std::snprintf(cost, sizeof(cost), "%.0f", double(skill.ManaCost));
				const UiColor tone = overloaded ? Hex(0xE3D4FF) : covered ? Hex(0x9DBBFF) : affordable ? Hex(0xFF6F7F) : Theme::Danger;
				draw.TextAligned(rect.Shrink(5.0f, 3.0f), overloaded ? std::string("free") : std::string(cost), Theme::Floating(13.0f, tone), { 1.0f, 1.0f });
			}
			if (!empty && !dodge && CanOvercast(state, traits, slot))
			{
				const UiColor tone = Hex(0x8FB4FF);
				draw.Rect(rect.Expand(2.0f), { .Border = WithAlpha(tone, 0.5f + 0.3f * std::sin(m_Time * 5.0f)), .BorderWidth = 1.4f, .Glow = WithAlpha(tone, 0.18f), .GlowSize = 8.0f, .Chamfer = 11.0f });
				draw.TextAligned(rect.Shrink(5.0f, 3.0f), Format("%.0f", double(OvercastPrice(state, traits, slot))), Theme::Floating(13.0f, Lighten(tone, 0.2f)), { 1.0f, 1.0f });
			}
			if (!empty && !dodge && cards != 0)
			{
				const UiColor glow = WithAlpha(ApexColor(m_Time + float(slot) * 0.8f));
				if (HasCard(cards, CardId::Refraction) && Refracts(skill.Ability))
					draw.TextAligned(rect.Shrink(6.0f, 4.0f), "x3", Theme::Floating(12.5f, glow, UiFont::Bold), { 1.0f, 0.0f });
				if (HasCard(cards, CardId::UnstableArcana) && slot >= 1)
					draw.Rect(rect.Expand(2.5f), { .Border = WithAlpha(glow, 0.85f), .BorderWidth = 1.4f, .Glow = WithAlpha(glow, 0.2f), .GlowSize = 8.0f, .Chamfer = 11.0f });
			}
			if (interaction.Hovered && !dragging)
			{
				UiTooltip& tooltip = ui.Tooltip();
				if (dodge)
					tooltip.Muted(m_Client.GetSession().Mode != GameMode::Shardfall ? "Dodge roll on Space, it cancels a cast without spending its mana" : "Dodge roll on Space, it cancels a cast and leaves the spell ready").Space(4.0f);
				else
					DescribeBrawlSpell(tooltip, skill.Ability);
			}
		}
	}

	void SessionState::DrawConnectionWarning()
	{
		const float age = m_Client.GetSnapshotAge();
		if (age < 0.5f)
			return;
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		char text[96];
		std::snprintf(text, sizeof(text), "Connection problem: no update from the server for %.1f s", double(age));
		const TextStyle style = Theme::Bold(15.0f, Theme::Warning);
		const UiRect pill = UiRect::Centered({ ui.GetSize().x * 0.5f, 122.0f }, { draw.MeasureWidth(text, style) + 64.0f, 34.0f });
		draw.Rect(pill, { .Fill = Rgba(0.16f, 0.1f, 0.04f, 0.92f), .Border = WithAlpha(Theme::Warning, 0.8f), .BorderWidth = 1.2f, .Chamfer = 9.0f });
		draw.Icon(UiIcon::Hourglass, { pill.X + 14.0f, pill.Center().y - 10.0f, 20.0f, 20.0f }, { .Color = Theme::Warning });
		draw.TextAligned({ pill.X + 40.0f, pill.Y, pill.W - 52.0f, pill.H }, text, style, { 0.0f, 0.5f });
	}

	void SessionState::DrawPauseMenu()
	{
		Ui& ui = m_App.GetUi();
		UiDrawList& draw = ui.Draw();
		m_PauseTime += ui.GetDeltaTime();
		const float appear = std::min(m_PauseTime / 0.3f, 1.0f);
		const UiRect screen = ui.GetScreen();
		const glm::vec2 center = screen.Center();
		draw.Veil(screen, appear);
		ui.Block(screen);

		if (m_PauseInvite)
			return DrawInvitePage(center);
		if (m_PauseAchievements)
			return DrawAchievementsPage(center);
		const bool subpage = m_PauseSettings || m_PauseWardrobe;
		TextStyle title = Theme::Title(subpage ? 30.0f : 44.0f);
		title.Tracking = subpage ? 2.5f : 7.0f;
		title.GlowColor = WithAlpha(Theme::Accent, 0.2f);
		title.GlowBlur = 10.0f;
		if (m_PauseSettings)
		{
			const UiRect panel = UiRect::Centered(center, { 600.0f, std::min(908.0f, screen.H - 40.0f) });
			ui.Panel(panel, 18.0f);
			UiRect content = panel.Shrink(36.0f, 30.0f);
			draw.Text({ content.X, content.Y }, "Attunement", title);
			draw.TextWrapped({ content.X, content.Y + 44.0f, content.W, 40.0f }, "Changes apply at once and are remembered.", Theme::Body(15.0f, Theme::Muted));
			ui.Separator({ content.X, content.Y + 74.0f, content.W, 10.0f });
			content.CutTop(100.0f);
			UiRect back = content.CutBottom(44.0f);
			content.CutBottom(12.0f);
			DrawSettings(ui, m_App, content);
			if (ui.Button(Ui::MakeId("pause.back"), back.CutLeft(170.0f), "Back"))
				m_PauseSettings = false;
			draw.TextAligned(back, "Esc returns", Theme::Body(13.5f, Theme::Faint), { 1.0f, 0.5f });
			return;
		}
		if (m_PauseWardrobe)
		{
			const UiRect panel = UiRect::Centered(center, { 640.0f, 646.0f });
			ui.Panel(panel, 18.0f);
			UiRect content = panel.Shrink(36.0f, 30.0f);
			draw.Text({ content.X, content.Y }, "Wardrobe", title);
			draw.TextWrapped({ content.X, content.Y + 44.0f, content.W, 40.0f }, "Everyone sees your new look at once.", Theme::Body(15.0f, Theme::Muted));
			ui.Separator({ content.X, content.Y + 74.0f, content.W, 10.0f });
			content.CutTop(100.0f);
			UiRect back = content.CutBottom(44.0f);
			Cosmetics style = m_Client.GetCosmetics();
			ui.Separator(content.CutTop(22.0f), "OUTFIT");
			content.CutTop(10.0f);
			bool restyled = DrawOutfitPicker(ui, content.CutTop(58.0f), style.Body);
			content.CutTop(16.0f);
			ui.Separator(content.CutTop(22.0f), "CASTING FOCUS");
			content.CutTop(10.0f);
			restyled |= DrawFocusPicker(ui, content.CutTop(58.0f), style.Cast);
			content.CutTop(16.0f);
			ui.Separator(content.CutTop(22.0f), "AURA");
			content.CutTop(10.0f);
			restyled |= DrawAuraPicker(ui, content.CutTop(58.0f), style.Glow);
			content.CutTop(16.0f);
			ui.Separator(content.CutTop(22.0f), "HEADWEAR");
			content.CutTop(10.0f);
			restyled |= DrawHeadpiecePicker(ui, content.CutTop(58.0f), style.Crown);
			if (restyled)
				m_Client.SetCosmetics(style);
			if (ui.Button(Ui::MakeId("pause.wardrobe.back"), back.CutLeft(170.0f), "Back"))
				m_PauseWardrobe = false;
			draw.TextAligned(back, "Esc returns", Theme::Body(13.5f, Theme::Faint), { 1.0f, 0.5f });
			return;
		}

		const GameMode mode = m_Client.GetSession().Mode;
		const bool canReturn = mode != GameMode::Hub && (!IsPvpMode(mode) || m_Server || m_Client.GetStatus().Match == MatchPhase::Ended);
		const char* returnLabel = mode == GameMode::Shardfall ? "Abandon the Run" : IsPvpMode(mode) && m_Client.GetStatus().Match != MatchPhase::Ended ? "End the Match" : "Return to the Sun's Cradle";
		const bool canInvite = m_App.GetSteam().HasLobby(m_LobbyTicket);
		const float count = 6.0f + (canReturn ? 1.0f : 0.0f) + (canInvite ? 1.0f : 0.0f);
		const float top = center.y - (count * 54.0f + 120.0f) * 0.5f;
		draw.TextAligned({ 0.0f, top, screen.W, 56.0f }, "Paused", title, { 0.5f, 0.5f });
		TextStyle motto = Theme::Caps(14.0f, Theme::Muted, 4.0f);
		motto.ColorBottom = Theme::Faint;
		draw.TextAligned({ 0.0f, top + 56.0f, screen.W, 20.0f }, m_Client.IsRunPaused() ? "the run waits for you" : "the world does not wait", motto, { 0.5f, 0.5f });
		ui.Separator({ center.x - 150.0f, top + 88.0f, 300.0f, 10.0f });
		UiRect entries{ center.x - 230.0f, top + 116.0f, 460.0f, count * 54.0f };
		const auto entry = [&](const char* id, const char* label)
		{
			const bool clicked = ui.MenuEntry(Ui::MakeId(id), entries.CutTop(48.0f), label, {}, false, true);
			entries.CutTop(6.0f);
			return clicked;
		};
		if (entry("pause.resume", "Resume"))
			m_Paused = false;
		if (canInvite && entry("pause.invite", "Invite Friends"))
			m_PauseInvite = true;
		if (entry("pause.wardrobe", "Wardrobe"))
			m_PauseWardrobe = true;
		if (entry("pause.achievements", "Achievements"))
			m_PauseAchievements = true;
		if (entry("pause.settings", "Attunement"))
			m_PauseSettings = true;
		if (canReturn && entry("pause.hub", returnLabel))
		{
			m_Client.ReturnToHub();
			m_Paused = false;
		}
		if (entry("pause.leave", m_Server ? "Stop Hosting" : "Leave the Game"))
			ReturnToMenu({});
		if (entry("pause.quit", "Quit to Desktop"))
			m_App.Quit();
	}

	void SessionState::DrawDistortionStage(WorldRenderer& renderer) const
	{
		const glm::vec3 right = glm::normalize(glm::vec3(m_Camera.View[0][0], 0.0f, m_Camera.View[2][0]));
		const glm::vec3 depth(-right.z, 0.0f, right.x);
		const auto spot = [&](float across, float along)
		{
			const glm::vec3 point = glm::vec3(m_CameraTarget.x, 0.0f, m_CameraTarget.z) + right * across + depth * along;
			return Ground(Vec2(point.x, point.z));
		};
		const auto loop = [&](float period, float offset) { return std::fmod(m_Time / period + offset, 1.0f); };
		const auto wave = [&](const glm::vec3& ground, float radius, float strength, float t)
		{
			const float grow = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
			DrawShockwave(renderer, ground, radius * (0.08f + 0.92f * grow), 0.35f * (0.6f + 0.8f * t), strength * (1.0f - t) * (1.0f - t));
		};
		if (m_App.GetSettings().DistortionStage == 2)
		{
			for (int i = 0; i < 4; i++)
				wave(spot(i % 2 == 0 ? -3.2f : 3.2f, i < 2 ? -1.6f : 1.6f), 2.2f, 0.26f, loop(1.6f, 0.17f * float(i)));
			return;
		}
		wave(spot(-5.0f, 0.0f), 1.9f, 0.26f, loop(1.6f, 0.0f));
		DrawHeatHaze(renderer, spot(-1.9f, 0.0f) + glm::vec3(0.0f, 1.0f, 0.0f), 1.1f, 0.035f, 3.0f, 1.4f, 11u);
		DrawImplosion(renderer, spot(1.5f, 0.0f) + glm::vec3(0.0f, 1.0f, 0.0f), 1.3f, 0.14f, m_Time * 1.2f);
		DrawGravityLens(renderer, spot(4.9f, 0.0f) + glm::vec3(0.0f, 1.4f, 0.0f), 0.32f, 0.62f, glm::vec3(0.75f, 0.55f, 1.1f) * 0.7f);
	}
}

namespace Game
{
	glm::vec3 SessionState::StageSpot(float across, float along) const
	{
		const glm::vec3 right = glm::normalize(glm::vec3(m_Camera.View[0][0], 0.0f, m_Camera.View[2][0]));
		const glm::vec3 depth(-right.z, 0.0f, right.x);
		const glm::vec3 point = glm::vec3(m_CameraTarget.x, 0.0f, m_CameraTarget.z) + right * across + depth * along;
		return Ground(Vec2(point.x, point.z));
	}

	std::vector<LevelFire> SessionState::StageFires() const
	{
		if (m_App.GetSettings().FireStage != 1)
			return {};
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		return {
			{ StageSpot(-2.5f, -1.4f) + up * 0.05f, 0.9f, FireKind::Campfire },
			{ StageSpot(-1.2f, -1.4f) + up * 0.8f, 0.55f, FireKind::Campfire },
			{ StageSpot(-0.1f, -1.4f) + up * 1.3f, 1.0f, FireKind::Lantern },
			{ StageSpot(0.9f, -1.4f) + up * 1.0f, 1.0f, FireKind::Candle },
			{ StageSpot(1.05f, -1.4f) + up * 1.02f, 1.0f, FireKind::Candle },
			{ StageSpot(2.1f, -1.4f) + up * 1.6f, 1.0f, FireKind::Torch },
		};
	}

	bool SessionState::StageBolt(int index, glm::vec3& center, float& facing) const
	{
		constexpr float Flight = 6.5f / 11.5f;
		const float t = std::fmod(m_Time + 0.5f * float(index), 1.0f) / Flight;
		if (t >= 1.0f)
			return false;
		const glm::vec3 right = glm::normalize(glm::vec3(m_Camera.View[0][0], 0.0f, m_Camera.View[2][0]));
		center = StageSpot(-3.25f + 6.5f * t, index == 0 ? -2.9f : 0.9f) + glm::vec3(0.0f, 1.0f, 0.0f);
		facing = std::atan2(right.z, right.x);
		return true;
	}

	int SessionState::StageBlastCount() const
	{
		return m_App.GetSettings().FireStage == 2 ? 6 : 1;
	}

	float SessionState::StageBlast(int index, glm::vec3& ground, float& power) const
	{
		constexpr float Period = 2.4f;
		struct Spot
		{
			float Across;
			float Along;
			float Power;
			float Delay;
		};
		static constexpr Spot Spots[] = {
			{ -5.5f, 2.5f, 1.0f, 0.0f },
			{ 0.0f, 2.9f, 1.0f, 0.15f },
			{ 5.5f, 2.5f, 1.0f, 0.3f },
			{ -4.5f, -2.6f, 0.0f, 0.08f },
			{ 0.0f, -2.9f, 0.45f, 0.22f },
			{ 4.5f, -2.6f, 0.75f, 0.36f },
		};
		if (m_App.GetSettings().FireStage == 2)
		{
			const Spot& spot = Spots[index];
			power = spot.Power;
			ground = StageSpot(spot.Across, spot.Along);
			if (m_App.GetSettings().StageTime >= 0.0f)
				return m_App.GetSettings().StageTime - spot.Delay;
			return std::fmod(m_Time - spot.Delay + 10.0f * Period, Period);
		}
		power = int(std::floor(m_Time / Period)) % 2 == 0 ? 1.0f : 0.0f;
		ground = StageSpot(1.8f, 0.3f);
		return std::fmod(m_Time, Period);
	}

	FireballLook SessionState::StageCharge() const
	{
		FireballLook look;
		look.Power = std::min(std::fmod(m_Time / 3.0f, 1.0f) * 1.25f, 1.0f);
		look.Size = FireballShot(look.Power).Size;
		look.Center = StageSpot(-2.2f, 0.4f) + glm::vec3(0.0f, FireballHoldHeight, 0.0f);
		look.Ground = look.Center.y - FireballHoldHeight;
		look.Grounded = true;
		return look;
	}

	float SessionState::StageComet(glm::vec3& ground) const
	{
		constexpr float Period = 4.5f;
		ground = StageSpot(2.6f, -1.0f);
		if (m_App.GetSettings().StageTime >= 0.0f)
			return m_App.GetSettings().StageTime;
		return std::fmod(m_Time, Period);
	}

	float SessionState::StageSpike() const
	{
		if (m_App.GetSettings().StageTime >= 0.0f)
			return m_App.GetSettings().StageTime;
		return std::fmod(m_Time, SpikeStagePeriod);
	}

	EntityState SessionState::StageSpikeEntity() const
	{
		const glm::vec3 right = glm::normalize(glm::vec3(m_Camera.View[0][0], 0.0f, m_Camera.View[2][0]));
		const glm::vec3 at = StageSpot(1.6f, -1.2f);
		EntityState spike;
		spike.Kind = EntityKind::Effect;
		spike.Variant = uint8_t(AbilityId::EarthSpike);
		spike.NetId = 0x7F000000u + uint32_t(std::floor(m_Time / SpikeStagePeriod)) % 4096u;
		spike.Position = Vec2(at.x, at.z);
		spike.Facing = std::atan2(right.z, right.x);
		if (uint32_t(std::floor(m_Time / SpikeStagePeriod)) % 2u == 1u)
			spike.Charge = SpikeWallFlag | SpikeShrapnelFlag;
		return spike;
	}

	void SessionState::UpdateSpikeStage(float dt)
	{
		if (m_App.GetSettings().FireStage != 4 || m_App.GetSettings().StageTime >= 0.0f)
			return;
		const AbilityDef& def = GetAbility(AbilityId::EarthSpike);
		const float t = StageSpike();
		const float before = t - dt;
		const EntityState spike = StageSpikeEntity();
		const glm::vec3 base = Ground(spike.Position, 0.05f);
		const float standing = (spike.Charge & SpikeWallFlag) != 0 ? BulwarkSeconds : def.EffectDuration;
		if (before < 0.0f)
			m_Effects->Burst(Fx::SpikeTremor, base, glm::vec3(0.0f, 1.0f, 0.0f), 1.5f);
		if (t < def.CastTime)
			m_Effects->Trail(Fx::SpikeTremor, 44ull << 40, base, dt, 0.7f + 0.6f * t / def.CastTime);
		if (before < def.CastTime && t >= def.CastTime)
		{
			EruptSpike(spike);
			m_Effects->Impact(AbilityId::EarthSpike, Ground(spike.Position), spike.Facing, 1.0f, EffectColor(spike), spike.NetId, [this](const glm::vec2& point) { return GroundHeight(Vec2(point.x, point.y)); });
			Rumble(Ground(spike.Position), ImpactRumble(AbilityId::EarthSpike, 0.0f), NoPlayer);
		}
		if (before < def.CastTime + standing && t >= def.CastTime + standing)
		{
			EndSpike(spike, standing);
		}
	}

	void SessionState::DrawSpikeStage(WorldRenderer& renderer) const
	{
		const AbilityDef& def = GetAbility(AbilityId::EarthSpike);
		const float t = StageSpike();
		const EntityState spike = StageSpikeEntity();
		if (t < def.CastTime)
			DrawSpikeTelegraph(renderer, Ground(spike.Position), spike.Facing, t / def.CastTime, 1.0f, spike.NetId);
		else if (t < def.CastTime + ((spike.Charge & SpikeWallFlag) != 0 ? BulwarkSeconds : def.EffectDuration) || m_App.GetSettings().StageTime >= 0.0f)
			DrawEarthSpike(renderer, m_SpellMeshes, SpikeLook(spike, t - def.CastTime));
	}

	void SessionState::DrawFireStage(WorldRenderer& renderer) const
	{
		if (m_App.GetSettings().FireStage == 4)
		{
			DrawSpikeStage(renderer);
			return;
		}
		if (m_App.GetSettings().FireStage == 3)
		{
			glm::vec3 ground;
			const float t = StageComet(ground);
			const AbilityDef& comet = GetAbility(AbilityId::Comet);
			if (t < comet.ImpactDelay)
			{
				const float progress = t / comet.ImpactDelay;
				DrawSpellMarkFor(renderer, AbilityId::Comet, ground, comet.ImpactRadius, progress, 0.0f, 960u, 1.0f);
				DrawCometFall(renderer, ground, ground + glm::vec3(0.0f, 0.45f, 0.0f) + CometDrop * (1.0f - progress * progress), glm::normalize(-CometDrop), 1.0f, progress, 960u);
			}
			else if (m_App.GetSettings().StageTime >= 0.0f)
			{
				const float age = t - comet.ImpactDelay;
				DrawCometBlast(renderer, { .Ground = ground, .Radius = comet.ImpactRadius, .Age = age }, 960u);
				GroundDecal crater = CraterDecal(ground, comet.ImpactRadius * 1.2f, CometCraterGlow, 960u);
				crater.Params.x *= std::exp(-age * CometCraterCooling);
				renderer.DrawDecal(crater);
			}
			return;
		}
		glm::vec3 center;
		float facing = 0.0f;
		if (StageBolt(0, center, facing))
		{
			const glm::vec3 color = SkillColor(AbilityId::EmberBolt);
			DrawProjectileVisual(renderer, AbilityId::EmberBolt, center, facing, color, m_Time, 900u);
			renderer.DrawLight({ .Position = center, .Radius = 2.8f, .Color = glm::pow(color, glm::vec3(2.2f)) * (SpellLightGain * 0.7f) });
			renderer.DrawTrail(40ull << 40, center, ProjectileTrail(AbilityId::EmberBolt));
		}
		if (StageBolt(1, center, facing))
		{
			FireballLook look;
			look.Center = center;
			look.Forward = Heading(facing);
			look.Size = FireballFullSize;
			look.Power = 1.0f;
			look.Ground = center.y - 1.0f;
			look.Grounded = true;
			look.Flying = true;
			DrawPurpleFireball(renderer, look, m_Time, 901u);
			renderer.DrawTrail(41ull << 40, center, ProjectileTrail(AbilityId::PurpleFireball, 1.0f, FireballFullSize));
			renderer.DrawLight({ .Position = center, .Radius = 5.0f, .Color = FireballColor(1.0f) * (SpellLightGain * 2.0f) });
		}
		const FireballLook charge = StageCharge();
		DrawPurpleFireball(renderer, charge, m_Time, 902u);
		for (int i = 0; i < StageBlastCount(); i++)
		{
			glm::vec3 ground;
			float power = 0.0f;
			const float age = StageBlast(i, ground, power);
			const float radius = GetAbility(AbilityId::PurpleFireball).ImpactRadius + (FireballFullBlast - GetAbility(AbilityId::PurpleFireball).ImpactRadius) * power;
			DrawFireballBlast(renderer, { .Center = ground + glm::vec3(0.0f, FireballBlastHeight, 0.0f), .Ground = ground, .Radius = radius, .Power = power, .Age = age }, 903u + uint32_t(i));
			if (m_App.GetSettings().StageTime >= 0.0f && age >= 0.0f)
			{
				GroundDecal crater = FireballCraterDecal(ground, radius, power, 903u + uint32_t(i));
				crater.Params.x *= std::exp(-age * FireballCraterCooling(power));
				renderer.DrawDecal(crater);
			}
		}
	}
}
