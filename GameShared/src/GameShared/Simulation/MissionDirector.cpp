#include "MissionDirector.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game
{
	namespace
	{
		constexpr float BossKillShare = 0.7f;
		constexpr float StartClearance = 16.0f;
		constexpr float PackSpacing = 7.0f;
		constexpr float PackRadius = 1.8f;

		EntityKind PickKind(Random& random, uint32_t tier)
		{
			struct Weight
			{
				EntityKind Kind;
				uint32_t Weight;
				uint32_t MinTier;
			};
			constexpr Weight Weights[] = {
				{ EntityKind::Chaser, 50, 1 },
				{ EntityKind::Shooter, 25, 1 },
				{ EntityKind::Brute, 10, 2 },
				{ EntityKind::Caster, 15, 3 },
			};
			uint32_t total = 0;
			for (const Weight& weight : Weights)
				total += tier >= weight.MinTier ? weight.Weight : 0;
			uint32_t roll = random.Below(std::max(total, 1u));
			for (const Weight& weight : Weights)
			{
				if (tier < weight.MinTier)
					continue;
				if (roll < weight.Weight)
					return weight.Kind;
				roll -= weight.Weight;
			}
			return EntityKind::Chaser;
		}

		EntityKind BossFor(MapKind map)
		{
			return map == MapKind::Caves ? EntityKind::Wyrm : EntityKind::Archivist;
		}
	}

	void MissionDirector::Begin(World& world)
	{
		const WorldSettings& settings = world.GetSettings();
		m_Revives = uint8_t(3 + std::min(settings.Players, 4u) - 1);
		SpawnPacks(world);
		m_Goal = std::max(1u, uint32_t(std::ceil(float(m_Spawned) * BossKillShare)));
	}

	void MissionDirector::SpawnPacks(World& world)
	{
		const WorldSettings& settings = world.GetSettings();
		const Level& level = world.GetLevel();
		Random& random = world.GetRandom();
		const uint32_t tier = std::clamp<uint32_t>(settings.Tier, 1u, MaxMissionTier);
		const uint32_t monsterLevel = MonsterLevelForTier(tier);
		const uint32_t players = std::max(settings.Players, 1u);
		float density = (settings.Map == MapKind::Caves ? 40.0f : 64.0f) * (1.0f + 0.04f * float(tier - 1)) * (1.0f + 0.3f * float(players - 1));
		if (HasModifier(settings.Modifiers, MissionModifier::Teeming))
			density *= 1.4f;
		const uint32_t target = uint32_t(std::lround(density));
		const float eliteChance = std::min((0.12f + 0.01f * float(tier)) * (HasModifier(settings.Modifiers, MissionModifier::Champions) ? 2.0f : 1.0f), 0.6f);

		FlowField reach;
		const Vec2 start[] = { level.MissionStart };
		reach.Build(level, world.GetNavGrid(), NarrowBodyRadius, start);
		float totalArea = 0.0f;
		for (const Region& region : level.Regions)
			if (region.EnemySpawns)
				totalArea += region.GetArea();
		if (totalArea <= 0.0f)
			return;

		std::vector<Vec2> centers;
		uint16_t pack = 0;
		for (uint32_t attempt = 0; attempt < 4000 && m_Spawned < target; attempt++)
		{
			float pick = random.Float() * totalArea;
			const Region* chosen = nullptr;
			for (const Region& region : level.Regions)
			{
				if (!region.EnemySpawns)
					continue;
				chosen = &region;
				pick -= region.GetArea();
				if (pick <= 0.0f)
					break;
			}
			const Vec2 center = { chosen->Min.x + random.Float() * (chosen->Max.x - chosen->Min.x), chosen->Min.y + random.Float() * (chosen->Max.y - chosen->Min.y) };
			if (!IsFree(level, center, 0.8f) || Length(center - level.MissionStart) < StartClearance)
				continue;
			if (level.BossArenaRadius > 0.0f && Length(center - level.BossArena) < level.BossArenaRadius + 3.0f)
				continue;
			if (!std::isfinite(reach.GetTravelDistance(center)))
				continue;
			if (std::any_of(centers.begin(), centers.end(), [&](Vec2 other) { return LengthSquared(other - center) < PackSpacing * PackSpacing; }))
				continue;
			centers.push_back(center);
			pack++;
			const uint32_t size = 3 + random.Below(3) + (tier >= 5 ? 1 : 0);
			const bool elitePack = random.Float() < eliteChance;
			for (uint32_t member = 0; member < size && m_Spawned < target + 4; member++)
			{
				const EntityKind kind = member == 0 && elitePack && tier >= 2 && random.Below(3) == 0 ? EntityKind::Brute : PickKind(random, tier);
				const EnemyDef* def = FindEnemyDef(kind);
				const bool elite = elitePack && member == 0;
				const float radius = def->Radius * (elite ? EliteSize : 1.0f);
				const Vec2 offset = FromAngle(random.Float() * 2.0f * Pi) * (PackRadius * random.Float());
				const Vec2 position = world.FindFreeSpotNear(center + offset, radius);
				if (!std::isfinite(reach.GetTravelDistance(position)))
					continue;
				const Entity entity = world.SpawnEnemy(kind, position, GetMonsterScaling(kind, monsterLevel, players, elite, settings.Modifiers));
				if (Enemy* enemy = world.GetEntities().TryGet<Enemy>(entity))
				{
					enemy->Awake = false;
					enemy->Pack = pack;
					enemy->Home = position;
					enemy->Wander = random.Float() * 3.0f;
				}
				m_Spawned++;
			}
		}
	}

	void MissionDirector::SetPhase(MissionPhase phase)
	{
		m_Phase = phase;
		m_PhaseTime = 0.0f;
	}

	void MissionDirector::Update(World& world, float dt)
	{
		m_PhaseTime += dt;
		if (m_Phase == MissionPhase::Victory || m_Phase == MissionPhase::Failed)
			return;
		if (m_Phase == MissionPhase::Explore && m_Kills >= m_Goal)
			AwakenBoss(world);
		uint32_t players = 0;
		uint32_t alive = 0;
		world.GetEntities().Each<PlayerController>([&](Entity, PlayerController& controller)
		{
			players++;
			alive += controller.State.Alive ? 1 : 0;
		});
		if (players > 0 && alive == 0 && m_Revives == 0)
		{
			SetPhase(MissionPhase::Failed);
			world.Emit({ WorldEventType::MissionFailed });
		}
	}

	void MissionDirector::AwakenBoss(World& world)
	{
		const WorldSettings& settings = world.GetSettings();
		const Level& level = world.GetLevel();
		const EntityKind kind = BossFor(settings.Map);
		const EnemyDef* def = FindEnemyDef(kind);
		const Vec2 position = world.FindFreeSpotNear(level.BossArenaRadius > 0.0f ? level.BossArena : level.PlayerSpawn, def ? def->Radius : 1.0f);
		m_Boss = world.SpawnBoss(kind, position, MonsterLevelForTier(settings.Tier) + 2);
		const Replicated* replicated = world.GetEntities().TryGet<Replicated>(m_Boss);
		m_BossNetId = replicated ? replicated->NetId : 0;
		SetPhase(MissionPhase::Boss);
		world.Emit({ WorldEventType::BossAwakened, NoPlayer, uint8_t(kind) });
	}

	void MissionDirector::OnKilled(World& world, Vec2 position, bool boss)
	{
		if (!boss)
		{
			m_Kills++;
			return;
		}
		if (m_Phase == MissionPhase::Boss)
			GrantVictory(world, position);
	}

	void MissionDirector::GrantVictory(World& world, Vec2 position)
	{
		const WorldSettings& settings = world.GetSettings();
		const MissionRewards rewards = world.GetRewards();
		const uint32_t level = std::min(MonsterLevelForTier(settings.Tier) + 2, MaxItemLevel);
		Random& random = world.GetRandom();
		SetPhase(MissionPhase::Victory);
		m_BossNetId = 0;
		m_Boss = {};

		world.GetEntities().Each<PlayerController>([&](Entity player, PlayerController& controller)
		{
			MaterialStock materials{};
			const float scale = (1.0f + 0.15f * float(level - 1)) * rewards.Quantity;
			materials[size_t(Material::Gold)] = uint32_t(std::lround(400.0f * scale * (0.9f + 0.2f * random.Float())));
			materials[size_t(Material::Shard)] = 4 + random.Below(3);
			materials[size_t(Material::Starsteel)] = 1 + settings.Tier / 3 + (random.Below(2) == 0 ? 1 : 0);
			materials[size_t(Material::ArcaneDust)] = 3 + random.Below(3);
			materials[size_t(Material::VoidShard)] = random.Float() < 0.25f * rewards.Quantity ? 1 : 0;
			world.GrantMaterials(player, materials);
			for (uint32_t i = 0; i < 2; i++)
			{
				const Vec2 spot = world.PlaceDrop(position, position + FromAngle(random.Float() * 2.0f * Pi) * (1.0f + random.Float()));
				world.SpawnItem(GenerateItem(random, world.AllocateItemId(), level, 0.3f + rewards.Rarity, Rarity::Rare), spot, NoPlayer, controller.PlayerId);
			}
			const Vec2 spot = world.PlaceDrop(position, position + FromAngle(random.Float() * 2.0f * Pi) * 1.4f);
			world.SpawnItem(GenerateGem(random, world.AllocateItemId(), level + 6), spot, NoPlayer, controller.PlayerId);
		});

		Chest chest;
		chest.Level = level;
		chest.RarityBonus = 0.5f + rewards.Rarity;
		chest.Quantity = rewards.Quantity;
		chest.Items = uint8_t(2 + CountModifiers(settings.Modifiers) / 2);
		const Level& map = world.GetLevel();
		const Vec2 toStart = Normalize(map.MissionStart - position);
		world.SpawnChest(world.FindFreeSpotNear(position + Perpendicular(toStart) * 2.5f, 0.8f), chest);
		world.SpawnGateway(world.FindFreeSpotNear(position + toStart * 3.5f, 1.0f));
		world.Emit({ WorldEventType::BossDefeated });
	}

	bool MissionDirector::ConsumeRevive()
	{
		if (m_Phase == MissionPhase::Victory)
			return true;
		if (m_Revives == 0 || m_Phase == MissionPhase::Failed)
			return false;
		m_Revives--;
		return true;
	}

	void MissionDirector::Describe(const World& world, GameStatus& status) const
	{
		status.Mission = m_Phase;
		status.Kills = uint16_t(std::min<uint32_t>(m_Kills, UINT16_MAX));
		status.KillGoal = uint16_t(std::min<uint32_t>(m_Goal, UINT16_MAX));
		status.Revives = m_Revives;
		status.BossNetId = m_BossNetId;
		status.MissionTime = m_PhaseTime;
		status.BossHealth = 0;
		if (const Health* health = world.GetEntities().TryGet<Health>(m_Boss))
			status.BossHealth = uint8_t(std::clamp((health->Current * 255 + health->Max - 1) / std::max(health->Max, 1), 0, 255));
	}
}
