#include "SoundSynth.h"
#include <GameShared/Core/Random.h>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float TwoPi = 6.28318530718f;

		float Glide(float from, float to, float t)
		{
			if (from <= 0.0f || to <= 0.0f)
				return from + (to - from) * t;
			return from * std::pow(to / from, t);
		}

		float Envelope(const SynthLayer& layer, float t)
		{
			const float attack = layer.Attack > 0.0f ? std::min(1.0f, t / layer.Attack) : 1.0f;
			const float releaseStart = layer.Duration - layer.Release;
			if (t <= releaseStart || layer.Release <= 0.0f)
				return attack;
			const float fade = std::clamp((layer.Duration - t) / layer.Release, 0.0f, 1.0f);
			return attack * fade * fade;
		}
	}

	std::vector<float> Synthesize(std::span<const SynthLayer> layers, uint32_t sampleRate, float peak, uint32_t seed)
	{
		float length = 0.0f;
		for (const SynthLayer& layer : layers)
			length = std::max(length, layer.Start + layer.Duration);
		std::vector<float> samples(size_t(std::ceil(length * float(sampleRate))) + 1, 0.0f);
		Random random(seed);

		for (const SynthLayer& layer : layers)
		{
			const size_t first = size_t(layer.Start * float(sampleRate));
			const size_t count = size_t(layer.Duration * float(sampleRate));
			float phase = 0.0f;
			float noise = random.Float() * 2.0f - 1.0f;
			float filtered = 0.0f;
			for (size_t i = 0; i < count && first + i < samples.size(); i++)
			{
				const float t = float(i) / float(sampleRate);
				const float progress = t / layer.Duration;
				float frequency = Glide(layer.FrequencyStart, layer.FrequencyEnd, progress);
				if (layer.VibratoDepth > 0.0f)
					frequency *= std::exp2(layer.VibratoDepth / 12.0f * std::sin(TwoPi * layer.VibratoRate * t));
				const float previous = phase;
				phase += frequency / float(sampleRate);
				phase -= std::floor(phase);

				float value = 0.0f;
				switch (layer.Wave)
				{
				case Waveform::Sine:
					value = std::sin(TwoPi * phase);
					break;
				case Waveform::Square:
					value = phase < layer.Duty ? 1.0f : -1.0f;
					break;
				case Waveform::Saw:
					value = 2.0f * phase - 1.0f;
					break;
				case Waveform::Triangle:
					value = 4.0f * std::abs(phase - 0.5f) - 1.0f;
					break;
				case Waveform::Noise:
					if (phase < previous)
						noise = random.Float() * 2.0f - 1.0f;
					value = noise;
					break;
				}

				if (layer.LowPassStart > 0.0f)
				{
					const float cutoff = Glide(layer.LowPassStart, layer.LowPassEnd > 0.0f ? layer.LowPassEnd : layer.LowPassStart, progress);
					const float alpha = 1.0f - std::exp(-TwoPi * cutoff / float(sampleRate));
					filtered += alpha * (value - filtered);
					value = filtered;
				}
				samples[first + i] += value * layer.Volume * Envelope(layer, t);
			}
		}

		float loudest = 0.0f;
		for (float sample : samples)
			loudest = std::max(loudest, std::abs(sample));
		if (loudest > 0.0f)
			for (float& sample : samples)
				sample *= peak / loudest;
		return samples;
	}
}
