#pragma once
#include "GameShared/Core/Math.h"
#include <cstdint>
#include <vector>

namespace Game
{
	class World;

	struct Orb
	{
		uint16_t Id = 0;
		Vec2 Position;
		Vec2 Velocity;
		uint16_t Value = 1;
		float Age = 0.0f;
		bool Drawn = false;
	};

	class OrbField
	{
	public:
		void Drop(Vec2 position, uint32_t xp, uint64_t seed);
		uint32_t Update(World& world, float dt);
		void Clear() { m_Orbs.clear(); }
		const std::vector<Orb>& GetOrbs() const { return m_Orbs; }

	private:
		void Fuse();

		std::vector<Orb> m_Orbs;
		uint16_t m_NextId = 1;
	};
}
