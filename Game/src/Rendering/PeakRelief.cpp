#include "PeakRelief.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float DomeHeight = 0.95f;
		constexpr float DomeShoulder = 0.12f;
		constexpr float DaisHeight = 0.55f;
		constexpr float DaisFoot = 7.0f;
		constexpr float TerraceStep = 0.3f;
		constexpr float TerraceSharpness = 0.45f;
		constexpr float TerraceWobble = 0.06f;
		constexpr float Rolling = 0.11f;
		constexpr float RollingScale = 1.0f / 9.0f;
		constexpr float CalmInside = 5.0f;
		constexpr float DaisLevel = 0.3f;
		constexpr float Knee = 0.25f;

		float Smooth(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float Glide(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			if (t < Knee)
				return t * t / (2.0f * Knee * (1.0f - Knee));
			if (t > 1.0f - Knee)
				return 1.0f - (1.0f - t) * (1.0f - t) / (2.0f * Knee * (1.0f - Knee));
			return (t - 0.5f * Knee) / (1.0f - Knee);
		}

		float Corner(int32_t x, int32_t y, uint32_t salt)
		{
			uint32_t h = uint32_t(x) * 0x8DA6B343u ^ uint32_t(y) * 0xD8163841u ^ salt * 0xCB1AB31Fu;
			h ^= h >> 15;
			h *= 0x2C1B3C6Du;
			h ^= h >> 12;
			h *= 0x297A2D39u;
			h ^= h >> 15;
			return float(h & 0xFFFFu) / 65535.0f;
		}

		float Lattice(Vec2 point, uint32_t salt)
		{
			const float fx = std::floor(point.x);
			const float fy = std::floor(point.y);
			const int32_t x = int32_t(fx);
			const int32_t y = int32_t(fy);
			const float u = Smooth(point.x - fx);
			const float v = Smooth(point.y - fy);
			return std::lerp(std::lerp(Corner(x, y, salt), Corner(x + 1, y, salt), u), std::lerp(Corner(x, y + 1, salt), Corner(x + 1, y + 1, salt), u), v);
		}

		float Fbm(Vec2 point, uint32_t salt)
		{
			return 0.6f * Lattice(point, salt) + 0.3f * Lattice(point * 2.03f + Vec2(5.3f, 1.7f), salt ^ 0x5A5Au) + 0.1f * Lattice(point * 4.1f + Vec2(2.9f, 8.1f), salt ^ 0x3C3Cu);
		}

		struct Sample
		{
			float Height = 0.0f;
			float Hollow = 0.0f;
			float Scree = 0.0f;
		};

		Sample Sculpt(const Plateau& peak, Vec2 point, uint32_t salt)
		{
			const float d = Length(point - peak.Center);
			const float rim = std::max(peak.RimDistance(point), 0.0f);
			const float settle = Smooth(rim / 2.5f) * Smooth((d - 3.0f) / 3.0f);
			const float s = std::max(d - PeakRelief::DaisTop - DaisLevel, 0.0f) / std::max(d + rim - PeakRelief::DaisTop - DaisLevel, 1e-3f) + TerraceWobble * (2.0f * Fbm(point * 0.11f, salt ^ 0x7171u) - 1.0f) * settle;
			const float dome = DomeHeight * (1.0f - Glide((s - DomeShoulder) / (1.0f - DomeShoulder)));
			const float x = dome / TerraceStep;
			const float terraced = TerraceStep * (x - TerraceSharpness * std::sin(2.0f * Pi * x) / (2.0f * Pi));
			const float dais = DaisHeight * (1.0f - Smooth((d - PeakRelief::DaisTop - DaisLevel) / (DaisFoot - PeakRelief::DaisTop - DaisLevel)));
			const float wave = 2.0f * Fbm(point * RollingScale, salt) - 1.0f;
			const float calm = Smooth((d - CalmInside) / 2.5f) * Smooth(rim / 2.0f);
			const float phase = x - std::floor(x);
			const float foot = dome > 0.02f && dome < DomeHeight - 0.02f ? Smooth(1.0f - std::abs(phase - 0.28f) / 0.15f) : 0.0f;
			return { terraced + dais + Rolling * wave * calm, std::clamp(-wave * 1.6f, 0.0f, 1.0f) * calm, foot * settle };
		}
	}

	void PeakRelief::Build(const Level& level)
	{
		Clear();
		if (level.Peak.IsEmpty())
			return;
		m_Peak = &level.Peak;
		const Plateau& peak = level.Peak;
		const size_t count = size_t(peak.Columns) * size_t(peak.Rows);
		m_Heights.assign(count, 0.0f);
		m_Hollows.assign(count, 0.0f);
		m_Screes.assign(count, 0.0f);
		const uint32_t salt = level.Seed * 0x9E3779B9u + 0x51CEu;
		for (int32_t j = 0; j < peak.Rows; j++)
			for (int32_t i = 0; i < peak.Columns; i++)
			{
				const size_t k = size_t(j) * size_t(peak.Columns) + size_t(i);
				const Sample sample = Sculpt(peak, peak.Origin + Vec2(float(i), float(j)) * peak.Spacing, salt);
				m_Heights[k] = sample.Height;
				m_Hollows[k] = sample.Hollow;
				m_Screes[k] = sample.Scree;
			}
		const auto survey = [&]
		{
			m_Lowest = 1e9f;
			m_Highest = -1e9f;
			m_Steepest = 0.0f;
			for (int32_t j = 1; j + 1 < peak.Rows; j++)
				for (int32_t i = 1; i + 1 < peak.Columns; i++)
				{
					const size_t k = size_t(j) * size_t(peak.Columns) + size_t(i);
					if (peak.Rim[k] < 0.0f)
						continue;
					m_Lowest = std::min(m_Lowest, m_Heights[k]);
					m_Highest = std::max(m_Highest, m_Heights[k]);
					const float dx = (m_Heights[k + 1] - m_Heights[k - 1]) / (2.0f * peak.Spacing);
					const float dy = (m_Heights[k + size_t(peak.Columns)] - m_Heights[k - size_t(peak.Columns)]) / (2.0f * peak.Spacing);
					m_Steepest = std::max(m_Steepest, std::sqrt(dx * dx + dy * dy));
				}
		};
		survey();
		const float scale = std::min({ 1.0f, MaxSlope / std::max(m_Steepest, 1e-6f), MaxRange / std::max(m_Highest - m_Lowest, 1e-6f) });
		if (scale < 1.0f)
		{
			for (float& height : m_Heights)
				height *= scale;
			survey();
		}
	}

	void PeakRelief::Clear()
	{
		m_Peak = nullptr;
		m_Heights.clear();
		m_Hollows.clear();
		m_Screes.clear();
		m_Lowest = 0.0f;
		m_Highest = 0.0f;
		m_Steepest = 0.0f;
	}

	float PeakRelief::Height(Vec2 position) const
	{
		return m_Peak ? m_Peak->Read(m_Heights, m_Peak->Locate(position)) : 0.0f;
	}

	Vec2 PeakRelief::Slope(Vec2 position) const
	{
		if (!m_Peak)
			return {};
		const float h = m_Peak->Spacing;
		return Vec2(Height(position + Vec2(h, 0.0f)) - Height(position - Vec2(h, 0.0f)), Height(position + Vec2(0.0f, h)) - Height(position - Vec2(0.0f, h))) / (2.0f * h);
	}

	float PeakRelief::Footing(Vec2 position, float radius) const
	{
		float lowest = Height(position);
		for (int32_t k = 0; k < 8; k++)
			lowest = std::min(lowest, Height(position + FromAngle(float(k) * Pi * 0.25f) * radius));
		return lowest;
	}

	float PeakRelief::Hollow(Vec2 position) const
	{
		return m_Peak ? m_Peak->Read(m_Hollows, m_Peak->Locate(position)) : 0.0f;
	}

	float PeakRelief::Scree(Vec2 position) const
	{
		return m_Peak ? m_Peak->Read(m_Screes, m_Peak->Locate(position)) : 0.0f;
	}
}
