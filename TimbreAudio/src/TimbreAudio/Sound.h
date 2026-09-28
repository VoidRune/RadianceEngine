#pragma once
#include "TimbreAudio/Bus.h"
#include "TimbreAudio/Types.h"
#include "TimbreAudio/Voice.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Timbre
{
	namespace Internal
	{
		struct SoundData;
		class EngineImpl;
	}

	enum class LoadMode
	{
		// Decode the whole file into memory. Best for short, frequently played sounds.
		Decompress,
		// Keep the encoded file in memory and decode while playing. Saves memory for longer sounds.
		Compressed,
		// Read and decode from disk while playing. Best for music and long ambience.
		Stream,
	};

	enum class LoadState
	{
		Loading,
		Ready,
		Failed,
	};

	enum class AttenuationModel
	{
		None,
		Inverse,
		Linear,
		Exponential,
		Custom,
	};

	struct AttenuationPoint
	{
		float Distance = 0.0f;
		float Volume = 1.0f;
	};

	// What happens when a sound is played while MaxInstances of it are already playing.
	enum class InstanceLimitMode
	{
		StealOldest,
		StealQuietest,
		RejectNew,
	};

	// How 3D voices of a sound fade with distance and direction.
	struct SpatialSettings
	{
		AttenuationModel Model = AttenuationModel::Inverse;
		// Full volume up to this distance.
		float MinDistance = 1.0f;
		// Distance where attenuation stops. With the linear model the sound is silent from here on.
		float MaxDistance = 100.0f;
		float Rolloff = 1.0f;
		float DopplerFactor = 1.0f;
		// Directional sound cone in degrees, relative to the voice direction. 360 disables it.
		float ConeInnerAngle = 360.0f;
		float ConeOuterAngle = 360.0f;
		float ConeOuterVolume = 0.0f;
		std::vector<AttenuationPoint> Curve;
		float AirAbsorption = 1.0f;
	};

	struct BusSend
	{
		Timbre::Bus Bus;
		float Level = 1.0f;
	};

	struct SoundDesc
	{
		LoadMode Mode = LoadMode::Decompress;
		// Load on a background thread. Voices played before loading finishes start once it's done.
		bool Async = false;

		// Defaults for every play of this sound.
		Timbre::Bus Bus;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		bool Looping = false;
		// Random variation per play, keeps repeated sounds like footsteps from sounding mechanical.
		// Volume is scaled by a random factor in [1 - VolumeVariation, 1].
		float VolumeVariation = 0.0f;
		// Pitch is shifted by a random amount in [-PitchVariation, PitchVariation] semitones.
		float PitchVariation = 0.0f;
		// Higher priority voices stay audible and are stolen last when voices run out.
		uint8_t Priority = 128;
		// Maximum number of simultaneous plays of this sound, 0 for no limit.
		uint32_t MaxInstances = 0;
		InstanceLimitMode LimitMode = InstanceLimitMode::StealOldest;
		SpatialSettings Spatial;
		std::vector<BusSend> Sends;

		// Loop region in seconds. When negative, the file's LOOPSTART / LOOPEND / LOOPLENGTH tags
		// are used if present, otherwise the whole sound.
		float LoopStart = -1.0f;
		float LoopEnd = -1.0f;
	};

	// A loaded sound. Cheap to copy; the audio data is freed once the last copy is gone and no
	// voice plays it anymore.
	class Sound
	{
	public:
		Sound() = default;

		bool IsValid() const { return m_Data != nullptr; }
		explicit operator bool() const { return IsValid(); }
		bool operator==(const Sound&) const = default;

		Voice Play(const PlayParams& params = {}) const;
		void StopAll(float fadeSeconds = 0.0f) const;
		// Number of voices currently playing this sound.
		uint32_t GetInstanceCount() const;

		LoadState GetState() const;
		bool IsLoaded() const { return GetState() == LoadState::Ready; }
		// Length in seconds, 0 until loaded.
		float GetDuration() const;
		uint64_t GetFrameCount() const;
		uint32_t GetSampleRate() const;
		uint32_t GetChannels() const;
		LoadMode GetMode() const;
		const std::string& GetPath() const;
		const SoundDesc& GetDesc() const;

	private:
		explicit Sound(std::shared_ptr<Internal::SoundData> data) : m_Data(std::move(data)) {}

		std::shared_ptr<Internal::SoundData> m_Data;

		friend class Internal::EngineImpl;
	};
}
