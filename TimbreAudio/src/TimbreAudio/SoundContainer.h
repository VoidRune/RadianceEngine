#pragma once
#include "TimbreAudio/Sound.h"
#include "TimbreAudio/Voice.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace Timbre
{
	namespace Internal
	{
		struct ContainerState;
	}

	enum class ContainerMode
	{
		Random,
		Shuffle,
		Sequence,
	};

	class SoundContainer
	{
	public:
		SoundContainer() = default;
		explicit SoundContainer(std::vector<Sound> sounds, ContainerMode mode = ContainerMode::Random);

		bool IsValid() const { return m_State != nullptr; }
		explicit operator bool() const { return IsValid(); }

		Voice Play(const PlayParams& params = {}) const;
		Sound Next() const;
		size_t GetCount() const;

	private:
		std::shared_ptr<Internal::ContainerState> m_State;
	};
}
