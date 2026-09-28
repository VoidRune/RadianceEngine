#pragma once
#include "TimbreAudio/Effects.h"
#include "TimbreAudio/Types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Timbre::Internal
{
	constexpr float Pi = 3.14159265358979f;

	inline Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline Vec3 operator*(const Vec3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
	inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline Vec3 Cross(const Vec3& a, const Vec3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	inline float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }

	inline Vec3 Normalize(const Vec3& v, const Vec3& fallback)
	{
		const float length = Length(v);
		return length > 1e-6f ? v * (1.0f / length) : fallback;
	}

	// Pan law used for 2D voices: center plays at full volume on both channels, panning fades out
	// the opposite channel along a sine curve, sqrt(2) * cos((|pan| + 1) * pi / 4). It's written as
	// cos - sin so center and hard panning come out exact.
	inline void PanGains(float pan, float& left, float& right)
	{
		pan = std::clamp(pan, -1.0f, 1.0f);
		const float angle = std::abs(pan) * (Pi * 0.25f);
		const float opposite = std::abs(pan) >= 1.0f ? 0.0f : std::max(0.0f, std::cos(angle) - std::sin(angle));
		left = pan > 0.0f ? opposite : 1.0f;
		right = pan < 0.0f ? opposite : 1.0f;
	}

	inline void ConstantPowerGains(float pan, float& left, float& right)
	{
		const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * (Pi * 0.25f);
		left = std::max(0.0f, std::cos(angle));
		right = std::max(0.0f, std::sin(angle));
	}

	struct BiquadCoefficients
	{
		float B0 = 1.0f;
		float B1 = 0.0f;
		float B2 = 0.0f;
		float A1 = 0.0f;
		float A2 = 0.0f;
	};

	struct BiquadState
	{
		float Z1 = 0.0f;
		float Z2 = 0.0f;
	};

	// Robert Bristow-Johnson's audio EQ cookbook filters.
	inline BiquadCoefficients ComputeBiquad(FilterType type, float frequency, float q, float gainDb, float sampleRate)
	{
		const float omega = 2.0f * Pi * std::clamp(frequency, 10.0f, sampleRate * 0.49f) / sampleRate;
		const float cosine = std::cos(omega);
		const float alpha = std::sin(omega) / (2.0f * std::max(q, 0.01f));
		const float a = std::pow(10.0f, gainDb / 40.0f);
		const float sqrtA2Alpha = 2.0f * std::sqrt(a) * alpha;

		float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
		switch (type)
		{
		case FilterType::LowPass:
			b0 = (1.0f - cosine) * 0.5f; b1 = 1.0f - cosine; b2 = b0;
			a0 = 1.0f + alpha; a1 = -2.0f * cosine; a2 = 1.0f - alpha;
			break;
		case FilterType::HighPass:
			b0 = (1.0f + cosine) * 0.5f; b1 = -(1.0f + cosine); b2 = b0;
			a0 = 1.0f + alpha; a1 = -2.0f * cosine; a2 = 1.0f - alpha;
			break;
		case FilterType::BandPass:
			b0 = alpha; b1 = 0.0f; b2 = -alpha;
			a0 = 1.0f + alpha; a1 = -2.0f * cosine; a2 = 1.0f - alpha;
			break;
		case FilterType::Notch:
			b0 = 1.0f; b1 = -2.0f * cosine; b2 = 1.0f;
			a0 = 1.0f + alpha; a1 = -2.0f * cosine; a2 = 1.0f - alpha;
			break;
		case FilterType::Peak:
			b0 = 1.0f + alpha * a; b1 = -2.0f * cosine; b2 = 1.0f - alpha * a;
			a0 = 1.0f + alpha / a; a1 = -2.0f * cosine; a2 = 1.0f - alpha / a;
			break;
		case FilterType::LowShelf:
			b0 = a * ((a + 1.0f) - (a - 1.0f) * cosine + sqrtA2Alpha);
			b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosine);
			b2 = a * ((a + 1.0f) - (a - 1.0f) * cosine - sqrtA2Alpha);
			a0 = (a + 1.0f) + (a - 1.0f) * cosine + sqrtA2Alpha;
			a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosine);
			a2 = (a + 1.0f) + (a - 1.0f) * cosine - sqrtA2Alpha;
			break;
		case FilterType::HighShelf:
			b0 = a * ((a + 1.0f) + (a - 1.0f) * cosine + sqrtA2Alpha);
			b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosine);
			b2 = a * ((a + 1.0f) + (a - 1.0f) * cosine - sqrtA2Alpha);
			a0 = (a + 1.0f) - (a - 1.0f) * cosine + sqrtA2Alpha;
			a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosine);
			a2 = (a + 1.0f) - (a - 1.0f) * cosine - sqrtA2Alpha;
			break;
		}
		const float inverse = 1.0f / a0;
		return { b0 * inverse, b1 * inverse, b2 * inverse, a1 * inverse, a2 * inverse };
	}

	// Transposed direct form II.
	inline float ProcessBiquad(const BiquadCoefficients& c, BiquadState& state, float x)
	{
		const float y = c.B0 * x + state.Z1;
		state.Z1 = c.B1 * x - c.A1 * y + state.Z2;
		state.Z2 = c.B2 * x - c.A2 * y;
		return y;
	}

	// Linear ramp towards a target, advanced once per mixing block.
	struct Ramp
	{
		float Value = 1.0f;
		float Target = 1.0f;
		uint32_t Remaining = 0;

		void Set(float value)
		{
			Value = Target = value;
			Remaining = 0;
		}

		void Start(float target, uint32_t frames)
		{
			Target = target;
			Remaining = frames;
			if (frames == 0)
				Value = target;
		}

		float Advance(uint32_t frames)
		{
			if (frames >= Remaining)
			{
				Value = Target;
				Remaining = 0;
			}
			else
			{
				Value += (Target - Value) * (float(frames) / float(Remaining));
				Remaining -= frames;
			}
			return Value;
		}

		bool IsDone() const { return Remaining == 0; }
	};

	// Saves the floating point control state and flushes denormals to zero while alive. Recursive
	// filters decaying into denormals would otherwise slow mixing down dramatically.
	class DenormalGuard
	{
	public:
		DenormalGuard();
		~DenormalGuard();
		DenormalGuard(const DenormalGuard&) = delete;
		DenormalGuard& operator=(const DenormalGuard&) = delete;

	private:
		uint32_t m_State = 0;
	};
}
