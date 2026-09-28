#pragma once
#include "TimbreAudio/AudioEngine.h"
#include "TimbreAudio/Internal/Commands.h"
#include "TimbreAudio/Internal/Dsp.h"
#include "TimbreAudio/Internal/SpscQueue.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace Timbre::Internal
{
	struct SoundAsset;
	struct SoundData;
	class StreamInstance;
	class Streamer;

	struct MixerConfig
	{
		uint32_t SampleRate = 48000;
		uint32_t MaxVoices = 512;
		uint32_t MaxAudibleVoices = 64;
		uint32_t MaxBuses = 32;
		uint32_t CommandCapacity = 8192;
		Timbre::Handedness Handedness = Timbre::Handedness::LeftHanded;
		float SpeedOfSound = 343.3f;
		Timbre::Interpolation Interpolation = Timbre::Interpolation::Cubic;
		Timbre::SpatialMode SpatialMode = Timbre::SpatialMode::Panning;
		float RearHighCutDb = 6.0f;
		bool MasterLimiter = true;
	};

	enum class VoiceState : uint8_t
	{
		Idle,
		Waiting,
		Playing,
		Ended,
	};

	inline uint64_t PackVoiceState(uint32_t generation, VoiceState state) { return (uint64_t(generation) << 8) | uint64_t(state); }
	inline uint32_t UnpackGeneration(uint64_t packed) { return uint32_t(packed >> 8); }
	inline VoiceState UnpackVoiceState(uint64_t packed) { return VoiceState(packed & 0xFF); }

	// Written by the mixer, read by the game thread.
	struct VoiceStatus
	{
		std::atomic<uint64_t> State = 0;
		// Playback position in source frames.
		std::atomic<uint64_t> Position = 0;
		std::atomic<float> Audibility = 0.0f;
	};

	// The audio thread's side of the engine. Render() and everything it calls runs on the audio
	// thread and never locks or allocates. Other threads talk to it through the command queue.
	class Mixer
	{
	public:
		static constexpr uint32_t BlockFrames = 512;
		static constexpr uint32_t MaxStep = 16;
		static constexpr uint32_t BinauralHistory = 64;

		Mixer(const MixerConfig& config, Streamer* streamer);
		~Mixer();
		Mixer(const Mixer&) = delete;
		Mixer& operator=(const Mixer&) = delete;

		// Mixes the next frames as interleaved stereo.
		void Render(float* output, uint32_t frames);

		SpscQueue<Command>& GetCommandQueue() { return m_Commands; }
		const VoiceStatus& GetVoiceStatus(uint32_t index) const { return m_Status[index]; }
		uint64_t GetCompletedBatch() const { return m_CompletedBatch.load(std::memory_order_acquire); }
		uint32_t GetSampleRate() const { return m_Config.SampleRate; }

		uint32_t GetAudibleVoiceCount() const { return m_AudibleCount.load(std::memory_order_relaxed); }
		uint32_t GetStreamCount() const { return m_StreamCount.load(std::memory_order_relaxed); }
		float GetCpuLoad() const { return m_CpuLoad.load(std::memory_order_relaxed); }
		uint64_t GetUnderruns() const { return m_Underruns.load(std::memory_order_relaxed); }

	private:
		struct Voice;
		struct Bus;
		struct Candidate
		{
			uint32_t Index;
			uint8_t Priority;
			float Score;
		};

		// Stream voices played in one batch that haven't started yet.
		struct StartGroup
		{
			uint32_t Id;
			uint32_t Members;
			uint32_t Ready;
		};

		static constexpr size_t MaxDyingVoices = 16;
		static constexpr size_t MaxStartGroups = 32;

		void Execute(const Command& command);
		void StartVoice(uint32_t index, uint32_t generation, const PlayCommand& play);
		void StopVoice(Voice& voice, uint32_t fadeFrames);
		void PauseVoice(Voice& voice, bool paused, uint32_t fadeFrames);
		void EndVoice(Voice& voice);
		void SetSend(Voice& voice, const SendCommand& command);
		void CreateBus(uint32_t index, uint32_t parent);
		void SetListener(const float* values);
		bool IsInBus(uint32_t bus, uint32_t ancestor) const;

		void RenderBlock(float* output, uint32_t frames);
		void UpdateBuses(uint32_t frames);
		void UpdateStartGroups();
		bool PrepareVoice(Voice& voice, uint32_t frames);
		bool NeedsLoopRestart(const Voice& voice) const;
		bool StartPlayback(Voice& voice);
		bool UpdateStream(Voice& voice);
		void PerformSeek(Voice& voice);
		void ComputeTargets(Voice& voice, const Bus& bus, uint32_t frames);
		void SetBinauralTargets(Voice& voice, float lateral) const;
		float Spatialize(const Voice& voice, float& pan, float& rear, float& airCutoff, float& doppler) const;
		bool HasStreamData(const Voice& voice, uint32_t frames) const;
		void RenderVoice(Voice& voice, uint32_t frames, bool fadeOut);
		template<bool Accumulate>
		void RenderBinaural(Voice& voice, const float* samples, float* destination, uint32_t frames, const float* target);
		void MixSends(Voice& voice, const float* wet, uint32_t frames);
		void AdvanceVoice(Voice& voice, uint32_t frames);
		void Fetch(const Voice& voice, int64_t start, uint32_t count, float* destination) const;
		void FetchStatic(const Voice& voice, int64_t start, uint32_t count, float* destination) const;
		void FetchStream(const Voice& voice, int64_t start, uint32_t count, float* destination) const;
		void Resample(const Voice& voice, const float* source, float* destination, uint32_t frames) const;
		void ApplyFilters(Voice& voice, float* samples, uint32_t frames);
		void Advance(Voice& voice, uint32_t frames);
		void MixBuses(float* output, uint32_t frames);
		void PublishStatus();
		uint64_t GetPosition(const Voice& voice) const;

		MixerConfig m_Config;
		Streamer* m_Streamer;
		SpscQueue<Command> m_Commands;

		std::vector<Voice> m_Voices;
		std::unique_ptr<VoiceStatus[]> m_Status;
		std::vector<uint32_t> m_ActiveVoices;
		std::vector<Candidate> m_Candidates;
		// Stolen voices fading out during the next block
		std::vector<Voice> m_Dying;
		std::vector<StartGroup> m_Groups;
		uint32_t m_MaxGroupWait = 12000;
		// Last batch whose commands were executed, and the running count of batches
		uint64_t m_ProcessedBatch = 0;
		uint32_t m_BatchSequence = 1;

		std::vector<Bus> m_Buses;
		// Parents before children
		std::vector<uint16_t> m_BusOrder;
		std::vector<float> m_BusBuffers;

		std::vector<float> m_SourceBuffer;
		std::vector<float> m_VoiceBuffer;
		std::vector<float> m_EffectBuffer;
		std::vector<float> m_BinauralBuffer;

		Vec3 m_ListenerPosition;
		Vec3 m_ListenerVelocity;
		Vec3 m_ListenerForward = { 0.0f, 0.0f, 1.0f };
		Vec3 m_ListenerRight = { 1.0f, 0.0f, 0.0f };
		bool m_Binaural = false;
		float m_RearCoefficient = 0.0f;

		std::unique_ptr<Limiter> m_Limiter;
		uint32_t m_DeclickFrames = 240;
		float m_LoadAverage = 0.0f;

		std::atomic<uint64_t> m_CompletedBatch = 0;
		std::atomic<uint32_t> m_AudibleCount = 0;
		std::atomic<uint32_t> m_StreamCount = 0;
		std::atomic<float> m_CpuLoad = 0.0f;
		std::atomic<uint64_t> m_Underruns = 0;
	};
}
