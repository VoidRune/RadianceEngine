#include "GameShared/World/Level.h"
#include "GameShared/World/LevelBuilder.h"
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int32_t HollowSize = 64;
		constexpr Vec2 Center = { 32.0f, 32.0f };
		constexpr float FloorRadius = 28.0f;
		constexpr float RoadRadius = 15.0f;
		constexpr float RoadWidth = 3.0f;
		constexpr float ClusterReach = 9.0f;
		constexpr Vec2 BossRing = { 32.0f, 49.0f };
		constexpr float BossRingRadius = 10.0f;
		constexpr float SocketRadius = 25.5f;
		constexpr TileRect Vault = { 52, 3, 9, 6 };
		constexpr TileRect VaultPassage = { 53, 9, 3, 6 };
		constexpr TileRect Sealed = { 52, 3, 9, 8 };

		Vec2 Around(Vec2 origin, float radius, float angle)
		{
			return origin + FromAngle(angle) * radius;
		}

		void Ring(LevelBuilder& builder, Vec2 origin, float radius, float width, TerrainId terrain)
		{
			constexpr int32_t Segments = 64;
			for (int32_t i = 0; i < Segments; i++)
				builder.Stroke(Around(origin, radius, 2.0f * Pi * float(i) / Segments), Around(origin, radius, 2.0f * Pi * float(i + 1) / Segments), width, terrain);
		}

		void Cluster(LevelBuilder& builder, Vec2 middle, float turn)
		{
			builder.Prop(PropId::MossBoulder, middle, turn);
			builder.Prop(PropId::StonePillar, Around(middle, 1.47f, turn));
			builder.Prop(PropId::StonePillar, Around(middle, 1.47f, turn + 2.27f));
			builder.Prop(PropId::Crystal, Around(middle, 1.3f, turn + 4.36f), turn);
			builder.Reserve(middle, 2.6f);
		}
	}

	Level BuildProvingHollowLevel(uint32_t seed)
	{
		LevelBuilder builder(MapKind::ProvingHollow, seed, HollowSize, HollowSize, TerrainId::Rock);
		builder.Disc(Center, FloorRadius, TerrainId::Grass);
		Ring(builder, Center, RoadRadius, RoadWidth, TerrainId::Dirt);
		builder.Disc(BossRing, BossRingRadius - 0.6f, TerrainId::Dirt);
		Ring(builder, BossRing, BossRingRadius - 0.3f, 1.2f, TerrainId::Cobblestone);
		builder.Reserve(BossRing, BossRingRadius);
		builder.SetBossArena(BossRing, BossRingRadius);
		builder.SetPlayerSpawn({ 32.0f, 11.0f });

		for (int32_t i = 0; i < 4; i++)
		{
			const float angle = Pi * 0.25f + float(i) * Pi * 0.5f;
			Cluster(builder, Around(Center, ClusterReach, angle), angle + 0.9f * float(i));
		}

		builder.Fill(Vault, TerrainId::Dirt);
		builder.Fill(VaultPassage, TerrainId::Dirt);
		builder.Reserve(Vault);
		builder.Reserve(VaultPassage);
		builder.Prop(PropId::IronGate, { 54.5f, 12.2f });
		builder.Wall(WallId::IronFence, { 53.0f, 11.5f }, { 56.0f, 11.5f });
		for (const Vec2 spot : { Vec2(53.3f, 4.0f), Vec2(59.7f, 4.0f), Vec2(57.6f, 7.4f) })
			builder.Prop(PropId::Crystal, spot);

		for (int32_t i = 0; i < 12; i++)
			builder.Socket(Around(Center, SocketRadius, Pi * (-0.5f + 1.0f / 12.0f) + float(i) * Pi / 6.0f));

		for (const float angle : { -2.8f, -0.34f, 0.64f, 2.5f })
			builder.Prop(PropId::Crystal, Around(BossRing, BossRingRadius + 0.9f, angle), angle);
		for (int32_t i = 0; i < 8; i++)
			builder.TryProp(PropId::Crystal, Around(Center, 20.5f + 1.5f * float(i % 2), Pi * 0.125f + float(i) * Pi * 0.25f), float(i), 1.5f);
		Random& random = builder.GetRandom();
		for (int32_t placed = 0, attempt = 0; placed < 40 && attempt < 400; attempt++)
		{
			const Vec2 spot = Around(Center, 4.0f + 23.0f * std::sqrt(random.Float()), random.Float() * 2.0f * Pi);
			const PropId decoration = attempt % 3 == 0 ? PropId::Mushrooms : attempt % 3 == 1 ? PropId::Flowers : PropId::GrassTuft;
			placed += builder.TryProp(decoration, spot, random.Float() * 2.0f * Pi, 0.4f) ? 1 : 0;
		}

		builder.Region("Proving Hollow", { 4, 4, 56, 56 });
		builder.Region("Sealed Vault", Sealed, false);
		return builder.Finish();
	}
}
