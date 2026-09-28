#include "TimbreAudio/SoundContainer.h"
#include <algorithm>
#include <mutex>
#include <numeric>
#include <random>

namespace Timbre
{
	namespace Internal
	{
		struct ContainerState
		{
			std::vector<Sound> Sounds;
			ContainerMode Mode = ContainerMode::Random;
			std::mutex Mutex;
			std::vector<uint32_t> Order;
			size_t Position = 0;
			int64_t Last = -1;
			std::minstd_rand Random;
		};
	}

	SoundContainer::SoundContainer(std::vector<Sound> sounds, ContainerMode mode)
	{
		std::erase_if(sounds, [](const Sound& sound) { return !sound.IsValid(); });
		if (sounds.empty())
			return;
		m_State = std::make_shared<Internal::ContainerState>();
		m_State->Sounds = std::move(sounds);
		m_State->Mode = mode;
		m_State->Random.seed(std::random_device{}());
	}

	Sound SoundContainer::Next() const
	{
		if (!m_State)
			return {};
		Internal::ContainerState& state = *m_State;
		std::scoped_lock lock(state.Mutex);
		const uint32_t count = uint32_t(state.Sounds.size());
		uint32_t index = 0;
		switch (state.Mode)
		{
		case ContainerMode::Sequence:
			index = uint32_t(state.Position++ % count);
			break;
		case ContainerMode::Shuffle:
			if (state.Position >= state.Order.size())
			{
				state.Order.resize(count);
				std::iota(state.Order.begin(), state.Order.end(), 0u);
				std::shuffle(state.Order.begin(), state.Order.end(), state.Random);
				if (count > 1 && int64_t(state.Order[0]) == state.Last)
					std::swap(state.Order[0], state.Order[1 + state.Random() % (count - 1)]);
				state.Position = 0;
			}
			index = state.Order[state.Position++];
			break;
		case ContainerMode::Random:
			if (count > 1 && state.Last >= 0)
			{
				index = std::uniform_int_distribution<uint32_t>(0, count - 2)(state.Random);
				if (index >= uint32_t(state.Last))
					index++;
			}
			else
				index = std::uniform_int_distribution<uint32_t>(0, count - 1)(state.Random);
			break;
		}
		state.Last = index;
		return state.Sounds[index];
	}

	Voice SoundContainer::Play(const PlayParams& params) const
	{
		const Sound sound = Next();
		return sound.IsValid() ? sound.Play(params) : Voice();
	}

	size_t SoundContainer::GetCount() const
	{
		return m_State ? m_State->Sounds.size() : 0;
	}
}
