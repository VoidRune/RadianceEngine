#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cstdint>

namespace Game
{
	using UiColor = glm::vec4;

	inline constexpr UiColor Rgba(float r, float g, float b, float a = 1.0f)
	{
		return UiColor(r, g, b, a);
	}

	inline constexpr UiColor WithAlpha(const glm::vec3& color, float alpha = 1.0f)
	{
		return UiColor(color, alpha);
	}

	inline constexpr UiColor WithAlpha(const UiColor& color, float alpha)
	{
		return UiColor(color.r, color.g, color.b, color.a * alpha);
	}

	inline UiColor Hex(uint32_t rgb, float alpha = 1.0f)
	{
		return UiColor(float((rgb >> 16) & 0xFF) / 255.0f, float((rgb >> 8) & 0xFF) / 255.0f, float(rgb & 0xFF) / 255.0f, alpha);
	}

	inline UiColor Lighten(const UiColor& color, float amount)
	{
		return UiColor(glm::mix(glm::vec3(color), glm::vec3(1.0f), amount), color.a);
	}

	inline UiColor Darken(const UiColor& color, float amount)
	{
		return UiColor(glm::vec3(color) * (1.0f - amount), color.a);
	}

	inline uint32_t PackColor(const UiColor& color)
	{
		const auto channel = [](float value) { return uint32_t(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
		return channel(color.r) | (channel(color.g) << 8) | (channel(color.b) << 16) | (channel(color.a) << 24);
	}

	struct UiRect
	{
		float X = 0.0f;
		float Y = 0.0f;
		float W = 0.0f;
		float H = 0.0f;

		static UiRect FromMinMax(glm::vec2 min, glm::vec2 max) { return { min.x, min.y, max.x - min.x, max.y - min.y }; }
		static UiRect Centered(glm::vec2 center, glm::vec2 size) { return { center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y }; }

		glm::vec2 Min() const { return { X, Y }; }
		glm::vec2 Max() const { return { X + W, Y + H }; }
		glm::vec2 Size() const { return { W, H }; }
		glm::vec2 Center() const { return { X + W * 0.5f, Y + H * 0.5f }; }
		float Right() const { return X + W; }
		float Bottom() const { return Y + H; }
		bool Contains(glm::vec2 point) const { return point.x >= X && point.y >= Y && point.x < X + W && point.y < Y + H; }

		UiRect Shrink(float amount) const { return Shrink(amount, amount); }
		UiRect Shrink(float x, float y) const { return { X + x, Y + y, std::max(W - 2.0f * x, 0.0f), std::max(H - 2.0f * y, 0.0f) }; }
		UiRect Expand(float amount) const { return { X - amount, Y - amount, W + 2.0f * amount, H + 2.0f * amount }; }
		UiRect Translate(glm::vec2 offset) const { return { X + offset.x, Y + offset.y, W, H }; }
		UiRect Intersect(const UiRect& other) const
		{
			const glm::vec2 min = glm::max(Min(), other.Min());
			const glm::vec2 max = glm::max(glm::min(Max(), other.Max()), min);
			return FromMinMax(min, max);
		}

		UiRect CutTop(float amount)
		{
			amount = std::min(amount, H);
			const UiRect piece{ X, Y, W, amount };
			Y += amount;
			H -= amount;
			return piece;
		}

		UiRect CutBottom(float amount)
		{
			amount = std::min(amount, H);
			H -= amount;
			return { X, Y + H, W, amount };
		}

		UiRect CutLeft(float amount)
		{
			amount = std::min(amount, W);
			const UiRect piece{ X, Y, amount, H };
			X += amount;
			W -= amount;
			return piece;
		}

		UiRect CutRight(float amount)
		{
			amount = std::min(amount, W);
			W -= amount;
			return { X + W, Y, amount, H };
		}
	};

	enum class UiMode : uint32_t
	{
		Rect,
		Glyph,
		Image,
		Orb,
		Triangle,
	};

	struct UiPrimitive
	{
		glm::vec4 Rect{ 0.0f };
		glm::vec4 Uv{ 0.0f, 0.0f, 1.0f, 1.0f };
		glm::vec4 Clip{ -1e6f, -1e6f, 1e6f, 1e6f };
		glm::vec4 Shape{ 0.0f };
		glm::vec4 Params{ 0.0f };
		glm::uvec4 Colors{ 0u };
		glm::uvec4 Info{ 0u };
	};
	static_assert(sizeof(UiPrimitive) == 112);
}
