#include "Soundscape.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float EmitterOn = 17.0f;
		constexpr float EmitterOff = 21.0f;
		constexpr size_t MaxEmitters = 6;
		constexpr float LayerRate = 1.2f;

		struct BedDef
		{
			const char* Path = nullptr;
			float Volume = 1.0f;
		};

		constexpr BedDef Beds[] = {
			{},
			{ "res/Audio/Ambience/Menu.ogg", 1.0f },
			{ "res/Audio/Ambience/Hub.ogg", 0.75f },
			{ "res/Audio/Ambience/Town.ogg", 0.85f },
			{ "res/Audio/Ambience/Caves.ogg", 0.8f },
			{ "res/Audio/Ambience/Crucible.ogg", 0.75f },
			{ "res/Audio/Ambience/Ashlands.ogg", 0.85f },
		};

		float FireVolume(PropId id)
		{
			switch (id)
			{
			case PropId::Forge:
				return 1.0f;
			case PropId::Fireplace:
				return 0.7f;
			case PropId::Brazier:
				return 0.55f;
			case PropId::WallTorch:
				return 0.3f;
			default:
				return 0.0f;
			}
		}
	}

	Soundscape::Soundscape(Timbre::AudioEngine& engine, Timbre::Bus bus, const SoundBank& bank)
		: m_Bank(bank)
	{
		static_assert(std::size(Beds) == size_t(AmbienceScene::Count));
		Timbre::SoundDesc desc;
		desc.Mode = Timbre::LoadMode::Stream;
		desc.Bus = bus;
		desc.Looping = true;
		desc.Priority = 250;
		for (size_t i = 0; i < m_Beds.size(); i++)
			if (Beds[i].Path)
			{
				desc.Volume = Beds[i].Volume;
				m_Beds[i] = engine.LoadSound(Beds[i].Path, desc);
			}
		desc.Volume = 1.0f;
		m_Storm = engine.LoadSound("res/Audio/Ambience/Storm.ogg", desc);
	}

	void Soundscape::SetScene(AmbienceScene scene, const Level* level)
	{
		for (Emitter& emitter : m_Emitters)
			emitter.Voice.Stop(1.0f);
		m_Emitters.clear();
		if (level)
			for (const PlacedProp& prop : level->Props)
				if (const float volume = FireVolume(prop.Id); volume > 0.0f)
					m_Emitters.push_back({ prop.Position, level->Map.GetHeight(prop.Position) + 1.0f, volume });
		m_NextCall = 6.0f + 10.0f * m_Random.Float();
		m_StormTarget = 0.0f;
		if (scene == m_Scene)
			return;
		m_Scene = scene;
		m_Bed.Stop(2.5f);
		m_Bed = {};
		if (const Timbre::Sound& bed = m_Beds[size_t(scene)])
			m_Bed = bed.Play({ .FadeIn = 3.0f, .StartTime = bed.GetDuration() * m_Random.Float() });
	}

	void Soundscape::Update(float dt, Vec2 focus, float ground)
	{
		UpdateEmitters(focus);
		m_NextCall -= dt;
		if (m_NextCall <= 0.0f)
			PlayDistantCall(focus, ground);
		UpdateLayer(m_StormVoice, m_StormLevel, m_StormTarget, dt, &m_Storm, Cue::Count);
		UpdateLayer(m_HeartVoice, m_HeartLevel, m_HeartTarget, dt, nullptr, Cue::Heartbeat);
		if (m_HeartVoice.IsPlaying())
			m_HeartVoice.SetPitch(0.92f + 0.22f * m_HeartLevel);
	}

	void Soundscape::UpdateEmitters(Vec2 focus)
	{
		size_t active = 0;
		std::vector<std::pair<float, size_t>> candidates;
		for (size_t i = 0; i < m_Emitters.size(); i++)
		{
			Emitter& emitter = m_Emitters[i];
			const float distance = Length(emitter.Position - focus);
			if (emitter.Voice.IsPlaying())
			{
				if (distance > EmitterOff)
				{
					emitter.Voice.Stop(1.5f);
					emitter.Voice = {};
				}
				else
					active++;
			}
			else if (distance < EmitterOn)
				candidates.push_back({ distance, i });
		}
		std::sort(candidates.begin(), candidates.end());
		for (const auto& [distance, index] : candidates)
		{
			if (active >= MaxEmitters)
				break;
			Emitter& emitter = m_Emitters[index];
			emitter.Voice = m_Bank.Play(Cue::Campfire, {
				.Volume = emitter.Volume,
				.Position = Timbre::Vec3(emitter.Position.x, emitter.Height, emitter.Position.y),
				.FadeIn = 1.5f,
				.StartTime = 70.0f * m_Random.Float(),
			});
			active++;
		}
	}

	void Soundscape::PlayDistantCall(Vec2 focus, float ground)
	{
		const float angle = m_Random.Float() * 6.2831853f;
		const auto around = [&](float distance, float height)
		{
			return Timbre::Vec3(focus.x + std::cos(angle) * distance, ground + height, focus.y + std::sin(angle) * distance);
		};
		switch (m_Scene)
		{
		case AmbienceScene::Town:
			m_Bank.Play(Cue::Crow, { .Volume = 0.7f + 0.3f * m_Random.Float(), .Position = around(13.0f + 9.0f * m_Random.Float(), 6.0f) });
			m_NextCall = 9.0f + 16.0f * m_Random.Float();
			break;
		case AmbienceScene::Hub:
			m_Bank.Play(Cue::Crow, { .Volume = 0.45f, .Pitch = 0.95f, .Position = around(20.0f, 8.0f) });
			m_NextCall = 30.0f + 30.0f * m_Random.Float();
			break;
		case AmbienceScene::Caves:
			m_Bank.Play(Cue::Rumble, { .Volume = 0.22f, .Pitch = 0.7f + 0.2f * m_Random.Float(), .Position = around(26.0f, 2.0f) });
			m_NextCall = 35.0f + 40.0f * m_Random.Float();
			break;
		case AmbienceScene::Ashlands:
			m_Bank.Play(Cue::Thunder, { .Volume = 0.3f, .Pitch = 0.75f + 0.1f * m_Random.Float(), .Position = around(32.0f, 12.0f) });
			m_NextCall = 25.0f + 35.0f * m_Random.Float();
			break;
		default:
			m_NextCall = 30.0f;
			break;
		}
	}

	void Soundscape::UpdateLayer(Timbre::Voice& voice, float& level, float target, float dt, const Timbre::Sound* sound, Cue cue)
	{
		target = std::clamp(target, 0.0f, 1.0f);
		level += std::clamp(target - level, -LayerRate * dt, LayerRate * dt);
		if (level <= 0.01f)
		{
			if (voice.IsPlaying())
				voice.Stop(0.4f);
			voice = {};
			return;
		}
		if (voice.IsPlaying())
		{
			voice.SetVolume(level, 0.1f);
			return;
		}
		voice = sound ? sound->Play({ .Volume = level, .FadeIn = 0.5f }) : m_Bank.Play(cue, { .Volume = level, .FadeIn = 0.5f });
	}
}
