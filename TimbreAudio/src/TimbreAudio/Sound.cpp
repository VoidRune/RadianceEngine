#include "TimbreAudio/Sound.h"
#include "TimbreAudio/Internal/EngineImpl.h"
#include "TimbreAudio/Internal/SoundAsset.h"

namespace Timbre
{
	namespace
	{
		const Internal::AudioFormat* GetReadyFormat(const std::shared_ptr<Internal::SoundData>& data)
		{
			if (data == nullptr || data->Asset->State.load(std::memory_order_acquire) != LoadState::Ready)
				return nullptr;
			return &data->Asset->Format;
		}
	}

	Voice Sound::Play(const PlayParams& params) const
	{
		return m_Data != nullptr ? m_Data->Engine->Play(*this, params) : Voice();
	}

	void Sound::StopAll(float fadeSeconds) const
	{
		if (m_Data != nullptr)
			m_Data->Engine->StopSound(*m_Data, fadeSeconds);
	}

	uint32_t Sound::GetInstanceCount() const
	{
		return m_Data != nullptr ? m_Data->Engine->GetInstanceCount(*m_Data) : 0;
	}

	LoadState Sound::GetState() const
	{
		return m_Data != nullptr ? m_Data->Asset->State.load(std::memory_order_acquire) : LoadState::Failed;
	}

	float Sound::GetDuration() const
	{
		const Internal::AudioFormat* format = GetReadyFormat(m_Data);
		return format != nullptr && format->SampleRate > 0 ? float(double(format->FrameCount) / format->SampleRate) : 0.0f;
	}

	uint64_t Sound::GetFrameCount() const
	{
		const Internal::AudioFormat* format = GetReadyFormat(m_Data);
		return format != nullptr ? format->FrameCount : 0;
	}

	uint32_t Sound::GetSampleRate() const
	{
		const Internal::AudioFormat* format = GetReadyFormat(m_Data);
		return format != nullptr ? format->SampleRate : 0;
	}

	uint32_t Sound::GetChannels() const
	{
		const Internal::AudioFormat* format = GetReadyFormat(m_Data);
		return format != nullptr ? format->Channels : 0;
	}

	LoadMode Sound::GetMode() const
	{
		return m_Data != nullptr ? m_Data->Asset->Mode : LoadMode::Decompress;
	}

	const std::string& Sound::GetPath() const
	{
		static const std::string empty;
		return m_Data != nullptr ? m_Data->Asset->Path : empty;
	}

	const SoundDesc& Sound::GetDesc() const
	{
		static const SoundDesc defaults;
		return m_Data != nullptr ? m_Data->Desc : defaults;
	}
}
