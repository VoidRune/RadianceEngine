#include "PeakRelief.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float DomeHeight = 0.4f;
		constexpr float DomeShoulder = 0.12f;
		constexpr float DaisHeight = 0.55f;
		constexpr float DaisFoot = 7.6f;
		constexpr float Rolling = 0.14f;
		constexpr float RollingScale = 1.0f / 7.0f;
		constexpr float Bumps = 0.07f;
		constexpr float BumpScale = 0.45f;
		constexpr float CalmInside = 4.6f;
		constexpr float DaisLevel = 0.3f;
		constexpr float Knee = 0.25f;
		constexpr float PlateSize = 4.2f;
		constexpr float PlateLift = 0.05f;
		constexpr float PlateTilt = 0.018f;
		constexpr float SeamWidth = 0.2f;
		constexpr float SeamDepth = 0.035f;
		constexpr float ShelfInner = 4.0f;
		constexpr float ShelfRiser = 0.34f;
		constexpr float PlateBlend = 0.34f;
		constexpr float ShelfRim = 1.8f;
		constexpr float ShelfClear = 1.1f;
		constexpr float ShelfWobble = 0.14f;
		constexpr float MinRise = 0.17f;
		constexpr uint32_t Craters = 3;
		constexpr float CraterDepth = 0.14f;
		constexpr float CraterBerm = 0.05f;

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
			const float s = std::max(d - PeakRelief::DaisTop - DaisLevel, 0.0f) / std::max(d + rim - PeakRelief::DaisTop - DaisLevel, 1e-3f);
			const float dome = DomeHeight * (1.0f - Glide((s - DomeShoulder) / (1.0f - DomeShoulder)));
			const float dais = DaisHeight * (1.0f - Smooth((d - PeakRelief::DaisTop - DaisLevel) / (DaisFoot - PeakRelief::DaisTop - DaisLevel)));
			const float wave = 2.0f * Fbm(point * RollingScale, salt) - 1.0f;
			const float calm = Smooth((d - CalmInside) / 2.5f) * Smooth(rim / 2.0f);
			const float bump = (2.0f * Fbm(point * BumpScale, salt ^ 0x6B6Bu) - 1.0f) * Smooth((d - 4.2f) / 1.2f) * Smooth((rim - 0.6f) / 1.2f);
			const float brink = 1.0f - Smooth((rim - 0.6f) / 2.2f);
			const float hollow = std::clamp(-wave * 1.6f, 0.0f, 1.0f) * calm;
			return { dome + dais + Rolling * wave * calm + Bumps * bump, hollow, std::max(brink * Smooth(d / 6.0f), 0.6f * hollow) };
		}

		float Plates(const Plateau& peak, Vec2 point, uint32_t salt, float& seam)
		{
			const Vec2 scaled = point / PlateSize;
			const int32_t cx = int32_t(std::floor(scaled.x));
			const int32_t cy = int32_t(std::floor(scaled.y));
			float nearest = 1e9f;
			float second = 1e9f;
			int32_t ox = cx;
			int32_t oy = cy;
			int32_t px = cx;
			int32_t py = cy;
			Vec2 feature = scaled;
			Vec2 other = scaled;
			for (int32_t dy = -1; dy <= 1; dy++)
				for (int32_t dx = -1; dx <= 1; dx++)
				{
					const Vec2 spot(float(cx + dx) + 0.15f + 0.7f * Corner(cx + dx, cy + dy, salt ^ 0x1111u), float(cy + dy) + 0.15f + 0.7f * Corner(cx + dx, cy + dy, salt ^ 0x2222u));
					if (const float distance = LengthSquared(scaled - spot); distance < nearest)
					{
						second = nearest;
						px = ox;
						py = oy;
						other = feature;
						nearest = distance;
						ox = cx + dx;
						oy = cy + dy;
						feature = spot;
					}
					else if (distance < second)
					{
						second = distance;
						px = cx + dx;
						py = cy + dy;
						other = spot;
					}
				}
			const float d = Length(point - peak.Center);
			const float calm = Smooth((d - 4.1f) / 0.8f) * Smooth((peak.RimDistance(point) - 0.6f * PeakRelief::LevelLip) / 0.6f);
			seam = (1.0f - Smooth((std::sqrt(second) - std::sqrt(nearest)) * PlateSize / SeamWidth)) * calm * Smooth((Fbm(point * 0.3f, salt ^ 0x6666u) - 0.32f) / 0.14f);
			const auto plate = [&](int32_t x, int32_t y, Vec2 spot)
			{
				const Vec2 tilt(Corner(x, y, salt ^ 0x3333u) - 0.5f, Corner(x, y, salt ^ 0x4444u) - 0.5f);
				return PlateLift * (2.0f * Corner(x, y, salt ^ 0x5555u) - 1.0f) + 2.0f * PlateTilt * Dot(tilt, point - spot * PlateSize);
			};
			const float own = plate(ox, oy, feature);
			const float shared = 0.5f * (own + plate(px, py, other));
			return std::lerp(shared, own, Smooth((std::sqrt(second) - std::sqrt(nearest)) * PlateSize / PlateBlend)) * calm - SeamDepth * seam;
		}

		float Down(const PeakRelief::Shelf& shelf)
		{
			return shelf.Outer ? -1.0f : 1.0f;
		}
	}

	float PeakRelief::Edge(const Shelf& shelf, int32_t step, float angle)
	{
		return shelf.Radius - Down(shelf) * float(step) * StepDepth + ShelfWobble * (2.0f * Lattice(Vec2(angle * 4.0f + float(step) * 7.3f, float(step) * 1.7f), shelf.Salt) - 1.0f);
	}

	bool PeakRelief::Fits(const Level& level, const Shelf& shelf) const
	{
		const Plateau& peak = level.Peak;
		for (const Shelf& other : m_Shelves)
			if (std::abs(std::remainder(other.Angle - shelf.Angle, 2.0f * Pi)) < 0.5f * (other.Span + shelf.Span) + 0.3f)
				return false;
		for (int32_t step = 0; step < shelf.Steps; step++)
		{
			const int32_t samples = int32_t(std::ceil(shelf.Span * shelf.Radius / 0.25f));
			for (int32_t k = 0; k <= samples; k++)
			{
				const float angle = shelf.Angle + shelf.Span * (float(k) / float(samples) - 0.5f);
				const Vec2 point = peak.Center + FromAngle(angle) * Edge(shelf, step, angle);
				if (peak.RimDistance(point) < (shelf.Outer ? 1.2f : ShelfRim) || Length(point - peak.Center) < ShelfInner + 0.4f)
					return false;
				for (const PlacedProp& prop : level.Props)
					if (prop.Id != PropId::StormSpire && LengthSquared(point - prop.Position) < ShelfClear * ShelfClear)
						return false;
			}
		}
		return true;
	}

	float PeakRelief::Lift(const Shelf& shelf, Vec2 offset, float& near, float& fade) const
	{
		const float r = Length(offset);
		const float angle = AngleOf(offset);
		const float off = std::abs(std::remainder(angle - shelf.Angle, 2.0f * Pi));
		fade = 0.0f;
		if (off > 0.5f * shelf.Span + 0.3f)
			return 0.0f;
		float lift = 0.0f;
		fade = Smooth((0.5f * shelf.Span - off) * r / ShelfFade) * (shelf.Outer ? 1.0f : Smooth((r - ShelfInner) / 1.2f));
		for (int32_t step = 0; step < shelf.Steps; step++)
		{
			const float edge = Edge(shelf, step, shelf.Angle + std::remainder(angle - shelf.Angle, 2.0f * Pi));
			near = std::max(near, Smooth((0.5f * shelf.Span + 0.3f - off) / 0.3f) * (1.0f - Smooth((std::abs(r - edge) - 0.5f) / 0.4f)));
			lift += shelf.Rise[size_t(step)] * Smooth((edge - r) * Down(shelf) / ShelfRiser + 0.5f);
		}
		return lift;
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
		m_Seams.assign(count, 0.0f);
		const uint32_t salt = level.Seed * 0x9E3779B9u + 0x51CEu;
		const auto place = [&](int32_t i, int32_t j) { return peak.Origin + Vec2(float(i), float(j)) * peak.Spacing; };
		for (int32_t j = 0; j < peak.Rows; j++)
			for (int32_t i = 0; i < peak.Columns; i++)
			{
				const size_t k = size_t(j) * size_t(peak.Columns) + size_t(i);
				const Sample sample = Sculpt(peak, place(i, j), salt);
				m_Heights[k] = sample.Height;
				m_Hollows[k] = sample.Hollow;
				m_Screes[k] = sample.Scree;
			}
		const auto survey = [&](bool slope)
		{
			m_Lowest = 1e9f;
			m_Highest = -1e9f;
			if (slope)
				m_Steepest = 0.0f;
			for (int32_t j = 1; j + 1 < peak.Rows; j++)
				for (int32_t i = 1; i + 1 < peak.Columns; i++)
				{
					const size_t k = size_t(j) * size_t(peak.Columns) + size_t(i);
					if (peak.Rim[k] < 0.0f)
						continue;
					m_Lowest = std::min(m_Lowest, m_Heights[k]);
					m_Highest = std::max(m_Highest, m_Heights[k]);
					if (!slope)
						continue;
					const float dx = (m_Heights[k + 1] - m_Heights[k - 1]) / (2.0f * peak.Spacing);
					const float dy = (m_Heights[k + size_t(peak.Columns)] - m_Heights[k - size_t(peak.Columns)]) / (2.0f * peak.Spacing);
					m_Steepest = std::max(m_Steepest, std::sqrt(dx * dx + dy * dy));
				}
		};
		survey(true);
		const float scale = std::min({ 1.0f, MaxSlope / std::max(m_Steepest, 1e-6f), MaxRange / std::max(m_Highest - m_Lowest, 1e-6f) });
		if (scale < 1.0f)
		{
			for (float& height : m_Heights)
				height *= scale;
			survey(true);
		}
		const std::vector<float> base = m_Heights;
		const auto ground = [&](Vec2 point) { return peak.Read(base, peak.Locate(point)); };
		const float summit = ground(peak.Center);

		int32_t serial = 0;
		const auto attempt = [&](float angle, float sway, bool outer, float near, float far, int32_t tries)
		{
			for (int32_t n = 0; n < tries; n++, serial++)
			{
				const auto roll = [&](int32_t k) { return Corner(serial, k, salt ^ 0x5E1Fu); };
				Shelf shelf{ .Angle = angle + sway * (roll(0) - 0.5f), .Span = 0.9f + 0.45f * roll(1), .Radius = near + (far - near) * roll(2), .Outer = outer, .Salt = salt ^ uint32_t(serial) * 0x9E37u };
				if (outer)
				{
					shelf.Steps = roll(3) < 0.55f ? 2 : 1;
					for (int32_t step = 0; step < shelf.Steps; step++)
						shelf.Rise[size_t(step)] = StepRise * (0.78f + 0.44f * roll(4 + step));
				}
				else
				{
					const float budget = summit - ground(peak.Center + FromAngle(shelf.Angle) * (shelf.Radius + 0.4f));
					shelf.Steps = std::min(3, int32_t(budget / MinRise));
					if (shelf.Steps == 0)
						continue;
					float total = 0.0f;
					for (int32_t step = 0; step < shelf.Steps; step++)
						total += shelf.Rise[size_t(step)] = 0.8f + 0.4f * roll(4 + step);
					for (int32_t step = 0; step < shelf.Steps; step++)
						shelf.Rise[size_t(step)] *= 0.95f * budget / total;
				}
				if (Fits(level, shelf))
				{
					m_Shelves.push_back(shelf);
					return;
				}
			}
		};
		attempt(-0.5f * Pi, 0.8f, false, 5.4f, 6.6f, 40);
		attempt(0.5f * Pi, 0.9f, true, 7.4f, 9.2f, 40);
		const int32_t extra = 2;
		for (int32_t n = 0; n < extra; n++)
		{
			const bool outer = Corner(n, 3, salt ^ 0x7E7Eu) < 0.5f;
			attempt(2.0f * Pi * Corner(n, 4, salt ^ 0x7E7Eu), 2.0f * Pi, outer, outer ? 7.2f : 5.4f, outer ? 9.6f : 7.4f, 50);
		}

		for (int32_t n = 0; n < 60 && m_Craters.size() < Craters; n++, serial++)
		{
			const auto roll = [&](int32_t k) { return Corner(serial, k, salt ^ 0xC2A7u); };
			const Crater crater{ peak.Center + FromAngle(2.0f * Pi * roll(0)) * (5.0f + 7.0f * roll(1)), 0.75f + 0.45f * roll(2) };
			bool open = peak.RimDistance(crater.Center) >= 2.2f + crater.Radius;
			for (const PlacedProp& prop : level.Props)
				open = open && Length(prop.Position - crater.Center) >= 1.6f + crater.Radius;
			for (const Crater& other : m_Craters)
				open = open && Length(other.Center - crater.Center) >= 4.0f;
			float near = 0.0f;
			float fade = 0.0f;
			for (const Shelf& shelf : m_Shelves)
				Lift(shelf, crater.Center - peak.Center, near, fade);
			if (open && near < 0.01f && Length(crater.Center - peak.Center) > DaisFoot - 0.5f)
				m_Craters.push_back(crater);
		}

		const auto detail = [&](Vec2 point, float start, float& seam)
		{
			const Vec2 offset = point - peak.Center;
			float height = start;
			float near = 0.0f;
			for (const Shelf& shelf : m_Shelves)
			{
				float fade = 0.0f;
				const float lift = Lift(shelf, offset, near, fade);
				if (fade <= 0.0f)
					continue;
				if (shelf.Outer)
					height += lift * fade;
				else
					height = std::max(height, std::lerp(height, ground(peak.Center + Normalize(offset) * (shelf.Radius + 0.4f)) + lift, fade));
			}
			for (const Crater& crater : m_Craters)
			{
				const float d = Length(point - crater.Center) / crater.Radius;
				if (d < 2.0f)
				{
					height += (d < 1.0f ? -CraterDepth * (1.0f - d * d) : 0.0f) + CraterBerm * std::exp(-std::pow((d - 1.0f) * crater.Radius / 0.3f, 2.0f));
					near = std::max(near, 1.0f - Smooth((d - 1.2f) / 0.5f));
				}
			}
			const float plates = Plates(peak, point, salt, seam) * (1.0f - near);
			seam *= 1.0f - near;
			return height + plates;
		};
		for (int32_t j = 0; j < peak.Rows; j++)
			for (int32_t i = 0; i < peak.Columns; i++)
			{
				const size_t k = size_t(j) * size_t(peak.Columns) + size_t(i);
				float seam = 0.0f;
				m_Heights[k] = detail(place(i, j), base[k], seam);
				m_Seams[k] = seam;
			}
		m_FineColumns = (peak.Columns - 1) * 2 + 1;
		m_FineRows = (peak.Rows - 1) * 2 + 1;
		m_Fine.resize(size_t(m_FineColumns) * size_t(m_FineRows));
		for (int32_t j = 0; j < m_FineRows; j++)
			for (int32_t i = 0; i < m_FineColumns; i++)
			{
				const Vec2 point = peak.Origin + Vec2(float(i), float(j)) * (0.5f * peak.Spacing);
				float seam = 0.0f;
				m_Fine[size_t(j) * size_t(m_FineColumns) + size_t(i)] = peak.RimDistance(point) < -1.0f ? ground(point) : detail(point, ground(point), seam);
			}
		survey(false);
	}

	void PeakRelief::Clear()
	{
		m_Peak = nullptr;
		m_Heights.clear();
		m_Fine.clear();
		m_Hollows.clear();
		m_Screes.clear();
		m_Seams.clear();
		m_Shelves.clear();
		m_Craters.clear();
		m_Lowest = 0.0f;
		m_Highest = 0.0f;
		m_Steepest = 0.0f;
	}

	float PeakRelief::Height(Vec2 position) const
	{
		if (!m_Peak)
			return 0.0f;
		const float step = 0.5f * m_Peak->Spacing;
		const float gx = std::clamp((position.x - m_Peak->Origin.x) / step, 0.0f, float(m_FineColumns - 1) - 1e-3f);
		const float gy = std::clamp((position.y - m_Peak->Origin.y) / step, 0.0f, float(m_FineRows - 1) - 1e-3f);
		const int32_t i = int32_t(gx);
		const int32_t j = int32_t(gy);
		const float* row = m_Fine.data() + size_t(j) * size_t(m_FineColumns) + size_t(i);
		const float* next = row + m_FineColumns;
		const float u = gx - float(i);
		return std::lerp(std::lerp(row[0], row[1], u), std::lerp(next[0], next[1], u), gy - float(j));
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

	float PeakRelief::Seam(Vec2 position) const
	{
		return m_Peak ? m_Peak->Read(m_Seams, m_Peak->Locate(position)) : 0.0f;
	}

	float PeakRelief::StepShadow(Vec2 position) const
	{
		if (!m_Peak)
			return 0.0f;
		const Vec2 offset = position - m_Peak->Center;
		const float r = Length(offset);
		const float angle = AngleOf(offset);
		float shadow = 0.0f;
		for (const Shelf& shelf : m_Shelves)
		{
			const float off = std::remainder(angle - shelf.Angle, 2.0f * Pi);
			const float fade = Smooth((0.5f * shelf.Span - std::abs(off)) * r / ShelfFade);
			for (int32_t step = 0; step < shelf.Steps && fade > 0.0f; step++)
				if (const float out = (r - Edge(shelf, step, shelf.Angle + off)) * Down(shelf) - 0.2f; out >= 0.0f && out < 0.6f)
					shadow = std::max(shadow, fade * (1.0f - Smooth(out / 0.55f)));
		}
		return shadow;
	}
}
