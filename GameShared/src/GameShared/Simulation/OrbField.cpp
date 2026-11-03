#include "OrbField.h"
#include "GameShared/Content/Shardfall.h"
#include "GameShared/Core/Random.h"
#include "GameShared/Simulation/World.h"
#include "GameShared/World/Collision.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr uint16_t OrbSizes[] = { 25, 5, 1 };
		constexpr float ScatterSpeed = 2.4f;
		constexpr float ScatterDrag = 5.0f;
		constexpr float OrbRadius = 0.1f;
		constexpr float PullAcceleration = 30.0f;
		constexpr float PullSpeed = 15.0f;
		constexpr float PickupRadius = 0.55f;
		constexpr float FuseAge = 2.0f;
		constexpr float FuseRange = 1.0f;
	}

	void OrbField::Drop(Vec2 position, uint32_t xp, uint64_t seed)
	{
		Random random(seed);
		for (const uint16_t size : OrbSizes)
			while (xp >= size)
			{
				xp -= size;
				const Vec2 heading = FromAngle(random.Float() * 2.0f * Pi);
				m_Orbs.push_back({ .Id = m_NextId, .Position = position, .Velocity = heading * (ScatterSpeed * (0.5f + 0.5f * random.Float())), .Value = size });
				m_NextId = uint16_t(m_NextId == UINT16_MAX ? 1 : m_NextId + 1);
			}
	}

	uint32_t OrbField::Update(World& world, float dt)
	{
		EntityRegistry& entities = world.GetEntities();
		std::vector<Vec2> players;
		entities.Each<PlayerController>([&](Entity entity, PlayerController& controller)
		{
			if (controller.State.Alive)
				if (const Transform* transform = entities.TryGet<Transform>(entity))
					players.push_back(transform->Position);
		});
		uint32_t collected = 0;
		for (size_t i = 0; i < m_Orbs.size();)
		{
			Orb& orb = m_Orbs[i];
			orb.Age += dt;
			const Vec2* nearest = nullptr;
			float closest = 1e30f;
			for (const Vec2& player : players)
				if (const float distance = LengthSquared(player - orb.Position); distance < closest)
				{
					closest = distance;
					nearest = &player;
				}
			if (nearest && (closest < OrbMagnet * OrbMagnet || orb.Age >= OrbRecall))
				orb.Drawn = true;
			if (orb.Drawn && nearest)
			{
				const Vec2 offset = *nearest - orb.Position;
				const float distance = Length(offset);
				const float speed = std::min(Length(orb.Velocity) + PullAcceleration * dt, PullSpeed);
				if (distance > 1e-4f)
				{
					orb.Velocity = offset * (speed / distance);
					orb.Position += offset * (std::min(speed * dt, distance) / distance);
				}
			}
			else
			{
				orb.Velocity *= std::exp(-ScatterDrag * dt);
				orb.Position = MoveCircle(world.GetLevel(), orb.Position, OrbRadius, orb.Velocity * dt);
			}
			if (nearest && LengthSquared(*nearest - orb.Position) <= PickupRadius * PickupRadius)
			{
				collected += orb.Value;
				orb = m_Orbs.back();
				m_Orbs.pop_back();
				continue;
			}
			i++;
		}
		Fuse();
		return collected;
	}

	void OrbField::Fuse()
	{
		for (size_t i = 0; i < m_Orbs.size(); i++)
		{
			Orb& keep = m_Orbs[i];
			if (keep.Drawn || keep.Age < FuseAge)
				continue;
			for (size_t j = i + 1; j < m_Orbs.size();)
			{
				const Orb& other = m_Orbs[j];
				if (other.Drawn || other.Age < FuseAge || LengthSquared(other.Position - keep.Position) >= FuseRange * FuseRange)
				{
					j++;
					continue;
				}
				const float total = float(keep.Value) + float(other.Value);
				keep.Position = (keep.Position * float(keep.Value) + other.Position * float(other.Value)) / total;
				keep.Value = uint16_t(std::min(total, 65535.0f));
				m_Orbs[j] = m_Orbs.back();
				m_Orbs.pop_back();
			}
		}
	}
}
