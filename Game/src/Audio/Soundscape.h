#pragma once
#include "Audio/SoundBank.h"
#include <GameShared/Core/Math.h>
#include <GameShared/Core/Random.h>
#include <GameShared/World/Level.h>
#include <TimbreAudio/AudioEngine.h>
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	enum class AmbienceScene : uint8_t
	{
		None,
		Menu,
		Hub,
		Town,
		Caves,
		Crucible,
		Ashlands,
		Caldera,
		FrozenLake,
		Count,
	};

	class Soundscape
	{
	public:
		Soundscape(Timbre::AudioEngine& engine, Timbre::Bus bus, const SoundBank& bank);

		void SetScene(AmbienceScene scene, const Level* level);
		AmbienceScene GetScene() const { return m_Scene; }
		void Update(float dt, Vec2 focus, float ground);
		void SetStorm(float intensity) { m_StormTarget = intensity; }
		void SetHeartbeat(float intensity) { m_HeartTarget = intensity; }

	private:
		struct Emitter
		{
			Vec2 Position;
			float Height = 0.0f;
			float Volume = 1.0f;
			Timbre::Voice Voice;
		};

		void UpdateEmitters(Vec2 focus);
		void PlayDistantCall(Vec2 focus, float ground);
		void UpdateLayer(Timbre::Voice& voice, float& level, float target, float dt, const Timbre::Sound* sound, Cue cue);

		const SoundBank& m_Bank;
		std::array<Timbre::Sound, size_t(AmbienceScene::Count)> m_Beds;
		Timbre::Sound m_Storm;
		AmbienceScene m_Scene = AmbienceScene::None;
		Timbre::Voice m_Bed;
		Timbre::Voice m_StormVoice;
		Timbre::Voice m_HeartVoice;
		std::vector<Emitter> m_Emitters;
		Random m_Random{ 0x5EEDu };
		float m_NextCall = 8.0f;
		float m_StormLevel = 0.0f;
		float m_StormTarget = 0.0f;
		float m_HeartLevel = 0.0f;
		float m_HeartTarget = 0.0f;
	};
}
