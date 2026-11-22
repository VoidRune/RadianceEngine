#include "PortalArt.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/Props.h>
#include <GameShared/World/Level.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <charconv>
#include <string_view>

namespace Game
{
	namespace
	{
		constexpr float BannerAngle = 62.0f * Pi / 180.0f;
		constexpr float BannerRadius = 3.726f;
		constexpr float BannerHeight = 0.795f - PortalTerraceHeight;
		constexpr float WallRadius = 3.72f;
		constexpr float WallClearance = 0.016f;
		constexpr glm::vec3 SheetBase{ 0.0f, 0.01f, 1.28f };
		constexpr float SheetHalf = 1.26f;
		constexpr float SheetSpring = 3.595f;
		constexpr float SheetApex = SheetSpring + SheetHalf;
		constexpr float RimRadius = 1.22f;
		constexpr float StepSeconds = 1.0f / 120.0f;
		constexpr int32_t MaxSteps = 8;
		constexpr int32_t Iterations = 8;
		constexpr float Damping = 1.4f;
		constexpr float Lift = 1.3f;
		constexpr glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
		constexpr glm::vec3 WindHeading{ 0.42f, 0.0f, 0.91f };
		constexpr float WindBase = 1.1f;
		constexpr float WindGust = 1.5f;
		constexpr glm::vec3 GlowColor{ 0.42f, 0.3f, 1.25f };

		glm::mat4 Turn(float yaw)
		{
			const float c = std::cos(yaw);
			const float s = std::sin(yaw);
			glm::mat4 m(1.0f);
			m[0] = glm::vec4(c, 0.0f, s, 0.0f);
			m[2] = glm::vec4(-s, 0.0f, c, 0.0f);
			return m;
		}

		bool GridJoint(std::string_view name, int32_t& column, int32_t& row)
		{
			const size_t split = name.find('r');
			if (name.size() < 4 || name[0] != 'c' || split == std::string_view::npos || split < 2 || split + 1 >= name.size())
				return false;
			const auto number = [](std::string_view digits, int32_t& value)
			{
				return std::from_chars(digits.data(), digits.data() + digits.size(), value).ec == std::errc{};
			};
			return number(name.substr(1, split - 1), column) && number(name.substr(split + 1), row);
		}

		float Wave(float time, float seed)
		{
			return 0.5f + 0.25f * std::sin(time * 0.37f + seed) + 0.15f * std::sin(time * 1.13f + seed * 2.1f) + 0.1f * std::sin(time * 2.71f + seed * 0.7f);
		}
	}

	glm::vec3 PortalRimPoint(const PlacedProp& gate, float along)
	{
		const float radius = RimRadius;
		const float side = SheetSpring;
		const float arc = Pi * radius;
		float walk = (along - std::floor(along)) * (2.0f * side + arc);
		glm::vec3 local(0.0f, SheetBase.y, SheetBase.z - 0.12f);
		if (walk < side)
			local += glm::vec3(-radius, walk, 0.0f);
		else if ((walk -= side) < arc)
		{
			const float angle = Pi - walk / radius;
			local += glm::vec3(std::cos(angle) * radius, side + std::sin(angle) * radius, 0.0f);
		}
		else
			local += glm::vec3(radius, side - (walk - arc), 0.0f);
		const glm::mat4 place = glm::translate(glm::mat4(1.0f), glm::vec3(gate.Position.x, gate.Base, gate.Position.y)) * Turn(gate.Yaw);
		return glm::vec3(place * glm::vec4(local, 1.0f));
	}

	void PortalArt::Clear()
	{
		m_Gates.clear();
		m_LastTime = -1.0f;
		m_Carry = 0.0f;
	}

