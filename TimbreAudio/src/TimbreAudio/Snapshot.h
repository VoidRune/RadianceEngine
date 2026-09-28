#pragma once
#include "TimbreAudio/Bus.h"
#include <cstdint>
#include <vector>

namespace Timbre
{
	namespace Internal
	{
		class EngineImpl;
	}

	struct SnapshotBus
	{
		Timbre::Bus Bus;
		float VolumeDb = 0.0f;
		float LowPassHz = 0.0f;
	};

	struct SnapshotDesc
	{
		std::vector<SnapshotBus> Buses;
	};

	class Snapshot
	{
	public:
		Snapshot() = default;

		bool IsActive() const;
		void Stop(float fadeSeconds = 0.0f) const;
		void SetIntensity(float intensity, float fadeSeconds = 0.0f) const;
		float GetIntensity() const;

		bool operator==(const Snapshot&) const = default;

	private:
		Snapshot(Internal::EngineImpl* engine, uint32_t index, uint32_t generation) : m_Engine(engine), m_Index(index), m_Generation(generation) {}

		Internal::EngineImpl* m_Engine = nullptr;
		uint32_t m_Index = 0;
		uint32_t m_Generation = 0;

		friend class Internal::EngineImpl;
	};
}
