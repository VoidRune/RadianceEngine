#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Game
{
	constexpr float Pi = 3.14159265358979f;

	struct Vec2
	{
		float x = 0.0f;
		float y = 0.0f;

		constexpr Vec2() = default;
		constexpr Vec2(float inX, float inY) : x(inX), y(inY) {}

		constexpr Vec2 operator+(Vec2 other) const { return { x + other.x, y + other.y }; }
		constexpr Vec2 operator-(Vec2 other) const { return { x - other.x, y - other.y }; }
		constexpr Vec2 operator-() const { return { -x, -y }; }
		constexpr Vec2 operator*(float scale) const { return { x * scale, y * scale }; }
		constexpr Vec2 operator/(float scale) const { return { x / scale, y / scale }; }
		constexpr Vec2& operator+=(Vec2 other) { x += other.x; y += other.y; return *this; }
		constexpr Vec2& operator-=(Vec2 other) { x -= other.x; y -= other.y; return *this; }
		constexpr Vec2& operator*=(float scale) { x *= scale; y *= scale; return *this; }
		constexpr bool operator==(const Vec2&) const = default;
	};

	constexpr float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
	constexpr float LengthSquared(Vec2 v) { return Dot(v, v); }
	constexpr Vec2 Lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
	inline float Length(Vec2 v) { return std::sqrt(Dot(v, v)); }

	inline Vec2 Normalize(Vec2 v)
	{
		const float length = Length(v);
		return length > 1e-6f ? v / length : Vec2();
	}

	inline Vec2 ClampLength(Vec2 v, float maxLength)
	{
		const float lengthSquared = Dot(v, v);
		return lengthSquared > maxLength * maxLength ? v * (maxLength / std::sqrt(lengthSquared)) : v;
	}

	inline Vec2 FromAngle(float angle) { return { std::cos(angle), std::sin(angle) }; }
	inline float AngleOf(Vec2 v) { return std::atan2(v.y, v.x); }
	inline float AngleBetween(float from, float to) { return std::remainder(to - from, 2.0f * Pi); }
	inline float LerpAngle(float a, float b, float t) { return a + AngleBetween(a, b) * t; }

	inline float ClosestPointParameter(Vec2 a, Vec2 b, Vec2 point)
	{
		const Vec2 segment = b - a;
		const float lengthSquared = Dot(segment, segment);
		return lengthSquared > 1e-12f ? std::clamp(Dot(point - a, segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;
	}

	inline bool SegmentTouchesCircle(Vec2 a, Vec2 b, Vec2 center, float radius, float& parameter)
	{
		parameter = ClosestPointParameter(a, b, center);
		return LengthSquared(Lerp(a, b, parameter) - center) <= radius * radius;
	}

	inline constexpr float NetPositionScale = 256.0f;

	inline uint16_t QuantizePosition(float value)
	{
		return uint16_t(std::clamp(value * NetPositionScale + 0.5f, 0.0f, 65535.0f));
	}

	inline float DequantizePosition(uint16_t value)
	{
		return float(value) / NetPositionScale;
	}

	inline Vec2 SnapToNetGrid(Vec2 position)
	{
		return { DequantizePosition(QuantizePosition(position.x)), DequantizePosition(QuantizePosition(position.y)) };
	}
}
