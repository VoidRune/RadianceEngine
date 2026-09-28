#include "TimbreAudio/AudioEngine.h"
#include "TimbreAudio/Internal/EngineImpl.h"

namespace Timbre
{
	AudioEngine::AudioEngine(const AudioEngineConfig& config)
		: m_Impl(std::make_unique<Internal::EngineImpl>(config))
	{
	}

	AudioEngine::~AudioEngine() = default;

	void AudioEngine::Update() { m_Impl->Update(); }

	Sound AudioEngine::LoadSound(const std::string& path, const SoundDesc& desc) { return m_Impl->LoadSound(path, desc); }
	Sound AudioEngine::LoadSound(std::vector<uint8_t> fileData, const SoundDesc& desc) { return m_Impl->LoadSound(std::move(fileData), desc); }
	Sound AudioEngine::CreateSound(std::span<const float> samples, uint32_t channels, uint32_t sampleRate, const SoundDesc& desc)
	{
		return m_Impl->CreateSound(samples, channels, sampleRate, desc);
	}

	Voice AudioEngine::Play(const Sound& sound, const PlayParams& params) { return m_Impl->Play(sound, params); }
	void AudioEngine::StopAll(float fadeSeconds) { m_Impl->StopAll(fadeSeconds); }

	Bus AudioEngine::GetMasterBus() const { return m_Impl->GetMasterBus(); }
	Bus AudioEngine::CreateBus(const std::string& name, Bus parent) { return m_Impl->CreateBus(name, parent); }
	Bus AudioEngine::FindBus(std::string_view name) const { return m_Impl->FindBus(name); }

	void AudioEngine::SetListener(const Listener& listener) { m_Impl->SetListener(listener); }
	Listener AudioEngine::GetListener() const { return m_Impl->GetListener(); }
	void AudioEngine::SetSpatialMode(SpatialMode mode) { m_Impl->SetSpatialMode(mode); }
	SpatialMode AudioEngine::GetSpatialMode() const { return m_Impl->GetSpatialMode(); }

	Snapshot AudioEngine::StartSnapshot(const SnapshotDesc& desc, float fadeSeconds, float intensity) { return m_Impl->StartSnapshot(desc, fadeSeconds, intensity); }

	std::vector<AudioDeviceInfo> AudioEngine::GetOutputDevices() const { return m_Impl->GetOutputDevices(); }
	bool AudioEngine::SetOutputDevice(const std::string& name) { return m_Impl->SetOutputDevice(name); }
	std::string AudioEngine::GetOutputDeviceName() const { return m_Impl->GetOutputDeviceName(); }

	uint32_t AudioEngine::GetSampleRate() const { return m_Impl->GetSampleRate(); }
	AudioStats AudioEngine::GetStats() const { return m_Impl->GetStats(); }

	void AudioEngine::Render(float* output, uint32_t frames) { m_Impl->Render(output, frames); }
}
