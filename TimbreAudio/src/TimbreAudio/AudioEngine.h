#pragma once
#include "TimbreAudio/Bus.h"
#include "TimbreAudio/Effects.h"
#include "TimbreAudio/FileSystem.h"
#include "TimbreAudio/Snapshot.h"
#include "TimbreAudio/Sound.h"
#include "TimbreAudio/Types.h"
#include "TimbreAudio/Voice.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Timbre
{
	// Which way the listener's right is, given its forward and up vectors.
	enum class Handedness
	{
		LeftHanded,
		RightHanded,
	};

	enum class Interpolation
	{
		Linear,
		Cubic,
	};

	enum class SpatialMode
	{
		Panning,
		Binaural,
	};

	struct AudioEngineConfig
	{
		// Mixing sample rate. 0 uses the output device's rate, which avoids resampling in the driver.
		uint32_t SampleRate = 0;
		// Voices that can play at once. Beyond that the least important voice is stolen.
		uint32_t MaxVoices = 512;
		// Voices that are mixed at once. The quietest and lowest priority voices beyond this keep
		// playing silently (virtualized) until they become important enough again.
		uint32_t MaxAudibleVoices = 64;
		uint32_t MaxBuses = 32;
		// Decoded audio buffered per streaming voice.
		float StreamBufferSeconds = 0.5f;
		// Device buffer length. 0 uses the backend default (around 10 ms).
		uint32_t BufferSizeMs = 0;
		// Output device name from GetOutputDevices(). Empty uses the system default and follows it when it changes.
		std::string OutputDevice;
		// Don't open an audio device; pull the mix with AudioEngine::Render() instead. For tools, tests and servers.
		bool NoOutput = false;
		Timbre::Handedness Handedness = Timbre::Handedness::LeftHanded;
		// World units per second, for the doppler effect.
		float SpeedOfSound = 343.3f;
		Timbre::Interpolation Interpolation = Timbre::Interpolation::Cubic;
		Timbre::SpatialMode SpatialMode = Timbre::SpatialMode::Panning;
		float RearHighCutDb = 6.0f;
		// Keeps the final mix from clipping when many loud sounds play at once.
		bool MasterLimiter = true;
		uint32_t LoaderThreads = 0;
		// Receives log messages. By default they are printed to stdout.
		LogCallback Log;
		// Where sound files are read from. By default they are read from disk.
		std::shared_ptr<AudioFileSystem> FileSystem;
	};

	struct Listener
	{
		Vec3 Position;
		Vec3 Forward = { 0.0f, 0.0f, 1.0f };
		Vec3 Up = { 0.0f, 1.0f, 0.0f };
		Vec3 Velocity;
	};

	struct AudioStats
	{
		uint32_t PlayingVoices = 0;
		uint32_t AudibleVoices = 0;
		uint32_t VirtualVoices = 0;
		uint32_t Streams = 0;
		// Fraction of the audio thread's time budget spent mixing.
		float CpuLoad = 0.0f;
		// Times a stream could not decode fast enough and had to pause.
		uint64_t StreamUnderruns = 0;
		// Memory used by loaded sounds.
		uint64_t SoundMemory = 0;
		uint32_t SampleRate = 0;
	};

	struct AudioDeviceInfo
	{
		std::string Name;
		bool IsDefault = false;
	};

	// The audio engine. Create one, load sounds, play them, and call Update() once per frame.
	// All functions are thread-safe. Changes made during a frame are applied together on Update(),
	// so a voice started and positioned in the same frame starts at the right place.
	// Sounds, voices and buses must not be used after the engine is destroyed.
	class AudioEngine
	{
	public:
		explicit AudioEngine(const AudioEngineConfig& config = {});
		~AudioEngine();
		AudioEngine(const AudioEngine&) = delete;
		AudioEngine& operator=(const AudioEngine&) = delete;

		// Applies all changes since the last call and runs OnEnd callbacks of finished voices.
		void Update();

		// Loads a sound file. Ogg Vorbis is the main format; WAV, FLAC and MP3 work too. Loading the
		// same path with the same mode again shares the audio data. Returns an empty Sound on failure.
		Sound LoadSound(const std::string& path, const SoundDesc& desc = {});
		// Loads a sound from an encoded file in memory. Stream mode decodes from the memory while playing.
		Sound LoadSound(std::vector<uint8_t> fileData, const SoundDesc& desc = {});
		// Creates a sound from interleaved samples in [-1, 1], for procedural audio.
		Sound CreateSound(std::span<const float> samples, uint32_t channels, uint32_t sampleRate, const SoundDesc& desc = {});

		Voice Play(const Sound& sound, const PlayParams& params = {});
		void StopAll(float fadeSeconds = 0.0f);

		Bus GetMasterBus() const;
		Bus CreateBus(const std::string& name, Bus parent = {});
		Bus FindBus(std::string_view name) const;

		void SetListener(const Listener& listener);
		Listener GetListener() const;
		void SetSpatialMode(SpatialMode mode);
		SpatialMode GetSpatialMode() const;

		Snapshot StartSnapshot(const SnapshotDesc& desc, float fadeSeconds = 0.0f, float intensity = 1.0f);

		std::vector<AudioDeviceInfo> GetOutputDevices() const;
		// Switches the output device. Empty uses the system default. Returns false on failure.
		bool SetOutputDevice(const std::string& name);
		std::string GetOutputDeviceName() const;

		uint32_t GetSampleRate() const;
		AudioStats GetStats() const;

		// Only with AudioEngineConfig::NoOutput: mixes the next frames into output as interleaved stereo.
		void Render(float* output, uint32_t frames);

	private:
		std::unique_ptr<Internal::EngineImpl> m_Impl;
	};
}
