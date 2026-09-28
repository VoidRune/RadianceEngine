#pragma once
#include "TimbreAudio/Bus.h"
#include "TimbreAudio/Types.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace Timbre
{
	class Effect;
	class Sound;

	namespace Internal
	{
		class EngineImpl;
	}

	// Per-play settings. Volume and pitch multiply the sound's defaults, the rest override them.
	struct PlayParams
	{
		float Volume = 1.0f;
		float Pitch = 1.0f;
		// Stereo position of 2D voices, -1 is left and 1 is right.
		float Pan = 0.0f;
		std::optional<bool> Looping;
		// Output bus, the sound's bus when empty.
		Timbre::Bus Bus;
		// World position. Setting it makes the voice 3D.
		std::optional<Vec3> Position;
		// World velocity, used for the doppler effect of 3D voices.
		Vec3 Velocity;
		float FadeIn = 0.0f;
		// Seconds to wait before the voice starts.
		float Delay = 0.0f;
		// Position in the sound to start from, in seconds.
		float StartTime = 0.0f;
		bool Paused = false;
		// Called from AudioEngine::Update() when the voice ends, whether it finished or was stopped.
		std::function<void()> OnEnd;
	};

	// Handle to a playing instance of a sound. Cheap to copy. Once a voice ends its handle becomes
	// inert: setters are ignored and IsPlaying() returns false, so handles can be kept around safely.
	// Like a pointer, a const Voice can still change the voice it refers to.
	// Changes take effect on AudioEngine::Update().
	class Voice
	{
	public:
		Voice() = default;

		// True until the voice finishes or is stopped, including while it is paused or delayed.
		bool IsPlaying() const;
		bool IsPaused() const;

		void Stop(float fadeSeconds = 0.0f) const;
		void SetPaused(bool paused, float fadeSeconds = 0.0f) const;

		void SetVolume(float volume, float fadeSeconds = 0.0f) const;
		void SetPitch(float pitch) const;
		void SetPan(float pan) const;
		void SetLooping(bool looping) const;

		// Makes the voice 3D and moves it.
		void SetPosition(const Vec3& position) const;
		void SetVelocity(const Vec3& velocity) const;
		// Facing direction, used by the sound cone of the sound's SpatialSettings.
		void SetDirection(const Vec3& direction) const;

		// Built-in filters, cheaper than effects. Use the low pass to muffle occluded sounds.
		// A cutoff of 0 disables the filter.
		void SetLowPass(float cutoffHz) const;
		void SetHighPass(float cutoffHz) const;

		void SetSend(const Bus& bus, float level, float fadeSeconds = 0.0f) const;
		float GetSend(const Bus& bus) const;

		void Seek(float seconds) const;

		// Voice effects process this voice only, after panning. For shared effects like reverb use a bus.
		void AddEffect(std::shared_ptr<Effect> effect) const;
		void RemoveEffect(const std::shared_ptr<Effect>& effect) const;

		float GetVolume() const;
		float GetPitch() const;
		float GetPan() const;
		bool IsLooping() const;
		// Playback position in seconds.
		float GetTime() const;
		Sound GetSound() const;

		bool operator==(const Voice&) const = default;

	private:
		Voice(Internal::EngineImpl* engine, uint32_t index, uint32_t generation) : m_Engine(engine), m_Index(index), m_Generation(generation) {}

		Internal::EngineImpl* m_Engine = nullptr;
		uint32_t m_Index = 0;
		uint32_t m_Generation = 0;

		friend class Internal::EngineImpl;
	};
}
