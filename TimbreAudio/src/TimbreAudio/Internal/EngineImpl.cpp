#include "EngineImpl.h"
#include "Decoder.h"
#include "Device.h"
#include "Loader.h"
#include "Log.h"
#include "Mixer.h"
#include "SoundAsset.h"
#include "Streamer.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace Timbre::Internal
{
	namespace
	{
		void FinishLoading(SoundAsset& asset, LoadState state)
		{
			asset.State.store(state, std::memory_order_release);
			asset.State.notify_all();
		}

		bool IsFinite(const Vec3& v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
		}

		// NaN or infinity in a sound's settings falls back to the defaults.
		SoundDesc SanitizeDesc(SoundDesc desc)
		{
			const auto finite = [](float& value, float fallback)
			{
				if (!std::isfinite(value))
					value = fallback;
			};
			const SoundDesc defaults;
			finite(desc.Volume, defaults.Volume);
			finite(desc.Pitch, defaults.Pitch);
			finite(desc.VolumeVariation, defaults.VolumeVariation);
			finite(desc.PitchVariation, defaults.PitchVariation);
			finite(desc.LoopStart, defaults.LoopStart);
			finite(desc.LoopEnd, defaults.LoopEnd);
			finite(desc.Spatial.MinDistance, defaults.Spatial.MinDistance);
			finite(desc.Spatial.MaxDistance, defaults.Spatial.MaxDistance);
			finite(desc.Spatial.Rolloff, defaults.Spatial.Rolloff);
			finite(desc.Spatial.DopplerFactor, defaults.Spatial.DopplerFactor);
			finite(desc.Spatial.ConeInnerAngle, defaults.Spatial.ConeInnerAngle);
			finite(desc.Spatial.ConeOuterAngle, defaults.Spatial.ConeOuterAngle);
			finite(desc.Spatial.ConeOuterVolume, defaults.Spatial.ConeOuterVolume);
			finite(desc.Spatial.AirAbsorption, defaults.Spatial.AirAbsorption);
			desc.Volume = std::max(desc.Volume, 0.0f);
			desc.Pitch = std::max(desc.Pitch, 0.0f);
			desc.Spatial.AirAbsorption = std::max(desc.Spatial.AirAbsorption, 0.0f);

			std::erase_if(desc.Spatial.Curve, [](const AttenuationPoint& point) { return !std::isfinite(point.Distance) || !std::isfinite(point.Volume); });
			for (AttenuationPoint& point : desc.Spatial.Curve)
				point.Volume = std::max(point.Volume, 0.0f);
			std::stable_sort(desc.Spatial.Curve.begin(), desc.Spatial.Curve.end(), [](const AttenuationPoint& a, const AttenuationPoint& b) { return a.Distance < b.Distance; });

			std::erase_if(desc.Sends, [](const BusSend& send) { return !send.Bus || !std::isfinite(send.Level); });
			for (BusSend& send : desc.Sends)
				send.Level = std::max(send.Level, 0.0f);
			return desc;
		}
	}

	EngineImpl::EngineImpl(const AudioEngineConfig& config)
		: m_Config(config)
	{
		if (m_Config.Log)
			m_LogToken = SetLogCallback(m_Config.Log);
		m_FileSystem = m_Config.FileSystem ? m_Config.FileSystem : CreateDiskFileSystem();
		m_Config.MaxVoices = std::clamp(m_Config.MaxVoices, 1u, 65536u);
		m_Config.MaxAudibleVoices = std::clamp(m_Config.MaxAudibleVoices, 1u, m_Config.MaxVoices);
		m_Config.MaxBuses = std::clamp(m_Config.MaxBuses, 1u, 1024u);
		m_Config.StreamBufferSeconds = std::clamp(m_Config.StreamBufferSeconds, 0.1f, 10.0f);

		m_Streamer = std::make_unique<Streamer>(m_FileSystem, m_Config.StreamBufferSeconds, !m_Config.NoOutput);

		uint32_t sampleRate = m_Config.SampleRate;
		if (!m_Config.NoOutput)
		{
			// The device may call back before the mixer exists; it renders silence until then.
			m_Device = std::make_unique<Device>(&EngineImpl::DeviceCallback, this);
			m_Device->Open(m_Config.OutputDevice, sampleRate, m_Config.BufferSizeMs);
			if (sampleRate == 0)
				sampleRate = m_Device->GetSampleRate();
		}
		m_SampleRate = sampleRate > 0 ? sampleRate : 48000;

		MixerConfig mixerConfig;
		mixerConfig.SampleRate = m_SampleRate;
		mixerConfig.MaxVoices = m_Config.MaxVoices;
		mixerConfig.MaxAudibleVoices = m_Config.MaxAudibleVoices;
		mixerConfig.MaxBuses = m_Config.MaxBuses;
		mixerConfig.CommandCapacity = std::max(8192u, m_Config.MaxVoices * 8);
		mixerConfig.Handedness = m_Config.Handedness;
		mixerConfig.SpeedOfSound = std::max(m_Config.SpeedOfSound, 1.0f);
		mixerConfig.Interpolation = m_Config.Interpolation;
		mixerConfig.SpatialMode = m_Config.SpatialMode;
		mixerConfig.RearHighCutDb = std::isfinite(m_Config.RearHighCutDb) ? std::clamp(m_Config.RearHighCutDb, 0.0f, 24.0f) : 0.0f;
		mixerConfig.MasterLimiter = m_Config.MasterLimiter;
		m_SpatialMode = m_Config.SpatialMode;
		m_Mixer = std::make_unique<Mixer>(mixerConfig, m_Streamer.get());
		m_RenderMixer.store(m_Mixer.get(), std::memory_order_release);
		const uint32_t loaderThreads = m_Config.LoaderThreads > 0 ? m_Config.LoaderThreads : std::clamp(std::thread::hardware_concurrency() / 2, 1u, 4u);
		m_Loader = std::make_unique<Loader>(std::min(loaderThreads, 16u));

		m_Slots.resize(m_Config.MaxVoices);
		m_FreeSlots.reserve(m_Config.MaxVoices);
		for (uint32_t i = m_Config.MaxVoices; i > 0; i--)
			m_FreeSlots.push_back(i - 1);
		m_ActiveSlots.reserve(m_Config.MaxVoices);
		m_Buses.reserve(m_Config.MaxBuses);
		m_Buses.push_back({ "Master" });

		if (m_Device)
			TIMBRE_LOG_INFO("Playing on {} at {} Hz", m_Device->GetName(), m_SampleRate);
	}

	EngineImpl::~EngineImpl()
	{
		// Stop the audio and loading threads first, then nothing else can touch the engine's data.
		m_Device.reset();
		m_RenderMixer.store(nullptr, std::memory_order_release);
		m_Loader.reset();

		std::scoped_lock lock(m_Mutex);
		for (Retired& retired : m_Retired)
			if (retired.Action)
				retired.Action();
		m_Retired.clear();
		for (VoiceSlot& slot : m_Slots)
			for (const std::shared_ptr<Effect>& effect : slot.Effects)
				effect->m_Attached.store(false, std::memory_order_release);
		for (BusRecord& bus : m_Buses)
			for (const std::shared_ptr<Effect>& effect : bus.Effects)
				effect->m_Attached.store(false, std::memory_order_release);
		m_Slots.clear();
		m_Buses.clear();
		m_PendingCallbacks.clear();
		if (m_LogToken != 0)
			ResetLogCallback(m_LogToken);
	}

	void EngineImpl::DeviceCallback(void* userData, float* output, uint32_t frames)
	{
		Mixer* mixer = static_cast<EngineImpl*>(userData)->m_RenderMixer.load(std::memory_order_acquire);
		if (mixer != nullptr)
			mixer->Render(output, frames);
		else
			std::fill(output, output + size_t(frames) * 2, 0.0f);
	}

	void EngineImpl::Update()
	{
		std::vector<std::function<void()>> callbacks;
		{
			std::scoped_lock lock(m_Mutex);
			for (size_t i = 0; i < m_ActiveSlots.size();)
			{
				const uint32_t index = m_ActiveSlots[i];
				const uint64_t state = m_Mixer->GetVoiceStatus(index).State.load(std::memory_order_acquire);
				if (UnpackGeneration(state) != m_Slots[index].Generation || UnpackVoiceState(state) != VoiceState::Ended)
				{
					i++;
					continue;
				}
				// The mixer is done with this voice, its resources can go right away.
				ReleaseSlot(index, callbacks);
				m_ActiveSlots[i] = m_ActiveSlots.back();
				m_ActiveSlots.pop_back();
				m_FreeSlots.push_back(index);
			}
			for (std::function<void()>& callback : m_PendingCallbacks)
				callbacks.push_back(std::move(callback));
			m_PendingCallbacks.clear();

			if (m_Device && m_Device->ConsumeStopped())
			{
				TIMBRE_LOG_WARNING("The audio device stopped, switching to the default device");
				m_Device->Open({}, m_SampleRate, m_Config.BufferSizeMs);
			}
		}

		// Callbacks run unlocked so they can play sounds.
		for (std::function<void()>& callback : callbacks)
			callback();
		callbacks.clear();

		std::scoped_lock lock(m_Mutex);
		ReleaseRetired();
		Publish();
	}

	void EngineImpl::Publish()
	{
		if (m_Pending.empty())
			return;
		Command end;
		end.Type = CommandType::EndBatch;
		end.Frame = m_NextBatch++;
		m_Pending.push_back(end);

		SpscQueue<Command>& queue = m_Mixer->GetCommandQueue();
		size_t pushed = 0;
		while (pushed < m_Pending.size() && queue.Push(m_Pending[pushed]))
			pushed++;
		queue.Publish();
		m_Pending.erase(m_Pending.begin(), m_Pending.begin() + ptrdiff_t(pushed));
		if (!m_Pending.empty() && !m_QueueFullWarned)
		{
			TIMBRE_LOG_WARNING("The command queue is full, the audio thread is not keeping up or not running");
			m_QueueFullWarned = true;
		}
	}

	void EngineImpl::Retire(std::shared_ptr<void> object, std::function<void()> action)
	{
		m_Retired.push_back({ m_NextBatch, std::move(object), std::move(action) });
	}

	void EngineImpl::ReleaseRetired()
	{
		const uint64_t completed = m_Mixer->GetCompletedBatch();
		const auto done = std::stable_partition(m_Retired.begin(), m_Retired.end(), [completed](const Retired& retired) { return retired.Batch > completed; });
		for (auto it = done; it != m_Retired.end(); ++it)
			if (it->Action)
				it->Action();
		m_Retired.erase(done, m_Retired.end());
	}

	uint32_t EngineImpl::ToFrames(float seconds) const
	{
		// Written so NaN ends up as 0.
		if (!(seconds > 0.0f))
			return 0;
		return uint32_t(std::min(seconds, 3600.0f) * float(m_SampleRate) + 0.5f);
	}

	bool EngineImpl::CheckFinite(bool finite) const
	{
		if (!finite && !m_WarnedNonFinite.exchange(true, std::memory_order_relaxed))
			TIMBRE_LOG_WARNING("Ignoring a NaN or infinite value passed to the audio engine, check for divisions by zero (like a velocity with dt == 0)");
		return finite;
	}

	float EngineImpl::RandomFloat()
	{
		// PCG32
		const uint64_t old = m_RandomState;
		m_RandomState = old * 6364136223846793005ULL + 0xda3e39cb94b95bdbULL;
		const uint32_t shifted = uint32_t(((old >> 18) ^ old) >> 27);
		const uint32_t rotation = uint32_t(old >> 59);
		const uint32_t value = (shifted >> rotation) | (shifted << ((32 - rotation) & 31));
		return float(value >> 8) * 0x1.0p-24f;
	}

	// Loading

	bool EngineImpl::LoadAsset(SoundAsset& asset)
	{
		std::unique_ptr<AudioFile> file;
		if (asset.FileData)
			file = OpenMemoryFile(asset.FileData);
		else
		{
			file = m_FileSystem->Open(asset.Path);
			if (file && asset.Mode == LoadMode::Compressed)
			{
				asset.FileData = ReadAll(*file);
				file = OpenMemoryFile(asset.FileData);
			}
		}
		if (!file)
		{
			TIMBRE_LOG_ERROR("Failed to open {}", asset.Path);
			FinishLoading(asset, LoadState::Failed);
			return false;
		}

		std::string error;
		const std::unique_ptr<Decoder> decoder = OpenDecoder(std::move(file), error);
		if (!decoder)
		{
			TIMBRE_LOG_ERROR("Failed to load {}: {}", asset.Path, error);
			FinishLoading(asset, LoadState::Failed);
			return false;
		}

		asset.Format = decoder->GetFormat();
		if (asset.Mode == LoadMode::Decompress)
		{
			DecodeToPcm16(*decoder, asset.Pcm);
			asset.Format.FrameCount = asset.Pcm.size() / asset.Format.Channels;
			asset.FileData.reset();
		}
		FinishLoading(asset, LoadState::Ready);
		return true;
	}

	Sound EngineImpl::RegisterSound(std::shared_ptr<SoundAsset> asset, const SoundDesc& desc, bool newAsset)
	{
		std::scoped_lock lock(m_Mutex);
		if (newAsset)
		{
			if (m_Assets.size() >= m_PruneAssetsAt)
			{
				std::erase_if(m_Assets, [](const std::weak_ptr<SoundAsset>& weak) { return weak.expired(); });
				std::erase_if(m_AssetCache, [](const auto& entry) { return entry.second.expired(); });
				m_PruneAssetsAt = std::max<size_t>(64, m_Assets.size() * 2);
			}
			m_Assets.push_back(asset);
		}

		auto data = std::make_shared<SoundData>();
		data->Asset = std::move(asset);
		data->Desc = SanitizeDesc(desc);
		data->Engine = this;
		return Sound(std::move(data));
	}

	Sound EngineImpl::LoadSound(const std::string& path, const SoundDesc& desc)
	{
		const std::string key = std::to_string(int(desc.Mode)) + ':' + path;
		std::shared_ptr<SoundAsset> asset;
		{
			std::scoped_lock lock(m_Mutex);
			const auto it = m_AssetCache.find(key);
			if (it != m_AssetCache.end())
				asset = it->second.lock();
		}
		// A synchronous load that finds the same file still loading in the background waits for it.
		if (asset && !desc.Async)
			asset->State.wait(LoadState::Loading, std::memory_order_acquire);
		if (asset && asset->State.load(std::memory_order_acquire) == LoadState::Failed)
			asset.reset();

		const bool newAsset = !asset;
		if (newAsset)
		{
			asset = std::make_shared<SoundAsset>();
			asset->Path = path;
			asset->Mode = desc.Mode;
			if (desc.Async)
				m_Loader->Enqueue([this, asset] { LoadAsset(*asset); });
			else if (!LoadAsset(*asset))
				return {};
			std::scoped_lock lock(m_Mutex);
			m_AssetCache[key] = asset;
		}
		return RegisterSound(std::move(asset), desc, newAsset);
	}

	Sound EngineImpl::LoadSound(std::vector<uint8_t> fileData, const SoundDesc& desc)
	{
		auto asset = std::make_shared<SoundAsset>();
		asset->Path = "<memory>";
		// The file is in memory already, streaming decodes straight from it.
		asset->Mode = desc.Mode == LoadMode::Decompress ? LoadMode::Decompress : LoadMode::Compressed;
		asset->FileData = std::make_shared<const std::vector<uint8_t>>(std::move(fileData));
		if (desc.Async)
			m_Loader->Enqueue([this, asset] { LoadAsset(*asset); });
		else if (!LoadAsset(*asset))
			return {};
		return RegisterSound(std::move(asset), desc, true);
	}

	Sound EngineImpl::CreateSound(std::span<const float> samples, uint32_t channels, uint32_t sampleRate, const SoundDesc& desc)
	{
		if (channels < 1 || channels > 2 || sampleRate == 0 || samples.size() < channels)
		{
			TIMBRE_LOG_ERROR("CreateSound needs mono or stereo samples and a sample rate");
			return {};
		}
		auto asset = std::make_shared<SoundAsset>();
		asset->Path = "<generated>";
		asset->Mode = LoadMode::Decompress;
		asset->Format.Channels = channels;
		asset->Format.SampleRate = sampleRate;
		asset->Format.FrameCount = samples.size() / channels;
		asset->Pcm.resize(size_t(asset->Format.FrameCount) * channels);
		for (size_t i = 0; i < asset->Pcm.size(); i++)
			asset->Pcm[i] = FloatToPcm16(std::isfinite(samples[i]) ? samples[i] : 0.0f);
		asset->State.store(LoadState::Ready, std::memory_order_release);
		return RegisterSound(std::move(asset), desc, true);
	}

	// Voices

	Voice EngineImpl::Play(const Sound& sound, const PlayParams& params)
	{
		const std::shared_ptr<SoundData>& data = sound.m_Data;
		if (!data || data->Engine != this || data->Asset->State.load(std::memory_order_acquire) == LoadState::Failed)
			return {};
		const SoundDesc& desc = data->Desc;

		// Bad values fall back to the defaults instead of reaching the mixer.
		const auto finite = [this](float value, float fallback) { return CheckFinite(std::isfinite(value)) ? value : fallback; };
		const float volume = std::max(finite(params.Volume, 1.0f), 0.0f);
		const float pitch = std::max(finite(params.Pitch, 1.0f), 0.0f);
		const float pan = std::clamp(finite(params.Pan, 0.0f), -1.0f, 1.0f);
		const float startTime = std::max(finite(params.StartTime, 0.0f), 0.0f);
		const bool hasPosition = params.Position.has_value() && CheckFinite(IsFinite(*params.Position));
		const bool hasVelocity = params.Velocity != Vec3() && CheckFinite(IsFinite(params.Velocity));

		std::scoped_lock lock(m_Mutex);
		if (desc.MaxInstances > 0)
		{
			uint32_t playing = 0;
			int64_t victim = -1;
			float victimScore = 0.0f;
			for (uint32_t index : data->Voices)
			{
				const VoiceSlot& slot = m_Slots[index];
				if (slot.Stopping || HasEnded(index))
					continue;
				playing++;
				const float score = desc.LimitMode == InstanceLimitMode::StealQuietest ? GetAudibility(index) : float(slot.Order);
				if (victim < 0 || score < victimScore)
				{
					victim = index;
					victimScore = score;
				}
			}
			if (playing >= desc.MaxInstances)
			{
				if (desc.LimitMode == InstanceLimitMode::RejectNew || victim < 0)
					return {};
				StopSlot(uint32_t(victim), 0.0f);
			}
		}

		const int64_t allocated = AllocateSlot(desc.Priority);
		if (allocated < 0)
			return {};
		const uint32_t index = uint32_t(allocated);
		VoiceSlot& slot = m_Slots[index];
		const uint32_t generation = slot.Generation + 1 == 0 ? 1 : slot.Generation + 1;
		slot = VoiceSlot{};
		slot.Generation = generation;
		slot.Active = true;
		slot.Sound = data;
		slot.Order = ++m_PlayCounter;
		slot.Priority = desc.Priority;
		slot.Bus = params.Bus ? ResolveBus(params.Bus) : ResolveBus(desc.Bus);
		slot.Volume = desc.Volume * volume * (1.0f - std::clamp(desc.VolumeVariation, 0.0f, 1.0f) * RandomFloat());
		slot.Pitch = desc.Pitch * pitch * SemitonesToPitch(desc.PitchVariation * (2.0f * RandomFloat() - 1.0f));
		slot.Pan = pan;
		slot.Looping = params.Looping.value_or(desc.Looping);
		slot.Paused = params.Paused;
		slot.OnEnd = params.OnEnd;
		data->Voices.push_back(index);
		m_ActiveSlots.push_back(index);

		if (data->Asset->Mode != LoadMode::Decompress)
		{
			slot.Stream = std::make_shared<StreamInstance>(data, startTime, slot.Looping);
			m_Streamer->Add(slot.Stream);
		}

		Command command;
		command.Type = CommandType::PlayVoice;
		command.Index = index;
		command.Generation = generation;
		command.Play.Sound = data.get();
		command.Play.Stream = slot.Stream.get();
		command.Play.StartTime = startTime;
		command.Play.Volume = slot.Volume;
		command.Play.Pitch = slot.Pitch;
		command.Play.Pan = slot.Pan;
		command.Play.DelayFrames = ToFrames(params.Delay);
		command.Play.FadeInFrames = ToFrames(params.FadeIn);
		command.Play.Bus = slot.Bus;
		command.Play.Looping = slot.Looping;
		command.Play.Paused = slot.Paused;
		m_Pending.push_back(command);
		if (hasPosition)
			PushVector(CommandType::SetVoicePosition, index, generation, *params.Position);
		if (hasVelocity)
			PushVector(CommandType::SetVoiceVelocity, index, generation, params.Velocity);
		for (const BusSend& send : desc.Sends)
			if (send.Bus.m_Engine == this && send.Bus.m_Index < m_Buses.size())
				ApplySend(index, slot, uint16_t(send.Bus.m_Index), send.Level, 0);
		return Voice(this, index, generation);
	}

	bool EngineImpl::HasEnded(uint32_t index) const
	{
		const uint64_t state = m_Mixer->GetVoiceStatus(index).State.load(std::memory_order_acquire);
		return UnpackGeneration(state) == m_Slots[index].Generation && UnpackVoiceState(state) == VoiceState::Ended;
	}

	int64_t EngineImpl::AllocateSlot(uint8_t priority)
	{
		if (!m_FreeSlots.empty())
		{
			const uint32_t index = m_FreeSlots.back();
			m_FreeSlots.pop_back();
			return index;
		}

		// Out of voices: steal the least important one, by priority, then loudness, then age.
		int64_t best = -1;
		uint8_t bestPriority = 0;
		float bestAudibility = 0.0f;
		uint64_t bestOrder = 0;
		for (uint32_t index : m_ActiveSlots)
		{
			const VoiceSlot& slot = m_Slots[index];
			const float audibility = GetAudibility(index);
			const bool better = best < 0 || slot.Priority < bestPriority
				|| (slot.Priority == bestPriority && (audibility < bestAudibility || (audibility == bestAudibility && slot.Order < bestOrder)));
			if (better)
			{
				best = index;
				bestPriority = slot.Priority;
				bestAudibility = audibility;
				bestOrder = slot.Order;
			}
		}
		if (best < 0 || bestPriority > priority)
			return -1;
		StealSlot(uint32_t(best));
		return best;
	}

	void EngineImpl::StealSlot(uint32_t index)
	{
		// The mixer may still be playing the old voice until it sees the next batch, so its
		// resources are retired instead of released.
		VoiceSlot& slot = m_Slots[index];
		if (slot.OnEnd)
			m_PendingCallbacks.push_back(std::move(slot.OnEnd));
		if (slot.Sound)
		{
			std::erase(slot.Sound->Voices, index);
			Retire(slot.Sound);
		}
		if (slot.Stream)
			Retire(slot.Stream, [this, stream = slot.Stream.get()] { m_Streamer->Remove(stream); });
		if (slot.Chain)
			Retire(slot.Chain);
		for (const std::shared_ptr<Effect>& effect : slot.Effects)
			DetachEffectLater(effect);
		std::erase(m_ActiveSlots, index);
		const uint32_t generation = slot.Generation;
		slot = VoiceSlot{};
		slot.Generation = generation;
	}

	void EngineImpl::ReleaseSlot(uint32_t index, std::vector<std::function<void()>>& callbacks)
	{
		VoiceSlot& slot = m_Slots[index];
		if (slot.OnEnd)
			callbacks.push_back(std::move(slot.OnEnd));
		if (slot.Sound)
			std::erase(slot.Sound->Voices, index);
		if (slot.Stream)
			m_Streamer->Remove(slot.Stream.get());
		for (const std::shared_ptr<Effect>& effect : slot.Effects)
			effect->m_Attached.store(false, std::memory_order_release);
		const uint32_t generation = slot.Generation;
		slot = VoiceSlot{};
		slot.Generation = generation;
	}

	float EngineImpl::GetAudibility(uint32_t index) const
	{
		// Voices the mixer hasn't started yet count at their volume, otherwise a burst of new
		// sounds would look silent and steal each other instead of old, quiet voices.
		const VoiceStatus& status = m_Mixer->GetVoiceStatus(index);
		const uint64_t state = status.State.load(std::memory_order_acquire);
		if (UnpackGeneration(state) != m_Slots[index].Generation || UnpackVoiceState(state) == VoiceState::Waiting)
			return m_Slots[index].Volume;
		return status.Audibility.load(std::memory_order_relaxed);
	}

	void EngineImpl::StopSlot(uint32_t index, float fadeSeconds)
	{
		VoiceSlot& slot = m_Slots[index];
		slot.Stopping = true;
		PushParam(CommandType::StopVoice, index, slot.Generation, 0.0f, ToFrames(fadeSeconds));
	}

	void EngineImpl::StopAll(float fadeSeconds)
	{
		std::scoped_lock lock(m_Mutex);
		for (uint32_t index : m_ActiveSlots)
			m_Slots[index].Stopping = true;
		PushParam(CommandType::StopAll, 0, 0, 0.0f, ToFrames(fadeSeconds));
	}

	EngineImpl::VoiceSlot* EngineImpl::FindSlot(uint32_t index, uint32_t generation)
	{
		if (index >= m_Slots.size())
			return nullptr;
		VoiceSlot& slot = m_Slots[index];
		return slot.Active && slot.Generation == generation ? &slot : nullptr;
	}

	const EngineImpl::VoiceSlot* EngineImpl::FindSlot(uint32_t index, uint32_t generation) const
	{
		return const_cast<EngineImpl*>(this)->FindSlot(index, generation);
	}

	void EngineImpl::PushParam(CommandType type, uint32_t index, uint32_t generation, float value, uint32_t frames, bool flag)
	{
		Command command;
		command.Type = type;
		command.Index = index;
		command.Generation = generation;
		command.Param = { value, frames, flag };
		m_Pending.push_back(command);
	}

	void EngineImpl::PushVector(CommandType type, uint32_t index, uint32_t generation, const Vec3& value)
	{
		Command command;
		command.Type = type;
		command.Index = index;
		command.Generation = generation;
		command.Vector[0] = value.x;
		command.Vector[1] = value.y;
		command.Vector[2] = value.z;
		m_Pending.push_back(command);
	}

	void EngineImpl::PushEffects(CommandType type, uint32_t index, uint32_t generation, EffectChain* chain)
	{
		Command command;
		command.Type = type;
		command.Index = index;
		command.Generation = generation;
		command.Effects = chain;
		m_Pending.push_back(command);
	}

	bool EngineImpl::IsVoicePlaying(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		if (FindSlot(index, generation) == nullptr)
			return false;
		// The mixer may have finished it since the last Update().
		const uint64_t state = m_Mixer->GetVoiceStatus(index).State.load(std::memory_order_acquire);
		return UnpackGeneration(state) != generation || UnpackVoiceState(state) != VoiceState::Ended;
	}

	bool EngineImpl::IsVoicePaused(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		return slot != nullptr && slot->Paused;
	}

	void EngineImpl::StopVoice(uint32_t index, uint32_t generation, float fadeSeconds)
	{
		if (!std::isfinite(fadeSeconds))
			fadeSeconds = 0.0f;
		std::scoped_lock lock(m_Mutex);
		if (FindSlot(index, generation) != nullptr)
			StopSlot(index, fadeSeconds);
	}

	void EngineImpl::SetVoicePaused(uint32_t index, uint32_t generation, bool paused, float fadeSeconds)
	{
		if (!std::isfinite(fadeSeconds))
			fadeSeconds = 0.0f;
		std::scoped_lock lock(m_Mutex);
		if (VoiceSlot* slot = FindSlot(index, generation))
		{
			slot->Paused = paused;
			PushParam(CommandType::PauseVoice, index, generation, 0.0f, ToFrames(fadeSeconds), paused);
		}
	}

	void EngineImpl::SetVoiceVolume(uint32_t index, uint32_t generation, float volume, float fadeSeconds)
	{
		if (!CheckFinite(std::isfinite(volume) && std::isfinite(fadeSeconds)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (VoiceSlot* slot = FindSlot(index, generation))
		{
			slot->Volume = std::max(volume, 0.0f);
			PushParam(CommandType::SetVoiceVolume, index, generation, slot->Volume, ToFrames(fadeSeconds));
		}
	}

	void EngineImpl::SetVoicePitch(uint32_t index, uint32_t generation, float pitch)
	{
		if (!CheckFinite(std::isfinite(pitch)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (VoiceSlot* slot = FindSlot(index, generation))
		{
			slot->Pitch = std::max(pitch, 0.0f);
			PushParam(CommandType::SetVoicePitch, index, generation, slot->Pitch);
		}
	}

	void EngineImpl::SetVoicePan(uint32_t index, uint32_t generation, float pan)
	{
		if (!CheckFinite(std::isfinite(pan)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (VoiceSlot* slot = FindSlot(index, generation))
		{
			slot->Pan = std::clamp(pan, -1.0f, 1.0f);
			PushParam(CommandType::SetVoicePan, index, generation, slot->Pan);
		}
	}

	void EngineImpl::SetVoiceLooping(uint32_t index, uint32_t generation, bool looping)
	{
		std::scoped_lock lock(m_Mutex);
		if (VoiceSlot* slot = FindSlot(index, generation))
		{
			slot->Looping = looping;
			PushParam(CommandType::SetVoiceLooping, index, generation, 0.0f, 0, looping);
		}
	}

	void EngineImpl::SetVoiceVector(uint32_t index, uint32_t generation, CommandType type, const Vec3& value)
	{
		if (!CheckFinite(IsFinite(value)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (FindSlot(index, generation) != nullptr)
			PushVector(type, index, generation, value);
	}

	void EngineImpl::SetVoiceFilter(uint32_t index, uint32_t generation, CommandType type, float cutoff)
	{
		if (!CheckFinite(std::isfinite(cutoff)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (FindSlot(index, generation) != nullptr)
			PushParam(type, index, generation, std::max(cutoff, 0.0f));
	}

	void EngineImpl::SetVoiceSend(uint32_t index, uint32_t generation, const Bus& bus, float level, float fadeSeconds)
	{
		if (!CheckFinite(std::isfinite(level) && std::isfinite(fadeSeconds)))
			return;
		std::scoped_lock lock(m_Mutex);
		VoiceSlot* slot = FindSlot(index, generation);
		if (slot != nullptr && bus.m_Engine == this && bus.m_Index < m_Buses.size())
			ApplySend(index, *slot, uint16_t(bus.m_Index), std::max(level, 0.0f), ToFrames(fadeSeconds));
	}

	float EngineImpl::GetVoiceSend(uint32_t index, uint32_t generation, const Bus& bus) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		if (slot == nullptr || bus.m_Engine != this)
			return 0.0f;
		for (uint32_t i = 0; i < slot->SendCount; i++)
			if (slot->Sends[i].Bus == bus.m_Index)
				return slot->Sends[i].Level;
		return 0.0f;
	}

	void EngineImpl::ApplySend(uint32_t index, VoiceSlot& slot, uint16_t bus, float level, uint32_t fadeFrames)
	{
		uint32_t found = slot.SendCount;
		for (uint32_t i = 0; i < slot.SendCount; i++)
			if (slot.Sends[i].Bus == bus)
				found = i;
		if (found == slot.SendCount)
		{
			if (level <= 0.0f)
				return;
			if (slot.SendCount == MaxVoiceSends)
			{
				TIMBRE_LOG_WARNING("A voice can send to at most {} buses", MaxVoiceSends);
				return;
			}
			slot.SendCount++;
		}
		slot.Sends[found] = { bus, level };
		if (level <= 0.0f)
			slot.Sends[found] = slot.Sends[--slot.SendCount];

		Command command;
		command.Type = CommandType::SetVoiceSend;
		command.Index = index;
		command.Generation = slot.Generation;
		command.Send = { bus, level, fadeFrames };
		m_Pending.push_back(command);
	}

	void EngineImpl::SeekVoice(uint32_t index, uint32_t generation, float seconds)
	{
		if (!CheckFinite(std::isfinite(seconds)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (FindSlot(index, generation) == nullptr)
			return;
		Command command;
		command.Type = CommandType::SeekVoice;
		command.Index = index;
		command.Generation = generation;
		command.Seconds = std::max(0.0, double(seconds));
		m_Pending.push_back(command);
	}

	void EngineImpl::AddVoiceEffect(uint32_t index, uint32_t generation, std::shared_ptr<Effect> effect)
	{
		std::scoped_lock lock(m_Mutex);
		VoiceSlot* slot = FindSlot(index, generation);
		if (slot == nullptr || !AttachEffect(effect))
			return;
		slot->Effects.push_back(std::move(effect));
		UpdateVoiceChain(index, *slot);
	}

	void EngineImpl::RemoveVoiceEffect(uint32_t index, uint32_t generation, const std::shared_ptr<Effect>& effect)
	{
		std::scoped_lock lock(m_Mutex);
		VoiceSlot* slot = FindSlot(index, generation);
		if (slot == nullptr)
			return;
		const auto it = std::find(slot->Effects.begin(), slot->Effects.end(), effect);
		if (it == slot->Effects.end())
			return;
		DetachEffectLater(*it);
		slot->Effects.erase(it);
		UpdateVoiceChain(index, *slot);
	}

	float EngineImpl::GetVoiceVolume(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		return slot != nullptr ? slot->Volume : 0.0f;
	}

	float EngineImpl::GetVoicePitch(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		return slot != nullptr ? slot->Pitch : 0.0f;
	}

	float EngineImpl::GetVoicePan(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		return slot != nullptr ? slot->Pan : 0.0f;
	}

	bool EngineImpl::IsVoiceLooping(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		return slot != nullptr && slot->Looping;
	}

	float EngineImpl::GetVoiceTime(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		if (slot == nullptr)
			return 0.0f;
		const VoiceStatus& status = m_Mixer->GetVoiceStatus(index);
		if (UnpackGeneration(status.State.load(std::memory_order_acquire)) != generation)
			return 0.0f;
		const SoundAsset& asset = *slot->Sound->Asset;
		if (asset.State.load(std::memory_order_acquire) != LoadState::Ready || asset.Format.SampleRate == 0)
			return 0.0f;
		return float(double(status.Position.load(std::memory_order_relaxed)) / asset.Format.SampleRate);
	}

	Sound EngineImpl::GetVoiceSound(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const VoiceSlot* slot = FindSlot(index, generation);
		return slot != nullptr ? Sound(slot->Sound) : Sound();
	}

	// Effects

	bool EngineImpl::AttachEffect(const std::shared_ptr<Effect>& effect)
	{
		if (!effect)
			return false;
		bool expected = false;
		if (!effect->m_Attached.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			TIMBRE_LOG_ERROR("An effect can only be used by one bus or voice at a time");
			return false;
		}
		effect->Initialize(m_SampleRate);
		return true;
	}

	void EngineImpl::DetachEffectLater(const std::shared_ptr<Effect>& effect)
	{
		Retire(effect, [raw = effect.get()] { raw->m_Attached.store(false, std::memory_order_release); });
	}

	std::shared_ptr<EffectChain> EngineImpl::BuildChain(const std::vector<std::shared_ptr<Effect>>& effects) const
	{
		if (effects.empty())
			return nullptr;
		auto chain = std::make_shared<EffectChain>();
		for (const std::shared_ptr<Effect>& effect : effects)
			chain->Effects.push_back(effect.get());
		return chain;
	}

	void EngineImpl::UpdateVoiceChain(uint32_t index, VoiceSlot& slot)
	{
		if (slot.Chain)
			Retire(slot.Chain);
		slot.Chain = BuildChain(slot.Effects);
		PushEffects(CommandType::SetVoiceEffects, index, slot.Generation, slot.Chain.get());
	}

	void EngineImpl::UpdateBusChain(uint32_t index, BusRecord& bus)
	{
		if (bus.Chain)
			Retire(bus.Chain);
		bus.Chain = BuildChain(bus.Effects);
		PushEffects(CommandType::SetBusEffects, index, 0, bus.Chain.get());
	}

	// Buses

	EngineImpl::BusRecord* EngineImpl::GetBusRecord(uint32_t index)
	{
		return index < m_Buses.size() ? &m_Buses[index] : nullptr;
	}

	const EngineImpl::BusRecord* EngineImpl::GetBusRecord(uint32_t index) const
	{
		return index < m_Buses.size() ? &m_Buses[index] : nullptr;
	}

	bool EngineImpl::IsInBus(uint32_t bus, uint32_t ancestor) const
	{
		for (size_t depth = 0; depth <= m_Buses.size(); depth++)
		{
			if (bus == ancestor)
				return true;
			if (bus == 0 || bus >= m_Buses.size())
				return false;
			bus = m_Buses[bus].Parent;
		}
		return false;
	}

	uint16_t EngineImpl::ResolveBus(const Bus& bus) const
	{
		return bus.m_Engine == this && bus.m_Index < m_Buses.size() ? uint16_t(bus.m_Index) : 0;
	}

	Bus EngineImpl::GetMasterBus()
	{
		return Bus(this, 0);
	}

	Bus EngineImpl::CreateBus(const std::string& name, const Bus& parent)
	{
		std::scoped_lock lock(m_Mutex);
		if (m_Buses.size() >= m_Config.MaxBuses)
		{
			TIMBRE_LOG_ERROR("Can't create bus {}, the limit of {} buses is reached", name, m_Config.MaxBuses);
			return {};
		}
		const uint32_t index = uint32_t(m_Buses.size());
		const uint32_t parentIndex = ResolveBus(parent);
		BusRecord record;
		record.Name = name;
		record.Parent = parentIndex;
		m_Buses.push_back(std::move(record));

		Command command;
		command.Type = CommandType::CreateBus;
		command.Index = index;
		command.Parent = parentIndex;
		m_Pending.push_back(command);
		return Bus(this, index);
	}

	Bus EngineImpl::FindBus(std::string_view name)
	{
		std::scoped_lock lock(m_Mutex);
		for (size_t i = 0; i < m_Buses.size(); i++)
			if (m_Buses[i].Name == name)
				return Bus(this, uint32_t(i));
		return {};
	}

	void EngineImpl::SetBusVolume(uint32_t index, float volume, float fadeSeconds)
	{
		if (!CheckFinite(std::isfinite(volume) && std::isfinite(fadeSeconds)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (BusRecord* bus = GetBusRecord(index))
		{
			bus->Volume = std::max(volume, 0.0f);
			PushParam(CommandType::SetBusVolume, index, 0, bus->Volume, ToFrames(fadeSeconds));
		}
	}

	float EngineImpl::GetBusVolume(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		return bus != nullptr ? bus->Volume : 0.0f;
	}

	void EngineImpl::SetBusMuted(uint32_t index, bool muted)
	{
		std::scoped_lock lock(m_Mutex);
		if (BusRecord* bus = GetBusRecord(index))
		{
			bus->Muted = muted;
			PushParam(CommandType::SetBusMuted, index, 0, 0.0f, 0, muted);
		}
	}

	bool EngineImpl::IsBusMuted(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		return bus != nullptr && bus->Muted;
	}

	void EngineImpl::SetBusPaused(uint32_t index, bool paused)
	{
		std::scoped_lock lock(m_Mutex);
		if (BusRecord* bus = GetBusRecord(index))
		{
			bus->Paused = paused;
			PushParam(CommandType::SetBusPaused, index, 0, 0.0f, 0, paused);
		}
	}

	bool EngineImpl::IsBusPaused(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		return bus != nullptr && bus->Paused;
	}

	void EngineImpl::SetBusPitch(uint32_t index, float pitch)
	{
		if (!CheckFinite(std::isfinite(pitch)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (BusRecord* bus = GetBusRecord(index))
		{
			bus->Pitch = std::max(pitch, 0.0f);
			PushParam(CommandType::SetBusPitch, index, 0, bus->Pitch);
		}
	}

	float EngineImpl::GetBusPitch(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		return bus != nullptr ? bus->Pitch : 1.0f;
	}

	void EngineImpl::SetBusLowPass(uint32_t index, float cutoff, float fadeSeconds)
	{
		if (!CheckFinite(std::isfinite(cutoff) && std::isfinite(fadeSeconds)))
			return;
		std::scoped_lock lock(m_Mutex);
		if (BusRecord* bus = GetBusRecord(index))
		{
			bus->LowPass = std::max(cutoff, 0.0f);
			PushParam(CommandType::SetBusLowPass, index, 0, bus->LowPass, ToFrames(fadeSeconds));
		}
	}

	float EngineImpl::GetBusLowPass(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		return bus != nullptr ? bus->LowPass : 0.0f;
	}

	void EngineImpl::SetBusDucking(uint32_t index, const DuckingSettings& settings)
	{
		std::scoped_lock lock(m_Mutex);
		if (GetBusRecord(index) == nullptr)
			return;
		uint32_t trigger = NoBus;
		if (settings.Trigger.m_Engine == this && settings.Trigger.m_Index < m_Buses.size())
		{
			if (IsInBus(index, settings.Trigger.m_Index))
				TIMBRE_LOG_WARNING("Bus {} can't duck under itself or one of its parents", m_Buses[index].Name);
			else
				trigger = settings.Trigger.m_Index;
		}

		const DuckingSettings defaults;
		const auto finiteOr = [this](float value, float fallback) { return CheckFinite(std::isfinite(value)) ? value : fallback; };
		Command command;
		command.Type = CommandType::SetBusDucking;
		command.Index = index;
		command.Ducking.Trigger = trigger;
		command.Ducking.Gain = DecibelsToLinear(std::clamp(finiteOr(settings.VolumeDb, defaults.VolumeDb), -120.0f, 0.0f));
		command.Ducking.Threshold = DecibelsToLinear(std::clamp(finiteOr(settings.ThresholdDb, defaults.ThresholdDb), -120.0f, 24.0f));
		command.Ducking.Attack = std::max(finiteOr(settings.AttackSeconds, defaults.AttackSeconds), 0.001f);
		command.Ducking.Release = std::max(finiteOr(settings.ReleaseSeconds, defaults.ReleaseSeconds), 0.001f);
		m_Pending.push_back(command);
	}

	void EngineImpl::AddBusEffect(uint32_t index, std::shared_ptr<Effect> effect)
	{
		std::scoped_lock lock(m_Mutex);
		BusRecord* bus = GetBusRecord(index);
		if (bus == nullptr || !AttachEffect(effect))
			return;
		bus->Effects.push_back(std::move(effect));
		UpdateBusChain(index, *bus);
	}

	void EngineImpl::RemoveBusEffect(uint32_t index, const std::shared_ptr<Effect>& effect)
	{
		std::scoped_lock lock(m_Mutex);
		BusRecord* bus = GetBusRecord(index);
		if (bus == nullptr)
			return;
		const auto it = std::find(bus->Effects.begin(), bus->Effects.end(), effect);
		if (it == bus->Effects.end())
			return;
		DetachEffectLater(*it);
		bus->Effects.erase(it);
		UpdateBusChain(index, *bus);
	}

	void EngineImpl::ClearBusEffects(uint32_t index)
	{
		std::scoped_lock lock(m_Mutex);
		BusRecord* bus = GetBusRecord(index);
		if (bus == nullptr || bus->Effects.empty())
			return;
		for (const std::shared_ptr<Effect>& effect : bus->Effects)
			DetachEffectLater(effect);
		bus->Effects.clear();
		UpdateBusChain(index, *bus);
	}

	void EngineImpl::StopBus(uint32_t index, float fadeSeconds)
	{
		std::scoped_lock lock(m_Mutex);
		if (GetBusRecord(index) == nullptr)
			return;
		for (uint32_t slot : m_ActiveSlots)
			if (IsInBus(m_Slots[slot].Bus, index))
				m_Slots[slot].Stopping = true;
		PushParam(CommandType::StopBus, index, 0, 0.0f, ToFrames(fadeSeconds));
	}

	std::string EngineImpl::GetBusName(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		return bus != nullptr ? bus->Name : std::string();
	}

	Bus EngineImpl::GetBusParent(uint32_t index) const
	{
		std::scoped_lock lock(m_Mutex);
		const BusRecord* bus = GetBusRecord(index);
		if (bus == nullptr || index == 0)
			return {};
		return Bus(const_cast<EngineImpl*>(this), bus->Parent);
	}

	// Sounds

	void EngineImpl::StopSound(SoundData& sound, float fadeSeconds)
	{
		std::scoped_lock lock(m_Mutex);
		for (uint32_t index : sound.Voices)
			if (!m_Slots[index].Stopping)
				StopSlot(index, fadeSeconds);
	}

	uint32_t EngineImpl::GetInstanceCount(const SoundData& sound) const
	{
		std::scoped_lock lock(m_Mutex);
		uint32_t count = 0;
		for (uint32_t index : sound.Voices)
			if (!m_Slots[index].Stopping && !HasEnded(index))
				count++;
		return count;
	}

	// Listener, devices and stats

	void EngineImpl::SetListener(const Listener& listener)
	{
		if (!CheckFinite(IsFinite(listener.Position) && IsFinite(listener.Forward) && IsFinite(listener.Up) && IsFinite(listener.Velocity)))
			return;
		std::scoped_lock lock(m_Mutex);
		m_Listener = listener;
		Command command;
		command.Type = CommandType::SetListener;
		const Vec3* vectors[4] = { &listener.Position, &listener.Forward, &listener.Up, &listener.Velocity };
		for (int i = 0; i < 4; i++)
		{
			command.Listener[i * 3 + 0] = vectors[i]->x;
			command.Listener[i * 3 + 1] = vectors[i]->y;
			command.Listener[i * 3 + 2] = vectors[i]->z;
		}
		m_Pending.push_back(command);
	}

	Listener EngineImpl::GetListener() const
	{
		std::scoped_lock lock(m_Mutex);
		return m_Listener;
	}

	void EngineImpl::SetSpatialMode(SpatialMode mode)
	{
		std::scoped_lock lock(m_Mutex);
		if (mode == m_SpatialMode)
			return;
		m_SpatialMode = mode;
		PushParam(CommandType::SetSpatialMode, 0, 0, 0.0f, 0, mode == SpatialMode::Binaural);
	}

	SpatialMode EngineImpl::GetSpatialMode() const
	{
		std::scoped_lock lock(m_Mutex);
		return m_SpatialMode;
	}

	// Snapshots

	Snapshot EngineImpl::StartSnapshot(const SnapshotDesc& desc, float fadeSeconds, float intensity)
	{
		if (!CheckFinite(std::isfinite(fadeSeconds) && std::isfinite(intensity)))
			return {};
		std::scoped_lock lock(m_Mutex);
		std::vector<SnapshotEntry> entries;
		for (const SnapshotBus& bus : desc.Buses)
			if (bus.Bus.m_Engine == this && bus.Bus.m_Index < m_Buses.size() && CheckFinite(std::isfinite(bus.VolumeDb) && std::isfinite(bus.LowPassHz)))
				entries.push_back({ uint16_t(bus.Bus.m_Index), bus.VolumeDb, std::max(bus.LowPassHz, 0.0f) });

		size_t index = 0;
		while (index < m_Snapshots.size() && m_Snapshots[index].Active)
			index++;
		if (index == m_Snapshots.size())
			m_Snapshots.emplace_back();
		SnapshotSlot& slot = m_Snapshots[index];
		slot.Generation = slot.Generation + 1 == 0 ? 1 : slot.Generation + 1;
		slot.Active = true;
		slot.Intensity = std::clamp(intensity, 0.0f, 1.0f);
		slot.Entries = std::move(entries);
		ApplySnapshots(slot.Entries, ToFrames(fadeSeconds));
		return Snapshot(this, uint32_t(index), slot.Generation);
	}

	EngineImpl::SnapshotSlot* EngineImpl::FindSnapshot(uint32_t index, uint32_t generation)
	{
		if (index >= m_Snapshots.size())
			return nullptr;
		SnapshotSlot& slot = m_Snapshots[index];
		return slot.Active && slot.Generation == generation ? &slot : nullptr;
	}

	const EngineImpl::SnapshotSlot* EngineImpl::FindSnapshot(uint32_t index, uint32_t generation) const
	{
		return const_cast<EngineImpl*>(this)->FindSnapshot(index, generation);
	}

	bool EngineImpl::IsSnapshotActive(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		return FindSnapshot(index, generation) != nullptr;
	}

	void EngineImpl::StopSnapshot(uint32_t index, uint32_t generation, float fadeSeconds)
	{
		if (!std::isfinite(fadeSeconds))
			fadeSeconds = 0.0f;
		std::scoped_lock lock(m_Mutex);
		SnapshotSlot* slot = FindSnapshot(index, generation);
		if (slot == nullptr)
			return;
		slot->Active = false;
		const std::vector<SnapshotEntry> entries = std::move(slot->Entries);
		slot->Entries.clear();
		ApplySnapshots(entries, ToFrames(fadeSeconds));
	}

	void EngineImpl::SetSnapshotIntensity(uint32_t index, uint32_t generation, float intensity, float fadeSeconds)
	{
		if (!CheckFinite(std::isfinite(intensity) && std::isfinite(fadeSeconds)))
			return;
		std::scoped_lock lock(m_Mutex);
		SnapshotSlot* slot = FindSnapshot(index, generation);
		if (slot == nullptr)
			return;
		slot->Intensity = std::clamp(intensity, 0.0f, 1.0f);
		ApplySnapshots(slot->Entries, ToFrames(fadeSeconds));
	}

	float EngineImpl::GetSnapshotIntensity(uint32_t index, uint32_t generation) const
	{
		std::scoped_lock lock(m_Mutex);
		const SnapshotSlot* slot = FindSnapshot(index, generation);
		return slot != nullptr ? slot->Intensity : 0.0f;
	}

	void EngineImpl::ApplySnapshots(const std::vector<SnapshotEntry>& entries, uint32_t fadeFrames)
	{
		std::vector<uint16_t> buses;
		for (const SnapshotEntry& entry : entries)
			if (std::find(buses.begin(), buses.end(), entry.Bus) == buses.end())
				buses.push_back(entry.Bus);

		for (uint16_t bus : buses)
		{
			float volumeDb = 0.0f;
			float lowPass = 0.0f;
			for (const SnapshotSlot& slot : m_Snapshots)
			{
				if (!slot.Active)
					continue;
				for (const SnapshotEntry& entry : slot.Entries)
				{
					if (entry.Bus != bus)
						continue;
					volumeDb += entry.VolumeDb * slot.Intensity;
					if (entry.LowPassHz > 0.0f && slot.Intensity > 0.0f)
					{
						const float cutoff = MaxLowPassHz * std::pow(std::min(entry.LowPassHz, MaxLowPassHz) / MaxLowPassHz, slot.Intensity);
						lowPass = lowPass > 0.0f ? std::min(lowPass, cutoff) : cutoff;
					}
				}
			}
			Command command;
			command.Type = CommandType::SetBusSnapshot;
			command.Index = bus;
			command.Snapshot = { DecibelsToLinear(std::clamp(volumeDb, -120.0f, 24.0f)), lowPass, fadeFrames };
			m_Pending.push_back(command);
		}
	}

	std::vector<AudioDeviceInfo> EngineImpl::GetOutputDevices() const
	{
		std::scoped_lock lock(m_Mutex);
		return m_Device ? m_Device->GetDevices() : std::vector<AudioDeviceInfo>();
	}

	bool EngineImpl::SetOutputDevice(const std::string& name)
	{
		std::scoped_lock lock(m_Mutex);
		if (!m_Device)
			return false;
		const bool opened = m_Device->Open(name, m_SampleRate, m_Config.BufferSizeMs);
		return opened && (name.empty() || m_Device->GetName() == name);
	}

	std::string EngineImpl::GetOutputDeviceName() const
	{
		std::scoped_lock lock(m_Mutex);
		return m_Device ? m_Device->GetName() : std::string();
	}

	AudioStats EngineImpl::GetStats() const
	{
		std::scoped_lock lock(m_Mutex);
		AudioStats stats;
		stats.PlayingVoices = uint32_t(m_ActiveSlots.size());
		stats.AudibleVoices = std::min(m_Mixer->GetAudibleVoiceCount(), stats.PlayingVoices);
		stats.VirtualVoices = stats.PlayingVoices - stats.AudibleVoices;
		stats.Streams = m_Mixer->GetStreamCount();
		stats.CpuLoad = m_Mixer->GetCpuLoad();
		stats.StreamUnderruns = m_Mixer->GetUnderruns();
		stats.SampleRate = m_SampleRate;
		for (const std::weak_ptr<SoundAsset>& weak : m_Assets)
		{
			const std::shared_ptr<SoundAsset> asset = weak.lock();
			if (asset && asset->State.load(std::memory_order_acquire) == LoadState::Ready)
				stats.SoundMemory += asset->GetMemoryBytes();
		}
		return stats;
	}

	void EngineImpl::Render(float* output, uint32_t frames)
	{
		if (!m_Config.NoOutput)
		{
			TIMBRE_LOG_ERROR("AudioEngine::Render() only works with AudioEngineConfig::NoOutput");
			std::fill(output, output + size_t(frames) * 2, 0.0f);
			return;
		}
		// Without a streaming thread, streams are decoded in step with the mix, which also makes
		// offline rendering deterministic.
		for (uint32_t done = 0; done < frames;)
		{
			const uint32_t count = std::min(Mixer::BlockFrames, frames - done);
			m_Streamer->Pump();
			m_Mixer->Render(output + size_t(done) * 2, count);
			done += count;
		}
	}
}
