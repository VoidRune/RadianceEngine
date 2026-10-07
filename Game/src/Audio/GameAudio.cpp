#include "GameAudio.h"
#include "Audio/SoundSynth.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Content/Items.h>
#include <algorithm>
#include <vector>

namespace Game
{
	namespace
	{
		struct SoundRecipe
		{
			Sfx Id;
			float Volume;
			uint32_t MaxInstances;
			float PitchVariation;
			std::vector<SynthLayer> Layers;
		};

		std::vector<SoundRecipe> Recipes()
		{
			using enum Waveform;
			return {
				{ Sfx::Bolt, 0.35f, 6, 1.0f, {
					{ .Wave = Square, .Duration = 0.16f, .FrequencyStart = 900.0f, .FrequencyEnd = 300.0f, .Volume = 0.6f, .Release = 0.14f, .LowPassStart = 5000.0f, .LowPassEnd = 1500.0f, .Duty = 0.35f },
					{ .Wave = Sine, .Duration = 0.1f, .FrequencyStart = 1800.0f, .FrequencyEnd = 600.0f, .Volume = 0.3f, .Release = 0.1f } } },
				{ Sfx::Arrow, 0.35f, 8, 1.5f, {
					{ .Wave = Noise, .Duration = 0.09f, .FrequencyStart = 6000.0f, .FrequencyEnd = 2000.0f, .Volume = 0.7f, .Release = 0.08f, .LowPassStart = 8000.0f, .LowPassEnd = 2500.0f },
					{ .Wave = Triangle, .Duration = 0.06f, .FrequencyStart = 1300.0f, .FrequencyEnd = 900.0f, .Volume = 0.25f, .Release = 0.06f } } },
				{ Sfx::EnemyShot, 0.4f, 6, 1.0f, {
					{ .Wave = Sine, .Duration = 0.18f, .FrequencyStart = 520.0f, .FrequencyEnd = 220.0f, .Volume = 0.7f, .Release = 0.15f, .VibratoDepth = 2.0f, .VibratoRate = 30.0f },
					{ .Wave = Noise, .Duration = 0.08f, .FrequencyStart = 2000.0f, .FrequencyEnd = 800.0f, .Volume = 0.3f, .Release = 0.08f, .LowPassStart = 1500.0f } } },
				{ Sfx::Slash, 0.5f, 6, 1.5f, {
					{ .Wave = Noise, .Duration = 0.18f, .FrequencyStart = 9000.0f, .FrequencyEnd = 3000.0f, .Volume = 0.9f, .Attack = 0.02f, .Release = 0.16f, .LowPassStart = 6000.0f, .LowPassEnd = 900.0f } } },
				{ Sfx::Nova, 0.7f, 4, 0.5f, {
					{ .Wave = Sine, .Duration = 0.45f, .FrequencyStart = 160.0f, .FrequencyEnd = 45.0f, .Volume = 1.0f, .Release = 0.4f },
					{ .Wave = Noise, .Duration = 0.35f, .FrequencyStart = 3000.0f, .FrequencyEnd = 400.0f, .Volume = 0.5f, .Release = 0.3f, .LowPassStart = 2500.0f, .LowPassEnd = 300.0f },
					{ .Wave = Triangle, .Duration = 0.3f, .FrequencyStart = 320.0f, .FrequencyEnd = 90.0f, .Volume = 0.3f, .Release = 0.25f } } },
				{ Sfx::Dash, 0.4f, 4, 1.0f, {
					{ .Wave = Noise, .Duration = 0.2f, .FrequencyStart = 2000.0f, .FrequencyEnd = 6000.0f, .Volume = 0.8f, .Attack = 0.05f, .Release = 0.12f, .LowPassStart = 800.0f, .LowPassEnd = 4000.0f } } },
				{ Sfx::Blink, 0.4f, 4, 0.5f, {
					{ .Wave = Sine, .Duration = 0.14f, .FrequencyStart = 400.0f, .FrequencyEnd = 2400.0f, .Volume = 0.6f, .Release = 0.08f, .VibratoDepth = 1.0f, .VibratoRate = 40.0f },
					{ .Wave = Square, .Duration = 0.1f, .FrequencyStart = 800.0f, .FrequencyEnd = 3200.0f, .Volume = 0.25f, .Release = 0.08f, .LowPassStart = 6000.0f, .Duty = 0.25f } } },
				{ Sfx::Hit, 0.45f, 6, 2.0f, {
					{ .Wave = Noise, .Duration = 0.07f, .FrequencyStart = 1800.0f, .FrequencyEnd = 600.0f, .Volume = 0.8f, .Release = 0.06f, .LowPassStart = 2500.0f, .LowPassEnd = 800.0f },
					{ .Wave = Sine, .Duration = 0.09f, .FrequencyStart = 220.0f, .FrequencyEnd = 90.0f, .Volume = 0.7f, .Release = 0.08f } } },
				{ Sfx::EnemyDeath, 0.55f, 6, 1.5f, {
					{ .Wave = Square, .Duration = 0.32f, .FrequencyStart = 330.0f, .FrequencyEnd = 60.0f, .Volume = 0.6f, .Release = 0.28f, .LowPassStart = 3000.0f, .LowPassEnd = 400.0f },
					{ .Wave = Noise, .Duration = 0.25f, .FrequencyStart = 2500.0f, .FrequencyEnd = 300.0f, .Volume = 0.6f, .Release = 0.22f, .LowPassStart = 4000.0f, .LowPassEnd = 500.0f } } },
				{ Sfx::Hurt, 0.55f, 3, 1.0f, {
					{ .Wave = Saw, .Duration = 0.16f, .FrequencyStart = 240.0f, .FrequencyEnd = 110.0f, .Volume = 0.8f, .Release = 0.12f, .LowPassStart = 1800.0f, .LowPassEnd = 700.0f },
					{ .Wave = Noise, .Duration = 0.08f, .FrequencyStart = 1200.0f, .FrequencyEnd = 500.0f, .Volume = 0.3f, .Release = 0.07f } } },
				{ Sfx::PlayerDeath, 0.7f, 2, 0.0f, {
					{ .Wave = Saw, .Duration = 0.9f, .FrequencyStart = 420.0f, .FrequencyEnd = 55.0f, .Volume = 0.7f, .Release = 0.6f, .LowPassStart = 2500.0f, .LowPassEnd = 300.0f },
					{ .Wave = Sine, .Duration = 0.9f, .FrequencyStart = 210.0f, .FrequencyEnd = 40.0f, .Volume = 0.5f, .Release = 0.6f } } },
				{ Sfx::Windup, 0.3f, 4, 0.5f, {
					{ .Wave = Square, .Duration = 0.32f, .FrequencyStart = 300.0f, .FrequencyEnd = 700.0f, .Volume = 0.45f, .Release = 0.1f, .LowPassStart = 2200.0f, .VibratoDepth = 0.5f, .VibratoRate = 18.0f } } },
				{ Sfx::BruteWindup, 0.6f, 2, 0.0f, {
					{ .Wave = Saw, .Duration = 0.7f, .FrequencyStart = 70.0f, .FrequencyEnd = 140.0f, .Volume = 0.8f, .Release = 0.2f, .LowPassStart = 600.0f, .LowPassEnd = 1200.0f, .VibratoDepth = 1.0f, .VibratoRate = 9.0f },
					{ .Wave = Noise, .Duration = 0.7f, .FrequencyStart = 400.0f, .FrequencyEnd = 900.0f, .Volume = 0.3f, .Release = 0.2f, .LowPassStart = 500.0f } } },
				{ Sfx::Pickup, 0.4f, 4, 0.0f, {
					{ .Wave = Square, .Duration = 0.06f, .FrequencyStart = 880.0f, .FrequencyEnd = 880.0f, .Volume = 0.5f, .Release = 0.02f, .LowPassStart = 5000.0f },
					{ .Wave = Square, .Start = 0.06f, .Duration = 0.08f, .FrequencyStart = 1320.0f, .FrequencyEnd = 1320.0f, .Volume = 0.5f, .Release = 0.05f, .LowPassStart = 5000.0f } } },
				{ Sfx::PowerUp, 0.5f, 2, 0.0f, {
					{ .Wave = Triangle, .Duration = 0.1f, .FrequencyStart = 523.0f, .FrequencyEnd = 523.0f, .Volume = 0.6f, .Release = 0.04f },
					{ .Wave = Triangle, .Start = 0.08f, .Duration = 0.1f, .FrequencyStart = 659.0f, .FrequencyEnd = 659.0f, .Volume = 0.6f, .Release = 0.04f },
					{ .Wave = Triangle, .Start = 0.16f, .Duration = 0.12f, .FrequencyStart = 784.0f, .FrequencyEnd = 784.0f, .Volume = 0.6f, .Release = 0.05f },
					{ .Wave = Triangle, .Start = 0.24f, .Duration = 0.25f, .FrequencyStart = 1046.0f, .FrequencyEnd = 1046.0f, .Volume = 0.6f, .Release = 0.18f },
					{ .Wave = Square, .Start = 0.24f, .Duration = 0.25f, .FrequencyStart = 523.0f, .FrequencyEnd = 523.0f, .Volume = 0.15f, .Release = 0.18f, .LowPassStart = 3000.0f } } },
				{ Sfx::Heal, 0.45f, 2, 0.0f, {
					{ .Wave = Sine, .Duration = 0.25f, .FrequencyStart = 660.0f, .FrequencyEnd = 880.0f, .Volume = 0.6f, .Release = 0.18f },
					{ .Wave = Sine, .Start = 0.05f, .Duration = 0.2f, .FrequencyStart = 1320.0f, .FrequencyEnd = 1320.0f, .Volume = 0.25f, .Release = 0.15f } } },
				{ Sfx::WaveStart, 0.5f, 1, 0.0f, {
					{ .Wave = Saw, .Duration = 0.5f, .FrequencyStart = 220.0f, .FrequencyEnd = 220.0f, .Volume = 0.6f, .Attack = 0.05f, .Release = 0.3f, .LowPassStart = 1200.0f, .LowPassEnd = 2400.0f },
					{ .Wave = Saw, .Duration = 0.5f, .FrequencyStart = 330.0f, .FrequencyEnd = 330.0f, .Volume = 0.45f, .Attack = 0.05f, .Release = 0.3f, .LowPassStart = 1200.0f, .LowPassEnd = 2400.0f },
					{ .Wave = Square, .Duration = 0.5f, .FrequencyStart = 110.0f, .FrequencyEnd = 110.0f, .Volume = 0.3f, .Attack = 0.05f, .Release = 0.3f, .LowPassStart = 600.0f } } },
				{ Sfx::WaveClear, 0.5f, 1, 0.0f, {
					{ .Wave = Triangle, .Duration = 0.15f, .FrequencyStart = 392.0f, .FrequencyEnd = 392.0f, .Volume = 0.6f, .Release = 0.06f },
					{ .Wave = Triangle, .Start = 0.12f, .Duration = 0.15f, .FrequencyStart = 494.0f, .FrequencyEnd = 494.0f, .Volume = 0.6f, .Release = 0.06f },
					{ .Wave = Triangle, .Start = 0.24f, .Duration = 0.15f, .FrequencyStart = 587.0f, .FrequencyEnd = 587.0f, .Volume = 0.6f, .Release = 0.06f },
					{ .Wave = Triangle, .Start = 0.36f, .Duration = 0.45f, .FrequencyStart = 784.0f, .FrequencyEnd = 784.0f, .Volume = 0.6f, .Release = 0.35f },
					{ .Wave = Sine, .Start = 0.36f, .Duration = 0.45f, .FrequencyStart = 1568.0f, .FrequencyEnd = 1568.0f, .Volume = 0.2f, .Release = 0.35f } } },
				{ Sfx::Defeat, 0.55f, 1, 0.0f, {
					{ .Wave = Saw, .Duration = 0.35f, .FrequencyStart = 330.0f, .FrequencyEnd = 330.0f, .Volume = 0.5f, .Release = 0.1f, .LowPassStart = 1500.0f },
					{ .Wave = Saw, .Start = 0.3f, .Duration = 0.35f, .FrequencyStart = 277.0f, .FrequencyEnd = 277.0f, .Volume = 0.5f, .Release = 0.1f, .LowPassStart = 1300.0f },
					{ .Wave = Saw, .Start = 0.6f, .Duration = 0.9f, .FrequencyStart = 220.0f, .FrequencyEnd = 196.0f, .Volume = 0.55f, .Release = 0.7f, .LowPassStart = 1100.0f, .LowPassEnd = 400.0f } } },
				{ Sfx::Spawn, 0.25f, 3, 2.0f, {
					{ .Wave = Sine, .Duration = 0.15f, .FrequencyStart = 90.0f, .FrequencyEnd = 180.0f, .Volume = 0.5f, .Release = 0.1f },
					{ .Wave = Noise, .Duration = 0.12f, .FrequencyStart = 600.0f, .FrequencyEnd = 200.0f, .Volume = 0.2f, .Release = 0.1f, .LowPassStart = 800.0f } } },
				{ Sfx::ItemPickup, 0.45f, 3, 0.0f, {
					{ .Wave = Triangle, .Duration = 0.07f, .FrequencyStart = 740.0f, .FrequencyEnd = 740.0f, .Volume = 0.6f, .Release = 0.03f },
					{ .Wave = Triangle, .Start = 0.06f, .Duration = 0.12f, .FrequencyStart = 1110.0f, .FrequencyEnd = 1110.0f, .Volume = 0.6f, .Release = 0.08f },
					{ .Wave = Noise, .Duration = 0.05f, .FrequencyStart = 7000.0f, .FrequencyEnd = 4000.0f, .Volume = 0.25f, .Release = 0.04f, .LowPassStart = 9000.0f } } },
				{ Sfx::RareDrop, 0.55f, 2, 0.0f, {
					{ .Wave = Sine, .Duration = 0.9f, .FrequencyStart = 1046.0f, .FrequencyEnd = 1046.0f, .Volume = 0.5f, .Attack = 0.01f, .Release = 0.8f, .VibratoDepth = 0.3f, .VibratoRate = 6.0f },
					{ .Wave = Sine, .Start = 0.08f, .Duration = 0.85f, .FrequencyStart = 1568.0f, .FrequencyEnd = 1568.0f, .Volume = 0.4f, .Release = 0.75f, .VibratoDepth = 0.3f, .VibratoRate = 6.5f },
					{ .Wave = Triangle, .Start = 0.16f, .Duration = 0.8f, .FrequencyStart = 2093.0f, .FrequencyEnd = 2093.0f, .Volume = 0.3f, .Release = 0.7f },
					{ .Wave = Sine, .Duration = 0.5f, .FrequencyStart = 130.0f, .FrequencyEnd = 98.0f, .Volume = 0.5f, .Release = 0.4f } } },
				{ Sfx::Upgrade, 0.55f, 1, 0.0f, {
					{ .Wave = Square, .Duration = 0.09f, .FrequencyStart = 587.0f, .FrequencyEnd = 587.0f, .Volume = 0.35f, .Release = 0.04f, .LowPassStart = 3500.0f, .Duty = 0.3f },
					{ .Wave = Square, .Start = 0.08f, .Duration = 0.09f, .FrequencyStart = 740.0f, .FrequencyEnd = 740.0f, .Volume = 0.35f, .Release = 0.04f, .LowPassStart = 3500.0f, .Duty = 0.3f },
					{ .Wave = Square, .Start = 0.16f, .Duration = 0.09f, .FrequencyStart = 880.0f, .FrequencyEnd = 880.0f, .Volume = 0.35f, .Release = 0.04f, .LowPassStart = 3500.0f, .Duty = 0.3f },
					{ .Wave = Triangle, .Start = 0.24f, .Duration = 0.5f, .FrequencyStart = 1175.0f, .FrequencyEnd = 1175.0f, .Volume = 0.6f, .Release = 0.4f, .VibratoDepth = 0.2f, .VibratoRate = 7.0f },
					{ .Wave = Sine, .Start = 0.24f, .Duration = 0.5f, .FrequencyStart = 587.0f, .FrequencyEnd = 587.0f, .Volume = 0.3f, .Release = 0.4f } } },
				{ Sfx::Equip, 0.45f, 2, 0.5f, {
					{ .Wave = Noise, .Duration = 0.16f, .FrequencyStart = 8000.0f, .FrequencyEnd = 5000.0f, .Volume = 0.5f, .Attack = 0.01f, .Release = 0.14f, .LowPassStart = 9000.0f, .LowPassEnd = 6000.0f },
					{ .Wave = Square, .Duration = 0.12f, .FrequencyStart = 1900.0f, .FrequencyEnd = 1700.0f, .Volume = 0.2f, .Release = 0.1f, .LowPassStart = 6000.0f, .Duty = 0.2f },
					{ .Wave = Sine, .Duration = 0.08f, .FrequencyStart = 300.0f, .FrequencyEnd = 180.0f, .Volume = 0.4f, .Release = 0.06f } } },
				{ Sfx::Crit, 0.5f, 4, 1.5f, {
					{ .Wave = Square, .Duration = 0.09f, .FrequencyStart = 1500.0f, .FrequencyEnd = 600.0f, .Volume = 0.45f, .Release = 0.08f, .LowPassStart = 6000.0f, .LowPassEnd = 2000.0f, .Duty = 0.3f },
					{ .Wave = Noise, .Duration = 0.1f, .FrequencyStart = 5000.0f, .FrequencyEnd = 1500.0f, .Volume = 0.6f, .Release = 0.09f, .LowPassStart = 7000.0f, .LowPassEnd = 2000.0f },
					{ .Wave = Sine, .Duration = 0.12f, .FrequencyStart = 260.0f, .FrequencyEnd = 80.0f, .Volume = 0.6f, .Release = 0.1f } } },
				{ Sfx::Door, 0.5f, 2, 0.4f, {
					{ .Wave = Noise, .Duration = 0.35f, .FrequencyStart = 600.0f, .FrequencyEnd = 2400.0f, .Volume = 0.55f, .Attack = 0.05f, .Release = 0.25f, .LowPassStart = 900.0f, .LowPassEnd = 3000.0f },
					{ .Wave = Sine, .Duration = 0.25f, .FrequencyStart = 140.0f, .FrequencyEnd = 70.0f, .Volume = 0.7f, .Release = 0.2f },
					{ .Wave = Triangle, .Start = 0.05f, .Duration = 0.3f, .FrequencyStart = 520.0f, .FrequencyEnd = 880.0f, .Volume = 0.25f, .Release = 0.2f, .VibratoDepth = 0.4f, .VibratoRate = 12.0f } } },
				{ Sfx::Fireball, 0.5f, 4, 0.6f, {
					{ .Wave = Noise, .Duration = 0.4f, .FrequencyStart = 900.0f, .FrequencyEnd = 300.0f, .Volume = 0.8f, .Attack = 0.03f, .Release = 0.3f, .LowPassStart = 1400.0f, .LowPassEnd = 400.0f },
					{ .Wave = Sine, .Duration = 0.3f, .FrequencyStart = 230.0f, .FrequencyEnd = 120.0f, .Volume = 0.5f, .Release = 0.25f } } },
				{ Sfx::Explosion, 0.7f, 4, 0.6f, {
					{ .Wave = Noise, .Duration = 0.55f, .FrequencyStart = 2500.0f, .FrequencyEnd = 200.0f, .Volume = 1.0f, .Release = 0.45f, .LowPassStart = 3000.0f, .LowPassEnd = 250.0f },
					{ .Wave = Sine, .Duration = 0.45f, .FrequencyStart = 95.0f, .FrequencyEnd = 38.0f, .Volume = 0.9f, .Release = 0.4f } } },
				{ Sfx::Zap, 0.4f, 4, 1.0f, {
					{ .Wave = Square, .Duration = 0.18f, .FrequencyStart = 2200.0f, .FrequencyEnd = 1100.0f, .Volume = 0.45f, .Release = 0.12f, .LowPassStart = 7000.0f, .VibratoDepth = 3.0f, .VibratoRate = 55.0f, .Duty = 0.2f },
					{ .Wave = Noise, .Duration = 0.1f, .FrequencyStart = 7000.0f, .FrequencyEnd = 3000.0f, .Volume = 0.4f, .Release = 0.08f, .LowPassStart = 9000.0f } } },
				{ Sfx::Frost, 0.45f, 3, 0.5f, {
					{ .Wave = Noise, .Duration = 0.4f, .FrequencyStart = 6000.0f, .FrequencyEnd = 9000.0f, .Volume = 0.5f, .Attack = 0.02f, .Release = 0.3f, .LowPassStart = 9000.0f, .LowPassEnd = 5000.0f },
					{ .Wave = Triangle, .Duration = 0.35f, .FrequencyStart = 1800.0f, .FrequencyEnd = 2500.0f, .Volume = 0.35f, .Release = 0.25f, .VibratoDepth = 0.5f, .VibratoRate = 18.0f } } },
				{ Sfx::Kill, 0.6f, 2, 0.0f, {
					{ .Wave = Square, .Duration = 0.08f, .FrequencyStart = 660.0f, .FrequencyEnd = 660.0f, .Volume = 0.35f, .Release = 0.04f, .LowPassStart = 4000.0f, .Duty = 0.3f },
					{ .Wave = Triangle, .Start = 0.07f, .Duration = 0.4f, .FrequencyStart = 990.0f, .FrequencyEnd = 990.0f, .Volume = 0.6f, .Release = 0.32f },
					{ .Wave = Sine, .Start = 0.07f, .Duration = 0.4f, .FrequencyStart = 1485.0f, .FrequencyEnd = 1485.0f, .Volume = 0.3f, .Release = 0.32f },
					{ .Wave = Noise, .Duration = 0.12f, .FrequencyStart = 3000.0f, .FrequencyEnd = 800.0f, .Volume = 0.35f, .Release = 0.1f, .LowPassStart = 4000.0f, .LowPassEnd = 900.0f },
					{ .Wave = Sine, .Duration = 0.2f, .FrequencyStart = 180.0f, .FrequencyEnd = 60.0f, .Volume = 0.6f, .Release = 0.15f } } },
				{ Sfx::Storm, 0.6f, 1, 0.0f, {
					{ .Wave = Noise, .Duration = 1.6f, .FrequencyStart = 300.0f, .FrequencyEnd = 900.0f, .Volume = 0.8f, .Attack = 0.4f, .Release = 0.9f, .LowPassStart = 400.0f, .LowPassEnd = 1400.0f },
					{ .Wave = Saw, .Duration = 1.6f, .FrequencyStart = 55.0f, .FrequencyEnd = 48.0f, .Volume = 0.5f, .Attack = 0.3f, .Release = 0.9f, .LowPassStart = 300.0f, .VibratoDepth = 1.0f, .VibratoRate = 5.0f },
					{ .Wave = Sine, .Start = 0.2f, .Duration = 1.2f, .FrequencyStart = 82.0f, .FrequencyEnd = 70.0f, .Volume = 0.5f, .Attack = 0.2f, .Release = 0.8f } } },
			};
		}

