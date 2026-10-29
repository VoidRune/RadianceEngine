#include "LevelRenderer.h"
#include "Rendering/Models.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>

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
		constexpr float SightSlope = CameraHeight / (CameraBack + CameraFocusOffset);

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

		constexpr float IslandBottom = -1.6f;
		constexpr float ShoreDrop = 0.08f;
		constexpr float ShoreFlare = 0.1f;
		constexpr float BaseFlare = 0.34f;
		constexpr float SinkDepth = 1.8f;
		constexpr float MaxTilt = 0.35f;

		float SmoothStep(float edge0, float edge1, float x)
		{
			const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float ValueNoise(Vec2 p, uint32_t salt)
		{
			const float fx = std::floor(p.x);
			const float fy = std::floor(p.y);
			const int32_t x = int32_t(fx);
			const int32_t y = int32_t(fy);
			const float u = SmoothStep(0.0f, 1.0f, p.x - fx);
			const float v = SmoothStep(0.0f, 1.0f, p.y - fy);
			const float bottom = std::lerp(CollapseNoise(x, y, salt), CollapseNoise(x + 1, y, salt), u);
			const float top = std::lerp(CollapseNoise(x, y + 1, salt), CollapseNoise(x + 1, y + 1, salt), u);
			return std::lerp(bottom, top, v);
		}

		float IslandSurface(Vec2 p, Vec2 center)
		{
			const float dome = 0.62f * (1.0f - SmoothStep(1.5f, 14.0f, Length(p - center)));
			const float hills = 0.4f * (ValueNoise(p / 4.5f, 0xA11Cu) - 0.5f) + 0.1f * (ValueNoise(p / 1.6f, 0xB0B5u) - 0.5f);
			return std::max(0.12f + dome + hills, 0.04f);
		}

		constexpr float IceTop = 0.06f;
		constexpr float SnowRise = 0.24f;
		constexpr float IceBottom = -0.75f;

		constexpr float SlabTop = 0.02f;
		constexpr float SlabJitter = 0.03f;
		constexpr float SlabInset = 0.06f;
		constexpr float GroutDepth = 0.05f;
		constexpr float ShelfDepth = 0.8f;
		constexpr float RuinGravity = 7.0f;
		constexpr float RuinFallSeconds = 2.8f;
		constexpr float RuinTumble = 0.45f;
		constexpr float RuinRise = 8.0f;

		glm::vec3 SlabColor(CellCoord coord, Vec2 seed)
		{
			const float pick = CollapseNoise(coord.X, coord.Y, 0x5AB5u);
			glm::vec3 color = glm::mix(ToColor(0xD6C8AA), ToColor(0xE7DFCE), SmoothStep(0.3f, 0.75f, ValueNoise(seed / 7.0f, 0x7A11u)));
			color = glm::mix(color, ToColor(0xC9B08A), SmoothStep(0.55f, 0.85f, ValueNoise(seed / 3.1f, 0x3C5Eu)) * 0.55f);
			if (pick > 0.93f)
				color = ToColor(0xF0ECE4);
			else if (pick < 0.05f)
				color = ToColor(0xC3CDD3);
			else if (pick < 0.12f)
				color = glm::mix(color, ToColor(0xBFA27A), 0.6f);
			return color * (0.93f + 0.12f * CollapseNoise(coord.X, coord.Y, 0x1D1Du));
		}

		glm::vec3 UndersideColor(const glm::vec3& centroid, float facet)
		{
			const Vec2 p(centroid.x, centroid.z);
			const float band = 0.5f + 0.5f * std::sin(centroid.y * 3.3f + 5.0f * ValueNoise(p / 3.7f, 0x8A1Du));
			glm::vec3 color = glm::mix(ToColor(0x9A7F5E), ToColor(0xC2A783), band);
			color = glm::mix(color, ToColor(0x7E6B5A), SmoothStep(-1.5f, -5.0f, centroid.y) * 0.5f);
			return color * (0.84f + 0.24f * facet);
		}

		float SnowAmount(const TileMap& map, Vec2 p)
		{
			const Vec2 q = p - Vec2(0.5f, 0.5f);
			const int32_t x = int32_t(std::floor(q.x));
			const int32_t y = int32_t(std::floor(q.y));
			const float u = SmoothStep(0.0f, 1.0f, q.x - float(x));
			const float v = SmoothStep(0.0f, 1.0f, q.y - float(y));
			const auto snow = [&](int32_t tx, int32_t ty) { return map.GetTerrain(tx, ty) == TerrainId::Snow ? 1.0f : 0.0f; };
			return std::lerp(std::lerp(snow(x, y), snow(x + 1, y), u), std::lerp(snow(x, y + 1), snow(x + 1, y + 1), u), v);
		}

		float IceHeight(const TileMap& map, Vec2 p)
		{
			return IceTop + SnowRise * SmoothStep(0.0f, 1.0f, SnowAmount(map, p));
		}

		glm::vec3 IceColor(const glm::vec3& centroid, float snow, float facet)
		{
			const Vec2 p(centroid.x, centroid.z);
			const float clear = SmoothStep(0.45f, 0.75f, ValueNoise(p / 5.0f, 0x1CE5u));
			glm::vec3 color = glm::mix(ToColor(0xB4D8EA), ToColor(0x5891B8), clear * 0.85f);
			color = glm::mix(color, ToColor(0xD2E9F4), SmoothStep(0.55f, 0.8f, ValueNoise(p / 1.7f, 0xF125u)) * 0.5f * (1.0f - clear));
			color = glm::mix(color, ToColor(0xDDE6EE), snow);
			return color * (0.92f + 0.16f * facet);
		}

		struct PolygonVertex
		{
			double X = 0.0;
			double Y = 0.0;
			CellCoord Edge;
			bool Frame = false;
		};

		std::vector<PolygonVertex> VoronoiPolygon(CellCoord cell)
		{
			const Vec2 seed = CellSeed(cell);
			std::vector<PolygonVertex> polygon = {
				{ double(cell.X - 1), double(cell.Y - 1), {}, true },
				{ double(cell.X + 2), double(cell.Y - 1), {}, true },
				{ double(cell.X + 2), double(cell.Y + 2), {}, true },
				{ double(cell.X - 1), double(cell.Y + 2), {}, true },
			};
			std::vector<PolygonVertex> clipped;
			for (int32_t dy = -2; dy <= 2; dy++)
				for (int32_t dx = -2; dx <= 2; dx++)
				{
					if (dx == 0 && dy == 0)
						continue;
					const CellCoord other{ cell.X + dx, cell.Y + dy };
					const Vec2 neighbour = CellSeed(other);
					const double nx = double(neighbour.x) - double(seed.x);
					const double ny = double(neighbour.y) - double(seed.y);
					const double mx = 0.5 * (double(neighbour.x) + double(seed.x));
					const double my = 0.5 * (double(neighbour.y) + double(seed.y));
					clipped.clear();
					for (size_t i = 0; i < polygon.size(); i++)
					{
						const PolygonVertex& a = polygon[i];
						const PolygonVertex& b = polygon[(i + 1) % polygon.size()];
						const double da = (a.X - mx) * nx + (a.Y - my) * ny;
						const double db = (b.X - mx) * nx + (b.Y - my) * ny;
						if (da <= 0.0)
							clipped.push_back(a);
						if ((da <= 0.0) == (db <= 0.0))
							continue;
						const double t = da / (da - db);
						PolygonVertex cut{ a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Edge, a.Frame };
						if (da <= 0.0)
						{
							cut.Edge = other;
							cut.Frame = false;
						}
						clipped.push_back(cut);
					}
					polygon.swap(clipped);
				}
			return polygon;
		}

		std::optional<Vec2> VoronoiCorner(CellCoord a, CellCoord b, CellCoord c)
		{
			std::array<CellCoord, 3> cells = { a, b, c };
			std::sort(cells.begin(), cells.end(), [](CellCoord l, CellCoord r) { return l.X != r.X ? l.X < r.X : l.Y < r.Y; });
			const Vec2 p = CellSeed(cells[0]);
			const Vec2 q = CellSeed(cells[1]);
			const Vec2 r = CellSeed(cells[2]);
			const double ax = p.x, ay = p.y, bx = q.x, by = q.y, cx = r.x, cy = r.y;
			const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
			if (std::abs(d) < 1e-12)
				return std::nullopt;
			const double a2 = ax * ax + ay * ay;
			const double b2 = bx * bx + by * by;
			const double c2 = cx * cx + cy * cy;
			return Vec2(float((a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d), float((a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d));
		}

		std::pair<int64_t, int64_t> CornerKey(Vec2 position)
		{
			return { std::llround(double(position.x) * 8192.0), std::llround(double(position.y) * 8192.0) };
		}

		uint32_t PackColor(const glm::vec3& color, float alpha = 1.0f)
		{
			const glm::uvec3 c(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
			return c.r | (c.g << 8) | (c.b << 16) | (uint32_t(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f) << 24);
		}

		float VeinField(const glm::vec3& point)
		{
			const Vec2 p(point.x, point.z);
			const float region = SmoothStep(0.45f, 0.7f, ValueNoise(p * 0.16f + Vec2(11.3f, 4.1f), 0x7E1Bu));
			const float taper = SmoothStep(0.35f, 0.6f, ValueNoise(p * 0.55f + Vec2(19.0f, 2.3f), 0x3A5Fu));
			return region * taper;
		}

		float CornerJitter(Vec2 position)
		{
			const auto [x, y] = CornerKey(position);
			return CollapseNoise(int32_t(x), int32_t(y), 0x6A11u) - 0.5f;
		}

		glm::vec3 TopColor(const glm::vec3& centroid, const glm::vec3& normal, float facet)
		{
			const Vec2 p(centroid.x, centroid.z);
			const float flat = SmoothStep(0.8f, 0.97f, normal.y);
			glm::vec3 color = glm::mix(ToColor(0x3B4760), ToColor(0x637594), SmoothStep(0.1f, 0.8f, centroid.y));
			color = glm::mix(color, ToColor(0x8190AA), SmoothStep(0.45f, 0.72f, ValueNoise(p / 2.2f, 0xC0DEu)) * 0.7f * flat);
			color = glm::mix(color, ToColor(0x4F4C7C), SmoothStep(0.6f, 0.84f, ValueNoise(p / 3.4f, 0xFACEu)) * 0.5f);
			color = glm::mix(color, ToColor(0x386673), SmoothStep(0.62f, 0.86f, ValueNoise(p / 2.9f, 0x7EA1u)) * 0.5f);
			return color * (0.8f + 0.4f * facet);
		}

		struct IslandMesh
		{
			std::vector<ModelVertex>& Vertices;
			std::vector<uint32_t>& Indices;
			const TileMap* Snow = nullptr;
			bool Bare = false;

			void Triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec3& facing, const glm::vec3& color)
			{
				glm::vec3 normal = glm::cross(b - a, c - a);
				const float length = glm::length(normal);
				if (length < 1e-7f)
					return;
				if (glm::dot(normal, facing) < 0.0f)
				{
					std::swap(b, c);
					normal = -normal;
				}
				normal /= length;
				for (const glm::vec3& point : { a, b, c })
				{
					Indices.push_back(uint32_t(Vertices.size()));
					Vertices.push_back({ .Position = point, .Normal = normal, .Color = PackColor(color, Bare ? 0.0f : Snow ? SnowAmount(*Snow, Vec2(point.x, point.z)) : VeinField(point)) });
				}
			}

			void Quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& facing, const glm::vec3& color)
			{
				Triangle(a, b, c, facing, color);
				Triangle(a, c, d, facing, color);
			}
		};
	}

	void IslandArt::Clear()
	{
		m_Cells.clear();
		m_Corners.clear();
		m_Chunks.clear();
		m_Lookup.clear();
		m_Islands.clear();
		m_Shown.clear();
		m_Width = 0;
		m_Height = 0;
		m_Sky = false;
		m_Level = nullptr;
	}

	void IslandArt::Build(WorldRenderer& renderer, const Level& level)
	{
		Clear();
		const TileMap& map = level.Map;
		m_Width = map.GetWidth();
		m_Height = map.GetHeight();
		m_Lookup.assign(size_t(m_Width) * size_t(m_Height), -1);
		m_Frozen = level.Kind == MapKind::FrozenLake;
		m_Sky = level.Skyborne;
		m_Level = &level;
		m_Islands = level.Islands;
		std::vector<int32_t> islandOf(m_Lookup.size(), -1);
		for (size_t i = 0; i < m_Islands.size(); i++)
			for (const TileCoord cell : m_Islands[i].Cells)
				if (map.IsInside(cell.X, cell.Y))
					islandOf[size_t(cell.Y) * size_t(m_Width) + size_t(cell.X)] = int32_t(i);
		const Vec2 center = { float(m_Width) * 0.5f, float(m_Height) * 0.5f };
		const auto surface = [&](Vec2 position, float jitter)
		{
			if (m_Sky)
				return SlabTop - GroutDepth;
			return m_Frozen ? IceHeight(map, position) + 0.012f * jitter : IslandSurface(position, center) + 0.1f * jitter;
		};
		std::map<std::pair<int32_t, int32_t>, uint32_t> chunks;
		for (int32_t y = 0; y < m_Height; y++)
			for (int32_t x = 0; x < m_Width; x++)
			{
				const int32_t island = islandOf[size_t(y) * size_t(m_Width) + size_t(x)];
				if (!IsBrawlFloor(map.GetTerrain(x, y)) && island < 0)
					continue;
				const CellCoord coord{ x, y };
				const CellCoord chunk = island >= 0 ? CellCoord{ -1 - island, INT32_MIN } : ChunkOf(coord);
				const auto [found, added] = chunks.try_emplace({ chunk.X, chunk.Y }, uint32_t(m_Chunks.size()));
				if (added)
					m_Chunks.push_back({ .Coord = chunk, .Tile = uint32_t(y * m_Width + x), .Island = island });
				m_Chunks[found->second].Cells.push_back(uint32_t(m_Cells.size()));
				m_Lookup[size_t(y) * size_t(m_Width) + size_t(x)] = int32_t(m_Cells.size());
				const Vec2 seed = CellSeed(coord);
				const float peak = m_Sky ? SlabTop + SlabJitter * (CollapseNoise(x, y, 0x2D7Fu) - 0.5f) : surface(seed, CollapseNoise(x, y, 0x2D7Fu) - 0.4f);
				m_Cells.push_back({ .Coord = coord, .Seed = seed, .Peak = peak, .Chunk = found->second });
			}
		if (m_Cells.empty())
			return;

		std::vector<CellCoord> across;
		std::vector<uint8_t> framed;
		for (Cell& cell : m_Cells)
		{
			const std::vector<PolygonVertex> polygon = VoronoiPolygon(cell.Coord);
			cell.FirstCorner = uint32_t(m_Corners.size());
			cell.CornerCount = uint32_t(polygon.size());
			for (size_t k = 0; k < polygon.size(); k++)
			{
				const PolygonVertex& previous = polygon[(k + polygon.size() - 1) % polygon.size()];
				const PolygonVertex& vertex = polygon[k];
				Vec2 position(float(vertex.X), float(vertex.Y));
				if (!previous.Frame && !vertex.Frame)
					position = VoronoiCorner(cell.Coord, previous.Edge, vertex.Edge).value_or(position);
				m_Corners.push_back({ position, surface(position, CornerJitter(position)) });
				across.push_back(vertex.Edge);
				framed.push_back(vertex.Frame ? 1 : 0);
			}
		}
		if (m_Sky)
		{
			BuildSkyMeshes(renderer, across, framed);
			return;
		}

		struct WallEdge
		{
			uint32_t A = 0;
			uint32_t B = 0;
			Vec2 Outward;
		};
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<LevelMeshDesc> meshes;
		IslandMesh mesh{ vertices, indices, m_Frozen ? &map : nullptr };
		const uint32_t material = renderer.GetPlainMaterial();
		const uint32_t flags = m_Frozen ? MeshVertexTint | MeshIce : MeshVertexTint | MeshLavaGlow | MeshCrystal;
		std::vector<WallEdge> walls;
		std::map<std::pair<int64_t, int64_t>, Vec2> flare;
		for (Chunk& chunk : m_Chunks)
		{
			const uint32_t firstIndex = uint32_t(indices.size());
			walls.clear();
			flare.clear();
			glm::vec3 pivot(0.0f);
			for (const uint32_t index : chunk.Cells)
			{
				const Cell& cell = m_Cells[index];
				const glm::vec3 peak(cell.Seed.x, cell.Peak, cell.Seed.y);
				pivot += peak;
				for (uint32_t k = 0; k < cell.CornerCount; k++)
				{
					const uint32_t ia = cell.FirstCorner + k;
					const uint32_t ib = cell.FirstCorner + (k + 1) % cell.CornerCount;
					const Corner& a = m_Corners[ia];
					const Corner& b = m_Corners[ib];
					chunk.Min = { std::min(chunk.Min.x, a.Position.x), std::min(chunk.Min.y, a.Position.y) };
					chunk.Max = { std::max(chunk.Max.x, a.Position.x), std::max(chunk.Max.y, a.Position.y) };
					const glm::vec3 pa(a.Position.x, a.Height, a.Position.y);
					const glm::vec3 pb(b.Position.x, b.Height, b.Position.y);
					const glm::vec3 up(0.0f, 1.0f, 0.0f);
					glm::vec3 normal = glm::cross(pa - peak, pb - peak);
					const float area = glm::length(normal);
					normal = area > 1e-7f ? normal * (normal.y < 0.0f ? -1.0f : 1.0f) / area : up;
					const glm::vec3 centroid = (peak + pa + pb) / 3.0f;
					const float facet = CollapseNoise(cell.Coord.X * 16 + int32_t(k), cell.Coord.Y, 0x51DEu);
					mesh.Triangle(peak, pa, pb, up, m_Frozen ? IceColor(centroid, SnowAmount(map, Vec2(centroid.x, centroid.z)), facet) : TopColor(centroid, normal, facet));
					const Cell* neighbour = framed[ia] ? nullptr : FindCell(across[ia]);
					if (neighbour && neighbour->Chunk == cell.Chunk)
						continue;
					if (!neighbour)
						chunk.Coast = true;
					else if (std::find(chunk.Neighbours.begin(), chunk.Neighbours.end(), neighbour->Chunk) == chunk.Neighbours.end())
						chunk.Neighbours.push_back(neighbour->Chunk);
					Vec2 outward = Normalize(Vec2(b.Position.y - a.Position.y, a.Position.x - b.Position.x));
					if (Dot(outward, (a.Position + b.Position) * 0.5f - cell.Seed) < 0.0f)
						outward = -outward;
					walls.push_back({ ia, ib, outward });
					flare[CornerKey(a.Position)] += outward;
					flare[CornerKey(b.Position)] += outward;
				}
			}
			chunk.Pivot = pivot / float(chunk.Cells.size());
			const uint32_t wallIndex = uint32_t(indices.size());
			const auto ring = [&](uint32_t corner, float height, float spread)
			{
				const Vec2 position = m_Corners[corner].Position;
				const Vec2 direction = Normalize(flare[CornerKey(position)]);
				const float amount = spread * (0.6f + 0.8f * ValueNoise(position * 1.7f, 0x5EA5u));
				return glm::vec3(position.x + direction.x * amount, height, position.y + direction.y * amount);
			};
			for (const WallEdge& wall : walls)
			{
				const Corner& a = m_Corners[wall.A];
				const Corner& b = m_Corners[wall.B];
				const glm::vec3 facing(wall.Outward.x, 0.0f, wall.Outward.y);
				const glm::vec3 topA(a.Position.x, a.Height, a.Position.y);
				const glm::vec3 topB(b.Position.x, b.Height, b.Position.y);
				const glm::vec3 shoreA = ring(wall.A, LavaSurface - ShoreDrop, m_Frozen ? 0.02f : ShoreFlare);
				const glm::vec3 shoreB = ring(wall.B, LavaSurface - ShoreDrop, m_Frozen ? 0.02f : ShoreFlare);
				const float shade = ValueNoise((a.Position + b.Position) * 1.3f, 0xD1CEu);
				const glm::vec3 edge = m_Frozen ? glm::mix(ToColor(0x7DB5D3), ToColor(0xB0D6E8), shade) : glm::mix(ToColor(0x1F2430), ToColor(0x343C4C), shade);
				mesh.Quad(topA, topB, shoreB, shoreA, facing, edge);
				const float bottom = m_Frozen ? IceBottom : IslandBottom;
				const float flare = m_Frozen ? 0.06f : BaseFlare;
				mesh.Quad(shoreA, shoreB, ring(wall.B, bottom, flare), ring(wall.A, bottom, flare), facing, m_Frozen ? ToColor(0x2A5B7C) : edge * 0.7f);
			}
			meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = wallIndex - firstIndex, .Material = material, .Layer = material, .Flags = flags });
			meshes.push_back({ .FirstIndex = wallIndex, .IndexCount = uint32_t(indices.size()) - wallIndex, .Material = material, .Layer = material, .Flags = flags });
		}
		const uint32_t base = renderer.AddLevelGeometry(vertices, indices, meshes);
		for (size_t i = 0; i < m_Chunks.size(); i++)
			m_Chunks[i].Mesh = base + 2 * uint32_t(i);
	}

	void IslandArt::BuildSkyMeshes(WorldRenderer& renderer, const std::vector<CellCoord>& across, const std::vector<uint8_t>& framed)
	{
		struct WallEdge
		{
			uint32_t A = 0;
			uint32_t B = 0;
			Vec2 Outward;
		};
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<LevelMeshDesc> meshes;
		IslandMesh mesh{ vertices, indices, nullptr, true };
		const uint32_t material = renderer.GetPlainMaterial();
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		std::vector<WallEdge> walls;
		for (Chunk& chunk : m_Chunks)
		{
			const uint32_t firstIndex = uint32_t(indices.size());
			walls.clear();
			glm::vec3 pivot(0.0f);
			for (const uint32_t index : chunk.Cells)
			{
				const Cell& cell = m_Cells[index];
				pivot += glm::vec3(cell.Seed.x, cell.Peak, cell.Seed.y);
				const glm::vec3 tone = SlabColor(cell.Coord, cell.Seed);
				const Vec2 tilt = Vec2(CollapseNoise(cell.Coord.X, cell.Coord.Y, 0x71A7u) - 0.5f, CollapseNoise(cell.Coord.X, cell.Coord.Y, 0x7B2Fu) - 0.5f) * 0.03f;
				const float margin = SlabInset;
				const auto top = [&](Vec2 p) { return glm::vec3(p.x, cell.Peak + Dot(tilt, p - cell.Seed), p.y); };
				const auto inset = [&](Vec2 corner)
				{
					const float reach = Length(corner - cell.Seed);
					return reach > margin * 2.0f ? cell.Seed + (corner - cell.Seed) * ((reach - margin) / reach) : Lerp(cell.Seed, corner, 0.5f);
				};
				const glm::vec3 middle = top(cell.Seed);
				for (uint32_t k = 0; k < cell.CornerCount; k++)
				{
					const uint32_t ia = cell.FirstCorner + k;
					const uint32_t ib = cell.FirstCorner + (k + 1) % cell.CornerCount;
					const Corner& a = m_Corners[ia];
					const Corner& b = m_Corners[ib];
					chunk.Min = { std::min(chunk.Min.x, a.Position.x), std::min(chunk.Min.y, a.Position.y) };
					chunk.Max = { std::max(chunk.Max.x, a.Position.x), std::max(chunk.Max.y, a.Position.y) };
					const glm::vec3 innerA = top(inset(a.Position));
					const glm::vec3 innerB = top(inset(b.Position));
					const glm::vec3 outerA(a.Position.x, a.Height, a.Position.y);
					const glm::vec3 outerB(b.Position.x, b.Height, b.Position.y);
					const float facet = CollapseNoise(cell.Coord.X * 16 + int32_t(k), cell.Coord.Y, 0x51DEu);
					mesh.Triangle(middle, innerA, innerB, up, tone * (0.97f + 0.06f * facet));
					Vec2 outward = Normalize(Vec2(b.Position.y - a.Position.y, a.Position.x - b.Position.x));
					if (Dot(outward, (a.Position + b.Position) * 0.5f - cell.Seed) < 0.0f)
						outward = -outward;
					mesh.Quad(innerA, innerB, outerB, outerA, glm::vec3(outward.x, 1.0f, outward.y), tone * 0.8f);
					const Cell* neighbour = framed[ia] ? nullptr : FindCell(across[ia]);
					if (neighbour && m_Chunks[neighbour->Chunk].Island != chunk.Island)
						neighbour = nullptr;
					if (neighbour && neighbour->Chunk == cell.Chunk)
						continue;
					if (!neighbour)
						chunk.Coast = true;
					else if (std::find(chunk.Neighbours.begin(), chunk.Neighbours.end(), neighbour->Chunk) == chunk.Neighbours.end())
						chunk.Neighbours.push_back(neighbour->Chunk);
					walls.push_back({ ia, ib, outward });
				}
			}
			chunk.Pivot = pivot / float(chunk.Cells.size());
			const uint32_t wallIndex = uint32_t(indices.size());
			float spread = 0.0f;
			for (const WallEdge& wall : walls)
				spread = std::max(spread, Length(m_Corners[wall.A].Position - Vec2(chunk.Pivot.x, chunk.Pivot.z)));
			const int32_t salt = int32_t(chunk.Tile);
			const float depth = ShelfDepth + (chunk.Island >= 0 ? 2.4f : 0.9f) + 0.8f * spread + 1.4f * CollapseNoise(salt, 3, 0xA9E7u);
			const glm::vec3 apex(chunk.Pivot.x + 0.6f * (CollapseNoise(salt, 1, 0xA9E7u) - 0.5f), -depth, chunk.Pivot.z + 0.6f * (CollapseNoise(salt, 2, 0xA9E7u) - 0.5f));
			const auto shelf = [&](const Corner& corner)
			{
				return glm::vec3(corner.Position.x, -ShelfDepth - 0.18f * (CornerJitter(corner.Position) + 0.5f), corner.Position.y);
			};
			const auto waist = [&](const Corner& corner)
			{
				const float jitter = CornerJitter(corner.Position + Vec2(3.7f, 1.3f));
				const float t = 0.38f + 0.2f * jitter;
				glm::vec3 point = glm::mix(shelf(corner), apex, t);
				point.x += 0.35f * jitter * (corner.Position.x - apex.x) / std::max(spread, 0.5f);
				point.z += 0.35f * jitter * (corner.Position.y - apex.z) / std::max(spread, 0.5f);
				return point;
			};
			for (const WallEdge& wall : walls)
			{
				const Corner& a = m_Corners[wall.A];
				const Corner& b = m_Corners[wall.B];
				const glm::vec3 facing(wall.Outward.x, 0.0f, wall.Outward.y);
				const glm::vec3 topA(a.Position.x, a.Height, a.Position.y);
				const glm::vec3 topB(b.Position.x, b.Height, b.Position.y);
				const float shade = ValueNoise((a.Position + b.Position) * 1.3f, 0xD1CEu);
				mesh.Quad(topA, topB, shelf(b), shelf(a), facing, glm::mix(ToColor(0xA88D68), ToColor(0xC4AA83), shade));
				const glm::vec3 down(wall.Outward.x, -0.6f, wall.Outward.y);
				const glm::vec3 midA = waist(a);
				const glm::vec3 midB = waist(b);
				const float facet = CollapseNoise(int32_t(wall.A), int32_t(wall.B), 0x6F1Cu);
				mesh.Quad(shelf(a), shelf(b), midB, midA, down, UndersideColor((shelf(a) + shelf(b) + midA + midB) * 0.25f, facet));
				mesh.Triangle(midA, midB, apex, down, UndersideColor((midA + midB + apex) / 3.0f, 1.0f - facet));
			}
			meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = wallIndex - firstIndex, .Material = material, .Layer = material, .Flags = MeshVertexTint });
			meshes.push_back({ .FirstIndex = wallIndex, .IndexCount = uint32_t(indices.size()) - wallIndex, .Material = material, .Layer = material, .Flags = MeshVertexTint });
		}
		const uint32_t base = renderer.AddLevelGeometry(vertices, indices, meshes);
		for (size_t i = 0; i < m_Chunks.size(); i++)
			m_Chunks[i].Mesh = base + 2 * uint32_t(i);
	}

	float IslandArt::CollapseTime(const CollapsePlan& plan, const Chunk& chunk) const
	{
		return chunk.Island >= 0 ? IslandCollapseTime(plan, m_Islands[size_t(chunk.Island)]) : ChunkCollapseTime(plan, chunk.Coord);
	}

	Vec2 IslandArt::Drift(const Chunk& chunk, float clock) const
	{
		return chunk.Island >= 0 ? IslandOffset(m_Islands[size_t(chunk.Island)], clock) : Vec2();
	}

	IslandArt::Motion IslandArt::SkyMotion(const LevelView& view, size_t index) const
	{
		const Chunk& chunk = m_Chunks[index];
		Motion motion;
		Vec2 outward;
		if (view.Brawl)
		{
			const float since = view.Clock - CollapseTime(view.Brawl->Plan, chunk);
			if (since >= RuinFallSeconds)
			{
				motion.Hidden = true;
				return motion;
			}
			motion.Warn = std::clamp((since + CollapseWarning) / CollapseWarning, 0.0f, 1.0f);
			motion.Fall = std::max(since, 0.0f);
			outward = Normalize(Vec2(chunk.Pivot.x, chunk.Pivot.z) - view.Brawl->Plan.Center);
		}
		const float t = motion.Fall;
		float drop = 0.5f * RuinGravity * t * t;
		if (view.Risen && chunk.Tile < view.Risen->size() && (*view.Risen)[chunk.Tile] != 0 && view.Restore < 1.0f)
		{
			const float rest = 1.0f - view.Restore;
			drop += rest * rest * rest * RuinRise;
		}
		const Vec2 drift = Drift(chunk, view.Clock);
		const float shake = t <= 0.0f && motion.Warn > 0.0f ? motion.Warn * motion.Warn * 0.03f : 0.0f;
		const float phase = float(chunk.Tile % 997u) * 0.37f;
		const glm::vec3 offset(drift.x + outward.x * t * 0.6f + shake * std::sin(view.Time * 47.0f + phase), -drop, drift.y + outward.y * t * 0.6f + shake * std::cos(view.Time * 41.0f - phase));
		motion.Transform = glm::translate(glm::mat4(1.0f), chunk.Pivot + offset);
		if (t > 0.0f && LengthSquared(outward) > 0.0f)
		{
			const float twist = CollapseNoise(int32_t(chunk.Tile), 5, 0xA9E7u) - 0.5f;
			const glm::vec3 axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(outward.x, 0.0f, outward.y)) + glm::vec3(outward.x, 0.0f, outward.y) * twist);
			motion.Transform = glm::rotate(motion.Transform, t * (0.25f + RuinTumble * t), axis);
		}
		motion.Transform = glm::translate(motion.Transform, -chunk.Pivot);
		return motion;
	}

	void IslandArt::DrawSky(WorldRenderer& renderer, const LevelView& view) const
	{
		m_Solid.assign(m_Chunks.size(), 1);
		m_Shown.assign(m_Chunks.size(), 0);
		m_Carry.resize(m_Chunks.size());
		m_Motions.resize(m_Chunks.size());
		for (size_t i = 0; i < m_Chunks.size(); i++)
		{
			m_Motions[i] = SkyMotion(view, i);
			const Chunk& chunk = m_Chunks[i];
			const bool rising = view.Risen && chunk.Tile < view.Risen->size() && (*view.Risen)[chunk.Tile] != 0 && view.Restore < 1.0f;
			m_Solid[i] = !rising && !m_Motions[i].Hidden && m_Motions[i].Fall <= 0.0f ? 1 : 0;
		}
		for (size_t i = 0; i < m_Chunks.size(); i++)
		{
			const Chunk& chunk = m_Chunks[i];
			const Motion& motion = m_Motions[i];
			if (motion.Hidden)
				continue;
			const glm::vec3 moved = glm::vec3(motion.Transform * glm::vec4(chunk.Pivot, 1.0f)) - chunk.Pivot;
			if (chunk.Max.x + moved.x < view.Min.x || chunk.Min.x + moved.x > view.Max.x || chunk.Max.y + moved.z < view.Min.y || chunk.Min.y + moved.z > view.Max.y)
				continue;
			m_Carry[i] = motion.Transform;
			m_Shown[i] = 1;
			renderer.DrawModel(chunk.Mesh, motion.Transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 0.0f);
			if (!m_Solid[i] || chunk.Coast || std::any_of(chunk.Neighbours.begin(), chunk.Neighbours.end(), [this](uint32_t other) { return m_Solid[other] == 0; }))
				renderer.DrawModel(chunk.Mesh + 1, motion.Transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 0.0f);
			if (motion.Warn <= 0.0f || motion.Fall > 0.0f)
				continue;
			for (const uint32_t index : chunk.Cells)
			{
				const Cell& cell = m_Cells[index];
				GroundDecal cracks;
				cracks.Center = glm::vec3(motion.Transform * glm::vec4(cell.Seed.x, cell.Peak, cell.Seed.y, 1.0f));
				cracks.Extent = glm::vec2(0.6f);
				cracks.Depth = 0.4f;
				cracks.Shape = DecalShape::Fracture;
				cracks.Color = glm::vec4(0.0f);
				cracks.Tint = glm::vec4(0.16f, 0.13f, 0.1f, 1.0f);
				cracks.Roughness = 0.9f;
				cracks.Params = glm::vec4(motion.Warn, 0.0f, 0.0f, 0.0f);
				renderer.DrawDecal(cracks);
			}
		}
	}

	std::optional<float> IslandArt::SkyHeight(const BrawlStatus* brawl, float clock, Vec2 position) const
	{
		int32_t island = -1;
		Vec2 local = position;
		if (brawl && !m_Islands.empty())
			if (island = IslandAt(m_Islands, brawl->Plan, clock, position); island >= 0)
				local = position - IslandOffset(m_Islands[size_t(island)], clock);
		const Cell* cell = FindCell(CellAt(local));
		if (!cell || m_Chunks[cell->Chunk].Island != island)
			return std::nullopt;
		float height = cell->Peak;
		if (brawl)
			if (const float since = clock - CollapseTime(brawl->Plan, m_Chunks[cell->Chunk]); since > 0.0f)
				height -= 0.5f * RuinGravity * since * since;
		return height;
	}

	const glm::mat4* IslandArt::Carrying(Vec2 position) const
	{
		const Cell* cell = FindCell(CellAt(position));
		if (!cell || cell->Chunk >= m_Shown.size() || !m_Shown[cell->Chunk])
			return nullptr;
		return &m_Carry[cell->Chunk];
	}

	const IslandArt::Cell* IslandArt::FindCell(CellCoord coord) const
	{
		if (coord.X < 0 || coord.Y < 0 || coord.X >= m_Width || coord.Y >= m_Height)
			return nullptr;
		const int32_t index = m_Lookup[size_t(coord.Y) * size_t(m_Width) + size_t(coord.X)];
		return index >= 0 ? &m_Cells[size_t(index)] : nullptr;
	}

	float IslandArt::SurfaceHeight(const Cell& cell, Vec2 position) const
	{
		float best = -1e9f;
		float height = cell.Peak;
		const Vec2 p = position - cell.Seed;
		for (uint32_t k = 0; k < cell.CornerCount; k++)
		{
			const Corner& a = m_Corners[cell.FirstCorner + k];
			const Corner& b = m_Corners[cell.FirstCorner + (k + 1) % cell.CornerCount];
			const Vec2 e0 = a.Position - cell.Seed;
			const Vec2 e1 = b.Position - cell.Seed;
			const float area = e0.x * e1.y - e0.y * e1.x;
			if (std::abs(area) < 1e-8f)
				continue;
			const float u = (p.x * e1.y - p.y * e1.x) / area;
			const float v = (e0.x * p.y - e0.y * p.x) / area;
			const float inside = std::min({ u, v, 1.0f - u - v });
			if (inside > best)
			{
				best = inside;
				height = cell.Peak + u * (a.Height - cell.Peak) + v * (b.Height - cell.Peak);
			}
		}
		return height;
	}

	std::optional<float> IslandArt::GroundHeight(const BrawlStatus* brawl, float clock, Vec2 position) const
	{
		if (m_Sky)
			return SkyHeight(brawl, clock, position);
		const Cell* cell = FindCell(CellAt(position));
		if (!cell)
			return std::nullopt;
		const float surface = SurfaceHeight(*cell, position);
		const float since = brawl ? brawl->Clock - ChunkCollapseTime(brawl->Plan, m_Chunks[cell->Chunk].Coord) : 0.0f;
		if (since <= 0.0f)
			return surface;
		const float fall = std::min(since / CollapseFallSeconds, 1.0f);
		return std::max(surface - fall * fall * SinkDepth, LavaWading);
	}

	void IslandArt::Draw(WorldRenderer& renderer, const LevelView& view) const
	{
		if (m_Sky)
		{
			DrawSky(renderer, view);
			return;
		}
		m_Solid.assign(m_Chunks.size(), 1);
		if (view.Brawl)
			for (size_t i = 0; i < m_Chunks.size(); i++)
			{
				const Chunk& chunk = m_Chunks[i];
				const bool rising = view.Risen && chunk.Tile < view.Risen->size() && (*view.Risen)[chunk.Tile] != 0 && view.Restore < 1.0f;
				m_Solid[i] = !rising && view.Brawl->Clock < ChunkCollapseTime(view.Brawl->Plan, chunk.Coord) ? 1 : 0;
			}
		for (size_t i = 0; i < m_Chunks.size(); i++)
		{
			const Chunk& chunk = m_Chunks[i];
			if (chunk.Max.x < view.Min.x || chunk.Min.x > view.Max.x || chunk.Max.y < view.Min.y || chunk.Min.y > view.Max.y)
				continue;
			float warn = 0.0f;
			float fall = 0.0f;
			Vec2 outward;
			if (view.Brawl)
			{
				const float since = view.Brawl->Clock - ChunkCollapseTime(view.Brawl->Plan, chunk.Coord);
				if (since >= CollapseFallSeconds)
					continue;
				warn = std::clamp((since + CollapseWarning) / CollapseWarning, 0.0f, 1.0f);
				fall = std::max(since, 0.0f) / CollapseFallSeconds;
				outward = Normalize(Vec2(chunk.Pivot.x, chunk.Pivot.z) - view.Brawl->Plan.Center);
			}
			float drop = fall * fall * SinkDepth;
			if (view.Risen && chunk.Tile < view.Risen->size() && (*view.Risen)[chunk.Tile] != 0 && view.Restore < 1.0f)
			{
				const float rise = 1.0f - (1.0f - view.Restore) * (1.0f - view.Restore);
				drop += (1.0f - rise) * SinkDepth;
			}
			const float shake = fall <= 0.0f && warn > 0.0f ? warn * warn * 0.035f : 0.0f;
			const float phase = float(chunk.Coord.X) * 3.1f + float(chunk.Coord.Y) * 1.7f;
			const glm::vec3 offset(shake * std::sin(view.Time * 47.0f + phase), -drop, shake * std::cos(view.Time * 41.0f - phase));
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), chunk.Pivot + offset);
			if (fall > 0.0f && LengthSquared(outward) > 0.0f)
				transform = glm::rotate(transform, fall * MaxTilt, glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(outward.x, 0.0f, outward.y)));
			transform = glm::translate(transform, -chunk.Pivot);
			const float heat = m_Frozen ? 0.0f : std::min(fall * 0.75f + warn * warn * 0.1f, 1.0f);
			renderer.DrawModel(chunk.Mesh, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), heat, LavaSurface);
			if (!m_Solid[i] || chunk.Coast || std::any_of(chunk.Neighbours.begin(), chunk.Neighbours.end(), [this](uint32_t other) { return m_Solid[other] == 0; }))
				renderer.DrawModel(chunk.Mesh + 1, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), heat, LavaSurface);
			if (warn <= 0.0f || fall > 0.0f)
				continue;
			for (const uint32_t index : chunk.Cells)
			{
				const Cell& cell = m_Cells[index];
				GroundDecal cracks;
				cracks.Center = { cell.Seed.x, cell.Peak, cell.Seed.y };
				cracks.Extent = glm::vec2(0.6f);
				cracks.Depth = 0.4f;
				cracks.Shape = DecalShape::Fracture;
				cracks.Color = m_Frozen ? glm::vec4(0.25f, 0.55f, 0.9f, 1.0f) : glm::vec4(1.6f, 0.5f, 0.13f, 1.0f);
				cracks.Tint = m_Frozen ? glm::vec4(0.06f, 0.14f, 0.22f, 1.0f) : glm::vec4(0.02f, 0.016f, 0.014f, 1.0f);
				cracks.Roughness = 0.9f;
				cracks.Params = glm::vec4(warn, 0.0f, 0.0f, 0.0f);
				renderer.DrawDecal(cracks);
			}
		}
	}

	float LevelRenderer::GroundHeight(const Level& level, const BrawlStatus* brawl, Vec2 position, float clock) const
	{
		if (m_Mountain)
			return m_Mountain->RimDistance(position) >= 0.0f ? m_Mountain->Height(position) : SampleGround(m_Scene, position).value_or(-60.0f);
		if (m_Island.IsBuilt())
			if (const std::optional<float> height = m_Island.GroundHeight(brawl, clock, position))
				return *height;
		if (m_Island.IsSky())
			return SlabTop;
		if (!m_LavaRuns.empty() && IsLavaTerrain(level, position))
			return LavaWading;
		if (brawl)
			return LavaWading;
		return SampleGround(m_Scene, position).value_or(level.Map.GetHeight(position));
	}

	bool LevelRenderer::IsOverSky(const BrawlStatus* brawl, Vec2 position, float clock) const
	{
		if (m_Mountain)
			return m_Mountain->RimDistance(position) < 0.0f;
		return m_Island.IsSky() && !m_Island.GroundHeight(brawl, clock, position);
	}

	glm::vec3 ToColor(uint32_t rgb)
	{
		const glm::vec3 srgb = glm::vec3(float((rgb >> 16) & 0xFF), float((rgb >> 8) & 0xFF), float(rgb & 0xFF)) / 255.0f;
		return glm::pow(srgb, glm::vec3(2.2f));
	}

	void LevelRenderer::Draw(WorldRenderer& renderer, const Level& level, const LevelView& view)
	{
		Prepare(renderer, level);
		DrawTerrain(renderer, level, view);
		m_Island.Draw(renderer, view);
		DrawLoose(renderer, view);
		DrawLava(renderer, view);
		DrawWalls(renderer, level, view);
		DrawProps(renderer, level, view);
		DrawScene(renderer, view);
		DrawPortals(renderer, level, view);
	}

	void LevelRenderer::Prepare(WorldRenderer& renderer, const Level& level)
	{
		const SceneKey key{ &level, level.Kind, level.Seed, level.Props.size(), level.Walls.size() };
		if (key == m_Key)
			return;
		m_Key = key;
		renderer.ClearLevelGeometry();
		if (level.Kind != MapKind::Abbey || !BuildAbbeyScene(level, renderer, m_Scene))
		{
			renderer.SetSkyMask(0, 0, {}, {}, {});
			m_Scene = BuildLevelScene(level, renderer);
		}
		m_Mountain = level.Relief.IsEmpty() ? nullptr : &level.Relief;
		if (m_Mountain)
			m_Island.Clear();
		else
			m_Island.Build(renderer, level);
		renderer.SetFrozenLiquid(level.Kind == MapKind::FrozenLake);
		const TileMap& map = level.Map;
		m_LavaRuns.clear();
		m_LavaHaze = level.Kind != MapKind::FrozenLake;
		for (int32_t y = 0; y < map.GetHeight() && !level.Skyborne; y++)
		{
			int32_t start = -1;
			for (int32_t x = 0; x <= map.GetWidth(); x++)
			{
				const TerrainId terrain = x < map.GetWidth() ? map.GetTerrain(x, y) : TerrainId::Count;
				const bool molten = terrain != TerrainId::Count && (IsHazardLiquid(terrain) || IsBrawlFloor(terrain));
				if (molten && start < 0)
					start = x;
				else if (!molten && start >= 0)
				{
					m_LavaRuns.push_back({ float(start), float(y), float(x), float(y + 1) });
					start = -1;
				}
			}
		}
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

	void LevelRenderer::DrawLava(WorldRenderer& renderer, const LevelView& view) const
	{
		glm::vec4 bounds(1e9f, 1e9f, -1e9f, -1e9f);
		for (const glm::vec4& run : m_LavaRuns)
			if (run.z >= view.Min.x && run.x <= view.Max.x && run.w >= view.Min.y && run.y <= view.Max.y)
			{
				renderer.DrawLava(run, LavaSurface, view.Time);
				bounds = glm::vec4(glm::min(glm::vec2(bounds), glm::vec2(run)), glm::max(glm::vec2(bounds.z, bounds.w), glm::vec2(run.z, run.w)));
			}
		if (!m_LavaHaze || bounds.x > bounds.z)
			return;
		bounds = glm::vec4(glm::max(glm::vec2(bounds), glm::vec2(view.Min.x, view.Min.y) - 1.0f), glm::min(glm::vec2(bounds.z, bounds.w), glm::vec2(view.Max.x, view.Max.y) + 1.0f));
		Distorter haze;
		haze.Center = glm::vec3((bounds.x + bounds.z) * 0.5f, LavaSurface + LavaHazeLift, (bounds.y + bounds.w) * 0.5f);
		haze.Shape = DistortionShape::Haze;
		haze.Align = DistortionAlign::Ground;
		haze.Extent = glm::vec2(bounds.z - bounds.x, bounds.w - bounds.y) * 0.5f;
		haze.Strength = LavaHazeStrength;
		haze.Soft = 0.06f;
		haze.Params = glm::vec4(1.4f, 0.45f, 0.8f, 0.0f);
		haze.Seed = 0x5A17u;
		renderer.DrawDistortion(haze);
	}

	void LevelRenderer::DrawLoose(WorldRenderer& renderer, const LevelView& view) const
	{
		for (const LooseItem& loose : m_Scene.Loose)
		{
			const SceneItem& item = loose.Item;
			const glm::mat4* carry = m_Island.Carrying(loose.Anchor);
			if (!carry)
				continue;
			const glm::vec3 anchor(item.Center.x, item.Base, item.Center.y);
			const glm::vec3 moved = glm::vec3(*carry * glm::vec4(anchor, 1.0f)) - anchor;
			const Vec2 center = item.Center + Vec2(moved.x, moved.z);
			if (center.x + item.Reach < view.Min.x || center.x - item.Reach > view.Max.x || center.y + item.Reach < view.Min.y || center.y - item.Reach > view.Max.y)
				continue;
			ModelInstance instance = item.Instance;
			const glm::mat4 rows = glm::transpose(*carry * glm::transpose(glm::mat4(instance.Rows[0], instance.Rows[1], instance.Rows[2], glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))));
			instance.Rows[0] = rows[0];
			instance.Rows[1] = rows[1];
			instance.Rows[2] = rows[2];
			if (item.Cut && moved.y > -0.01f)
			{
				const Vec2 closest = { std::clamp(view.Focus.x, item.Center.x - item.Extent.x, item.Center.x + item.Extent.x), std::clamp(view.Focus.y, item.Center.y - item.Extent.y, item.Center.y + item.Extent.y) };
				if (const float cut = CutFactor(view, closest, item.Top); cut > 0.0f)
				{
					instance.Shading.z = item.Base + CutHeight;
					instance.Shading.w = 0.85f * cut;
				}
			}
			renderer.SubmitModel(item.Mesh, instance);
		}
	}

	void LevelRenderer::DrawScene(WorldRenderer& renderer, const LevelView& view) const
	{
		for (const SceneChunk& chunk : m_Scene.Chunks)
		{
			if ((chunk.Items.empty() && chunk.Boxes.empty()) || chunk.Max.x < view.Min.x || chunk.Min.x > view.Max.x || chunk.Max.y < view.Min.y || chunk.Min.y > view.Max.y)
				continue;
			for (const SceneItem& item : chunk.Items)
			{
				if (item.Center.x + item.Reach < view.Min.x || item.Center.x - item.Reach > view.Max.x || item.Center.y + item.Reach < view.Min.y || item.Center.y - item.Reach > view.Max.y)
					continue;
				ModelInstance instance = item.Instance;
				if (item.Bob > 0.0f)
					instance.Rows[1].w += item.Bob * std::sin(view.Time * 0.55f + item.BobPhase);
				if (item.Center.x + item.Reach < view.CameraMin.x || item.Center.x - item.Reach > view.CameraMax.x || item.Center.y + item.Reach < view.CameraMin.y || item.Center.y - item.Reach > view.CameraMax.y)
				{
					renderer.SubmitModel(item.Mesh, instance, true);
					continue;
				}
				float cut = 0.0f;
				if (item.Cut)
				{
					const Vec2 closest = { std::clamp(view.Focus.x, item.Center.x - item.Extent.x, item.Center.x + item.Extent.x), std::clamp(view.Focus.y, item.Center.y - item.Extent.y, item.Center.y + item.Extent.y) };
					cut = CutFactor(view, closest, item.Top);
				}
				if (cut <= 0.0f && (item.Instance.Info.w & MeshWater) != 0)
				{
					instance.Shading.z = view.Time;
					renderer.SubmitModel(item.Mesh, instance);
					continue;
				}
				if (cut <= 0.0f)
				{
					renderer.SubmitModel(item.Mesh, instance);
					continue;
				}
				instance.Shading.z = item.Base + CutHeight;
				instance.Shading.w = 0.85f * cut;
				renderer.SubmitModel(item.Mesh, instance);
			}
			for (const SceneBox& box : chunk.Boxes)
			{
				if (box.Center.x + box.Reach < view.Min.x || box.Center.x - box.Reach > view.Max.x || box.Center.z + box.Reach < view.Min.y || box.Center.z - box.Reach > view.Max.y)
					continue;
				const bool outside = box.Center.x + box.Reach < view.CameraMin.x || box.Center.x - box.Reach > view.CameraMax.x || box.Center.z + box.Reach < view.CameraMin.y || box.Center.z - box.Reach > view.CameraMax.y;
				if (box.ShadowOnly || (outside && box.Emission < 0.9f))
				{
					renderer.DrawShadowCaster(box.Mesh, box.Center, box.Half, box.Yaw);
					continue;
				}
				if (outside)
					continue;
				const float bottom = box.Center.y - box.Half.y;
				const float fullTop = box.Center.y + box.Half.y;
				float top = fullTop;
				if (box.Cut)
				{
					const Vec2 center = { box.Center.x, box.Center.z };
					const Vec2 toFocus = view.Focus - center;
					const float distance = Length(toFocus);
					const Vec2 closest = distance > 1e-4f ? center + toFocus * (std::min(distance, box.Reach) / distance) : center;
					const float cut = CutFactor(view, closest, top);
					if (cut > 0.0f)
						top = std::min(top, box.Base + glm::mix(top - box.Base, CutHeight + 0.05f, cut));
				}
				if (top < fullTop && box.Emission < 0.9f)
					renderer.DrawShadowCaster(box.Mesh, box.Center, box.Half, box.Yaw);
				if (top - bottom < 0.005f)
					continue;
				renderer.Draw(box.Mesh, { box.Center.x, (top + bottom) * 0.5f, box.Center.z }, { box.Half.x, (top - bottom) * 0.5f, box.Half.z }, box.Color, box.Yaw, box.Emission, box.Base);
			}
		}
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
				const size_t index = size_t(y) * size_t(map.GetWidth()) + size_t(x);
				const uint8_t art = index < m_Scene.Cells.size() ? m_Scene.Cells[index] : 0;
				if ((art & CellTop) && (art & CellSides))
					continue;
				if (def.Style == TerrainStyle::Molten)
					continue;
				if (IsBrawlFloor(cell.Terrain))
					continue;
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
					if (top < height + RockHeight)
						renderer.DrawShadowCaster(MeshKind::Box, { cx, height + (RockHeight - 0.1f) * 0.5f, cz }, { 0.5f, (RockHeight + 0.1f) * 0.5f, 0.5f });
					break;
				}
				case TerrainStyle::Liquid:
				{
					const float ripple = 0.5f + 0.5f * std::sin(view.Time * 1.7f + shade * 6.28f);
					renderer.DrawBox({ cx, height - 0.17f, cz }, { 0.5f, 0.03f, 0.5f }, glm::mix(base, accent, ripple * 0.6f), 0.0f, 0.12f, height - 0.2f, { .Roughness = 0.08f });
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
					if (art & CellSides)
						bottom = height - 0.1f;
					const glm::vec3 color = glm::mix(base, accent, shade * 0.5f);
					if (art & CellTop)
					{
						Column(renderer, cx, cz, bottom, height - FloorBottom, { 0.5f, 0.5f }, color * 0.8f, height);
						break;
					}
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
		for (size_t index = 0; index < level.Walls.size(); index++)
		{
			const WallSegment& wall = level.Walls[index];
			if (index < m_Scene.WallDrawn.size() && m_Scene.WallDrawn[index])
				continue;
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
				if (cut > 0.0f)
				{
					const float fullTop = wall.Base + def.Height + 0.06f;
					renderer.DrawShadowCaster(MeshKind::Box, { center.x, (fullTop + bottom) * 0.5f, center.y }, { halfLength, (fullTop - bottom) * 0.5f, def.Thickness * 0.5f }, yaw);
				}
				renderer.DrawBox({ center.x, top + 0.03f, center.y }, { halfLength + 0.005f, 0.03f, def.Thickness * 0.5f + 0.03f }, ToColor(def.Top), yaw, 0.0f, wall.Base);
			}
		}
	}

	void LevelRenderer::DrawProps(WorldRenderer& renderer, const Level& level, const LevelView& view) const
	{
		for (size_t i = 0; i < level.Props.size() && i < m_PropReach.size(); i++)
		{
			if (i < m_Scene.PropDrawn.size() && m_Scene.PropDrawn[i])
				continue;
			const PlacedProp& prop = level.Props[i];
			const float reach = m_PropReach[i];
			if (prop.Position.x + reach < view.Min.x || prop.Position.x - reach > view.Max.x || prop.Position.y + reach < view.Min.y || prop.Position.y - reach > view.Max.y)
				continue;
			glm::vec3 lift(0.0f);
			if (m_Island.IsSky())
			{
				const glm::mat4* carry = m_Island.Carrying(prop.Position);
				if (!carry)
					continue;
				const glm::vec3 anchor(prop.Position.x, prop.Base, prop.Position.y);
				lift = glm::vec3(*carry * glm::vec4(anchor, 1.0f)) - anchor;
			}
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
				const float fullTop = part.Base + part.Height;
				const float top = std::min(fullTop, cutTop);
				const Vec2 center = prop.Position + Rotate(part.Offset, axis) + Vec2(lift.x, lift.z);
				const float base = prop.Base + lift.y;
				if (top < fullTop && part.Emission < 0.9f)
					renderer.DrawShadowCaster(part.Mesh, { center.x, base + (bottom + fullTop) * 0.5f, center.y }, { part.Half.x, part.Height * 0.5f, part.Half.y }, prop.Yaw + part.Yaw);
				if (top - bottom < 0.005f)
					continue;
				renderer.Draw(part.Mesh, { center.x, base + (bottom + top) * 0.5f, center.y }, { part.Half.x, (top - bottom) * 0.5f, part.Half.y }, ToColor(part.Color), prop.Yaw + part.Yaw, part.Emission, base);
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
			if ((!hovered && !reachable && distance > 8.0f) || (portal.Hidden && !reachable))
				continue;
			const float ground = level.Map.GetHeight(portal.Position);
			const float pulse = 0.65f + 0.35f * std::sin(view.Time * 4.0f + float(i));
			const glm::vec3 center = { portal.Position.x, ground, portal.Position.y };
			if (hovered)
			{
				DrawGroundRing(renderer, center, 0.75f, PortalGold, 1.5f * pulse, 0.05f);
				DrawGroundRing(renderer, center, 0.45f, PortalGold, 1.2f * pulse, 0.035f, 8.0f, 0.2f);
			}
			else
				DrawGroundRing(renderer, center, 0.6f, PortalGold * (reachable ? 0.9f : 0.5f), reachable ? 1.2f * pulse : 0.6f, 0.035f);
		}
	}
}
