#pragma once
#include "TimbreAudio/AudioEngine.h"
#include "TimbreAudio/Internal/Commands.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Timbre::Internal
{
	struct SoundAsset;
	struct SoundData;
	class Device;
	class Loader;
	class Mixer;
	class StreamInstance;
	class Streamer;

	// The game side of the engine. Owns voice slots and buses, turns API calls into commands for
	// the mixer, and frees resources only after the mixer has confirmed it no longer uses them.
	class EngineImpl
	{
	public:
		explicit EngineImpl(const AudioEngineConfig& config);
		~EngineImpl();
		EngineImpl(const EngineImpl&) = delete;
		EngineImpl& operator=(const EngineImpl&) = delete;

		void Update();

		Sound LoadSound(const std::string& path, const SoundDesc& desc);
		Sound LoadSound(std::vector<uint8_t> fileData, const SoundDesc& desc);
		Sound CreateSound(std::span<const float> samples, uint32_t channels, uint32_t sampleRate, const SoundDesc& desc);
		Voice Play(const Sound& sound, const PlayParams& params);
		void StopAll(float fadeSeconds);

		Bus GetMasterBus();
		Bus CreateBus(const std::string& name, const Bus& parent);
		Bus FindBus(std::string_view name);

		void SetListener(const Listener& listener);
		Listener GetListener() const;
		void SetSpatialMode(SpatialMode mode);
		SpatialMode GetSpatialMode() const;

		Snapshot StartSnapshot(const SnapshotDesc& desc, float fadeSeconds, float intensity);
		bool IsSnapshotActive(uint32_t index, uint32_t generation) const;
		void StopSnapshot(uint32_t index, uint32_t generation, float fadeSeconds);
		void SetSnapshotIntensity(uint32_t index, uint32_t generation, float intensity, float fadeSeconds);
		float GetSnapshotIntensity(uint32_t index, uint32_t generation) const;

		std::vector<AudioDeviceInfo> GetOutputDevices() const;
		bool SetOutputDevice(const std::string& name);
		std::string GetOutputDeviceName() const;
		uint32_t GetSampleRate() const { return m_SampleRate; }
		AudioStats GetStats() const;
		void Render(float* output, uint32_t frames);

		// Voice handles
		bool IsVoicePlaying(uint32_t index, uint32_t generation) const;
		bool IsVoicePaused(uint32_t index, uint32_t generation) const;
		void StopVoice(uint32_t index, uint32_t generation, float fadeSeconds);
		void SetVoicePaused(uint32_t index, uint32_t generation, bool paused, float fadeSeconds);
		void SetVoiceVolume(uint32_t index, uint32_t generation, float volume, float fadeSeconds);
		void SetVoicePitch(uint32_t index, uint32_t generation, float pitch);
		void SetVoicePan(uint32_t index, uint32_t generation, float pan);
		void SetVoiceLooping(uint32_t index, uint32_t generation, bool looping);
		void SetVoiceVector(uint32_t index, uint32_t generation, CommandType type, const Vec3& value);
		void SetVoiceFilter(uint32_t index, uint32_t generation, CommandType type, float cutoff);
		void SetVoiceSend(uint32_t index, uint32_t generation, const Bus& bus, float level, float fadeSeconds);
		float GetVoiceSend(uint32_t index, uint32_t generation, const Bus& bus) const;
		void SeekVoice(uint32_t index, uint32_t generation, float seconds);
		void AddVoiceEffect(uint32_t index, uint32_t generation, std::shared_ptr<Effect> effect);
		void RemoveVoiceEffect(uint32_t index, uint32_t generation, const std::shared_ptr<Effect>& effect);
		float GetVoiceVolume(uint32_t index, uint32_t generation) const;
		float GetVoicePitch(uint32_t index, uint32_t generation) const;
		float GetVoicePan(uint32_t index, uint32_t generation) const;
		bool IsVoiceLooping(uint32_t index, uint32_t generation) const;
		float GetVoiceTime(uint32_t index, uint32_t generation) const;
		Sound GetVoiceSound(uint32_t index, uint32_t generation) const;

		// Bus handles
		void SetBusVolume(uint32_t index, float volume, float fadeSeconds);
		float GetBusVolume(uint32_t index) const;
		void SetBusMuted(uint32_t index, bool muted);
		bool IsBusMuted(uint32_t index) const;
		void SetBusPaused(uint32_t index, bool paused);
		bool IsBusPaused(uint32_t index) const;
		void SetBusPitch(uint32_t index, float pitch);
		float GetBusPitch(uint32_t index) const;
		void SetBusLowPass(uint32_t index, float cutoff, float fadeSeconds);
		float GetBusLowPass(uint32_t index) const;
		void SetBusDucking(uint32_t index, const DuckingSettings& settings);
		void AddBusEffect(uint32_t index, std::shared_ptr<Effect> effect);
		void RemoveBusEffect(uint32_t index, const std::shared_ptr<Effect>& effect);
		void ClearBusEffects(uint32_t index);
		void StopBus(uint32_t index, float fadeSeconds);
		std::string GetBusName(uint32_t index) const;
		Bus GetBusParent(uint32_t index) const;

		// Sound handles
		void StopSound(SoundData& sound, float fadeSeconds);
		uint32_t GetInstanceCount(const SoundData& sound) const;

	private:
		struct SendRecord
		{
			uint16_t Bus = 0;
			float Level = 0.0f;
		};

		struct VoiceSlot
		{
			uint32_t Generation = 0;
			bool Active = false;
			bool Stopping = false;
			bool Paused = false;
			bool Looping = false;
			uint8_t Priority = 128;
			uint16_t Bus = 0;
			float Volume = 1.0f;
			float Pitch = 1.0f;
			float Pan = 0.0f;
			uint64_t Order = 0;
			SendRecord Sends[MaxVoiceSends];
			uint32_t SendCount = 0;
			std::shared_ptr<SoundData> Sound;
			std::shared_ptr<StreamInstance> Stream;
			std::vector<std::shared_ptr<Effect>> Effects;
			std::shared_ptr<EffectChain> Chain;
			std::function<void()> OnEnd;
		};

		struct BusRecord
		{
			std::string Name;
			uint32_t Parent = 0;
			float Volume = 1.0f;
			bool Muted = false;
			bool Paused = false;
			float Pitch = 1.0f;
			float LowPass = 0.0f;
			std::vector<std::shared_ptr<Effect>> Effects;
			std::shared_ptr<EffectChain> Chain;
		};

		struct SnapshotEntry
		{
			uint16_t Bus = 0;
			float VolumeDb = 0.0f;
			float LowPassHz = 0.0f;
		};

		struct SnapshotSlot
		{
			uint32_t Generation = 0;
			bool Active = false;
			float Intensity = 1.0f;
			std::vector<SnapshotEntry> Entries;
		};

		// Released once the mixer has processed the batch it was retired in.
		struct Retired
		{
			uint64_t Batch = 0;
			std::shared_ptr<void> Object;
			std::function<void()> Action;
		};

		static void DeviceCallback(void* userData, float* output, uint32_t frames);

		VoiceSlot* FindSlot(uint32_t index, uint32_t generation);
		const VoiceSlot* FindSlot(uint32_t index, uint32_t generation) const;
		BusRecord* GetBusRecord(uint32_t index);
		const BusRecord* GetBusRecord(uint32_t index) const;
		bool IsInBus(uint32_t bus, uint32_t ancestor) const;
		uint16_t ResolveBus(const Bus& bus) const;

		void PushParam(CommandType type, uint32_t index, uint32_t generation, float value, uint32_t frames = 0, bool flag = false);
		void PushVector(CommandType type, uint32_t index, uint32_t generation, const Vec3& value);
		void PushEffects(CommandType type, uint32_t index, uint32_t generation, EffectChain* chain);
		void Publish();
		void Retire(std::shared_ptr<void> object, std::function<void()> action = {});
		void ReleaseRetired();
		uint32_t ToFrames(float seconds) const;
		// Logs a warning the first time a NaN or infinite value is passed in. Returns finite.
		bool CheckFinite(bool finite) const;

		bool LoadAsset(SoundAsset& asset);
		Sound RegisterSound(std::shared_ptr<SoundAsset> asset, const SoundDesc& desc, bool newAsset);

		bool AttachEffect(const std::shared_ptr<Effect>& effect);
		void DetachEffectLater(const std::shared_ptr<Effect>& effect);
		std::shared_ptr<EffectChain> BuildChain(const std::vector<std::shared_ptr<Effect>>& effects) const;
		void UpdateVoiceChain(uint32_t index, VoiceSlot& slot);
		void UpdateBusChain(uint32_t index, BusRecord& bus);
		void ApplySend(uint32_t index, VoiceSlot& slot, uint16_t bus, float level, uint32_t fadeFrames);
		SnapshotSlot* FindSnapshot(uint32_t index, uint32_t generation);
		const SnapshotSlot* FindSnapshot(uint32_t index, uint32_t generation) const;
		void ApplySnapshots(const std::vector<SnapshotEntry>& entries, uint32_t fadeFrames);

		int64_t AllocateSlot(uint8_t priority);
		void StealSlot(uint32_t index);
		void ReleaseSlot(uint32_t index, std::vector<std::function<void()>>& callbacks);
		void StopSlot(uint32_t index, float fadeSeconds);
		float GetAudibility(uint32_t index) const;
		// The mixer finished the voice in this slot, Update() just hasn't released it yet.
		bool HasEnded(uint32_t index) const;
		float RandomFloat();

		mutable std::mutex m_Mutex;
		AudioEngineConfig m_Config;
		std::shared_ptr<AudioFileSystem> m_FileSystem;
		uint32_t m_SampleRate = 0;
		std::unique_ptr<Streamer> m_Streamer;
		std::unique_ptr<Mixer> m_Mixer;
		std::unique_ptr<Device> m_Device;
		std::unique_ptr<Loader> m_Loader;
		std::atomic<Mixer*> m_RenderMixer = nullptr;

		std::vector<VoiceSlot> m_Slots;
		std::vector<uint32_t> m_FreeSlots;
		std::vector<uint32_t> m_ActiveSlots;
		uint64_t m_PlayCounter = 0;
		std::vector<BusRecord> m_Buses;
		std::vector<SnapshotSlot> m_Snapshots;
		SpatialMode m_SpatialMode = SpatialMode::Panning;

		std::vector<Command> m_Pending;
		uint64_t m_NextBatch = 1;
		std::vector<Retired> m_Retired;
		std::vector<std::function<void()>> m_PendingCallbacks;
		bool m_QueueFullWarned = false;
		uint64_t m_LogToken = 0;
		mutable std::atomic<bool> m_WarnedNonFinite = false;

		std::unordered_map<std::string, std::weak_ptr<SoundAsset>> m_AssetCache;
		std::vector<std::weak_ptr<SoundAsset>> m_Assets;
		size_t m_PruneAssetsAt = 64;
		Listener m_Listener;
		uint64_t m_RandomState = 0x853c49e6748fea9bULL;
	};
}
