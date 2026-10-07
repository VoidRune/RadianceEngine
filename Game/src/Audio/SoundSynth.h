#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace Game
{
	enum class Waveform : uint8_t
	{
		Sine,
		Square,
		Saw,
		Triangle,
		Noise,
	};

	struct SynthLayer
	{
		Waveform Wave = Waveform::Sine;
		float Start = 0.0f;
		float Duration = 0.2f;
		float FrequencyStart = 440.0f;
		float FrequencyEnd = 440.0f;
		float Volume = 1.0f;
		float Attack = 0.004f;
		float Release = 0.2f;
		float LowPassStart = 0.0f;
		float LowPassEnd = 0.0f;
		float VibratoDepth = 0.0f;
		float VibratoRate = 0.0f;
		float Duty = 0.5f;
	};

	std::vector<float> Synthesize(std::span<const SynthLayer> layers, uint32_t sampleRate, float peak = 0.85f, uint32_t seed = 1);
}