		bool IsEnemy(EntityKind kind)
		{
			return IsEnemyKind(kind);
		}

		bool Rose(uint8_t before, uint8_t after, EntityFlags flag)
		{
			return !(before & uint8_t(flag)) && (after & uint8_t(flag));
		}
	}

	GameAudio::GameAudio(Timbre::AudioEngine& engine)
		: m_Engine(engine), m_Bus(engine.CreateBus("Effects"))
	{
		const uint32_t sampleRate = engine.GetSampleRate() > 0 ? engine.GetSampleRate() : 48000;
		for (const SoundRecipe& recipe : Recipes())
		{
			Timbre::SoundDesc desc;
			desc.Bus = m_Bus;
			desc.Volume = recipe.Volume;
			desc.MaxInstances = recipe.MaxInstances;
			desc.LimitMode = Timbre::InstanceLimitMode::StealOldest;
			desc.PitchVariation = recipe.PitchVariation;
			desc.Spatial.MinDistance = 3.0f;
			desc.Spatial.MaxDistance = 45.0f;
			desc.Spatial.DopplerFactor = 0.0f;
			const std::vector<float> samples = Synthesize(recipe.Layers, sampleRate, 0.85f, uint32_t(recipe.Id) + 1);
			m_Sounds[size_t(recipe.Id)] = engine.CreateSound(samples, 1, sampleRate, desc);
		}
	}

