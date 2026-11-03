#pragma once
#include "GameShared/Net/Protocol.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Game
{
	struct NetMotion
	{
		int32_t X = 0;
		int32_t Y = 0;
		int32_t VelocityX = 0;
		int32_t VelocityY = 0;
		uint32_t Tick = 0;
	};

	struct NetBody
	{
		uint32_t NetId = 0;
		EntityKind Kind = EntityKind::Player;
		uint8_t Owner = UINT8_MAX;
		uint16_t Tag = 0;
		uint8_t Variant = 0;
		uint8_t Scale = 16;
		uint8_t Health = 255;
		uint32_t Flags = 0;
		uint8_t Charge = 0;
		uint8_t Ward = 0;
		uint16_t CastX = 0;
		uint16_t CastY = 0;
		uint16_t CastAim = 0;
		uint16_t Facing = 0;
		NetMotion Motion;
	};

	struct NetOrb
	{
		uint16_t Id = 0;
		uint16_t Value = 0;
		NetMotion Motion;
	};

	struct SnapshotFrame
	{
		bool Valid = false;
		uint32_t Epoch = 0;
		uint32_t Tick = 0;
		std::vector<uint8_t> Block;
		std::vector<NetBody> Bodies;
		std::vector<NetOrb> Orbs;
	};

	void MergeHits(std::vector<HitEvent>& hits);

	class SnapshotEncoder
	{
	public:
		void Reset();
		void Acknowledge(uint32_t tick) { m_Acked = tick; }
		std::vector<uint8_t> Encode(const ServerSnapshot& snapshot, Vec2 focus);

	private:
		struct Sample
		{
			uint32_t Id = 0;
			int32_t X = 0;
			int32_t Y = 0;
		};

		static constexpr uint32_t Depth = 32;

		std::array<SnapshotFrame, Depth> m_Frames;
		uint32_t m_Acked = 0;
		uint32_t m_SampleTick = 0;
		std::vector<Sample> m_Bodies;
		std::vector<Sample> m_Orbs;
		std::vector<Sample> m_NextBodies;
		std::vector<Sample> m_NextOrbs;
	};

	class SnapshotDecoder
	{
	public:
		void Reset();
		bool Decode(std::span<const uint8_t> data, ServerSnapshot& snapshot);
		uint32_t GetLatestTick() const { return m_Latest; }
		uint32_t GetFailures() const { return m_Failures; }

	private:
		bool Fail();

		static constexpr uint32_t Depth = 64;

		std::array<SnapshotFrame, Depth> m_Frames;
		uint32_t m_Latest = 0;
		uint32_t m_LatestEpoch = 0;
		uint32_t m_Failures = 0;
	};
}
