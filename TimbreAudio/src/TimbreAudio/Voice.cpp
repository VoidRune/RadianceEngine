#include "TimbreAudio/Voice.h"
#include "TimbreAudio/Internal/EngineImpl.h"
#include "TimbreAudio/Sound.h"

namespace Timbre
{
	bool Voice::IsPlaying() const { return m_Engine != nullptr && m_Engine->IsVoicePlaying(m_Index, m_Generation); }
	bool Voice::IsPaused() const { return m_Engine != nullptr && m_Engine->IsVoicePaused(m_Index, m_Generation); }

	void Voice::Stop(float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->StopVoice(m_Index, m_Generation, fadeSeconds);
	}

	void Voice::SetPaused(bool paused, float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoicePaused(m_Index, m_Generation, paused, fadeSeconds);
	}

	void Voice::SetVolume(float volume, float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceVolume(m_Index, m_Generation, volume, fadeSeconds);
	}

	void Voice::SetPitch(float pitch) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoicePitch(m_Index, m_Generation, pitch);
	}

	void Voice::SetPan(float pan) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoicePan(m_Index, m_Generation, pan);
	}

	void Voice::SetLooping(bool looping) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceLooping(m_Index, m_Generation, looping);
	}

	void Voice::SetPosition(const Vec3& position) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceVector(m_Index, m_Generation, Internal::CommandType::SetVoicePosition, position);
	}

	void Voice::SetVelocity(const Vec3& velocity) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceVector(m_Index, m_Generation, Internal::CommandType::SetVoiceVelocity, velocity);
	}

	void Voice::SetDirection(const Vec3& direction) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceVector(m_Index, m_Generation, Internal::CommandType::SetVoiceDirection, direction);
	}

	void Voice::SetLowPass(float cutoffHz) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceFilter(m_Index, m_Generation, Internal::CommandType::SetVoiceLowPass, cutoffHz);
	}

	void Voice::SetHighPass(float cutoffHz) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceFilter(m_Index, m_Generation, Internal::CommandType::SetVoiceHighPass, cutoffHz);
	}

	void Voice::SetSend(const Bus& bus, float level, float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetVoiceSend(m_Index, m_Generation, bus, level, fadeSeconds);
	}

	float Voice::GetSend(const Bus& bus) const { return m_Engine != nullptr ? m_Engine->GetVoiceSend(m_Index, m_Generation, bus) : 0.0f; }

	void Voice::Seek(float seconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SeekVoice(m_Index, m_Generation, seconds);
	}

	void Voice::AddEffect(std::shared_ptr<Effect> effect) const
	{
		if (m_Engine != nullptr)
			m_Engine->AddVoiceEffect(m_Index, m_Generation, std::move(effect));
	}

	void Voice::RemoveEffect(const std::shared_ptr<Effect>& effect) const
	{
		if (m_Engine != nullptr)
			m_Engine->RemoveVoiceEffect(m_Index, m_Generation, effect);
	}

	float Voice::GetVolume() const { return m_Engine != nullptr ? m_Engine->GetVoiceVolume(m_Index, m_Generation) : 0.0f; }
	float Voice::GetPitch() const { return m_Engine != nullptr ? m_Engine->GetVoicePitch(m_Index, m_Generation) : 0.0f; }
	float Voice::GetPan() const { return m_Engine != nullptr ? m_Engine->GetVoicePan(m_Index, m_Generation) : 0.0f; }
	bool Voice::IsLooping() const { return m_Engine != nullptr && m_Engine->IsVoiceLooping(m_Index, m_Generation); }
	float Voice::GetTime() const { return m_Engine != nullptr ? m_Engine->GetVoiceTime(m_Index, m_Generation) : 0.0f; }
	Sound Voice::GetSound() const { return m_Engine != nullptr ? m_Engine->GetVoiceSound(m_Index, m_Generation) : Sound(); }
}