	void GameAudio::Play(Sfx sfx, std::optional<Vec2> position, float volume, float pitch)
	{
		const Timbre::Sound& sound = m_Sounds[size_t(sfx)];
		if (!sound)
			return;
		Timbre::PlayParams params;
		params.Volume = volume;
		params.Pitch = pitch;
		if (position)
			params.Position = Timbre::Vec3(position->x, 0.0f, position->y);
		sound.Play(params);
		m_Played++;
	}

	void GameAudio::Reset()
	{
		m_Tracked.clear();
		m_Status = {};
		m_Local = {};
		m_Inventory = {};
		m_Primed = false;
	}

	void GameAudio::Observe(const GameClient& client)
	{
		const uint32_t self = client.GetPlayerNetId();
		if (const EntityState* player = client.FindEntity(self))
		{
			m_Listener = player->Position;
			m_Engine.SetListener({ .Position = { player->Position.x, 0.0f, player->Position.y }, .Forward = { 0.0f, 0.0f, 1.0f }, .Up = { 0.0f, 1.0f, 0.0f } });
		}

		const bool primed = m_Primed;
		m_FrameVolleys.clear();
		for (auto& [netId, tracked] : m_Tracked)
			tracked.Seen = false;
		for (const EntityState& entity : client.GetEntities())
		{
			const auto [it, inserted] = m_Tracked.try_emplace(entity.NetId);
			Tracked& tracked = it->second;
			if (primed)
			{
				if (inserted)
					OnAppear(entity, entity.Owner != NoPlayer && entity.Owner == client.GetPlayerId());
				else
					OnChange(entity, tracked, entity.NetId == self);
			}
			tracked = { entity.Kind, entity.Variant, entity.Owner, entity.Health, entity.Flags, entity.Position, true };
		}
		for (auto it = m_Tracked.begin(); it != m_Tracked.end();)
		{
			if (it->second.Seen)
			{
				++it;
				continue;
			}
			if (primed)
				OnDisappear(it->second, client);
			it = m_Tracked.erase(it);
		}

		const GameStatus& status = client.GetStatus();
		if (primed && status.Mode == GameMode::Mission && status.Mission != m_Status.Mission)
		{
			if (status.Mission == MissionPhase::Victory)
				Play(Sfx::WaveClear, std::nullopt, 1.3f, 0.9f);
			else if (status.Mission == MissionPhase::Failed)
				Play(Sfx::Defeat);
		}
		if (primed && status.Mode == GameMode::Horde && status.Phase != m_Status.Phase)
		{
			if (status.Phase == GamePhase::Wave)
				Play(Sfx::WaveStart);
			else if (status.Phase == GamePhase::Defeat)
				Play(Sfx::Defeat);
			else if (m_Status.Phase == GamePhase::Wave)
				Play(Sfx::WaveClear);
		}
		m_Status = status;
		if (primed)
		{
			ObserveLocalPlayer(client);
			ObserveInventory(client.GetInventory());
			for (const HitEvent& hit : client.GetNewHits())
				if (hit.Critical && !hit.OnPlayer)
					Play(Sfx::Crit, hit.Position, 0.8f);
		}
		m_Local = client.GetLocalPlayer();
		m_Inventory = client.GetInventory();
		m_Primed = true;
	}

