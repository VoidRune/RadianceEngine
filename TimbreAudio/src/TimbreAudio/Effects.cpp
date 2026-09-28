#include "TimbreAudio/Effects.h"
#include "TimbreAudio/Internal/Dsp.h"
#include "TimbreAudio/Types.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Timbre
{
	namespace
	{
		// Parameters can be set from any thread to anything; NaN or infinity falls back to the default.
		float Load(const std::atomic<float>& value, float fallback)
		{
			const float result = value.load(std::memory_order_relaxed);
			return std::isfinite(result) ? result : fallback;
		}

		// Coefficient of a one pole smoother that settles in about the given time.
		float SmoothingCoefficient(float milliseconds, float sampleRate)
		{
			return std::exp(-1.0f / (std::max(milliseconds, 0.01f) * 0.001f * sampleRate));
		}

		// Rational tanh approximation, exact at 0 and saturating at +-1 from |x| = 3 on.
		float SoftClip(float x)
		{
			x = std::clamp(x, -3.0f, 3.0f);
			const float x2 = x * x;
			return x * (27.0f + x2) / (27.0f + 9.0f * x2);
		}
	}

	// BiquadFilter

	BiquadFilter::BiquadFilter(FilterType type, float frequency, float q, float gainDb)
		: m_Type(type), m_Frequency(frequency), m_Q(q), m_Gain(gainDb)
	{
	}

	void BiquadFilter::Initialize(uint32_t sampleRate)
	{
		m_SampleRate = float(sampleRate);
		m_ActiveFrequency = -1.0f;
		std::memset(m_State, 0, sizeof(m_State));
	}

	void BiquadFilter::UpdateCoefficients()
	{
		const FilterType type = m_Type.load(std::memory_order_relaxed);
		const float frequency = Load(m_Frequency, 1000.0f);
		const float q = Load(m_Q, 0.7071f);
		const float gain = Load(m_Gain, 0.0f);
		if (type == m_ActiveType && frequency == m_ActiveFrequency && q == m_ActiveQ && gain == m_ActiveGain)
			return;
		const Internal::BiquadCoefficients c = Internal::ComputeBiquad(type, frequency, q, gain, m_SampleRate);
		m_Coefficients[0] = c.B0;
		m_Coefficients[1] = c.B1;
		m_Coefficients[2] = c.B2;
		m_Coefficients[3] = c.A1;
		m_Coefficients[4] = c.A2;
		m_ActiveType = type;
		m_ActiveFrequency = frequency;
		m_ActiveQ = q;
		m_ActiveGain = gain;
	}

	void BiquadFilter::Process(float* samples, uint32_t frames)
	{
		UpdateCoefficients();
		const Internal::BiquadCoefficients c = { m_Coefficients[0], m_Coefficients[1], m_Coefficients[2], m_Coefficients[3], m_Coefficients[4] };
		for (uint32_t channel = 0; channel < 2; channel++)
		{
			Internal::BiquadState state = { m_State[channel][0], m_State[channel][1] };
			for (uint32_t i = 0; i < frames; i++)
				samples[i * 2 + channel] = Internal::ProcessBiquad(c, state, samples[i * 2 + channel]);
			m_State[channel][0] = state.Z1;
			m_State[channel][1] = state.Z2;
		}
	}

	// Delay

	Delay::Delay(float delaySeconds, float feedback, float wet, float maxDelaySeconds)
		: m_Delay(delaySeconds), m_Feedback(feedback), m_Wet(wet), m_MaxDelay(std::max(maxDelaySeconds, 0.01f))
	{
	}

	void Delay::Initialize(uint32_t sampleRate)
	{
		m_SampleRate = float(sampleRate);
		m_Length = uint32_t(m_MaxDelay * m_SampleRate) + 2;
		m_Buffer.assign(size_t(m_Length) * 2, 0.0f);
		m_Position = 0;
		m_CurrentDelay = -1.0f;
	}

	void Delay::Process(float* samples, uint32_t frames)
	{
		const float target = std::clamp(Load(m_Delay, 0.25f) * m_SampleRate, 1.0f, float(m_Length - 2));
		const float feedback = std::clamp(Load(m_Feedback, 0.35f), 0.0f, 0.98f);
		const float wet = Load(m_Wet, 0.3f);
		const float dry = Load(m_Dry, 1.0f);
		if (m_CurrentDelay < 0.0f)
			m_CurrentDelay = target;
		// Changing the delay time glides over about 50 ms, jumping would click.
		const float glide = 1.0f - SmoothingCoefficient(50.0f, m_SampleRate);
		const float length = float(m_Length);

		for (uint32_t i = 0; i < frames; i++)
		{
			m_CurrentDelay += (target - m_CurrentDelay) * glide;
			float read = float(m_Position) - m_CurrentDelay;
			if (read < 0.0f)
				read += length;
			uint32_t index = uint32_t(read);
			if (index >= m_Length)
				index -= m_Length;
			const float t = read - std::floor(read);
			const uint32_t next = index + 1 == m_Length ? 0 : index + 1;
			for (uint32_t channel = 0; channel < 2; channel++)
			{
				const float a = m_Buffer[index * 2 + channel];
				const float echo = a + t * (m_Buffer[next * 2 + channel] - a);
				const float input = samples[i * 2 + channel];
				m_Buffer[m_Position * 2 + channel] = input + echo * feedback;
				samples[i * 2 + channel] = input * dry + echo * wet;
			}
			if (++m_Position == m_Length)
				m_Position = 0;
		}
	}

	// Reverb

	namespace
	{
		constexpr uint32_t LineTuning[] = { 1201, 1453, 1627, 1831, 2069, 2269, 2503, 2711 };
		constexpr float ModulationRates[] = { 0.0f, 0.47f, 0.0f, 0.61f, 0.0f, 0.73f, 0.0f, 0.89f };
		constexpr uint32_t DiffuserTuning[] = { 229, 173, 611, 447 };
		constexpr float DiffuserGains[] = { 0.75f, 0.75f, 0.625f, 0.625f };
		constexpr float MaxPreDelay = 0.5f;
		constexpr float ReverbOutputScale = 0.65f;

		void Hadamard8(float* values)
		{
			for (uint32_t size = 1; size < 8; size *= 2)
				for (uint32_t i = 0; i < 8; i += 2 * size)
					for (uint32_t j = i; j < i + size; j++)
					{
						const float a = values[j];
						const float b = values[j + size];
						values[j] = a + b;
						values[j + size] = a - b;
					}
			for (uint32_t i = 0; i < 8; i++)
				values[i] *= 0.35355339f;
		}
	}

	Reverb::Reverb(float roomSize, float damping, float wet, float width)
		: m_RoomSize(roomSize), m_Damping(damping), m_Wet(wet), m_Width(width)
	{
	}

	void Reverb::Initialize(uint32_t sampleRate)
	{
		m_SampleRate = float(sampleRate);
		const float scale = m_SampleRate / 48000.0f;
		m_ModulationDepth = 6.0f * scale;
		m_ModulationTime = 0.0;
		m_PreDelayBuffer.assign(size_t(MaxPreDelay * m_SampleRate) + 2, 0.0f);
		m_PreDelayIndex = 0;
		for (uint32_t i = 0; i < DiffuserCount; i++)
		{
			m_Diffusers[i].Buffer.assign(std::max<size_t>(1, size_t(float(DiffuserTuning[i]) * scale)), 0.0f);
			m_Diffusers[i].Index = 0;
			m_Diffusers[i].Gain = DiffuserGains[i];
		}
		for (uint32_t i = 0; i < LineCount; i++)
		{
			Line& line = m_Lines[i];
			line.Length = std::max(8.0f, std::round(float(LineTuning[i]) * scale));
			line.Buffer.assign(size_t(line.Length + m_ModulationDepth) + 4, 0.0f);
			line.Index = 0;
			line.Delay = line.Length;
			line.Damping = 0.0f;
		}
	}

	void Reverb::Process(float* samples, uint32_t frames)
	{
		const float roomSize = std::clamp(Load(m_RoomSize, 0.7f), 0.0f, 1.0f);
		const float decaySetting = Load(m_DecayTime, 0.0f);
		const float decay = decaySetting > 0.0f ? std::clamp(decaySetting, 0.05f, 60.0f) : 0.093f / -std::log10(0.7f + 0.28f * roomSize);
		const float damping = std::clamp(Load(m_Damping, 0.5f), 0.0f, 1.0f) * 0.4f;
		const float wet = Load(m_Wet, 0.25f) * ReverbOutputScale;
		const float width = std::clamp(Load(m_Width, 1.0f), 0.0f, 1.0f);
		const float wet1 = wet * (width * 0.5f + 0.5f);
		const float wet2 = wet * ((1.0f - width) * 0.5f);
		const float dry = Load(m_Dry, 1.0f);
		const uint32_t preDelaySize = uint32_t(m_PreDelayBuffer.size());
		const uint32_t preDelay = uint32_t(std::clamp(Load(m_PreDelay, 0.02f), 0.0f, MaxPreDelay) * m_SampleRate);

		m_ModulationTime += double(frames) / m_SampleRate;
		float gains[LineCount];
		float delayStep[LineCount];
		for (uint32_t i = 0; i < LineCount; i++)
		{
			Line& line = m_Lines[i];
			gains[i] = std::pow(10.0f, -3.0f * line.Length / (decay * m_SampleRate));
			const float target = line.Length + m_ModulationDepth * float(std::sin(6.283185307 * ModulationRates[i] * m_ModulationTime));
			delayStep[i] = (target - line.Delay) / float(frames);
		}

		for (uint32_t n = 0; n < frames; n++)
		{
			const float left = samples[n * 2 + 0];
			const float right = samples[n * 2 + 1];

			m_PreDelayBuffer[m_PreDelayIndex] = 0.5f * (left + right);
			float input = m_PreDelayBuffer[m_PreDelayIndex >= preDelay ? m_PreDelayIndex - preDelay : m_PreDelayIndex + preDelaySize - preDelay];
			if (++m_PreDelayIndex == preDelaySize)
				m_PreDelayIndex = 0;

			for (Diffuser& diffuser : m_Diffusers)
			{
				const float delayed = diffuser.Buffer[diffuser.Index];
				const float output = delayed - diffuser.Gain * input;
				diffuser.Buffer[diffuser.Index] = input + diffuser.Gain * output;
				if (++diffuser.Index == diffuser.Buffer.size())
					diffuser.Index = 0;
				input = output;
			}

			float outputs[LineCount];
			for (uint32_t i = 0; i < LineCount; i++)
			{
				Line& line = m_Lines[i];
				line.Delay += delayStep[i];
				const uint32_t size = uint32_t(line.Buffer.size());
				float read = float(line.Index) - line.Delay;
				if (read < 0.0f)
					read += float(size);
				const uint32_t index = std::min(uint32_t(read), size - 1);
				const uint32_t next = index + 1 == size ? 0 : index + 1;
				const float fraction = read - float(index);
				const float delayed = line.Buffer[index] + fraction * (line.Buffer[next] - line.Buffer[index]);
				line.Damping = delayed + damping * (line.Damping - delayed);
				outputs[i] = line.Damping;
			}

			float mixed[LineCount];
			std::copy(std::begin(outputs), std::end(outputs), std::begin(mixed));
			Hadamard8(mixed);
			for (uint32_t i = 0; i < LineCount; i++)
			{
				Line& line = m_Lines[i];
				line.Buffer[line.Index] = input + gains[i] * mixed[i];
				if (++line.Index == line.Buffer.size())
					line.Index = 0;
			}

			const float reverbLeft = outputs[0] - outputs[1] + outputs[2] - outputs[3] + outputs[4] - outputs[5] + outputs[6] - outputs[7];
			const float reverbRight = outputs[0] + outputs[1] - outputs[2] - outputs[3] + outputs[4] + outputs[5] - outputs[6] - outputs[7];
			samples[n * 2 + 0] = reverbLeft * wet1 + reverbRight * wet2 + left * dry;
			samples[n * 2 + 1] = reverbRight * wet1 + reverbLeft * wet2 + right * dry;
		}
	}

	// Compressor

	Compressor::Compressor(float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb)
		: m_Threshold(thresholdDb), m_Ratio(ratio), m_Attack(attackMs), m_Release(releaseMs), m_Makeup(makeupDb)
	{
	}

	void Compressor::Initialize(uint32_t sampleRate)
	{
		m_SampleRate = float(sampleRate);
		m_Envelope = 0.0f;
	}

	void Compressor::Process(float* samples, uint32_t frames)
	{
		const float threshold = Load(m_Threshold, -18.0f);
		const float slope = 1.0f - 1.0f / std::max(Load(m_Ratio, 4.0f), 1.0f);
		const float knee = std::max(Load(m_Knee, 6.0f), 0.0f);
		const float attack = SmoothingCoefficient(Load(m_Attack, 5.0f), m_SampleRate);
		const float release = SmoothingCoefficient(Load(m_Release, 120.0f), m_SampleRate);
		const float makeup = Load(m_Makeup, 0.0f);

		// The envelope is the gain reduction in decibels, the level is detected on both channels together.
		float envelope = m_Envelope;
		float peakReduction = 0.0f;
		for (uint32_t i = 0; i < frames; i++)
		{
			const float peak = std::max(std::abs(samples[i * 2]), std::abs(samples[i * 2 + 1]));
			const float over = LinearToDecibels(peak) - threshold;
			float reduction = 0.0f;
			if (over >= knee * 0.5f)
				reduction = slope * over;
			else if (over > -knee * 0.5f)
			{
				const float x = over + knee * 0.5f;
				reduction = slope * x * x / (2.0f * knee);
			}
			const float coefficient = reduction > envelope ? attack : release;
			envelope = reduction + coefficient * (envelope - reduction);
			const float gain = DecibelsToLinear(makeup - envelope);
			samples[i * 2] *= gain;
			samples[i * 2 + 1] *= gain;
			peakReduction = std::max(peakReduction, envelope);
		}
		m_Envelope = envelope;
		m_GainReduction.store(peakReduction, std::memory_order_relaxed);
	}

	// Limiter

	Limiter::Limiter(float ceilingDb, float releaseMs)
		: m_Ceiling(ceilingDb), m_Release(releaseMs)
	{
	}

	void Limiter::Initialize(uint32_t sampleRate)
	{
		m_SampleRate = float(sampleRate);
		m_Lookahead = std::max(1u, uint32_t(0.0015f * m_SampleRate));
		m_Delay.assign(size_t(m_Lookahead) * 2, 0.0f);
		m_Required.assign(m_Lookahead, 1.0f);
		m_Window.assign(m_Lookahead, 1.0f);
		m_Queue.assign(size_t(m_Lookahead) + 1, 0);
		m_QueueHead = 0;
		m_QueueSize = 0;
		m_Time = 0;
		m_WindowSum = double(m_Lookahead);
		m_Gain = 1.0f;
	}

	void Limiter::Process(float* samples, uint32_t frames)
	{
		const float ceiling = DecibelsToLinear(std::min(Load(m_Ceiling, -0.3f), 0.0f));
		const float release = 1.0f - SmoothingCoefficient(Load(m_Release, 80.0f), m_SampleRate);
		const uint32_t lookahead = m_Lookahead;
		const uint32_t capacity = lookahead + 1;

		float lowestGain = 1.0f;
		for (uint32_t i = 0; i < frames; i++)
		{
			const float left = samples[i * 2 + 0];
			const float right = samples[i * 2 + 1];
			const float peak = std::max(std::abs(left), std::abs(right));
			const uint32_t slot = uint32_t(m_Time % lookahead);
			const float required = peak > ceiling ? ceiling / peak : 1.0f;
			m_Required[slot] = required;

			// Minimum required gain over the look-ahead window, with a monotonic queue of frame times.
			while (m_QueueSize > 0 && m_Required[m_Queue[(m_QueueHead + m_QueueSize - 1) % capacity] % lookahead] >= required)
				m_QueueSize--;
			m_Queue[(m_QueueHead + m_QueueSize) % capacity] = m_Time;
			m_QueueSize++;
			while (m_Queue[m_QueueHead] + lookahead <= m_Time)
			{
				m_QueueHead = (m_QueueHead + 1) % capacity;
				m_QueueSize--;
			}
			const float minimum = m_Required[m_Queue[m_QueueHead] % lookahead];

			// Averaging the minimum over the window ramps the gain down across the look-ahead and
			// reaches the required gain exactly when the peak leaves the delay line.
			m_WindowSum += double(minimum) - double(m_Window[slot]);
			m_Window[slot] = minimum;
			const float smoothed = float(m_WindowSum / double(lookahead));
			m_Gain = smoothed < m_Gain ? smoothed : m_Gain + (smoothed - m_Gain) * release;
			lowestGain = std::min(lowestGain, m_Gain);

			m_Delay[slot * 2 + 0] = left;
			m_Delay[slot * 2 + 1] = right;
			const uint32_t output = uint32_t((m_Time + 1) % lookahead);
			samples[i * 2 + 0] = m_Delay[output * 2 + 0] * m_Gain;
			samples[i * 2 + 1] = m_Delay[output * 2 + 1] * m_Gain;
			m_Time++;
		}
		m_GainReduction.store(-LinearToDecibels(lowestGain), std::memory_order_relaxed);
	}

	// Distortion

	Distortion::Distortion(float drive, float mix, float outputDb)
		: m_Drive(drive), m_Mix(mix), m_Output(outputDb)
	{
	}

	void Distortion::Process(float* samples, uint32_t frames)
	{
		const float drive = std::clamp(Load(m_Drive, 0.5f), 0.0f, 1.0f);
		const float gain = 1.0f + drive * drive * 49.0f;
		const float mix = std::clamp(Load(m_Mix, 1.0f), 0.0f, 1.0f);
		const float output = DecibelsToLinear(Load(m_Output, 0.0f));
		for (uint32_t i = 0; i < frames * 2; i++)
		{
			const float x = samples[i];
			samples[i] = (x + (SoftClip(x * gain) - x) * mix) * output;
		}
	}
}
