#include "TimbreAudio/Snapshot.h"
#include "TimbreAudio/Internal/EngineImpl.h"

namespace Timbre
{
	bool Snapshot::IsActive() const { return m_Engine != nullptr && m_Engine->IsSnapshotActive(m_Index, m_Generation); }

	void Snapshot::Stop(float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->StopSnapshot(m_Index, m_Generation, fadeSeconds);
	}

	void Snapshot::SetIntensity(float intensity, float fadeSeconds) const
	{
		if (m_Engine != nullptr)
			m_Engine->SetSnapshotIntensity(m_Index, m_Generation, intensity, fadeSeconds);
	}

	float Snapshot::GetIntensity() const { return m_Engine != nullptr ? m_Engine->GetSnapshotIntensity(m_Index, m_Generation) : 0.0f; }
}