	void GameAudio::OnAppear(const EntityState& entity, bool own)
	{
		const std::optional<Vec2> where = own ? std::nullopt : std::optional<Vec2>(entity.Position);
		const float loudness = own ? 1.15f : 0.85f;
		switch (entity.Kind)
		{
		case EntityKind::Projectile:
		{
			if (!m_FrameVolleys.insert({ entity.Owner, entity.Owner != NoPlayer ? entity.Tag : uint16_t(entity.Variant) }).second)
				return;
			switch (AbilityId(entity.Variant))
			{
			case AbilityId::Bolt:
				Play(Sfx::Bolt, where, loudness);
				break;
			case AbilityId::Orb:
				Play(Sfx::Frost, where, loudness * 0.8f, 1.3f);
				break;
			case AbilityId::Spit:
				Play(Sfx::EnemyShot, where, loudness);
				break;
			case AbilityId::Volley:
				Play(Sfx::Arrow, where, loudness * 1.3f);
				break;
			case AbilityId::Quarrel:
				Play(Sfx::Arrow, where, loudness * 1.2f, 0.7f);
				break;
			case AbilityId::PiercingShot:
				Play(Sfx::Arrow, where, loudness * 1.4f, 0.55f);
				break;
			case AbilityId::Fireball:
				Play(Sfx::Fireball, where, loudness);
				break;
			case AbilityId::IceSpear:
				Play(Sfx::Frost, where, loudness, 1.5f);
				break;
			case AbilityId::ChainLightning:
			case AbilityId::TomeVolley:
				Play(Sfx::Zap, where, loudness, AbilityId(entity.Variant) == AbilityId::TomeVolley ? 0.8f : 1.0f);
				break;
			case AbilityId::InkSpiral:
				Play(Sfx::EnemyShot, where, loudness * 0.6f, 0.7f);
				break;
			case AbilityId::ShardBurst:
				Play(Sfx::Frost, where, loudness, 1.7f);
				break;
			default:
				Play(Sfx::Arrow, where, loudness);
				break;
			}
			break;
		}
		case EntityKind::Effect:
		{
			const AbilityId ability = AbilityId(entity.Variant);
			const AbilityDef& def = GetAbility(ability);
			if (entity.Has(EntityFlags::Pending))
			{
				Play(ability == AbilityId::Meteor ? Sfx::Fireball : Sfx::Windup, entity.Position, ability == AbilityId::Meteor ? 1.2f : 0.6f, ability == AbilityId::Meteor ? 0.6f : 1.4f);
				break;
			}
			switch (def.Kind)
			{
			case AbilityKind::Nova:
				if (ability == AbilityId::FrostNova)
					Play(Sfx::Frost, where, loudness * 1.2f, 0.8f);
				else
					Play(Sfx::Nova, where, loudness, ability == AbilityId::Slam ? 0.65f : ability == AbilityId::Cyclone ? 1.3f : 1.0f);
				break;
			case AbilityKind::Melee:
				if (ability == AbilityId::Bite)
					Play(Sfx::Slash, where, 0.6f, 0.75f);
				else if (ability == AbilityId::GroundSlam)
					Play(Sfx::Nova, where, loudness * 1.2f, 0.55f);
				else
					Play(Sfx::Slash, where, ability == AbilityId::Stab ? loudness * 0.7f : loudness, ability == AbilityId::Chop ? 0.7f : ability == AbilityId::Stab ? 1.4f : 1.0f);
				break;
			case AbilityKind::Projectile:
			case AbilityKind::Ground:
				Play(ability == AbilityId::RainOfArrows ? Sfx::Arrow : Sfx::Explosion, entity.Position, ability == AbilityId::Meteor ? 1.4f : 1.0f, ability == AbilityId::RainOfArrows ? 0.7f : ability == AbilityId::Meteor ? 0.7f : 1.0f);
				break;
			case AbilityKind::Heal:
				Play(Sfx::Heal, where, loudness);
				break;
			case AbilityKind::Dash:
				Play(Sfx::Blink, where, loudness);
				break;
			}
			break;
		}
		case EntityKind::Loot:
		{
			if (LootKind(entity.Variant) != LootKind::Item || entity.Owner != NoPlayer)
				break;
			if (m_Status.Mode == GameMode::Royale && LengthSquared(entity.Position - m_Listener) > 64.0f)
				break;
			const Rarity rarity = ItemTagRarity(entity.Tag);
			if (rarity >= Rarity::Rare)
				Play(Sfx::RareDrop, entity.Position, rarity == Rarity::Rare ? 0.45f : 1.0f, rarity == Rarity::Rare ? 0.8f : rarity == Rarity::Mythic ? 1.25f : 1.0f);
			break;
		}
		case EntityKind::Chaser:
		case EntityKind::Shooter:
		case EntityKind::Brute:
		case EntityKind::Caster:
			if (m_Status.Mode != GameMode::Royale)
				Play(Sfx::Spawn, entity.Position, 1.0f, entity.Kind == EntityKind::Brute ? 0.6f : 1.0f);
			break;
		case EntityKind::Archivist:
		case EntityKind::Wyrm:
			Play(Sfx::Spawn, entity.Position, 1.6f, 0.4f);
			Play(Sfx::WaveStart, std::nullopt, 1.0f, 0.6f);
			break;
		case EntityKind::Chest:
			if (ChestKind(entity.Tag) == ChestKind::Boss)
				Play(Sfx::RareDrop, entity.Position, 1.2f, 0.8f);
			break;
		case EntityKind::Gateway:
			Play(Sfx::Blink, entity.Position, 1.2f, 0.5f);
			break;
		default:
			break;
		}
	}

