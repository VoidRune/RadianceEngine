#include "TimbreAudio/Bus.h"
#include "TimbreAudio/Internal/EngineImpl.h"

namespace Timbre
{
	void Bus::SetVolume(float volume, float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusVolume(m_Index, volume, fadeSeconds);
	}

	float Bus::GetVolume() const { return m_Engine != nullptr ? m_Engine->GetBusVolume(m_Index) : 0.0f; }

	void Bus::SetMuted(bool muted) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusMuted(m_Index, muted);
	}

	bool Bus::IsMuted() const { return m_Engine != nullptr && m_Engine->IsBusMuted(m_Index); }

	void Bus::SetPaused(bool paused) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusPaused(m_Index, paused);
	}

	bool Bus::IsPaused() const { return m_Engine != nullptr && m_Engine->IsBusPaused(m_Index); }

	void Bus::SetPitch(float pitch) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusPitch(m_Index, pitch);
	}

	float Bus::GetPitch() const { return m_Engine != nullptr ? m_Engine->GetBusPitch(m_Index) : 1.0f; }

	void Bus::SetLowPass(float cutoffHz, float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusLowPass(m_Index, cutoffHz, fadeSeconds);
	}

	float Bus::GetLowPass() const { return m_Engine != nullptr ? m_Engine->GetBusLowPass(m_Index) : 0.0f; }

	void Bus::SetDucking(const DuckingSettings& settings) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusDucking(m_Index, settings);
	}

	void Bus::ClearDucking() const
	{
		if (m_Engine != nullptr)
			m_Engine->SetBusDucking(m_Index, DuckingSettings{});
	}

	void Bus::AddEffect(std::shared_ptr<Effect> effect) const
	{
		if (m_Engine != nullptr)
			m_Engine->AddBusEffect(m_Index, std::move(effect));
	}

	void Bus::RemoveEffect(const std::shared_ptr<Effect>& effect) const
	{
		if (m_Engine != nullptr)
			m_Engine->RemoveBusEffect(m_Index, effect);
	}

	void Bus::ClearEffects() const
	{
		if (m_Engine != nullptr)
			m_Engine->ClearBusEffects(m_Index);
	}

	void Bus::StopAll(float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->StopBus(m_Index, fadeSeconds);
	}

	std::string Bus::GetName() const { return m_Engine != nullptr ? m_Engine->GetBusName(m_Index) : std::string(); }
	Bus Bus::GetParent() const { return m_Engine != nullptr ? m_Engine->GetBusParent(m_Index) : Bus(); }
}