	void PortalArt::Build(WorldRenderer& renderer, const Level& level)
	{
		Clear();
		const ModelLibrary& models = renderer.GetModels();
		if (const std::optional<uint32_t> floor = models.FindMesh("MainPortal/Floor"))
			renderer.AddMeshFlags(*floor, MeshGround);
		m_BannerMeshes.clear();
		if (const std::optional<uint32_t> mesh = models.FindMesh("MainPortal/Banner"))
			m_BannerMeshes.push_back(*mesh);
		const std::optional<uint32_t> skeleton = models.FindSkeleton("PortalBanner");
		m_Rest.clear();
		m_Joint.clear();
		m_Links.clear();
		m_Columns = m_Rows = 0;
		if (skeleton)
		{
			const ModelSkeleton& rig = models.GetSkeleton(*skeleton);
			m_JointCount = std::min(rig.JointCount(), MaxModelJoints);
			for (uint32_t i = 0; i < m_JointCount; i++)
			{
				int32_t column = 0;
				int32_t row = 0;
				if (GridJoint(rig.Names[i], column, row))
				{
					m_Columns = std::max(m_Columns, column + 1);
					m_Rows = std::max(m_Rows, row + 1);
				}
			}
			m_Rest.assign(size_t(m_Columns * m_Rows), glm::vec3(0.0f));
			m_Joint.assign(m_Rest.size(), 0u);
			for (uint32_t i = 0; i < m_JointCount; i++)
			{
				int32_t column = 0;
				int32_t row = 0;
				if (!GridJoint(rig.Names[i], column, row))
					continue;
				const size_t index = size_t(row * m_Columns + column);
				m_Rest[index] = glm::vec3(glm::inverse(rig.InverseBind[i])[3]);
				m_Joint[index] = i;
			}
		}
		if (m_BannerMeshes.empty() || m_Columns < 2 || m_Rows < 3)
			m_Rest.clear();
		m_RestInverse.clear();
		for (int32_t row = 0; row < m_Rows && !m_Rest.empty(); row++)
			for (int32_t column = 0; column < m_Columns; column++)
				m_RestInverse.push_back(glm::inverse(Frame(m_Rest, column, row)));
		const auto link = [&](int32_t c0, int32_t r0, int32_t c1, int32_t r1, float stiffness)
		{
			if (c1 < 0 || c1 >= m_Columns || r1 < 0 || r1 >= m_Rows)
				return;
			const uint16_t a = uint16_t(r0 * m_Columns + c0);
			const uint16_t b = uint16_t(r1 * m_Columns + c1);
			m_Links.push_back({ a, b, glm::length(m_Rest[a] - m_Rest[b]), stiffness });
		};
		for (int32_t row = 0; row < m_Rows && !m_Rest.empty(); row++)
			for (int32_t column = 0; column < m_Columns; column++)
			{
				link(column, row, column + 1, row, 1.0f);
				link(column, row, column, row + 1, 1.0f);
				link(column, row, column + 1, row + 1, 0.8f);
				link(column, row, column - 1, row + 1, 0.8f);
				link(column, row, column + 2, row, 0.25f);
				link(column, row, column, row + 2, 0.25f);
			}

		for (const PlacedProp& prop : level.Props)
		{
			if (prop.Id != PropId::CalderaGate)
				continue;
			Gate gate;
			gate.Place = glm::translate(glm::mat4(1.0f), glm::vec3(prop.Position.x, prop.Base, prop.Position.y)) * Turn(prop.Yaw);
			gate.Seed = uint32_t(m_Gates.size()) * 7919u + uint32_t(prop.Position.x * 13.0f);
			for (size_t side = 0; side < 2; side++)
			{
				const float angle = side == 0 ? -BannerAngle : BannerAngle;
				Cloth& cloth = gate.Banners[side];
				const glm::vec3 outward(-std::sin(angle), 0.0f, -std::cos(angle));
				const glm::vec3 anchor = outward * BannerRadius + glm::vec3(0.0f, BannerHeight, 0.0f);
				cloth.Place = gate.Place * glm::translate(glm::mat4(1.0f), anchor) * Turn(Pi - angle);
				cloth.WallPoint = glm::vec3(gate.Place * glm::vec4(outward * WallRadius, 1.0f));
				cloth.WallNormal = glm::normalize(glm::vec3(gate.Place * glm::vec4(outward, 0.0f)));
				cloth.Seed = float(side) * 2.7f + float(gate.Seed % 97u) * 0.31f;
				Reset(cloth);
			}
			m_Gates.push_back(gate);
		}
	}