	void GameAudio::OnChange(const EntityState& entity, const Tracked& before, bool self)
	{
		const std::optional<Vec2> where = self ? std::nullopt : std::optional<Vec2>(entity.Position);
		if (entity.Health < before.Health && entity.Health > 0)
		{
			if (IsEnemy(entity.Kind))
				Play(Sfx::Hit, entity.Position, 1.0f, entity.Kind == EntityKind::Brute ? 0.7f : 1.0f);
			else if (entity.Kind == EntityKind::Player)
				Play(Sfx::Hurt, where, self ? 1.0f : 0.6f);
		}
		if (IsEnemy(entity.Kind) && Rose(before.Flags, entity.Flags, EntityFlags::Attacking))
			Play(entity.Kind == EntityKind::Brute ? Sfx::BruteWindup : Sfx::Windup, entity.Position);
		if (entity.Kind == EntityKind::Player)
		{
			if (!self && LengthSquared(entity.Position - before.Position) > 36.0f)
				Play(Sfx::Door, entity.Position, 0.7f);
			if (Rose(before.Flags, entity.Flags, EntityFlags::Dead))
				Play(Sfx::PlayerDeath, where, self ? 1.0f : 0.7f);
			else if (Rose(before.Flags, entity.Flags, EntityFlags::Dashing))
				Play(Sfx::Dash, where, self ? 1.0f : 0.7f);
		}
	}

