#include "LevelRenderer.h"
#include "Rendering/Models.h"
#include "Rendering/WorldRenderer.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float RockHeight = 1.5f;
		constexpr float CutHeight = 0.6f;
		constexpr float VoidDepth = 3.5f;
		constexpr float StepHeight = 0.125f;
		constexpr float WallPiece = 1.0f;
		constexpr glm::vec3 PortalGold = { 1.0f, 0.78f, 0.3f };
		constexpr float SightSlope = 17.0f / 11.0f;

		float Hash(int32_t x, int32_t y, int32_t salt = 0)
		{
			uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + uint32_t(salt) * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return float((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
		}

		float CutFactor(const LevelView& view, Vec2 point, float top)
		{
			if (!view.Cutaway)
				return 0.0f;
			const float depth = view.Focus.y - point.y;
			if (depth < -0.3f)
				return 0.0f;
			const float width = 1.6f + 0.25f * std::max(depth, 0.0f);
			const float side = std::clamp((width + 0.6f - std::abs(point.x - view.Focus.x)) / 0.6f, 0.0f, 1.0f);
			const float sight = 0.5f + SightSlope * std::max(depth, 0.0f);
			const float above = std::clamp((top - view.FocusHeight - sight) / 0.5f, 0.0f, 1.0f);
			return side * above;
		}

		float SurfaceHeight(const TileMap& map, int32_t x, int32_t y, float own)
		{
			const Cell& cell = map.Get(x, y);
			switch (GetTerrain(cell.Terrain).Style)
			{
			case TerrainStyle::Void:
				return own - VoidDepth;
			case TerrainStyle::Solid:
				return own;
			case TerrainStyle::Liquid:
				return float(cell.Level) * HeightUnit - 0.6f;
			default:
				return float(cell.Level) * HeightUnit;
			}
		}

		Vec2 SlopeLocal(Direction slope, float along, float across)
		{
			switch (slope)
			{
			case Direction::North:
				return { across, along };
			case Direction::South:
				return { across, 1.0f - along };
			case Direction::East:
				return { along, across };
			default:
				return { 1.0f - along, across };
			}
		}

		float SlopeYaw(Direction slope)
		{
			switch (slope)
			{
			case Direction::North:
				return 0.0f;
			case Direction::East:
				return -HalfPi;
			case Direction::South:
				return Pi;
			default:
				return HalfPi;
			}
		}

		void Column(WorldRenderer& renderer, float x, float z, float bottom, float top, glm::vec2 half, const glm::vec3& color, float ground, float yaw = 0.0f)
		{
			if (top - bottom < 0.002f)
				return;
			renderer.DrawBox({ x, (top + bottom) * 0.5f, z }, { half.x, (top - bottom) * 0.5f, half.y }, color, yaw, 0.0f, ground);
		}
	}

	glm::vec3 ToColor(uint32_t rgb)
	{
		const glm::vec3 srgb = glm::vec3(float((rgb >> 16) & 0xFF), float((rgb >> 8) & 0xFF), float(rgb & 0xFF)) / 255.0f;
		return glm::pow(srgb, glm::vec3(2.2f));
	}

	void LevelRenderer::Draw(WorldRenderer& renderer, const Level& level, const LevelView& view)
	{
		DrawTerrain(renderer, level, view);
		DrawWalls(renderer, level, view);
		DrawProps(renderer, level, view);
		DrawPortals(renderer, level, view);
	}

	void LevelRenderer::DrawTerrain(WorldRenderer& renderer, const Level& level, const LevelView& view) const
	{
		const TileMap& map = level.Map;
		const int32_t minX = std::max(int32_t(std::floor(view.Min.x)), 0);
		const int32_t maxX = std::min(int32_t(std::floor(view.Max.x)), map.GetWidth() - 1);
		const int32_t minY = std::max(int32_t(std::floor(view.Min.y)), 0);
		const int32_t maxY = std::min(int32_t(std::floor(view.Max.y)), map.GetHeight() - 1);
		for (int32_t y = minY; y <= maxY; y++)
		{
			for (int32_t x = minX; x <= maxX; x++)
			{
				const Cell& cell = map.Get(x, y);
				const TerrainDef& def = GetTerrain(cell.Terrain);
				const float height = float(cell.Level) * HeightUnit;
				const float shade = Hash(x, y);
				const float cx = float(x) + 0.5f;
				const float cz = float(y) + 0.5f;
				const glm::vec3 base = ToColor(def.Color);
				const glm::vec3 accent = ToColor(def.Accent);
				switch (def.Style)
				{
				case TerrainStyle::Void:
					break;
				case TerrainStyle::Solid:
				{
					bool deep = true;
					for (int32_t dy = -1; dy <= 1 && deep; dy++)
						for (int32_t dx = -1; dx <= 1 && deep; dx++)
							deep = !map.IsWalkable(x + dx, y + dy) && map.GetTerrain(x + dx, y + dy) != TerrainId::Water;
					const float top = height + glm::mix(RockHeight, CutHeight, CutFactor(view, { cx, cz }, height + RockHeight));
					Column(renderer, cx, cz, height - 0.1f, top, { 0.5f, 0.5f }, (deep ? base : accent) * (0.85f + 0.3f * shade), height);
					break;
				}
				case TerrainStyle::Liquid:
				{
					const float ripple = 0.5f + 0.5f * std::sin(view.Time * 1.7f + shade * 6.28f);
					renderer.DrawBox({ cx, height - 0.17f, cz }, { 0.5f, 0.03f, 0.5f }, glm::mix(base, accent, ripple * 0.6f), 0.0f, 0.12f, height - 0.2f);
					Column(renderer, cx, cz, height - 0.6f, height - 0.2f, { 0.5f, 0.5f }, base * 0.35f, height - 0.6f);
					break;
				}
				default:
				{
					float bottom = height - 0.12f;
					for (const Direction side : { Direction::North, Direction::East, Direction::South, Direction::West })
					{
						int32_t nx = x;
						int32_t ny = y;
						TileMap::Step(side, nx, ny);
						bottom = std::min(bottom, SurfaceHeight(map, nx, ny, height) - 0.02f);
					}
					const glm::vec3 color = glm::mix(base, accent, shade * 0.5f);
					if (cell.Slope != Direction::None && cell.Climb > 0)
					{
						const float rise = float(cell.Climb) * HeightUnit;
						Column(renderer, cx, cz, bottom, height, { 0.5f, 0.5f }, accent, height);
						if (def.Style == TerrainStyle::Ground)
						{
							renderer.Draw(MeshKind::Wedge, { cx, height + rise * 0.5f, cz }, { 0.5f, rise * 0.5f, 0.5f }, color, SlopeYaw(cell.Slope), 0.0f, height);
							break;
						}
						const int32_t steps = std::max(2, int32_t(std::lround(rise / StepHeight)));
						for (int32_t i = 0; i < steps; i++)
						{
							const float along = (float(i) + 0.5f) / float(steps);
							const Vec2 local = SlopeLocal(cell.Slope, along, 0.5f);
							const bool vertical = cell.Slope == Direction::North || cell.Slope == Direction::South;
							const glm::vec2 half = vertical ? glm::vec2(0.5f, 0.5f / float(steps)) : glm::vec2(0.5f / float(steps), 0.5f);
							const float top = height + rise * float(i + 1) / float(steps);
							Column(renderer, float(x) + local.x, float(y) + local.y, height, top, half, i % 2 == 0 ? color : glm::mix(color, accent, 0.25f), height);
						}
						break;
					}
					switch (def.Style)
					{
					case TerrainStyle::Slabs:
					{
						Column(renderer, cx, cz, bottom, height - 0.01f, { 0.5f, 0.5f }, accent * 0.8f, height);
						if (cell.Terrain == TerrainId::Cobblestone)
						{
							for (int32_t i = 0; i < 4; i++)
							{
								const float jitter = Hash(x, y, i + 1);
								const glm::vec3 stone = glm::mix(base, accent, jitter * 0.6f);
								Column(renderer, cx + (i % 2 == 0 ? -0.25f : 0.25f), cz + (i < 2 ? -0.25f : 0.25f), height - 0.08f, height + 0.01f * jitter, { 0.215f, 0.215f }, stone, height, (jitter - 0.5f) * 0.3f);
							}
						}
						else
						{
							const glm::vec3 slab = cell.Terrain == TerrainId::Marble && ((x + y) & 1) ? glm::mix(base, accent, 0.45f) : color;
							Column(renderer, cx, cz, height - 0.08f, height, { 0.47f, 0.47f }, slab, height);
						}
						break;
					}
					case TerrainStyle::Planks:
					{
						Column(renderer, cx, cz, bottom, height - 0.01f, { 0.5f, 0.5f }, accent, height);
						for (int32_t i = 0; i < 3; i++)
						{
							const float tint = Hash(x + (y % 2) * 7, y, i + 3);
							Column(renderer, cx, float(y) + (float(i) + 0.5f) / 3.0f, height - 0.06f, height, { 0.5f, 0.155f }, glm::mix(base, accent, 0.15f + tint * 0.3f) * (0.9f + 0.2f * tint), height);
						}
						break;
					}
					default:
						Column(renderer, cx, cz, bottom, height, { 0.5f, 0.5f }, color, height);
						if (cell.Terrain == TerrainId::Grass && shade < 0.35f)
							renderer.Draw(MeshKind::Cone, { cx + (Hash(x, y, 1) - 0.5f) * 0.6f, height + 0.09f, cz + (Hash(x, y, 2) - 0.5f) * 0.6f }, { 0.07f, 0.09f, 0.07f }, glm::mix(accent, glm::vec3(0.35f, 0.5f, 0.15f), 0.4f), 0.0f, 0.0f, height);
						break;
					}
					break;
				}
				}
			}
		}
	}

	void LevelRenderer::DrawWalls(WorldRenderer& renderer, const Level& level, const LevelView& view) const
	{
		for (const WallSegment& wall : level.Walls)
		{
			if (std::max(wall.From.x, wall.To.x) < view.Min.x - 1.0f || std::min(wall.From.x, wall.To.x) > view.Max.x + 1.0f || std::max(wall.From.y, wall.To.y) < view.Min.y - 1.0f || std::min(wall.From.y, wall.To.y) > view.Max.y + 1.0f)
				continue;
			const WallDef& def = GetWall(wall.Style);
			const Vec2 span = wall.To - wall.From;
			const float length = Length(span);
			if (length < 1e-4f)
				continue;
			const Vec2 direction = span / length;
			const float yaw = AngleOf(span);
			const int32_t pieces = std::max(1, int32_t(std::ceil(length / WallPiece)));
			for (int32_t i = 0; i < pieces; i++)
			{
				Vec2 start = Lerp(wall.From, wall.To, float(i) / float(pieces));
				Vec2 end = Lerp(wall.From, wall.To, float(i + 1) / float(pieces));
				if (i == 0)
					start -= direction * (def.Thickness * 0.5f);
				if (i == pieces - 1)
					end += direction * (def.Thickness * 0.5f);
				const Vec2 center = (start + end) * 0.5f;
				const float halfLength = Length(end - start) * 0.5f;
				const float cut = CutFactor(view, center, wall.Base + def.Height);
				const float top = wall.Base + glm::mix(def.Height, CutHeight, cut);
				const float bottom = wall.Bottom - 0.05f;
				renderer.DrawBox({ center.x, (top + bottom) * 0.5f, center.y }, { halfLength, (top - bottom) * 0.5f, def.Thickness * 0.5f }, ToColor(def.Color), yaw, 0.0f, wall.Base);
				renderer.DrawBox({ center.x, top + 0.03f, center.y }, { halfLength + 0.005f, 0.03f, def.Thickness * 0.5f + 0.03f }, ToColor(def.Top), yaw, 0.0f, wall.Base);
			}
		}
	}

	void LevelRenderer::DrawProps(WorldRenderer& renderer, const Level& level, const LevelView& view)
	{
		if (m_Level != &level || m_PropReach.size() != level.Props.size())
		{
			m_Level = &level;
			m_PropReach.clear();
			m_PropTop.clear();
			for (const PlacedProp& prop : level.Props)
			{
				float reach = 0.0f;
				float top = 0.0f;
				for (const PropPart& part : GetProp(prop.Id).Parts)
				{
					reach = std::max(reach, Length(part.Offset) + std::max(part.Half.x, part.Half.y));
					top = std::max(top, part.Base + part.Height);
				}
				m_PropReach.push_back(reach);
				m_PropTop.push_back(top);
			}
		}

		for (size_t i = 0; i < level.Props.size(); i++)
		{
			const PlacedProp& prop = level.Props[i];
			const float reach = m_PropReach[i];
			if (prop.Position.x + reach < view.Min.x || prop.Position.x - reach > view.Max.x || prop.Position.y + reach < view.Min.y || prop.Position.y - reach > view.Max.y)
				continue;
			const PropDef& def = GetProp(prop.Id);
			float cutTop = 1e9f;
			if (def.Tall)
			{
				const Vec2 toFocus = view.Focus - prop.Position;
				const float distance = Length(toFocus);
				const Vec2 closest = distance > 1e-4f ? prop.Position + toFocus * (std::min(distance, reach) / distance) : prop.Position;
				const float cut = CutFactor(view, closest, prop.Base + m_PropTop[i]);
				if (cut > 0.0f)
					cutTop = glm::mix(m_PropTop[i], CutHeight + 0.05f, cut);
			}
			const Vec2 axis = FromAngle(prop.Yaw);
			for (const PropPart& part : def.Parts)
			{
				const float bottom = part.Base;
				const float top = std::min(part.Base + part.Height, cutTop);
				if (top - bottom < 0.005f)
					continue;
				const Vec2 center = prop.Position + Rotate(part.Offset, axis);
				renderer.Draw(part.Mesh, { center.x, prop.Base + (bottom + top) * 0.5f, center.y }, { part.Half.x, (top - bottom) * 0.5f, part.Half.y }, ToColor(part.Color), prop.Yaw + part.Yaw, part.Emission, prop.Base);
			}
		}
	}

	void LevelRenderer::DrawPortals(WorldRenderer& renderer, const Level& level, const LevelView& view) const
	{
		for (size_t i = 0; i < level.Portals.size(); i++)
		{
			const Portal& portal = level.Portals[i];
			const float distance = Length(portal.Position - view.Focus);
			const bool hovered = int32_t(i) == view.HoveredPortal;
			const bool reachable = int32_t(i) == view.ReachablePortal;
			if (!hovered && !reachable && distance > 8.0f)
				continue;
			const float ground = level.Map.GetHeight(portal.Position);
			const float pulse = 0.65f + 0.35f * std::sin(view.Time * 4.0f + float(i));
			const glm::vec3 center = { portal.Position.x, ground + 0.03f, portal.Position.y };
			if (hovered)
			{
				DrawRing(renderer, center, 0.75f, 24, PortalGold, pulse, 0.06f);
				DrawRing(renderer, center + glm::vec3(0.0f, 0.02f, 0.0f), 0.45f, 16, PortalGold, pulse * 0.8f, 0.04f);
			}
			else
				DrawRing(renderer, center, 0.6f, 20, PortalGold * (reachable ? 0.9f : 0.5f), reachable ? 0.7f * pulse : 0.3f, 0.04f);
		}
	}
}