	glm::vec3 PortalArt::Point(const std::vector<glm::vec3>& points, int32_t column, int32_t row) const
	{
		column = std::clamp(column, 0, m_Columns - 1);
		row = std::clamp(row, 0, m_Rows - 1);
		return points[size_t(row * m_Columns + column)];
	}

	glm::mat4 PortalArt::Frame(const std::vector<glm::vec3>& points, int32_t column, int32_t row) const
	{
		const glm::vec3 across = Point(points, column + 1, row) - Point(points, column - 1, row);
		const glm::vec3 up = Point(points, column, row - 1) - Point(points, column, row + 1);
		const glm::vec3 x = glm::normalize(across);
		const glm::vec3 z = glm::normalize(glm::cross(across, up));
		const glm::vec3 y = glm::cross(z, x);
		glm::mat4 frame(1.0f);
		frame[0] = glm::vec4(x, 0.0f);
		frame[1] = glm::vec4(y, 0.0f);
		frame[2] = glm::vec4(z, 0.0f);
		frame[3] = glm::vec4(Point(points, column, row), 1.0f);
		return frame;
	}

	void PortalArt::Reset(Cloth& cloth) const
	{
		cloth.Position.resize(m_Rest.size());
		for (size_t i = 0; i < m_Rest.size(); i++)
			cloth.Position[i] = glm::vec3(cloth.Place * glm::vec4(m_Rest[i], 1.0f));
		cloth.Previous = cloth.Position;
	}

	void PortalArt::Step(Cloth& cloth, float dt, float time) const
	{
		const float gust = Wave(time, cloth.Seed);
		const glm::vec3 heading = glm::normalize(WindHeading + glm::vec3(0.35f * std::sin(time * 0.23f + cloth.Seed), 0.0f, 0.0f));
		const float keep = std::exp(-Damping * dt);
		std::vector<glm::vec3> normals(cloth.Position.size());
		for (int32_t row = 0; row < m_Rows; row++)
			for (int32_t column = 0; column < m_Columns; column++)
			{
				const glm::vec3 across = Point(cloth.Position, column + 1, row) - Point(cloth.Position, column - 1, row);
				const glm::vec3 up = Point(cloth.Position, column, row - 1) - Point(cloth.Position, column, row + 1);
				const glm::vec3 n = glm::cross(across, up);
				const float length = glm::length(n);
				normals[size_t(row * m_Columns + column)] = length > 1e-6f ? n / length : glm::vec3(0.0f, 0.0f, 1.0f);
			}
		for (int32_t row = 1; row < m_Rows; row++)
			for (int32_t column = 0; column < m_Columns; column++)
			{
				const size_t i = size_t(row * m_Columns + column);
				const glm::vec3 velocity = (cloth.Position[i] - cloth.Previous[i]) / dt;
				const float flutter = std::sin(time * 6.3f + float(row) * 1.1f + float(column) * 0.7f + cloth.Seed) * std::sin(time * 2.9f - float(row) * 0.5f + cloth.Seed);
				const glm::vec3 wind = heading * (WindBase + WindGust * gust) * (1.0f + 0.25f * flutter);
				const glm::vec3 relative = wind - velocity;
				const glm::vec3 push = normals[i] * (Lift * glm::dot(relative, normals[i]));
				const glm::vec3 next = cloth.Position[i] + (cloth.Position[i] - cloth.Previous[i]) * keep + (Gravity + push) * (dt * dt);
				cloth.Previous[i] = cloth.Position[i];
				cloth.Position[i] = next;
			}
		for (int32_t column = 0; column < m_Columns; column++)
		{
			const size_t i = size_t(column);
			cloth.Position[i] = glm::vec3(cloth.Place * glm::vec4(m_Rest[i], 1.0f));
			cloth.Previous[i] = cloth.Position[i];
		}
		for (int32_t pass = 0; pass < Iterations; pass++)
		{
			for (const Link& link : m_Links)
			{
				glm::vec3& a = cloth.Position[link.A];
				glm::vec3& b = cloth.Position[link.B];
				const glm::vec3 delta = b - a;
				const float length = glm::length(delta);
				if (length < 1e-6f)
					continue;
				const glm::vec3 fix = delta * ((length - link.Length) / length * 0.5f * link.Stiffness);
				const bool pinnedA = link.A < uint16_t(m_Columns);
				const bool pinnedB = link.B < uint16_t(m_Columns);
				if (pinnedA && !pinnedB)
					b -= fix * 2.0f;
				else if (pinnedB && !pinnedA)
					a += fix * 2.0f;
				else if (!pinnedA)
				{
					a += fix;
					b -= fix;
				}
			}
			for (size_t i = size_t(m_Columns); i < cloth.Position.size(); i++)
			{
				glm::vec3& p = cloth.Position[i];
				const float depth = glm::dot(p - cloth.WallPoint, cloth.WallNormal);
				if (depth < WallClearance)
					p += cloth.WallNormal * (WallClearance - depth);
			}
		}
	}