	void GameAudio::OnDisappear(const Tracked& last, const GameClient& client)
	{
		if (IsEnemy(last.Kind))
		{
			Play(Sfx::EnemyDeath, last.Position, last.Kind == EntityKind::Brute ? 1.3f : 1.0f, last.Kind == EntityKind::Brute ? 0.6f : 1.0f);
			return;
		}
		if (last.Kind != EntityKind::Loot)
			return;
		for (const EntityState& entity : client.GetEntities())
			if (entity.Kind == EntityKind::Player && entity.NetId != client.GetPlayerNetId() && LengthSquared(entity.Position - last.Position) < 2.0f)
			{
				Play(Sfx::Pickup, last.Position, 0.7f);
				return;
			}
	}

	void GameAudio::ObserveLocalPlayer(const GameClient& client)
	{
		const LocalPlayerState& local = client.GetLocalPlayer();
		if (!local.Valid || !m_Local.Valid)
			return;
		for (size_t i = 1; i < local.Buffs.size(); i++)
			if (local.Buffs[i] > m_Local.Buffs[i])
			{
				Play(Sfx::PowerUp);
				return;
			}
		if (local.State.Alive && m_Local.State.Alive && m_Local.Health > 0 && local.Health - m_Local.Health >= HealthOrbHeal / 2 && local.MaxHealth == m_Local.MaxHealth)
			Play(Sfx::Heal);
	}

