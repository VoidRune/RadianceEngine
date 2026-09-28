#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>

namespace Timbre
{
	struct Vec3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr Vec3() = default;
		constexpr Vec3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) {}

		// Converts from any vector type with x, y and z members, such as glm::vec3.
		template<typename T>
			requires (!std::is_same_v<T, Vec3>) && requires(const T& v) { float(v.x); float(v.y); float(v.z); }
		constexpr Vec3(const T& v) : x(float(v.x)), y(float(v.y)), z(float(v.z)) {}

		constexpr bool operator==(const Vec3&) const = default;
	};

	enum class LogLevel
	{
		Info,
		Warning,
		Error,
	};

	// Receives the engine's log messages. Called from the game, loading and streaming threads, so it
	// must be thread-safe. It must not call back into the AudioEngine, which may be holding its lock.
	using LogCallback = std::function<void(LogLevel level, std::string_view message)>;

	inline float DecibelsToLinear(float decibels) { return std::pow(10.0f, decibels / 20.0f); }
	inline float LinearToDecibels(float linear) { return linear > 1e-8f ? 20.0f * std::log10(linear) : -160.0f; }
	inline float SemitonesToPitch(float semitones) { return std::exp2(semitones / 12.0f); }
}
