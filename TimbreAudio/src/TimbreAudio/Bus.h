#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace Timbre
{
	class Effect;
	struct DuckingSettings;

	namespace Internal
	{
		class EngineImpl;
	}

	// A mixer group such as "Music", "SFX" or "UI". Voices play into a bus, buses mix into their
	// parent and everything ends up in the master bus. Changes take effect on AudioEngine::Update().
	// A default constructed Bus is empty; where a bus is optional, empty means the default.
	class Bus
	{
	public:
		Bus() = default;

		bool IsValid() const { return m_Engine != nullptr; }
		explicit operator bool() const { return IsValid(); }
		bool operator==(const Bus&) const = default;

		void SetVolume(float volume, float fadeSeconds = 0.0f) const;
		float GetVolume() const;
		void SetMuted(bool muted) const;
		bool IsMuted() const;
		// Pauses every voice in this bus and its children, like the game world while a menu is open.
		void SetPaused(bool paused) const;
		bool IsPaused() const;
		// Playback speed of every voice in this bus and its children, for slow motion.
		void SetPitch(float pitch) const;
		float GetPitch() const;
		void SetLowPass(float cutoffHz, float fadeSeconds = 0.0f) const;
		float GetLowPass() const;
		void SetDucking(const DuckingSettings& settings) const;
		void ClearDucking() const;

		// Effects run in the order they are added, before the bus volume is applied.
		void AddEffect(std::shared_ptr<Effect> effect) const;
		void RemoveEffect(const std::shared_ptr<Effect>& effect) const;
		void ClearEffects() const;

		// Stops every voice playing in this bus and its children.
		void StopAll(float fadeSeconds = 0.0f) const;

		std::string GetName() const;
		Bus GetParent() const;

	private:
		Bus(Internal::EngineImpl* engine, uint32_t index) : m_Engine(engine), m_Index(index) {}

		Internal::EngineImpl* m_Engine = nullptr;
		uint32_t m_Index = 0;

		friend class Internal::EngineImpl;
	};

	struct DuckingSettings
	{
		Bus Trigger;
		float VolumeDb = -12.0f;
		float ThresholdDb = -40.0f;
		float AttackSeconds = 0.05f;
		float ReleaseSeconds = 0.6f;
	};
}