	void PortalArt::Draw(WorldRenderer& renderer, const LevelView& view)
	{
		if (m_Gates.empty())
			return;
		const float time = view.Time;
		float dt = m_LastTime < 0.0f ? 0.0f : time - m_LastTime;
		m_LastTime = time;
		const bool reset = dt > 0.5f || dt < 0.0f;
		dt = std::clamp(dt, 0.0f, 0.25f);
		m_Surge += (float(view.GateFocus) - m_Surge) * (1.0f - std::exp(-3.0f * dt));
		if (!m_Rest.empty())
		{
			m_Carry += dt;
			int32_t steps = 0;
			for (Gate& gate : m_Gates)
				for (Cloth& cloth : gate.Banners)
					if (reset)
						Reset(cloth);
			while (m_Carry >= StepSeconds && steps < MaxSteps)
			{
				m_Carry -= StepSeconds;
				steps++;
				const float at = time - m_Carry;
				for (Gate& gate : m_Gates)
					for (Cloth& cloth : gate.Banners)
						Step(cloth, StepSeconds, at);
			}
			m_Carry = std::min(m_Carry, StepSeconds);
		}

		for (const Gate& gate : m_Gates)
		{
			if (!m_Rest.empty())
				for (const Cloth& cloth : gate.Banners)
				{
					JointMatrices palette;
					palette.fill(cloth.Place);
					for (int32_t row = 0; row < m_Rows; row++)
						for (int32_t column = 0; column < m_Columns; column++)
						{
							const size_t index = size_t(row * m_Columns + column);
							palette[m_Joint[index]] = Frame(cloth.Position, column, row) * m_RestInverse[index];
						}
					const uint32_t skin = renderer.AddPalette(palette, m_JointCount);
					for (const uint32_t mesh : m_BannerMeshes)
						renderer.DrawModel(mesh, glm::mat4(1.0f), skin);
				}

			const glm::vec3 base = glm::vec3(gate.Place * glm::vec4(SheetBase, 1.0f));
			const glm::vec3 right = glm::normalize(glm::vec3(gate.Place[0]));
			const glm::vec3 normal = -glm::normalize(glm::vec3(gate.Place[2]));
			const float breathe = 0.92f + 0.08f * std::sin(time * 1.3f + float(gate.Seed));
			renderer.GetPortal().Draw({
				.Base = base,
				.Right = right,
				.Normal = normal,
				.HalfWidth = SheetHalf,
				.Spring = SheetSpring,
				.Apex = SheetApex,
				.Intensity = breathe,
				.Surge = m_Surge,
				.Seed = gate.Seed,
			});
			const glm::vec3 eye = base + glm::vec3(0.0f, SheetSpring * 0.72f, 0.0f);
			renderer.DrawLight({ eye + normal * 1.1f, 7.8f, GlowColor * (2.2f + 1.2f * m_Surge) * breathe });
			renderer.DrawLight({ base + glm::vec3(0.0f, 0.5f, 0.0f) + normal * 1.7f, 3.8f, GlowColor * (1.1f + 0.6f * m_Surge) * breathe });
		}
	}
}