	void GameAudio::ObserveInventory(const ServerInventory& inventory)
	{
		const auto has = [](const auto& items, uint32_t id) { return std::any_of(items.begin(), items.end(), [id](const Item& item) { return item.Id == id; }); };
		const auto heldBefore = [&](uint32_t id) { return has(m_Inventory.Pack, id) || has(m_Inventory.Gear, id) || has(m_Inventory.Gems, id); };
		for (size_t i = 0; i < inventory.Upgrades.size(); i++)
			if (inventory.Upgrades[i] > m_Inventory.Upgrades[i])
			{
				Play(Sfx::Upgrade);
				return;
			}
		for (const Item& item : inventory.Pack)
			if (item.IsValid() && !heldBefore(item.Id))
			{
				Play(Sfx::ItemPickup, std::nullopt, 1.0f, 1.0f + 0.08f * float(item.Quality));
				return;
			}
		const auto equipped = [&](const auto& now, const auto& before)
		{
			for (size_t i = 0; i < now.size(); i++)
				if (now[i].IsValid() && now[i].Id != before[i].Id && has(m_Inventory.Pack, now[i].Id))
					return true;
			return false;
		};
		if (equipped(inventory.Gear, m_Inventory.Gear) || equipped(inventory.Gems, m_Inventory.Gems))
			Play(Sfx::Equip);
	}
}
