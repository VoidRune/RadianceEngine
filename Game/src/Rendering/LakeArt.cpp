#include "LakeArt.h"
#include "Rendering/Animation.h"
#include "Rendering/LevelRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace Game
{
	namespace
	{
		constexpr float BedMargin = 30.0f;
		constexpr float BedSpacing = 1.0f;
		constexpr float CrackStep = 0.55f;
		constexpr float HalfPi = Pi * 0.5f;

		struct Rng
		{
			uint32_t State;

			float Next()
			{
				State = State * 1664525u + 1013904223u;
				uint32_t x = State;
				x ^= x >> 15;
				x *= 0x2C1B3C6Du;
				x ^= x >> 12;
				return float(x & 0xFFFFFu) / float(0xFFFFFu);
			}

			float Range(float low, float high) { return low + (high - low) * Next(); }
		};

		float Smooth(float a, float b, float x)
		{
			const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float Noise2(Vec2 p, uint32_t salt)
		{
			const float fx = std::floor(p.x);
			const float fy = std::floor(p.y);
			const int32_t x = int32_t(fx);
			const int32_t y = int32_t(fy);
			const float u = Smooth(0.0f, 1.0f, p.x - fx);
			const float v = Smooth(0.0f, 1.0f, p.y - fy);
			return std::lerp(std::lerp(CollapseNoise(x, y, salt), CollapseNoise(x + 1, y, salt), u), std::lerp(CollapseNoise(x, y + 1, salt), CollapseNoise(x + 1, y + 1, salt), u), v);
		}

		float Noise3(const glm::vec3& p, uint32_t salt)
		{
			const glm::vec3 cell = glm::floor(p);
			const int32_t x = int32_t(cell.x);
			const int32_t y = int32_t(cell.y);
			const int32_t z = int32_t(cell.z);
			const float u = Smooth(0.0f, 1.0f, p.x - cell.x);
			const float v = Smooth(0.0f, 1.0f, p.y - cell.y);
			const float w = Smooth(0.0f, 1.0f, p.z - cell.z);
			const auto at = [&](int32_t dx, int32_t dy, int32_t dz) { return CollapseNoise(x + dx, z + dz, salt + uint32_t(y + dy) * 2654435761u); };
			const float below = std::lerp(std::lerp(at(0, 0, 0), at(1, 0, 0), u), std::lerp(at(0, 0, 1), at(1, 0, 1), u), w);
			const float above = std::lerp(std::lerp(at(0, 1, 0), at(1, 1, 0), u), std::lerp(at(0, 1, 1), at(1, 1, 1), u), w);
			return std::lerp(below, above, v);
		}

		float RockRelief(const glm::vec3& p)
		{
			const glm::vec3 scaled = p * glm::vec3(0.58f, 0.44f, 0.58f);
			const glm::ivec3 cell(glm::floor(scaled));
			float nearest = 1e9f;
			float second = 1e9f;
			glm::ivec3 owner(0);
			glm::vec3 feature(0.0f);
			for (int32_t dz = -1; dz <= 1; dz++)
				for (int32_t dy = -1; dy <= 1; dy++)
					for (int32_t dx = -1; dx <= 1; dx++)
					{
						const glm::ivec3 id = cell + glm::ivec3(dx, dy, dz);
						const uint32_t salt = 0x9C11u + uint32_t(id.y) * 2654435761u;
						const glm::vec3 spot = glm::vec3(id) + glm::vec3(CollapseNoise(id.x, id.z, salt), CollapseNoise(id.x, id.z, salt ^ 0x55u), CollapseNoise(id.x, id.z, salt ^ 0xAAu));
						const float distance = glm::dot(scaled - spot, scaled - spot);
						if (distance < nearest)
						{
							second = nearest;
							nearest = distance;
							owner = id;
							feature = spot;
						}
						else if (distance < second)
							second = distance;
					}
			const float border = std::sqrt(second) - std::sqrt(nearest);
			const uint32_t salt = 0x9D23u + uint32_t(owner.y) * 2654435761u;
			const glm::vec3 tilt = glm::vec3(CollapseNoise(owner.x, owner.z, salt), CollapseNoise(owner.x, owner.z, salt ^ 0x33u), CollapseNoise(owner.x, owner.z, salt ^ 0x66u)) - 0.5f;
			const float block = 0.55f * (CollapseNoise(owner.x, owner.z, salt ^ 0x99u) - 0.5f) + 0.5f * glm::dot(tilt, scaled - feature);
			const float layer = p.y * 0.52f + 0.9f * Noise3(p * 0.25f, 0x9E31u);
			const float strata = Smooth(0.7f, 0.92f, layer - std::floor(layer));
			const float lumps = 2.0f * Noise3(p * 1.4f + glm::vec3(2.3f, 0.0f, 5.1f), 0x9F43u) - 1.0f;
			const float buttress = 2.0f * Noise3(p * glm::vec3(0.17f, 0.06f, 0.17f), 0xA057u) - 1.0f;
			return 1.1f * block + 0.26f * strata + 0.08f * lumps + 0.7f * buttress - 0.2f * (1.0f - Smooth(0.0f, 0.09f, border));
		}

		struct Cells
		{
			std::vector<uint32_t> Indices;
			std::vector<LevelMeshDesc> Meshes;
			std::vector<std::pair<Vec2, Vec2>> Bounds;
		};

		Cells Bucket(const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, float size, const LevelMeshDesc& base)
		{
			std::map<std::pair<int32_t, int32_t>, std::vector<uint32_t>> groups;
			for (size_t t = 0; t + 2 < indices.size(); t += 3)
			{
				const glm::vec3 centre = (vertices[indices[t]].Position + vertices[indices[t + 1]].Position + vertices[indices[t + 2]].Position) / 3.0f;
				std::vector<uint32_t>& group = groups[{ int32_t(std::floor(centre.x / size)), int32_t(std::floor(centre.z / size)) }];
				group.insert(group.end(), { indices[t], indices[t + 1], indices[t + 2] });
			}
			Cells cells;
			for (const auto& [key, group] : groups)
			{
				Vec2 low(1e9f, 1e9f);
				Vec2 high(-1e9f, -1e9f);
				for (const uint32_t index : group)
				{
					low = { std::min(low.x, vertices[index].Position.x), std::min(low.y, vertices[index].Position.z) };
					high = { std::max(high.x, vertices[index].Position.x), std::max(high.y, vertices[index].Position.z) };
				}
				LevelMeshDesc desc = base;
				desc.FirstIndex = uint32_t(cells.Indices.size());
				desc.IndexCount = uint32_t(group.size());
				cells.Indices.insert(cells.Indices.end(), group.begin(), group.end());
				cells.Meshes.push_back(desc);
				cells.Bounds.push_back({ low, high });
			}
			return cells;
		}

		uint32_t SwayBits(float weight, float phase, float amplitude)
		{
			const uint32_t w = uint32_t(std::clamp(weight, 0.0f, 1.0f) * 255.0f + 0.5f);
			const uint32_t p = uint32_t(std::fmod(std::max(phase, 0.0f), 1.0f) * 255.0f + 0.5f) & 0xFFu;
			const uint32_t a = uint32_t(std::clamp(amplitude, 0.0f, 2.55f) * 100.0f + 0.5f);
			return w | (p << 8) | (a << 16);
		}

		uint32_t Pack(const glm::vec3& color, float alpha)
		{
			const glm::uvec3 c(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
			return c.r | (c.g << 8) | (c.b << 16) | (uint32_t(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f) << 24);
		}

		uint32_t PackTangent(const glm::vec3& tangent, float sign)
		{
			const auto snorm = [](float value) { return uint32_t(int32_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 511.0f))) & 0x3FFu; };
			return snorm(tangent.x) | (snorm(tangent.y) << 10) | (snorm(tangent.z) << 20) | (sign < 0.0f ? 1u << 30 : 0u);
		}

		ModelVertex Vertex(const glm::vec3& position, const glm::vec3& normal, glm::vec2 uv, uint32_t color)
		{
			return { .Position = position, .U = uv.x, .Normal = normal, .V = uv.y, .Color = color };
		}

		struct MeshBuild
		{
			std::vector<ModelVertex> Vertices;
			std::vector<uint32_t> Indices;

			void Quad(const ModelVertex& a, const ModelVertex& b, const ModelVertex& c, const ModelVertex& d)
			{
				const uint32_t base = uint32_t(Vertices.size());
				Vertices.insert(Vertices.end(), { a, b, c, d });
				Indices.insert(Indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
			}
		};

		float SampleSpline(const Vec2* points, size_t count, std::vector<Vec2>& out, std::vector<float>& along)
		{
			out.clear();
			along.clear();
			constexpr int32_t Steps = 14;
			for (size_t i = 0; i + 1 < count; i++)
			{
				const Vec2 p0 = points[i > 0 ? i - 1 : i];
				const Vec2 p1 = points[i];
				const Vec2 p2 = points[i + 1];
				const Vec2 p3 = points[std::min(i + 2, count - 1)];
				for (int32_t k = 0; k < Steps; k++)
				{
					const float t = float(k) / float(Steps);
					const float t2 = t * t;
					const float t3 = t2 * t;
					out.push_back((p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f);
				}
			}
			out.push_back(points[count - 1]);
			float total = 0.0f;
			for (size_t i = 0; i < out.size(); i++)
			{
				if (i > 0)
					total += Length(out[i] - out[i - 1]);
				along.push_back(total);
			}
			return total;
		}

		Vec2 PointAlong(const std::vector<Vec2>& points, const std::vector<float>& along, float at)
		{
			if (points.size() < 2)
				return {};
			const float total = along.back();
			if (at <= 0.0f)
				return points[0] + Normalize(points[1] - points[0]) * at;
			if (at >= total)
				return points.back() + Normalize(points.back() - points[points.size() - 2]) * (at - total);
			const size_t i = size_t(std::upper_bound(along.begin(), along.end(), at) - along.begin());
			const size_t a = std::min(std::max<size_t>(i, 1), points.size() - 1) - 1;
			const float span = std::max(along[a + 1] - along[a], 1e-4f);
			return points[a] + (points[a + 1] - points[a]) * ((at - along[a]) / span);
		}

		std::vector<uint32_t> MeshesNamed(const WorldRenderer& renderer, std::string_view name)
		{
			std::vector<uint32_t> found;
			const ModelLibrary& models = renderer.GetModels();
			if (const std::optional<uint32_t> first = models.FindMesh(name))
			{
				found.push_back(*first);
				for (uint32_t extra = 1;; extra++)
				{
					const std::optional<uint32_t> more = models.FindMesh(std::string(name) + "." + std::to_string(extra));
					if (!more)
						break;
					found.push_back(*more);
				}
			}
			return found;
		}
	}

	void RegisterLakeMeshes(WorldRenderer& renderer)
	{
		const uint32_t plain = renderer.GetPlainMaterial();
		{
			MeshBuild fish;
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			const uint32_t white = Pack(glm::vec3(1.0f), 1.0f);
			constexpr int32_t Rings = 6;
			const float widths[Rings + 1] = { 0.0f, 0.07f, 0.1f, 0.09f, 0.06f, 0.02f, 0.11f };
			for (int32_t i = 0; i < Rings; i++)
			{
				const float z0 = 0.5f - float(i) / float(Rings);
				const float z1 = 0.5f - float(i + 1) / float(Rings);
				const float v0 = 1.0f - 0.65f * float(i) / float(Rings);
				const float v1 = 1.0f - 0.65f * float(i + 1) / float(Rings);
				fish.Quad(Vertex({ -widths[i], 0.0f, z0 }, up, { 0.0f, v0 }, white), Vertex({ widths[i], 0.0f, z0 }, up, { 0.0f, v0 }, white), Vertex({ widths[i + 1], 0.0f, z1 }, up, { 0.0f, v1 }, white), Vertex({ -widths[i + 1], 0.0f, z1 }, up, { 0.0f, v1 }, white));
			}
			renderer.AddEffectMesh("LakeFish", fish.Vertices, fish.Indices, plain, MeshUnderGlow);
		}
		{
			MeshBuild jelly;
			const uint32_t white = Pack(glm::vec3(1.0f), 1.0f);
			constexpr int32_t Sides = 12;
			constexpr int32_t Bands = 5;
			const auto bell = [&](int32_t side, int32_t band)
			{
				const float t = float(band) / float(Bands);
				const float a = 2.0f * Pi * float(side) / float(Sides);
				const float r = std::sin(t * 1.5f);
				return glm::vec3(std::cos(a) * r * 0.5f, 0.42f * std::cos(t * 1.5f) + 0.05f, std::sin(a) * r * 0.5f);
			};
			for (int32_t side = 0; side < Sides; side++)
				for (int32_t band = 0; band < Bands; band++)
				{
					const float v0 = 0.45f + 0.55f * float(band) / float(Bands);
					const float v1 = 0.45f + 0.55f * float(band + 1) / float(Bands);
					const glm::vec3 up(0.0f, 1.0f, 0.0f);
					jelly.Quad(Vertex(bell(side, band), up, { 0.0f, v0 }, white), Vertex(bell(side + 1, band), up, { 0.0f, v0 }, white), Vertex(bell(side + 1, band + 1), up, { 0.0f, v1 }, white), Vertex(bell(side, band + 1), up, { 0.0f, v1 }, white));
				}
			for (int32_t strand = 0; strand < 7; strand++)
			{
				const float a = 2.0f * Pi * float(strand) / 7.0f + 0.3f;
				const glm::vec3 side(std::sin(a) * 0.03f, 0.0f, -std::cos(a) * 0.03f);
				const float r = 0.2f + 0.2f * float(strand % 3) / 2.0f;
				const glm::vec3 root(std::cos(a) * r, 0.0f, std::sin(a) * r);
				for (int32_t k = 0; k < 6; k++)
				{
					const float t0 = float(k) / 6.0f;
					const float t1 = float(k + 1) / 6.0f;
					const glm::vec3 p0 = root + glm::vec3(0.06f * std::sin(t0 * 9.0f + a), -1.5f * t0, 0.06f * std::cos(t0 * 7.0f + a));
					const glm::vec3 p1 = root + glm::vec3(0.06f * std::sin(t1 * 9.0f + a), -1.5f * t1, 0.06f * std::cos(t1 * 7.0f + a));
					const glm::vec3 up(0.0f, 1.0f, 0.0f);
					jelly.Quad(Vertex(p0 - side, up, { 0.0f, 0.6f * (1.0f - t0) }, white), Vertex(p0 + side, up, { 0.0f, 0.6f * (1.0f - t0) }, white), Vertex(p1 + side, up, { 0.0f, 0.6f * (1.0f - t1) }, white), Vertex(p1 - side, up, { 0.0f, 0.6f * (1.0f - t1) }, white));
				}
			}
			renderer.AddEffectMesh("LakeJelly", jelly.Vertices, jelly.Indices, plain, MeshUnderGlow);
		}
	}

	void LakeArt::Clear()
	{
		m_Width = 0;
		m_Height = 0;
		m_Features.clear();
		m_Items.clear();
		m_Above.clear();
		m_Floes.clear();
		m_Mounds.clear();
		m_Schools.clear();
		m_FishSlots.clear();
		m_Jellies.clear();
		m_Holes.clear();
		m_Times.clear();
		m_Shelf.clear();
		m_Alive.clear();
		m_Thin.clear();
		m_Field.clear();
		m_HavePlan = false;
		m_Bed = UINT32_MAX;
		m_Shafts = UINT32_MAX;
		m_Land = UINT32_MAX;
		m_Bank = UINT32_MAX;
		m_LeviathanMeshes.clear();
		m_Trench.clear();
		m_TrenchAlong.clear();
		m_TrenchLength = 0.0f;
		m_Snow = UINT32_MAX;
		m_Drifters.clear();
		m_Route.clear();
		m_RouteAlong.clear();
		m_RouteDepth.clear();
		m_Growth = UINT32_MAX;
		m_Walls.clear();
		m_WallsUnder.clear();
		m_Flora.clear();
		m_FloraGlow.clear();
		m_ShoreIce.clear();
		m_WallSpots.clear();
		m_LedgeSpots.clear();
		m_Glows.clear();
		m_LeviathanJoints.clear();
		m_LeviathanSpine.clear();
		m_Leviathan = {};
	}

	void LakeArt::Build(WorldRenderer& renderer, const Level& level)
	{
		Clear();
		m_Width = level.Map.GetWidth();
		m_Height = level.Map.GetHeight();
		m_Center = { float(m_Width) * 0.5f, float(m_Height) * 0.5f };
		m_Seed = level.Seed;
		m_Terrains.resize(size_t(m_Width) * size_t(m_Height));
		for (int32_t y = 0; y < m_Height; y++)
			for (int32_t x = 0; x < m_Width; x++)
				m_Terrains[size_t(y) * size_t(m_Width) + size_t(x)] = level.Map.GetTerrain(x, y);
		m_Fish = renderer.FindEffectMesh("LakeFish");
		m_JellyMesh = renderer.FindEffectMesh("LakeJelly");
		m_LeviathanMeshes = MeshesNamed(renderer, "Abyssal/Leviathan");
		BuildLeviathanRig(renderer);
		m_Layout = MakeLakeLayout(level.Seed);
		m_ShoreLevel.Build(level);
		m_GraveSide = 1.0f;
		for (const PlacedProp& prop : level.Props)
			if (prop.Id == PropId::GraveSkull)
				m_GraveSide = prop.Position.x < m_Center.x ? 1.0f : -1.0f;
		BuildTrench();
		BuildRoute();
		BuildBed(renderer, level);
		BuildWalls(renderer, level);
		BuildSnow(renderer, level);
		BuildFeatures(renderer, level);
		BuildRuins(renderer, level);
		BuildCreatures(level);
		BuildShore(renderer, level);
		BuildMounds(renderer, level);
		BuildLand(renderer, level);
		BuildBank(renderer, level);
		BuildHarbor(renderer, level);
		BuildFlora(renderer, level);
		BuildShoreIce(renderer, level);
	}

	void LakeArt::AddPatches(WorldRenderer& renderer, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const LevelMeshDesc& desc, float size, std::vector<Patch>& patches)
	{
		if (indices.empty())
			return;
		const Cells cells = Bucket(vertices, indices, size, desc);
		const uint32_t base = renderer.AddLevelGeometry(vertices, cells.Indices, cells.Meshes);
		for (size_t i = 0; i < cells.Meshes.size(); i++)
			patches.push_back({ base + uint32_t(i), cells.Bounds[i].first, cells.Bounds[i].second });
	}

	float LakeArt::GorgeHalf(Vec2 local) const
	{
		float along = 0.0f;
		if (m_Trench.size() >= 2)
			TrenchDistance(local, &along);
		return GorgeHalf(local, along);
	}

	float LakeArt::GorgeHalf(Vec2 local, float along) const
	{
		const float outside = Smooth(24.0f, 31.0f, std::abs(local.x)) * (local.x * m_Layout.Side < 0.0f ? 1.0f : 0.0f);
		const float ending = Smooth(0.0f, 11.0f, m_TrenchLength - along);
		return LakeTrenchHalf * (0.7f + 0.3f * ending) * std::lerp(1.0f, 0.6f, outside);
	}

	float LakeArt::LandHeight(Vec2 p) const
	{
		const Vec2 local = p - m_Center;
		const float outside = Smooth(24.0f, 31.0f, std::abs(local.x)) * (local.x * m_Layout.Side < 0.0f ? 1.0f : 0.0f);
		float along = 0.0f;
		const float gorge = m_Trench.size() < 2 ? 1e9f : TrenchDistance(local, &along);
		const float half = GorgeHalf(local, along);
		const float ridge = outside * (1.7f + 1.5f * Noise2(p * 0.17f, 0xE717u)) * (1.0f - Smooth(half + 1.5f, half + 8.0f, gorge));
		const float inReach = outside * (1.0f - Smooth(half + 4.0f, half + 13.0f, gorge));
		const float lumps = inReach > 0.0f ? inReach * 0.6f * RockRelief(glm::vec3(p.x * 0.55f, 0.0f, p.y * 0.55f)) : 0.0f;
		return m_ShoreLevel.Height(p) + ridge + lumps;
	}

	void LakeArt::BuildWalls(WorldRenderer& renderer, const Level& level)
	{
		std::optional<uint32_t> cliff = renderer.FindMaterialSlot("GroundFrostRock");
		if (!cliff)
			cliff = renderer.FindMaterialSlot("GroundCliff");
		const std::optional<uint32_t> snow = renderer.FindMaterialSlot("GroundSnow");
		if (!cliff || !snow || m_Trench.size() < 2)
			return;
		Rng rng{ level.Seed ^ 0x3A11u };
		constexpr float Step = 0.34f;
		constexpr float Rise = 0.34f;
		constexpr float Start = -5.0f;
		struct Station
		{
			Vec2 Centre;
			Vec2 Out;
			float Along = 0.0f;
			float Half = 0.0f;
		};
		std::vector<Station> path;
		const auto sideStation = [&](float along, float side)
		{
			const Vec2 centre = TrenchPoint(along);
			const Vec2 tangent = Normalize(TrenchPoint(along + 0.4f) - TrenchPoint(along - 0.4f));
			return Station{ centre, Vec2(-tangent.y, tangent.x) * side, along, GorgeHalf(centre, along) };
		};
		const float end = m_TrenchLength;
		const int32_t run = int32_t((end - Start) / Step);
		for (int32_t c = 0; c <= run; c++)
			path.push_back(sideStation(Start + (end - Start) * float(c) / float(run), -1.0f));
		{
			const Vec2 centre = TrenchPoint(end);
			const Vec2 tangent = Normalize(TrenchPoint(end) - TrenchPoint(end - 0.8f));
			const Vec2 normal(-tangent.y, tangent.x);
			const float half = GorgeHalf(centre, end);
			const int32_t arc = std::max(int32_t(Pi * half / Step), 4);
			for (int32_t k = 1; k < arc; k++)
			{
				const float a = Pi * float(k) / float(arc);
				path.push_back({ centre, normal * -std::cos(a) + tangent * std::sin(a), end, half });
			}
		}
		for (int32_t c = run; c >= 0; c--)
			path.push_back(sideStation(Start + (end - Start) * float(c) / float(run), 1.0f));

		const int32_t columns = int32_t(path.size());
		std::vector<float> tops(static_cast<size_t>(columns));
		std::vector<float> bottoms(static_cast<size_t>(columns));
		std::vector<float> landish(static_cast<size_t>(columns));
		float highest = -1e9f;
		float lowest = 1e9f;
		for (int32_t c = 0; c < columns; c++)
		{
			const Station& s = path[size_t(c)];
			const Vec2 lip = s.Centre + s.Out * (s.Half + 0.6f);
			const float land = LandHeight(m_Center + lip);
			const float onLand = Smooth(LakeWaterLine - 0.25f, LakeWaterLine + 0.25f, land);
			landish[size_t(c)] = onLand;
			tops[size_t(c)] = std::lerp(BedHeight(lip), land, onLand);
			bottoms[size_t(c)] = std::max(std::min(BedHeight(s.Centre), BedHeight(s.Centre + s.Out * s.Half * 0.7f)) - 1.6f, LakeWaterLine - 15.0f);
			bottoms[size_t(c)] = std::min(bottoms[size_t(c)], tops[size_t(c)] - 0.6f);
			highest = std::max(highest, tops[size_t(c)]);
			lowest = std::min(lowest, bottoms[size_t(c)]);
		}
		const int32_t face = int32_t((highest - lowest) / Rise) + 2;
		const int32_t height = face + 3;
		std::vector<glm::vec3> grid(size_t(columns) * size_t(height));
		for (int32_t c = 0; c < columns; c++)
		{
			const Station& s = path[size_t(c)];
			const float top = tops[size_t(c)];
			const float bottom = bottoms[size_t(c)];
			const float onLand = landish[size_t(c)];
			const glm::vec3 out(s.Out.x, 0.0f, s.Out.y);
			const auto at = [&](float reach, float y) { return glm::vec3(m_Center.x + s.Centre.x + s.Out.x * reach, y, m_Center.y + s.Centre.y + s.Out.y * reach); };
			const auto ground = [&](float reach)
			{
				const Vec2 local = s.Centre + s.Out * reach;
				return std::lerp(BedHeight(local), LandHeight(m_Center + local), onLand);
			};
			const float reaches[3] = { std::lerp(2.0f, 4.3f, onLand), std::lerp(1.2f, 2.6f, onLand), std::lerp(0.6f, 1.1f, onLand) };
			const float lifts[3] = { std::lerp(-0.06f, -0.1f, onLand), std::lerp(0.05f, 0.07f, onLand), std::lerp(0.03f, 0.05f, onLand) };
			for (int32_t k = 0; k < 3; k++)
				grid[size_t(c) * size_t(height) + size_t(k)] = at(s.Half + reaches[k], ground(s.Half + reaches[k]) + lifts[k]);
			const float span = std::max(top - bottom, 1.0f);
			const float shelfScale = (0.45f + 0.55f * Noise2(Vec2(s.Along * 0.12f, s.Out.x * 3.0f + s.Out.y * 7.0f), 0xA6B1u)) * s.Half * 0.17f * Smooth(4.0f, 7.0f, span);
			const float shelfA = span * (0.3f + 0.08f * (Noise2(Vec2(s.Along * 0.09f, 1.7f), 0xA6C3u) - 0.5f));
			const float shelfB = span * (0.62f + 0.08f * (Noise2(Vec2(s.Along * 0.09f, 4.1f), 0xA6D5u) - 0.5f));
			const float route = RouteDepth(std::clamp(s.Along, 0.0f, m_RouteLength));
			for (int32_t r = 0; r < face; r++)
			{
				const float t = float(r) / float(face - 1);
				const float y = std::lerp(top - 0.18f, bottom, t);
				const float below = top - y;
				const float taper = s.Half * (1.0f - 0.16f * Smooth(0.0f, 1.0f, below / span));
				const glm::vec3 base = at(taper, y);
				const float relief = RockRelief(base);
				const float push = std::clamp(0.42f + 0.62f * relief, 0.0f, 1.45f) * Smooth(0.0f, 1.3f, below) + 0.32f - 0.3f * Smooth(0.0f, 0.5f, below);
				const float overhang = 0.18f * Smooth(0.25f, 1.1f, below) * (1.0f - Smooth(1.1f, 2.4f, below));
				const float ledges = shelfScale * (Smooth(shelfA - 0.2f, shelfA + 0.25f, below) + Smooth(shelfB - 0.2f, shelfB + 0.25f, below));
				float reach = taper - (push - overhang) - ledges;
				const float clear = 1.0f - Smooth(2.0f, 3.6f, std::abs(y - route));
				reach = std::max(reach, std::lerp(0.6f, std::min(2.5f, s.Half - 0.3f), clear));
				grid[size_t(c) * size_t(height) + size_t(r + 3)] = at(reach, y);
			}
		}
		const auto point = [&](int32_t c, int32_t r) { return grid[size_t(std::clamp(c, 0, columns - 1)) * size_t(height) + size_t(std::clamp(r, 0, height - 1))]; };
		std::vector<float> lengths(static_cast<size_t>(columns), 0.0f);
		for (int32_t c = 1; c < columns; c++)
			lengths[size_t(c)] = lengths[size_t(c - 1)] + Length(path[size_t(c)].Centre + path[size_t(c)].Out * path[size_t(c)].Half - path[size_t(c - 1)].Centre - path[size_t(c - 1)].Out * path[size_t(c - 1)].Half);
		float orient = 1.0f;
		{
			const int32_t c = run / 2;
			const int32_t r = height / 2;
			const glm::vec3 inward = -glm::vec3(path[size_t(c)].Out.x, 0.0f, path[size_t(c)].Out.y);
			orient = glm::dot(glm::cross(point(c, r + 1) - point(c, r - 1), point(c + 1, r) - point(c - 1, r)), inward) >= 0.0f ? 1.0f : -1.0f;
		}
		std::vector<ModelVertex> vertices;
		vertices.reserve(grid.size());
		for (int32_t c = 0; c < columns; c++)
		{
			float profile = point(c, 3).y;
			for (int32_t k = 3; k > 0; k--)
				profile += glm::length(point(c, k) - point(c, k - 1));
			for (int32_t r = 0; r < height; r++)
			{
				const glm::vec3 p = point(c, r);
				if (r > 0 && r <= 3)
					profile -= glm::length(p - point(c, r - 1));
				const float across = r <= 3 ? profile : p.y;
				const glm::vec3 du = point(c + 1, r) - point(c - 1, r);
				const glm::vec3 dv = point(c, r + 1) - point(c, r - 1);
				glm::vec3 normal = glm::cross(dv, du) * orient;
				normal = glm::length(normal) > 1e-8f ? glm::normalize(normal) : glm::vec3(0.0f, 1.0f, 0.0f);
				const float above = p.y - LakeWaterLine;
				const float dry = Smooth(0.3f, 1.2f, above);
				const float ledge = Smooth(0.35f, 0.75f, normal.y) * dry;
				const float lipSnow = (r < 2 ? 1.0f : r == 2 ? 0.85f : r == 3 ? 0.4f : 0.0f) * dry;
				const float dusting = Smooth(0.5f, 0.68f, Noise3(p * 0.9f, 0xA161u) + 0.3f * normal.y);
				const float rime = Smooth(0.05f, 0.4f, normal.y) * dry * Smooth(0.55f, 0.75f, Noise3(p * glm::vec3(1.6f, 0.5f, 1.6f), 0xA49Bu)) * 0.55f;
				const float snowy = std::clamp(std::max({ lipSnow, ledge * (0.6f + 0.4f * dusting), rime }), 0.0f, r < 2 && dry > 0.99f ? 1.0f : 0.985f);
				const float cavity = Smooth(-0.25f, 0.45f, RockRelief(p + normal * 0.3f) - RockRelief(p) + 0.2f);
				const float depth = Smooth(0.0f, 14.0f, -above);
				const float wetBand = 1.0f - Smooth(0.05f, 0.75f + 0.35f * Noise3(p * 1.3f, 0xA385u), above);
				const float banding = 0.75f + 0.5f * Noise3(glm::vec3(p.x * 0.15f, p.y * 1.1f, p.z * 0.15f), 0xA5A7u);
				const float brightness = std::clamp((0.2f + 0.6f * cavity) * banding * (1.0f - 0.3f * depth), 0.0f, 1.0f);
				const float glaze = std::clamp(std::max(wetBand * 0.85f, dry * (1.0f - normal.y) * Smooth(0.6f, 0.82f, Noise3(glm::vec3(p.x * 2.2f, p.y * 0.25f, p.z * 2.2f), 0xA7E9u))), 0.0f, 1.0f) * (1.0f - snowy);
				const float hue = Noise3(p * 0.11f + glm::vec3(3.1f, 0.0f, 1.7f), 0xA8FBu);
				const glm::vec3 dPdu = glm::normalize(du);
				glm::vec3 tangentU = dPdu - normal * glm::dot(normal, dPdu);
				tangentU = glm::length(tangentU) > 1e-6f ? glm::normalize(tangentU) : glm::vec3(1.0f, 0.0f, 0.0f);
				ModelVertex vertex;
				vertex.Position = p;
				vertex.U = lengths[size_t(c)];
				vertex.V = -across;
				vertex.Normal = normal;
				vertex.Weights = 255;
				vertex.Tangent = PackTangent(tangentU, glm::dot(glm::cross(normal, tangentU), -dv) >= 0.0f ? 1.0f : -1.0f);
				vertex.Color = Pack(glm::vec3(snowy, glaze, brightness), hue);
				vertices.push_back(vertex);
				if (r > 4 && above < -1.5f && above > -13.0f)
				{
					if (normal.y < 0.45f && rng.Next() < 0.03f)
					{
						bool spaced = true;
						for (const WallSpot& other : m_WallSpots)
							spaced = spaced && glm::length(other.Position - p) > 4.2f;
						if (spaced)
							m_WallSpots.push_back({ p, normal });
					}
					else if (normal.y > 0.7f && rng.Next() < 0.25f)
					{
						bool spaced = true;
						for (const WallSpot& other : m_LedgeSpots)
							spaced = spaced && glm::length(other.Position - p) > 1.6f;
						if (spaced)
							m_LedgeSpots.push_back({ p, normal });
					}
				}
			}
		}
		std::vector<uint32_t> indices;
		for (int32_t c = 0; c + 1 < columns; c++)
			for (int32_t r = 0; r + 1 < height; r++)
			{
				const uint32_t a = uint32_t(c * height + r);
				const uint32_t b = a + uint32_t(height);
				const uint32_t d = b + 1;
				const uint32_t e = a + 1;
				for (const auto& [i, j, k] : { std::array<uint32_t, 3>{ a, b, d }, std::array<uint32_t, 3>{ a, d, e } })
				{
					const glm::vec3 facing = vertices[i].Normal + vertices[j].Normal + vertices[k].Normal;
					if (glm::dot(glm::cross(vertices[j].Position - vertices[i].Position, vertices[k].Position - vertices[i].Position), facing) < 0.0f)
						indices.insert(indices.end(), { i, k, j });
					else
						indices.insert(indices.end(), { i, j, k });
				}
			}
		const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(indices.size()), .Material = *cliff, .Layer = *snow, .Flags = MeshLayered | MeshRock };
		std::vector<uint32_t> above;
		std::vector<uint32_t> below;
		for (size_t t = 0; t + 2 < indices.size(); t += 3)
		{
			const float high = std::max({ vertices[indices[t]].Position.y, vertices[indices[t + 1]].Position.y, vertices[indices[t + 2]].Position.y });
			const float low = std::min({ vertices[indices[t]].Position.y, vertices[indices[t + 1]].Position.y, vertices[indices[t + 2]].Position.y });
			if (high > LakeWaterLine - 0.05f)
				above.insert(above.end(), { indices[t], indices[t + 1], indices[t + 2] });
			if (low < LakeWaterLine + 0.05f)
				below.insert(below.end(), { indices[t], indices[t + 1], indices[t + 2] });
		}
		AddPatches(renderer, vertices, above, desc, 9.0f, m_Walls);
		AddPatches(renderer, vertices, below, desc, 9.0f, m_WallsUnder);
	}

	void LakeArt::BuildTrench()
	{
		m_TrenchLength = SampleSpline(m_Layout.Trench.data(), m_Layout.Trench.size(), m_Trench, m_TrenchAlong);
	}

	Vec2 LakeArt::TrenchPoint(float along) const
	{
		return PointAlong(m_Trench, m_TrenchAlong, along);
	}

	void LakeArt::BuildRoute()
	{
		m_RouteLength = SampleSpline(m_Layout.Route.data(), m_Layout.Route.size(), m_Route, m_RouteAlong);
		float middle = 0.0f;
		float closest = 1e9f;
		for (size_t i = 0; i < m_Route.size(); i++)
			if (Length(m_Route[i]) < closest)
			{
				closest = Length(m_Route[i]);
				middle = m_RouteAlong[i];
			}
		std::vector<float> depth(m_Route.size());
		for (size_t i = 0; i < m_Route.size(); i++)
		{
			const float profile = -4.3f - 11.5f * Smooth(7.0f, 30.0f, std::abs(m_RouteAlong[i] - middle));
			depth[i] = std::clamp(std::max(profile, BedHeight(m_Route[i]) + 3.0f), -30.0f, -3.6f);
		}
		for (int32_t pass = 0; pass < 6; pass++)
		{
			std::vector<float> blurred(depth.size());
			for (size_t i = 0; i < depth.size(); i++)
			{
				float sum = 0.0f;
				int32_t count = 0;
				for (int32_t k = -4; k <= 4; k++)
				{
					const int32_t j = std::clamp(int32_t(i) + k, 0, int32_t(depth.size()) - 1);
					sum += depth[size_t(j)];
					count++;
				}
				blurred[i] = std::max(sum / float(count), depth[i] - 0.4f);
			}
			depth.swap(blurred);
		}
		m_RouteDepth = depth;
	}

	Vec2 LakeArt::RoutePoint(float along) const
	{
		return PointAlong(m_Route, m_RouteAlong, along);
	}

	float LakeArt::RouteDepth(float along) const
	{
		if (m_RouteDepth.empty())
			return -6.0f;
		if (along <= 0.0f)
			return m_RouteDepth.front();
		if (along >= m_RouteLength)
			return m_RouteDepth.back();
		const size_t i = size_t(std::upper_bound(m_RouteAlong.begin(), m_RouteAlong.end(), along) - m_RouteAlong.begin());
		const size_t a = std::min(std::max<size_t>(i, 1), m_RouteAlong.size() - 1) - 1;
		const float t = (along - m_RouteAlong[a]) / std::max(m_RouteAlong[a + 1] - m_RouteAlong[a], 1e-4f);
		return std::lerp(m_RouteDepth[a], m_RouteDepth[a + 1], t);
	}

	float LakeArt::TrenchDistance(Vec2 local, float* along) const
	{
		float best = 1e9f;
		float where = 0.0f;
		for (size_t i = 0; i + 1 < m_Trench.size(); i++)
		{
			const Vec2 a = m_Trench[i];
			const Vec2 ab = m_Trench[i + 1] - a;
			const float t = std::clamp(Dot(local - a, ab) / std::max(Dot(ab, ab), 1e-6f), 0.0f, 1.0f);
			const float d = Length(local - (a + ab * t));
			if (d < best)
			{
				best = d;
				where = m_TrenchAlong[i] + (m_TrenchAlong[i + 1] - m_TrenchAlong[i]) * t;
			}
		}
		if (along)
			*along = where;
		return best;
	}

	float LakeArt::BedHeight(Vec2 local) const
	{
		const float side = m_Layout.Side;
		const float deep = Smooth(5.0f, 21.0f, -local.y) * 2.6f + Smooth(19.0f, 30.0f, std::abs(local.x)) * 3.0f + Smooth(18.0f, 46.0f, side * local.x) * 5.0f;
		const float hills = 1.2f * (Noise2(local * 0.11f, 0x71A1u) - 0.5f) + 0.5f * (Noise2(local * 0.33f, 0x7CA7u) - 0.5f);
		float along = 0.0f;
		const float distance = TrenchDistance(local, &along);
		const float ending = Smooth(0.0f, 11.0f, m_TrenchLength - along);
		const float outside = Smooth(24.0f, 31.0f, std::abs(local.x)) * (local.x * side < 0.0f ? 1.0f : 0.0f);
		const float half = LakeTrenchHalf * (0.7f + 0.3f * ending) * std::lerp(1.0f, 0.6f, outside);
		const float raw = 1.0f - Smooth(half - 0.2f, half + 0.4f, distance);
		const float floor = (15.5f + 4.5f * (1.0f - Smooth(0.0f, 18.0f, along)) + 3.0f * (Noise2(local * 0.08f, 0x7E43u) - 0.5f)) * ending;
		float bed = -6.3f - deep + hills - raw * floor;
		if (side * local.x > 14.0f)
		{
			const float trough = 1.0f - Smooth(m_Layout.ChannelHalf(local.x) * 0.25f, m_Layout.ChannelHalf(local.x) * 0.95f, std::abs(local.y - m_Layout.Channel(local.x)));
			bed -= trough * 8.0f * Smooth(14.0f, 30.0f, side * local.x);
		}
		const float shore = m_ShoreLevel.Floor(m_Center + local) - 0.25f;
		return std::max(bed, shore - raw * 40.0f);
	}

	void LakeArt::BuildSnow(WorldRenderer& renderer, const Level& level)
	{
		if (m_Trench.size() < 2)
			return;
		Rng rng{ level.Seed ^ 0x5A0Fu };
		MeshBuild specks;
		for (int32_t i = 0; i < 1400; i++)
		{
			const float along = rng.Range(0.0f, m_TrenchLength);
			const Vec2 centre = TrenchPoint(along);
			const Vec2 tangent = Normalize(TrenchPoint(along + 0.5f) - TrenchPoint(along - 0.5f));
			const Vec2 local = centre + Vec2(-tangent.y, tangent.x) * (LakeTrenchHalf * rng.Range(-0.95f, 0.95f));
			const glm::vec3 p(m_Center.x + local.x, rng.Range(-22.5f, -4.8f), m_Center.y + local.y);
			const bool bright = rng.Next() < 0.05f;
			const float r = bright ? rng.Range(0.07f, 0.11f) : rng.Range(0.022f, 0.05f);
			const uint32_t color = bright ? Pack(glm::vec3(0.25f, 0.95f, 1.0f), rng.Range(0.6f, 1.0f)) : Pack(glm::vec3(0.72f, 0.88f, 0.95f), rng.Range(0.18f, 0.42f));
			specks.Quad(Vertex(p, glm::vec3(-r, 0.0f, -r), { -1.0f, 5.0f }, color), Vertex(p, glm::vec3(r, 0.0f, -r), { 1.0f, 5.0f }, color), Vertex(p, glm::vec3(r, 0.0f, r), { 1.0f, 7.0f }, color), Vertex(p, glm::vec3(-r, 0.0f, r), { -1.0f, 7.0f }, color));
		}
				const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(specks.Indices.size()), .Material = renderer.GetPlainMaterial(), .Layer = renderer.GetPlainMaterial(), .Flags = MeshUnderGlow };
		m_Snow = renderer.AddLevelGeometry(specks.Vertices, specks.Indices, { desc });

		MeshBuild growth;
		const glm::vec3 tones[] = { { 0.1f, 1.0f, 0.78f }, { 0.62f, 0.32f, 1.0f }, { 0.2f, 0.72f, 1.0f }, { 0.35f, 1.0f, 0.45f } };
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		for (int32_t cluster = 0; cluster < 55; cluster++)
		{
			const float along = rng.Range(0.0f, m_TrenchLength);
			const Vec2 centre = TrenchPoint(along);
			const Vec2 tangent = Normalize(TrenchPoint(along + 0.5f) - TrenchPoint(along - 0.5f));
			const Vec2 hub = centre + Vec2(-tangent.y, tangent.x) * (LakeTrenchHalf * rng.Range(0.3f, 0.95f) * (rng.Next() < 0.5f ? -1.0f : 1.0f));
			if (BedHeight(hub) > -7.5f)
				continue;
			const glm::vec3 tone = tones[cluster % 4];
			const int32_t count = 6 + int32_t(rng.Next() * 12.0f);
			for (int32_t k = 0; k < count; k++)
			{
				const Vec2 at = hub + FromAngle(rng.Range(0.0f, 6.28f)) * (1.3f * rng.Next() * rng.Next());
				const float y = BedHeight(at) + 0.05f;
				const float r = rng.Range(0.04f, 0.12f);
				const uint32_t color = Pack(tone, rng.Range(0.45f, 1.0f));
				const glm::vec3 p(m_Center.x + at.x, y, m_Center.y + at.y);
				growth.Quad(Vertex(p + glm::vec3(-r, 0.0f, -r), up, { -1.0f, 8.0f }, color), Vertex(p + glm::vec3(r, 0.0f, -r), up, { 1.0f, 8.0f }, color), Vertex(p + glm::vec3(r, 0.0f, r), up, { 1.0f, 10.0f }, color), Vertex(p + glm::vec3(-r, 0.0f, r), up, { -1.0f, 10.0f }, color));
			}
		}
		if (!growth.Indices.empty())
		{
			const LevelMeshDesc glowDesc{ .FirstIndex = 0, .IndexCount = uint32_t(growth.Indices.size()), .Material = renderer.GetPlainMaterial(), .Layer = renderer.GetPlainMaterial(), .Flags = MeshUnderGlow };
			m_Growth = renderer.AddLevelGeometry(growth.Vertices, growth.Indices, { glowDesc });
		}
	}

	void LakeArt::BuildFlora(WorldRenderer& renderer, const Level& level)
	{
		Rng rng{ level.Seed ^ 0xF10Au };
		MeshBuild body;
		MeshBuild glow;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		const auto lit = [&](const glm::vec3& p, const glm::vec3& n, const glm::vec3& color, uint32_t sway)
		{
			ModelVertex vertex = Vertex(p, n, { 0.0f, 0.0f }, Pack(color, 1.0f));
			vertex.Joints = sway;
			return vertex;
		};
		const auto shine = [&](const glm::vec3& p, const glm::vec3& n, glm::vec2 uv, const glm::vec3& color, float strength, uint32_t sway)
		{
			ModelVertex vertex = Vertex(p, n, uv, Pack(color, strength));
			vertex.Joints = sway;
			return vertex;
		};
		const auto triangle = [](MeshBuild& build, const ModelVertex& a, const ModelVertex& b, const ModelVertex& c)
		{
			const uint32_t base = uint32_t(build.Vertices.size());
			build.Vertices.insert(build.Vertices.end(), { a, b, c });
			build.Indices.insert(build.Indices.end(), { base, base + 1, base + 2 });
		};
		const auto halo = [&](const glm::vec3& p, float radius, const glm::vec3& color, float strength, uint32_t sway)
		{
			const glm::vec3 size(radius, 0.0f, 0.0f);
			glow.Quad(shine(p, size, { -1.0f, 14.0f }, color, strength, sway), shine(p, size, { 1.0f, 14.0f }, color, strength, sway), shine(p, size, { 1.0f, 16.0f }, color, strength, sway), shine(p, size, { -1.0f, 16.0f }, color, strength, sway));
		};
		const auto bulb = [&](const glm::vec3& centre, float radius, const glm::vec3& color, float strength, float phase, uint32_t sway)
		{
			constexpr int32_t Bands = 4;
			constexpr int32_t Sides = 7;
			const auto at = [&](int32_t band, int32_t side)
			{
				const float lat = Pi * (float(band) / float(Bands) - 0.5f);
				const float lon = 2.0f * Pi * float(side) / float(Sides);
				return glm::vec3(std::cos(lat) * std::cos(lon), std::sin(lat) * 1.25f, std::cos(lat) * std::sin(lon));
			};
			for (int32_t band = 0; band < Bands; band++)
				for (int32_t side = 0; side < Sides; side++)
				{
					const glm::vec3 a = at(band, side);
					const glm::vec3 b = at(band, side + 1);
					const glm::vec3 c = at(band + 1, side + 1);
					const glm::vec3 d = at(band + 1, side);
					glow.Quad(shine(centre + a * radius, glm::normalize(a), { phase, 12.0f }, color, strength, sway), shine(centre + b * radius, glm::normalize(b), { phase, 12.0f }, color, strength, sway), shine(centre + c * radius, glm::normalize(c), { phase, 12.0f }, color, strength, sway), shine(centre + d * radius, glm::normalize(d), { phase, 12.0f }, color, strength, sway));
				}
		};
		const auto stalk = [&](MeshBuild& build, const std::vector<glm::vec3>& spine, const std::vector<float>& widths, const glm::vec3& face, const glm::vec3& baseColor, const glm::vec3& tipColor, float phase, float amplitude, float weightFrom, float weightTo)
		{
			const size_t count = spine.size();
			const uint32_t first = uint32_t(build.Vertices.size());
			for (size_t k = 0; k < count; k++)
			{
				const float t = float(k) / float(count - 1);
				const glm::vec3 along = glm::normalize(spine[std::min(k + 1, count - 1)] - spine[k > 0 ? k - 1 : 0]);
				glm::vec3 across = glm::cross(along, face);
				across = glm::length(across) > 1e-5f ? glm::normalize(across) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 normal = glm::normalize(glm::cross(across, along) + up * 0.6f);
				const uint32_t sway = SwayBits(std::lerp(weightFrom, weightTo, t), phase, amplitude);
				const glm::vec3 color = glm::mix(baseColor, tipColor, t);
				build.Vertices.push_back(lit(spine[k] - across * widths[k], normal, color, sway));
				build.Vertices.push_back(lit(spine[k] + across * widths[k], normal, color, sway));
			}
			for (uint32_t k = 0; k + 1 < uint32_t(count); k++)
			{
				const uint32_t a = first + k * 2;
				build.Indices.insert(build.Indices.end(), { a, a + 1, a + 3, a, a + 3, a + 2 });
			}
		};
		const auto glowStalk = [&](const std::vector<glm::vec3>& spine, const std::vector<float>& widths, const glm::vec3& face, const glm::vec3& color, float rootStrength, float tipStrength, float phase, float amplitude)
		{
			const size_t count = spine.size();
			const uint32_t first = uint32_t(glow.Vertices.size());
			for (size_t k = 0; k < count; k++)
			{
				const float t = float(k) / float(count - 1);
				const glm::vec3 along = glm::normalize(spine[std::min(k + 1, count - 1)] - spine[k > 0 ? k - 1 : 0]);
				glm::vec3 across = glm::cross(along, face);
				across = glm::length(across) > 1e-5f ? glm::normalize(across) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 normal = glm::normalize(glm::cross(across, along));
				const uint32_t sway = SwayBits(t, phase, amplitude);
				const float strength = std::lerp(rootStrength, tipStrength, t * t);
				glow.Vertices.push_back(shine(spine[k] - across * widths[k], normal, { phase, 12.0f }, color, strength, sway));
				glow.Vertices.push_back(shine(spine[k] + across * widths[k], normal, { phase, 12.0f }, color, strength, sway));
			}
			for (uint32_t k = 0; k + 1 < uint32_t(count); k++)
			{
				const uint32_t a = first + k * 2;
				glow.Indices.insert(glow.Indices.end(), { a, a + 1, a + 3, a, a + 3, a + 2 });
			}
		};
		const auto curve = [&](const glm::vec3& root, const glm::vec3& lean, float length, int32_t segments, float droop)
		{
			std::vector<glm::vec3> spine;
			for (int32_t k = 0; k <= segments; k++)
			{
				const float t = float(k) / float(segments);
				spine.push_back(root + up * (length * t * (1.0f - 0.35f * droop * t)) + lean * (length * t * t));
			}
			return spine;
		};
		const glm::vec3 glowTones[] = { { 0.12f, 0.95f, 1.0f }, { 0.68f, 0.32f, 1.0f }, { 1.0f, 0.32f, 0.78f }, { 0.4f, 1.0f, 0.42f }, { 0.25f, 0.6f, 1.0f } };
		const auto world = [&](Vec2 local, float y) { return glm::vec3(m_Center.x + local.x, y, m_Center.y + local.y); };
		const auto trenchSpot = [&](float lateral, Vec2& local, float& along)
		{
			along = rng.Range(2.0f, m_TrenchLength - 1.0f);
			const Vec2 centre = TrenchPoint(along);
			const Vec2 tangent = Normalize(TrenchPoint(along + 0.5f) - TrenchPoint(along - 0.5f));
			local = centre + Vec2(-tangent.y, tangent.x) * (GorgeHalf(centre) * rng.Range(-lateral, lateral));
		};

		std::vector<WallSpot> spots = m_WallSpots;
		std::vector<WallSpot> ledges = m_LedgeSpots;
		for (size_t i = ledges.size(); i > 1; i--)
			std::swap(ledges[i - 1], ledges[std::min(size_t(rng.Next() * float(i)), i - 1)]);
		size_t nextLedge = 0;
		const auto anchor = [&](float lateral, Vec2& local, float& ground, float& along, float share)
		{
			if (nextLedge < ledges.size() && rng.Next() < share)
			{
				const glm::vec3 p = ledges[nextLedge++].Position;
				local = Vec2(p.x - m_Center.x, p.z - m_Center.y);
				ground = p.y;
				along = 0.0f;
				return true;
			}
			trenchSpot(lateral, local, along);
			ground = BedHeight(local);
			return false;
		};
		for (const WallSpot& spot : spots)
		{
			glm::vec3 out(spot.Outward.x, 0.0f, spot.Outward.z);
			if (glm::length(out) < 0.35f)
				continue;
			out = glm::normalize(out);
			const glm::vec3 tangent = glm::normalize(glm::cross(up, out));
			const glm::vec3 tone = glowTones[size_t(rng.Next() * 2.99f) % 5 == 2 ? 4 : size_t(rng.Next() * 1.99f)];
			const glm::vec3 shell = rng.Next() < 0.5f ? glm::vec3(0.2f, 0.12f, 0.32f) : glm::vec3(0.08f, 0.2f, 0.24f);
			const int32_t plates = 2 + int32_t(rng.Next() * 3.5f);
			float rise = 0.0f;
			for (int32_t k = 0; k < plates; k++)
			{
				const float radius = rng.Range(0.4f, 1.0f) * (1.0f - 0.12f * float(k));
				const float tilt = rng.Range(0.1f, 0.3f);
				const glm::vec3 reach = glm::normalize(out * std::cos(tilt) + up * std::sin(tilt));
				const glm::vec3 centre = spot.Position + up * rise + tangent * rng.Range(-0.35f, 0.35f) - out * 0.08f;
				const glm::vec3 normal = glm::normalize(glm::cross(reach, tangent)) * (glm::cross(reach, tangent).y < 0.0f ? -1.0f : 1.0f);
				const float wobble = rng.Range(0.0f, 6.28f);
				constexpr int32_t Arc = 12;
				std::vector<glm::vec3> rim;
				for (int32_t s = 0; s <= Arc; s++)
				{
					const float a = Pi * (float(s) / float(Arc) - 0.5f);
					const float r = radius * (1.0f + 0.13f * std::sin(a * 5.0f + wobble) + 0.07f * std::sin(a * 11.0f + wobble * 2.0f)) * (0.55f + 0.45f * std::cos(a * 0.9f));
					rim.push_back(centre + (reach * std::cos(a) + tangent * std::sin(a)) * r);
				}
				const glm::vec3 thick = -normal * 0.045f;
				const uint32_t sway = SwayBits(0.0f, 0.0f, 0.0f);
				for (int32_t s = 0; s < Arc; s++)
				{
					triangle(body, lit(centre, normal, shell * 0.8f, sway), lit(rim[size_t(s)], normal, shell * 1.25f, sway), lit(rim[size_t(s + 1)], normal, shell * 1.25f, sway));
					triangle(body, lit(centre + thick, -normal, shell * 0.5f, sway), lit(rim[size_t(s + 1)] + thick, -normal, shell * 0.6f, sway), lit(rim[size_t(s)] + thick, -normal, shell * 0.6f, sway));
					const glm::vec3 edgeOut = glm::normalize(rim[size_t(s)] + rim[size_t(s + 1)] - centre * 2.0f);
					body.Quad(lit(rim[size_t(s)], edgeOut, shell, sway), lit(rim[size_t(s + 1)], edgeOut, shell, sway), lit(rim[size_t(s + 1)] + thick, edgeOut, shell, sway), lit(rim[size_t(s)] + thick, edgeOut, shell, sway));
					const glm::vec3 inA = glm::mix(rim[size_t(s)], centre, 0.13f) + normal * 0.006f;
					const glm::vec3 inB = glm::mix(rim[size_t(s + 1)], centre, 0.13f) + normal * 0.006f;
					const float strength = 0.75f + 0.25f * std::sin(float(s) * 0.9f + wobble);
					glow.Quad(shine(rim[size_t(s)] + normal * 0.006f, normal, { 0.0f, 12.0f }, tone, strength, sway), shine(rim[size_t(s + 1)] + normal * 0.006f, normal, { 0.0f, 12.0f }, tone, strength, sway), shine(inB, normal, { 0.0f, 12.0f }, tone, strength * 0.25f, sway), shine(inA, normal, { 0.0f, 12.0f }, tone, strength * 0.25f, sway));
					glow.Quad(shine(rim[size_t(s)] + normal * 0.002f, edgeOut, { 0.0f, 12.0f }, tone, strength * 0.8f, sway), shine(rim[size_t(s + 1)] + normal * 0.002f, edgeOut, { 0.0f, 12.0f }, tone, strength * 0.8f, sway), shine(rim[size_t(s + 1)] + thick, edgeOut, { 0.0f, 12.0f }, tone, strength * 0.5f, sway), shine(rim[size_t(s)] + thick, edgeOut, { 0.0f, 12.0f }, tone, strength * 0.5f, sway));
				}
				rise += rng.Range(0.22f, 0.5f);
			}
		}

		int32_t lights = 0;
		if (m_Trench.size() >= 2)
		{
			for (int32_t bush = 0; bush < 34; bush++)
			{
				Vec2 local;
				float along = 0.0f;
				float ground = 0.0f;
				const bool ledge = anchor(0.8f, local, ground, along, 0.8f);
				if (ground > (ledge ? LakeWaterLine - 2.8f : -8.0f))
					continue;
				const glm::vec3 root = world(local, ground - 0.05f);
				const glm::vec3 tone = glowTones[size_t(bush) % 5];
				const float phase = rng.Next();
				const float amplitude = rng.Range(0.05f, 0.11f);
				const int32_t stems = 5 + int32_t(rng.Next() * 7.0f);
				for (int32_t s = 0; s < stems; s++)
				{
					const float a = rng.Range(0.0f, 6.28f);
					const glm::vec3 lean = glm::vec3(std::cos(a), 0.0f, std::sin(a)) * rng.Range(0.15f, 0.55f);
					const float length = rng.Range(0.7f, 2.0f);
					const std::vector<glm::vec3> spine = curve(root + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * rng.Range(0.0f, 0.15f), lean, length, 5, 0.3f);
					std::vector<float> widths;
					for (int32_t k = 0; k <= 5; k++)
						widths.push_back(0.03f * (1.0f - 0.5f * float(k) / 5.0f));
					glowStalk(spine, widths, glm::vec3(std::sin(a), 0.0f, -std::cos(a)), tone, 0.06f, 0.45f, phase, amplitude * length);
					glowStalk(spine, widths, glm::vec3(std::cos(a), 0.0f, std::sin(a)), tone, 0.06f, 0.45f, phase, amplitude * length);
					const uint32_t tip = SwayBits(1.0f, phase, amplitude * length);
					const float size = rng.Range(0.07f, 0.15f);
					bulb(spine.back() + up * size * 0.6f, size, tone, rng.Range(0.75f, 1.0f), rng.Next(), tip);
					halo(spine.back() + up * size * 0.6f, size * rng.Range(3.5f, 5.0f), tone, 0.42f, tip);
				}
				if (lights < 8 && bush % 3 == 0)
				{
					m_Glows.push_back({ root + up * 1.0f, tone });
					lights++;
				}
			}
			for (int32_t cluster = 0; cluster < 26; cluster++)
			{
				Vec2 local;
				float along = 0.0f;
				float ground = 0.0f;
				const bool ledge = anchor(0.95f, local, ground, along, 0.7f);
				if (ground > (ledge ? LakeWaterLine - 2.2f : -7.5f))
					continue;
				const glm::vec3 tone = rng.Next() < 0.6f ? glowTones[0] : glowTones[3];
				const glm::vec3 shell = rng.Next() < 0.5f ? glm::vec3(0.55f, 0.2f, 0.1f) : glm::vec3(0.42f, 0.12f, 0.3f);
				const int32_t tubes = 4 + int32_t(rng.Next() * 7.0f);
				for (int32_t t = 0; t < tubes; t++)
				{
					const Vec2 at = local + FromAngle(rng.Range(0.0f, 6.28f)) * (0.45f * rng.Next());
					const float base = (ledge ? ground : BedHeight(at)) - 0.1f;
					const float height = rng.Range(0.3f, 1.6f);
					const float radius = rng.Range(0.06f, 0.17f) * (0.7f + 0.4f * height);
					const glm::vec3 lean = glm::vec3(rng.Range(-0.12f, 0.12f), 1.0f, rng.Range(-0.12f, 0.12f));
					const glm::vec3 bottom = world(at, base);
					const glm::vec3 top = bottom + glm::normalize(lean) * height;
					constexpr int32_t Sides = 6;
					const uint32_t still = SwayBits(0.0f, 0.0f, 0.0f);
					for (int32_t s = 0; s < Sides; s++)
					{
						const float a0 = 2.0f * Pi * float(s) / float(Sides);
						const float a1 = 2.0f * Pi * float(s + 1) / float(Sides);
						const glm::vec3 r0(std::cos(a0), 0.0f, std::sin(a0));
						const glm::vec3 r1(std::cos(a1), 0.0f, std::sin(a1));
						const glm::vec3 n = glm::normalize(r0 + r1);
						body.Quad(lit(bottom + r0 * radius * 0.8f, n, shell * 0.55f, still), lit(bottom + r1 * radius * 0.8f, n, shell * 0.55f, still), lit(top + r1 * radius, n, shell, still), lit(top + r0 * radius, n, shell, still));
						body.Quad(lit(top + r0 * radius, -n, shell * 0.3f, still), lit(top + r1 * radius, -n, shell * 0.3f, still), lit(top + r1 * radius * 0.7f - up * 0.08f, -n, shell * 0.15f, still), lit(top + r0 * radius * 0.7f - up * 0.08f, -n, shell * 0.15f, still));
						glow.Quad(shine(top + r0 * radius * 1.01f, n, { 0.0f, 12.0f }, tone, 0.95f, still), shine(top + r1 * radius * 1.01f, n, { 0.0f, 12.0f }, tone, 0.95f, still), shine(top + r1 * radius * 1.01f - up * 0.05f, n, { 0.0f, 12.0f }, tone, 0.0f, still), shine(top + r0 * radius * 1.01f - up * 0.05f, n, { 0.0f, 12.0f }, tone, 0.0f, still));
						glow.Quad(shine(top + r0 * radius * 0.72f - up * 0.07f, up, { 0.0f, 12.0f }, tone, 0.7f, still), shine(top + r1 * radius * 0.72f - up * 0.07f, up, { 0.0f, 12.0f }, tone, 0.7f, still), shine(top - up * 0.07f, up, { 0.0f, 12.0f }, tone, 0.25f, still), shine(top - up * 0.07f, up, { 0.0f, 12.0f }, tone, 0.25f, still));
					}
				}
			}
			for (int32_t anemone = 0; anemone < 30; anemone++)
			{
				Vec2 local;
				float along = 0.0f;
				float ground = 0.0f;
				const bool ledge = anchor(0.9f, local, ground, along, 0.75f);
				if (ground > (ledge ? LakeWaterLine - 2.0f : -7.0f))
					continue;
				const glm::vec3 tone = glowTones[size_t(rng.Next() * 4.99f)];
				const glm::vec3 core = world(local, ground - 0.02f);
				const float phase = rng.Next();
				const float spread = rng.Range(0.6f, 1.3f);
				const int32_t arms = 9 + int32_t(rng.Next() * 7.0f);
				const uint32_t still = SwayBits(0.0f, 0.0f, 0.0f);
				constexpr int32_t Sides = 8;
				for (int32_t s = 0; s < Sides; s++)
				{
					const float a0 = 2.0f * Pi * float(s) / float(Sides);
					const float a1 = 2.0f * Pi * float(s + 1) / float(Sides);
					const glm::vec3 r0(std::cos(a0), 0.0f, std::sin(a0));
					const glm::vec3 r1(std::cos(a1), 0.0f, std::sin(a1));
					body.Quad(lit(core + r0 * spread * 0.32f, r0, glm::vec3(0.16f, 0.07f, 0.16f), still), lit(core + r1 * spread * 0.32f, r1, glm::vec3(0.16f, 0.07f, 0.16f), still), lit(core + r1 * spread * 0.2f + up * spread * 0.32f, glm::normalize(r1 + up), glm::vec3(0.24f, 0.1f, 0.22f), still), lit(core + r0 * spread * 0.2f + up * spread * 0.32f, glm::normalize(r0 + up), glm::vec3(0.24f, 0.1f, 0.22f), still));
					glow.Quad(shine(core + r0 * spread * 0.2f + up * spread * 0.33f, up, { phase, 12.0f }, tone, 0.6f, still), shine(core + r1 * spread * 0.2f + up * spread * 0.33f, up, { phase, 12.0f }, tone, 0.6f, still), shine(core + up * spread * 0.3f, up, { phase, 12.0f }, tone, 0.15f, still), shine(core + up * spread * 0.3f, up, { phase, 12.0f }, tone, 0.15f, still));
				}
				for (int32_t arm = 0; arm < arms; arm++)
				{
					const float a = 2.0f * Pi * (float(arm) + rng.Range(-0.3f, 0.3f)) / float(arms);
					const glm::vec3 outward(std::cos(a), 0.0f, std::sin(a));
					const float length = spread * rng.Range(0.75f, 1.25f);
					std::vector<glm::vec3> spine;
					std::vector<float> widths;
					const glm::vec3 base = core + outward * spread * 0.18f + up * spread * 0.3f;
					for (int32_t k = 0; k <= 6; k++)
					{
						const float t = float(k) / 6.0f;
						spine.push_back(base + outward * (length * t) + up * (length * (0.55f * t - 0.45f * t * t)));
						widths.push_back(0.03f * (1.0f - 0.7f * t) * spread + 0.006f);
					}
					glowStalk(spine, widths, up, tone, 0.12f, 0.95f, std::fmod(phase + 0.03f * float(arm), 1.0f), 0.07f * length);
				}
				if (lights < 8 && anemone % 5 == 0)
				{
					m_Glows.push_back({ core + up * 0.9f, tone });
					lights++;
				}
			}
			for (int32_t group = 0; group < 18; group++)
			{
				Vec2 local;
				float along = 0.0f;
				trenchSpot(0.7f, local, along);
				const float ground = BedHeight(local);
				if (ground > -10.0f)
					continue;
				const float phase = rng.Next();
				const int32_t strands = 3 + int32_t(rng.Next() * 4.0f);
				for (int32_t s = 0; s < strands; s++)
				{
					const Vec2 at = local + FromAngle(rng.Range(0.0f, 6.28f)) * (0.6f * rng.Next());
					const float base = BedHeight(at) - 0.05f;
					const float length = std::min(rng.Range(2.4f, 6.2f), LakeWaterLine - 2.5f - base);
					if (length < 1.0f)
						continue;
					const float a = rng.Range(0.0f, 6.28f);
					const glm::vec3 lean = glm::vec3(std::cos(a), 0.0f, std::sin(a)) * rng.Range(0.05f, 0.18f);
					const int32_t segments = 12;
					const std::vector<glm::vec3> spine = curve(world(at, base), lean, length, segments, 0.1f);
					std::vector<float> widths;
					for (int32_t k = 0; k <= segments; k++)
						widths.push_back(0.05f * (0.4f + 0.6f * std::sin(Pi * std::min(0.12f + float(k) / float(segments), 1.0f))));
					const float amplitude = 0.16f * length;
					const float own = std::fmod(phase + 0.07f * float(s), 1.0f);
					glowStalk(spine, widths, glm::vec3(std::sin(a), 0.0f, -std::cos(a)), glm::vec3(0.45f, 0.82f, 1.0f), 0.04f, 0.2f, own, amplitude);
					for (int32_t k = 3; k <= segments; k += 2)
					{
						const float t = float(k) / float(segments);
						halo(spine[size_t(k)], rng.Range(0.07f, 0.13f), glm::vec3(0.55f, 0.92f, 1.0f), 0.55f + 0.35f * t, SwayBits(t, own, amplitude));
					}
				}
			}
		}

		const auto shelfSpot = [&](Vec2& local, float low, float high) -> bool
		{
			for (int32_t attempt = 0; attempt < 30; attempt++)
			{
				const float a = rng.Range(0.0f, 6.28f);
				const float r = std::sqrt(rng.Next());
				local = Vec2(std::cos(a) * 30.0f * r, std::sin(a) * 19.0f * r);
				const float bed = BedHeight(local);
				if (m_ShoreLevel.Distance(m_Center + local) > -0.4f || bed < low || bed > high)
					continue;
				if (m_Trench.size() >= 2 && TrenchDistance(local) < GorgeHalf(local) + 1.2f)
					continue;
				return true;
			}
			return false;
		};
		const auto tuft = [&](Vec2 local, float size, bool olive)
		{
			const float bed = BedHeight(local);
			const float phase = rng.Next();
			const glm::vec3 base = olive ? glm::vec3(0.08f, 0.2f, 0.06f) : glm::vec3(0.22f, 0.17f, 0.04f);
			const glm::vec3 tip = olive ? glm::vec3(0.34f, 0.58f, 0.16f) : glm::vec3(0.56f, 0.46f, 0.12f);
			const int32_t blades = 6 + int32_t(rng.Next() * 6.0f);
			const float scale = std::min(size * rng.Range(0.7f, 1.2f), (LakeWaterLine - 0.6f - bed) * 0.9f);
			if (scale < 0.12f)
				return;
			for (int32_t b = 0; b < blades; b++)
			{
				const float a = rng.Range(0.0f, 6.28f);
				const Vec2 at = local + FromAngle(a) * (0.22f * rng.Next());
				const glm::vec3 lean = glm::vec3(std::cos(a), 0.0f, std::sin(a)) * rng.Range(0.1f, 0.5f);
				const float length = scale * rng.Range(0.45f, 1.0f);
				const std::vector<glm::vec3> spine = curve(world(at, BedHeight(at) - 0.03f), lean, length, 4, 0.4f);
				std::vector<float> widths;
				for (int32_t k = 0; k <= 4; k++)
					widths.push_back(0.075f * std::pow(1.0f - float(k) / 4.0f, 0.6f) + 0.003f);
				stalk(body, spine, widths, glm::vec3(std::sin(a), 0.0f, -std::cos(a)), base, tip, std::fmod(phase + 0.04f * float(b), 1.0f), 0.12f * length, 0.0f, 1.0f);
			}
		};
		for (int32_t meadow = 0; meadow < 22; meadow++)
		{
			Vec2 centre;
			if (!shelfSpot(centre, -4.2f, LakeWaterLine - 0.8f))
				continue;
			const float radius = rng.Range(1.5f, 3.6f);
			const bool olive = rng.Next() < 0.72f;
			const float size = rng.Range(0.7f, 1.3f);
			const int32_t count = 18 + int32_t(rng.Next() * 18.0f);
			for (int32_t k = 0; k < count; k++)
			{
				const Vec2 local = centre + FromAngle(rng.Range(0.0f, 6.28f)) * (radius * std::sqrt(rng.Next()));
				const float bed = BedHeight(local);
				if (m_ShoreLevel.Distance(m_Center + local) > -0.4f || bed > LakeWaterLine - 0.7f || (m_Trench.size() >= 2 && TrenchDistance(local) < GorgeHalf(local) + 1.0f))
					continue;
				tuft(local, size * (1.0f - 0.4f * Length(local - centre) / radius), olive);
			}
		}
		for (int32_t single = 0; single < 90; single++)
		{
			Vec2 local;
			if (shelfSpot(local, -9.0f, LakeWaterLine - 0.9f))
				tuft(local, rng.Range(0.4f, 0.9f), rng.Next() < 0.7f);
		}
		for (int32_t grove = 0; grove < 10; grove++)
		{
			Vec2 local;
			if (!shelfSpot(local, -7.5f, -3.4f))
				continue;
			bool clear = true;
			for (float u = 0.0f; u < m_RouteLength && clear; u += 1.0f)
				clear = Length(RoutePoint(u) - local) > 6.0f;
			if (!clear)
				continue;
			const int32_t count = 4 + int32_t(rng.Next() * 6.0f);
			for (int32_t k = 0; k < count; k++)
			{
				const Vec2 at = local + FromAngle(rng.Range(0.0f, 6.28f)) * (2.2f * std::sqrt(rng.Next()));
				const float base = BedHeight(at) - 0.05f;
				const float length = std::min(rng.Range(2.2f, 4.6f), LakeWaterLine - 1.5f - base);
				if (length < 1.2f)
					continue;
				const float a = rng.Range(0.0f, 6.28f);
				const glm::vec3 face(std::sin(a), 0.0f, -std::cos(a));
				const glm::vec3 lean = glm::vec3(std::cos(a), 0.0f, std::sin(a)) * rng.Range(0.05f, 0.25f);
				constexpr int32_t Segments = 10;
				const std::vector<glm::vec3> spine = curve(world(at, base), lean, length, Segments, 0.05f);
				const float phase = rng.Next();
				const float amplitude = 0.13f * length;
				const std::vector<float> widths(Segments + 1, 0.028f);
				stalk(body, spine, widths, face, glm::vec3(0.24f, 0.2f, 0.05f), glm::vec3(0.42f, 0.34f, 0.08f), phase, amplitude, 0.0f, 1.0f);
				for (int32_t s = 2; s <= Segments; s++)
				{
					const float t = float(s) / float(Segments);
					const float side = s % 2 == 0 ? 1.0f : -1.0f;
					const float leafAngle = a + side * rng.Range(0.9f, 1.6f);
					const glm::vec3 outward(std::cos(leafAngle), 0.0f, std::sin(leafAngle));
					const float leaf = rng.Range(0.3f, 0.6f) * (1.0f - 0.35f * t);
					std::vector<glm::vec3> blade;
					for (int32_t j = 0; j <= 3; j++)
					{
						const float u = float(j) / 3.0f;
						blade.push_back(spine[size_t(s)] + outward * (leaf * u) + up * (leaf * (0.55f * u - 0.25f * u * u)));
					}
					const std::vector<float> leafWidths = { 0.014f, 0.09f, 0.08f, 0.012f };
					stalk(body, blade, leafWidths, up, glm::vec3(0.36f, 0.3f, 0.07f), glm::vec3(0.6f, 0.48f, 0.12f), phase, amplitude, t, t + 0.06f);
				}
			}
		}
		for (int32_t fan = 0; fan < 60; fan++)
		{
			Vec2 local;
			if (!shelfSpot(local, -10.0f, LakeWaterLine - 0.8f))
				continue;
			const float base = BedHeight(local) - 0.04f;
			const float size = std::min(rng.Range(0.2f, 0.48f), (LakeWaterLine - 0.7f - base) * 0.8f);
			const float a = rng.Range(0.0f, 6.28f);
			const glm::vec3 across(std::cos(a), 0.0f, std::sin(a));
			const glm::vec3 facing(-std::sin(a), 0.0f, std::cos(a));
			const glm::vec3 deep = rng.Next() < 0.7f ? glm::vec3(0.4f, 0.04f, 0.06f) : glm::vec3(0.45f, 0.18f, 0.03f);
			const uint32_t phaseBits = SwayBits(0.0f, rng.Next(), 0.05f);
			const glm::vec3 root = world(local, base);
			constexpr int32_t Ribs = 7;
			for (int32_t r = 0; r < Ribs; r++)
			{
				const float a0 = Pi * (0.12f + 0.76f * float(r) / float(Ribs));
				const float a1 = Pi * (0.12f + 0.76f * float(r + 1) / float(Ribs));
				const float l0 = size * (0.85f + 0.15f * std::sin(float(r) * 2.3f + a));
				const float l1 = size * (0.85f + 0.15f * std::sin(float(r + 1) * 2.3f + a));
				const glm::vec3 p0 = root + (across * std::cos(a0) + up * std::sin(a0)) * l0;
				const glm::vec3 p1 = root + (across * std::cos(a1) + up * std::sin(a1)) * l1;
				const uint32_t tipBits = (phaseBits & ~0xFFu) | 255u;
				triangle(body, lit(root, glm::normalize(facing + up * 0.5f), deep * 0.5f, phaseBits & ~0xFFu), lit(p0, glm::normalize(facing + up * 0.5f), deep, tipBits), lit(p1, glm::normalize(facing + up * 0.5f), deep, tipBits));
			}
		}
		const uint32_t plain = renderer.GetPlainMaterial();
		if (!body.Indices.empty())
		{
			const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(body.Indices.size()), .Material = plain, .Layer = plain, .Flags = MeshVertexTint };
			AddPatches(renderer, body.Vertices, body.Indices, desc, 10.0f, m_Flora);
		}
		if (!glow.Indices.empty())
		{
			const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(glow.Indices.size()), .Material = plain, .Layer = plain, .Flags = MeshUnderGlow };
			AddPatches(renderer, glow.Vertices, glow.Indices, desc, 10.0f, m_FloraGlow);
		}
	}

	void LakeArt::BuildBed(WorldRenderer& renderer, const Level& level)
	{
		const std::optional<uint32_t> rock = renderer.FindMaterialSlot("GroundRock");
		const std::optional<uint32_t> sand = renderer.FindMaterialSlot("GroundSand");
		const int32_t columns = int32_t((float(m_Width) + 2.0f * BedMargin) / BedSpacing) + 1;
		const int32_t rows = int32_t((float(m_Height) + 2.0f * BedMargin) / BedSpacing) + 1;
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(size_t(columns) * size_t(rows));
		const auto at = [&](int32_t x, int32_t y) { return Vec2(float(x) * BedSpacing - BedMargin, float(y) * BedSpacing - BedMargin); };
		for (int32_t y = 0; y < rows; y++)
			for (int32_t x = 0; x < columns; x++)
			{
				const Vec2 p = at(x, y);
				const Vec2 local = p - m_Center;
				const float height = BedHeight(local);
				const float dx = BedHeight(local + Vec2(0.5f, 0.0f)) - BedHeight(local - Vec2(0.5f, 0.0f));
				const float dy = BedHeight(local + Vec2(0.0f, 0.5f)) - BedHeight(local - Vec2(0.0f, 0.5f));
				const float wall = 1.0f - Smooth(LakeTrenchHalf * 0.7f, LakeTrenchHalf * 1.4f, TrenchDistance(local));
				const float blend = Smooth(0.42f, 0.6f, Noise2(p * 0.17f, 0x5A1Du)) * (1.0f - wall);
				vertices.push_back({ .Position = { p.x, height, p.y }, .Normal = glm::normalize(glm::vec3(-dx, 1.0f, -dy)), .Color = Pack(glm::vec3(blend, 1.0f, 1.0f), 1.0f) });
			}
		for (int32_t y = 0; y + 1 < rows; y++)
			for (int32_t x = 0; x + 1 < columns; x++)
			{
				const uint32_t a = uint32_t(y * columns + x);
				const uint32_t b = a + 1;
				const uint32_t c = a + uint32_t(columns);
				const uint32_t d = c + 1;
				indices.insert(indices.end(), { a, b, d, a, d, c });
			}
		const uint32_t material = rock.value_or(renderer.GetPlainMaterial());
		const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(indices.size()), .Material = material, .Layer = sand.value_or(material), .Flags = MeshGround | MeshLayered };
		m_Bed = renderer.AddLevelGeometry(vertices, indices, { desc });

		MeshBuild shafts;
		Rng rng{ level.Seed ^ 0x51AFu };
		for (int32_t i = 0; i < 22; i++)
		{
			const Vec2 p = m_Center + Vec2(rng.Range(-24.0f, 24.0f), rng.Range(-15.0f, 15.0f));
			const float width = rng.Range(0.5f, 1.5f);
			const float alpha = rng.Range(0.05f, 0.14f);
			const glm::vec3 side(0.94f * width, 0.0f, 0.34f * width);
			const glm::vec3 slant(-0.5f, 0.0f, 0.75f);
			const glm::vec3 top(p.x, -0.3f, p.y);
			const glm::vec3 bottom = top + slant * 5.2f + glm::vec3(0.0f, -5.2f, 0.0f);
			const uint32_t faint = Pack(glm::vec3(0.26f, 0.9f, 0.86f), alpha);
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			shafts.Quad(Vertex(top - side, up, { -1.0f, 0.82f }, faint), Vertex(top + side, up, { 1.0f, 0.82f }, faint), Vertex(bottom + side, up, { 1.0f, 0.0f }, faint), Vertex(bottom - side, up, { -1.0f, 0.0f }, faint));
		}
		const LevelMeshDesc shaftDesc{ .FirstIndex = 0, .IndexCount = uint32_t(shafts.Indices.size()), .Material = renderer.GetPlainMaterial(), .Layer = renderer.GetPlainMaterial(), .Flags = MeshUnderGlow };
		m_Shafts = renderer.AddLevelGeometry(shafts.Vertices, shafts.Indices, { shaftDesc });
	}

	void LakeArt::AddItem(const WorldRenderer& renderer, std::string_view name, const glm::mat4& transform, const glm::vec4& tint)
	{
		for (const uint32_t mesh : MeshesNamed(renderer, name))
			m_Items.push_back({ mesh, renderer.MakeModelInstance(mesh, transform, NoPalette, tint) });
	}

	bool LakeArt::Settle(const WorldRenderer& renderer, std::string_view name, glm::mat4& transform, Vec2 local, float sink, bool spanCheck) const
	{
		glm::vec3 low(1e9f);
		glm::vec3 high(-1e9f);
		const ModelLibrary& models = renderer.GetModels();
		for (const uint32_t mesh : MeshesNamed(renderer, name))
		{
			low = glm::min(low, models.GetMesh(mesh).Min);
			high = glm::max(high, models.GetMesh(mesh).Max);
		}
		if (low.x > high.x)
			return false;
		float top = -1e9f;
		float lowest = 1e9f;
		float highest = -1e9f;
		bool overGorge = false;
		const auto sample = [&](Vec2 offset)
		{
			const Vec2 at = local + offset;
			overGorge = overGorge || TrenchDistance(at) < GorgeHalf(at) + 0.35f;
			const float bed = BedHeight(at);
			lowest = std::min(lowest, bed);
			highest = std::max(highest, bed);
		};
		sample(Vec2(0.0f, 0.0f));
		for (int32_t corner = 0; corner < 8; corner++)
		{
			const glm::vec3 c((corner & 1) ? high.x : low.x, (corner & 2) ? high.y : low.y, (corner & 4) ? high.z : low.z);
			top = std::max(top, glm::vec3(transform * glm::vec4(c, 1.0f)).y);
		}
		for (int32_t i = 0; i <= 2; i++)
			for (int32_t j = 0; j <= 2; j++)
			{
				const glm::vec3 c(std::lerp(low.x, high.x, 0.08f + 0.42f * float(i)), low.y, std::lerp(low.z, high.z, 0.08f + 0.42f * float(j)));
				const glm::vec3 world = glm::vec3(transform * glm::vec4(c, 1.0f));
				sample(Vec2(world.x, world.z));
			}
		if (spanCheck && (overGorge || highest - lowest > 1.5f))
			return false;
		const float y = lowest - sink;
		if (y + top > LakeWaterLine - 0.2f)
			return false;
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_Center.x + local.x, y, m_Center.y + local.y)) * transform;
		return true;
	}

	void LakeArt::BuildRuins(WorldRenderer& renderer, const Level& level)
	{
		Rng rng{ level.Seed ^ 0xB1EDu };
		rng.Next();
		const float flip = -m_Layout.Side;
		const auto rest = [&](Vec2 local, float reach)
		{
			float lowest = BedHeight(local);
			for (int32_t k = 0; k < 6; k++)
			{
				const Vec2 at = local + FromAngle(float(k) * 1.047f) * reach;
				if (m_Trench.size() < 2 || TrenchDistance(at) > GorgeHalf(at) + 0.4f)
					lowest = std::min(lowest, BedHeight(at));
			}
			return lowest;
		};
		const auto orient = [](float yaw, float scale, float tiltX, float tiltZ)
		{
			glm::mat4 transform = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			transform = glm::rotate(transform, tiltX, glm::vec3(1.0f, 0.0f, 0.0f));
			transform = glm::rotate(transform, tiltZ, glm::vec3(0.0f, 0.0f, 1.0f));
			return glm::scale(transform, glm::vec3(scale));
		};
		const auto place = [&](std::string_view name, float x, float y, float yaw, float scale, float sink = 0.0f, float tiltX = 0.0f, float tiltZ = 0.0f)
		{
			const Vec2 local(x * flip, y);
			if (TrenchDistance(local) < LakeTrenchHalf + 2.0f)
				return;
			glm::mat4 transform = orient(yaw, scale, tiltX, tiltZ);
			if (Settle(renderer, name, transform, local, sink, true))
				AddItem(renderer, name, transform);
		};
		const char* columns[] = { "Ruins/Column_A", "Ruins/Column_B", "Ruins/Column_C" };
		for (int32_t row = 0; row < 2; row++)
			for (int32_t i = 0; i < 5; i++)
			{
				const float x = -12.0f + float(i) * 3.4f + rng.Range(-0.2f, 0.2f);
				const float y = 1.2f + float(row) * 5.4f;
				if (rng.Next() < 0.2f)
					continue;
				place(columns[int32_t(rng.Next() * 2.99f)], x, y, rng.Range(0.0f, 6.28f), rng.Range(1.5f, 1.9f), 0.2f, rng.Range(-0.08f, 0.08f), rng.Range(-0.08f, 0.08f));
			}
		place("Ruins/Arch_A", -5.0f, 9.3f, 0.2f, 1.9f, 0.3f);
		place("Ruins/Wall_A", -15.5f, 3.4f, 1.5f, 1.8f, 0.3f);
		place("Ruins/Pedestal_A", -5.2f, 3.9f, 0.5f, 2.0f, 0.2f);
		place("Ruins/Rubble_A", -8.8f, 3.8f, 1.1f, 2.0f, 0.1f);
		place("Ruins/Rubble_B", -2.0f, 1.0f, 2.3f, 2.1f, 0.1f);
		place("Citadel/FallenColumn_A", -1.5f, 6.0f, 0.9f, 1.7f, 0.1f);
		place("Citadel/Statue_A", 9.8f, -2.8f, 0.5f, 4.2f, 0.9f, -1.39f, 0.2f);
		place("Ruins/Column_B", 14.2f, 2.8f, 0.0f, 1.8f, 0.2f, 0.1f, -0.05f);
		place("Ruins/Column_A", 3.0f, -6.4f, 2.0f, 1.8f, 0.2f, -0.06f, 0.07f);
		place("Ruins/Column_C", 6.0f, -7.8f, 1.0f, 1.8f, 0.2f);
		place("Ruins/Wall_B", 4.0f, -9.5f, 0.1f, 1.8f, 0.4f);
		place("Ruins/Rubble_A", 5.5f, 1.3f, 0.7f, 2.2f, 0.15f);
		place("Ruins/Rubble_B", 14.0f, -5.0f, 0.3f, 2.4f, 0.15f);
		place("Ruins/Rubble_B", -12.0f, -6.0f, 1.9f, 2.4f, 0.15f);
		place("Citadel/FallenColumn_A", -11.0f, -4.0f, 0.4f, 1.9f, 0.1f);
		place("Ruins/Arch_A", 17.0f, -3.2f, 2.3f, 1.9f, 0.3f, 0.0f, 0.18f);
		if (m_Trench.size() < 2)
			return;
		const auto set = [&](std::string_view name, Vec2 local, float yaw, float scale, float sink, float tiltX = 0.0f, float tiltZ = 0.0f)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_Center.x + local.x, rest(local, 0.7f * scale) - sink, m_Center.y + local.y)) * orient(yaw, scale, tiltX, tiltZ);
			glm::mat4 check = orient(yaw, scale, tiltX, tiltZ);
			if (Settle(renderer, name, check, local, sink, false))
				AddItem(renderer, name, transform);
		};
		const auto span = [&](std::string_view name, Vec2 local, Vec2 out, float yaw, float scale, float sink, float tiltX = 0.0f, float tiltZ = 0.0f)
		{
			for (int32_t attempt = 0; attempt < 14; attempt++)
			{
				glm::mat4 transform = orient(yaw + (attempt % 2 == 1 ? HalfPi : 0.0f), scale, tiltX, tiltZ);
				if (Settle(renderer, name, transform, local + out * (0.9f * float(attempt / 2)), sink, true))
				{
					AddItem(renderer, name, transform);
					return;
				}
			}
		};
		float start = 0.0f;
		for (size_t i = 0; i < m_Trench.size(); i++)
			if (std::abs(m_Trench[i].x) < 25.0f)
			{
				start = m_TrenchAlong[i];
				break;
			}
		const float fractions[] = { 0.15f, 0.42f, 0.62f, 0.8f };
		for (int32_t k = 0; k < 4; k++)
		{
			const float along = start + (m_TrenchLength - 6.0f - start) * fractions[k];
			const Vec2 centre = TrenchPoint(along);
			const Vec2 tangent = Normalize(TrenchPoint(along + 0.5f) - TrenchPoint(along - 0.5f));
			const float edge = k % 2 == 0 ? 1.0f : -1.0f;
			const Vec2 out = Vec2(-tangent.y, tangent.x) * edge;
			const Vec2 lip = centre + out * (GorgeHalf(centre, along) + 0.7f);
			const Vec2 into = -out;
			const float facing = std::atan2(into.x, into.y);
			const float along_ = std::atan2(tangent.x, tangent.y);
			switch (k)
			{
			case 0:
				for (int32_t step = 0; step < 6; step++)
					set("Ruins/Pedestal_A", lip + out * (0.2f + 0.25f * float(step % 2)) + tangent * (1.5f * (float(step) - 2.5f)), facing + rng.Range(-0.1f, 0.1f), 1.55f, 0.15f + 0.1f * float(step % 3), rng.Range(-0.08f, 0.08f), rng.Range(-0.08f, 0.08f));
				set("Ruins/Column_C", lip - out * 0.2f + tangent * 2.4f, rng.Range(0.0f, 6.28f), 1.7f, 0.2f, 0.06f, 0.0f);
				break;
			case 1:
				span("Ruins/Arch_A", lip - into * 2.2f, out, along_, 1.9f, 0.3f, 0.0f, 0.06f);
				set("Ruins/Rubble_B", lip + into * 2.2f, rng.Range(0.0f, 6.28f), 2.2f, 0.1f);
				set("Ruins/Column_B", lip + into * 3.6f + tangent * 1.5f, facing, 1.7f, 0.4f, 1.35f, 0.1f);
				break;
			case 2:
				span("Citadel/Statue_A", lip + out * 1.8f, out, facing, 3.8f, 0.7f, -1.15f, 0.25f);
				set("Ruins/Rubble_A", lip - into * 0.8f + tangent * 1.8f, rng.Range(0.0f, 6.28f), 2.0f, 0.1f);
				break;
			default:
				span("Ruins/Wall_A", lip + out * 1.2f, out, along_, 1.8f, 0.6f, 0.3f, 0.08f);
				set("Citadel/FallenColumn_A", lip - into * 0.4f + tangent * 2.6f, facing, 1.8f, 0.0f, 0.3f, 0.0f);
				set("Ruins/Column_A", lip + into * 4.6f - tangent * 1.0f, facing + 0.4f, 1.7f, 0.5f, 1.4f, -0.2f);
				break;
			}
		}
	}

	void LakeArt::BuildFeatures(WorldRenderer& renderer, const Level& level)
	{
		std::map<std::pair<int32_t, int32_t>, MeshBuild> builds;
		std::map<std::pair<int32_t, int32_t>, glm::vec3> pivots;
		const auto buildFor = [&](Vec2 p) -> MeshBuild*
		{
			const CellCoord cell = CellAt(p);
			if (!IsBrawlFloor(level.Map.GetTerrain(cell.X, cell.Y)))
				return nullptr;
			const CellCoord chunk = ChunkOf(cell);
			return &builds[{ chunk.X, chunk.Y }];
		};
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		Rng rng{ level.Seed ^ 0xC2A5u };
		for (int32_t cloud = 0; cloud < 52; cloud++)
		{
			const Vec2 hub = m_Center + Vec2(rng.Range(-21.0f, 21.0f), rng.Range(-12.0f, 12.0f));
			const float spread = rng.Range(0.5f, 1.7f);
			const int32_t count = 10 + int32_t(rng.Next() * 34.0f);
			const float layer = rng.Range(0.08f, 0.9f);
			for (int32_t k = 0; k < count; k++)
			{
				const Vec2 p = hub + FromAngle(rng.Range(0.0f, 6.28f)) * (spread * rng.Next() * rng.Next());
				MeshBuild* build = buildFor(p);
				if (!build)
					continue;
				const float r = rng.Range(0.012f, 0.06f);
				const float y = -layer - rng.Range(0.0f, 0.35f);
				const uint32_t color = Pack(glm::vec3(0.84f, 0.96f, 1.0f), rng.Range(0.16f, 0.44f));
				const uint32_t first = uint32_t(build->Vertices.size());
				build->Vertices.push_back(Vertex({ p.x, y, p.y }, up, { 0.0f, 3.0f }, color));
				constexpr int32_t Sides = 7;
				for (int32_t s = 0; s < Sides; s++)
				{
					const float a = 6.2832f * float(s) / float(Sides);
					build->Vertices.push_back(Vertex({ p.x + std::cos(a) * r, y, p.y + std::sin(a) * r * 0.8f }, up, { std::cos(a), std::sin(a) + 3.0f }, color));
				}
				for (int32_t s = 0; s < Sides; s++)
					build->Indices.insert(build->Indices.end(), { first, first + 1 + uint32_t(s), first + 1 + uint32_t((s + 1) % Sides) });
			}
		}
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<LevelMeshDesc> meshes;
		std::vector<std::pair<CellCoord, std::pair<Vec2, Vec2>>> extra;
		const uint32_t plain = renderer.GetPlainMaterial();
		for (auto& [key, build] : builds)
		{
			const uint32_t baseVertex = uint32_t(vertices.size());
			const uint32_t firstIndex = uint32_t(indices.size());
			Vec2 low(1e9f, 1e9f);
			Vec2 high(-1e9f, -1e9f);
			for (const ModelVertex& vertex : build.Vertices)
			{
				low = { std::min(low.x, vertex.Position.x), std::min(low.y, vertex.Position.z) };
				high = { std::max(high.x, vertex.Position.x), std::max(high.y, vertex.Position.z) };
				vertices.push_back(vertex);
			}
			for (const uint32_t index : build.Indices)
				indices.push_back(baseVertex + index);
			meshes.push_back({ .FirstIndex = firstIndex, .IndexCount = uint32_t(build.Indices.size()), .Material = plain, .Layer = plain, .Flags = MeshUnderGlow });
			extra.push_back({ CellCoord{ key.first, key.second }, { low, high } });
		}
		if (meshes.empty())
			return;
		const uint32_t base = renderer.AddLevelGeometry(vertices, indices, meshes);
		for (size_t i = 0; i < extra.size(); i++)
			m_Features.push_back({ .Chunk = extra[i].first, .Mesh = base + uint32_t(i), .Pivot = glm::vec3(0.5f * (extra[i].second.first.x + extra[i].second.second.x), 0.0f, 0.5f * (extra[i].second.first.y + extra[i].second.second.y)), .Min = extra[i].second.first, .Max = extra[i].second.second });
	}

	void LakeArt::BuildCreatures(const Level& level)
	{
		Rng rng{ level.Seed ^ 0xF15Eu };
		const glm::vec3 tones[] = { { 0.2f, 1.0f, 0.95f }, { 0.35f, 0.85f, 1.0f }, { 0.15f, 1.0f, 0.75f } };
		for (int32_t i = 0; i < 6; i++)
		{
			School school;
			for (int32_t attempt = 0; attempt < 12; attempt++)
			{
				school.Center = m_Center + Vec2(rng.Range(-16.0f, 16.0f), rng.Range(-8.0f, 8.0f));
				if (TrenchDistance(school.Center - m_Center) > LakeTrenchHalf + 3.5f)
					break;
			}
			school.Depth = rng.Range(1.6f, 3.2f);
			school.Radius = rng.Range(4.0f, 9.5f);
			school.Speed = rng.Range(0.09f, 0.16f) * (rng.Next() < 0.5f ? -1.0f : 1.0f);
			school.Phase = rng.Range(0.0f, 6.28f);
			school.Size = rng.Range(0.34f, 0.5f);
			school.First = uint32_t(m_FishSlots.size());
			school.Count = 22 + uint32_t(rng.Next() * 22.0f);
			school.Color = tones[i % 3];
			for (uint32_t k = 0; k < school.Count; k++)
				m_FishSlots.push_back(glm::vec4(rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(0.0f, 6.28f)));
			m_Schools.push_back(school);
		}
		for (int32_t i = 0; i < 12; i++)
			m_Jellies.push_back({ glm::vec3(m_Center.x + rng.Range(-22.0f, 22.0f), -rng.Range(1.4f, 3.8f), m_Center.y + rng.Range(-13.0f, 13.0f)), rng.Range(0.55f, 1.1f), rng.Range(0.0f, 6.28f), i % 3 == 0 ? glm::vec3(0.6f, 0.45f, 1.0f) : glm::vec3(0.25f, 0.95f, 1.0f) });
	}

	void LakeArt::BuildShore(WorldRenderer& renderer, const Level& level)
	{
		Rng rng{ level.Seed ^ 0x6A7Eu };
		float side = 1.0f;
		for (const PlacedProp& prop : level.Props)
			if (prop.Id == PropId::GraveSkull)
				side = prop.Position.x < m_Center.x ? 1.0f : -1.0f;
		const auto above = [&](std::string_view name, const glm::vec3& at, float yaw, float scale, float tiltX = 0.0f, float tiltZ = 0.0f, bool under = true, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f))
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_Center.x + at.x, at.y, m_Center.y + at.z));
			transform = glm::rotate(transform, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			transform = glm::rotate(transform, tiltX, glm::vec3(1.0f, 0.0f, 0.0f));
			transform = glm::rotate(transform, tiltZ, glm::vec3(0.0f, 0.0f, 1.0f));
			transform = glm::scale(transform, glm::vec3(scale));
			for (const uint32_t mesh : MeshesNamed(renderer, name))
			{
				const ModelInstance instance = renderer.MakeModelInstance(mesh, transform, NoPalette, tint, 0.0f, at.y);
				m_Above.push_back({ mesh, instance });
				if (under && renderer.GetModels().GetMesh(mesh).Transmission <= 0.0f)
					m_Items.push_back({ mesh, instance });
			}
		};
		const float graveX = side * LakeGraveX;
		const glm::vec3 ribAt(graveX, LakeWaterLine + 0.3f, LakeGraveZ + 1.1f);
		const glm::vec3 skullAt(graveX - side * 9.0f, LakeWaterLine + 1.95f, LakeGraveZ + LakeSkullZ);
		above("Leviathan/Ribcage", ribAt, side > 0.0f ? -HalfPi - 0.07f : HalfPi + 0.07f, LakeGraveScale, 0.0f, 0.05f * side, true, glm::vec4(1.55f, 1.5f, 1.4f, 0.0f));
		above("Leviathan/RibIce", ribAt, side > 0.0f ? -HalfPi - 0.07f : HalfPi + 0.07f, LakeGraveScale, 0.0f, 0.05f * side, false);
		above("Leviathan/Skull", skullAt, side > 0.0f ? 2.55f : -2.55f, LakeSkullScale, 0.05f, 0.13f * side, true, glm::vec4(1.55f, 1.5f, 1.4f, 0.0f));
		above("Leviathan/SkullIce", skullAt, side > 0.0f ? 2.55f : -2.55f, LakeSkullScale, 0.05f, 0.13f * side, false);
		const char* slabs[] = { "Abyss/IceSlab_A", "Abyss/IceSlab_B", "Abyss/IceSlab_C" };
		for (const PlacedProp& prop : level.Props)
			if (prop.Id == PropId::GraveSlab || prop.Id == PropId::GraveShard)
			{
				const Vec2 local = prop.Position - m_Center;
				const int32_t hx = int32_t(prop.Position.x * 16.0f);
				const int32_t hy = int32_t(prop.Position.y * 16.0f);
				const float tiltA = (CollapseNoise(hx, hy, 0x7A11u) - 0.5f) * 0.8f;
				const float tiltB = (CollapseNoise(hx, hy, 0x7A23u) - 0.5f) * 0.8f;
				const float scale = 0.85f + 0.3f * CollapseNoise(hx, hy, 0x7A37u);
				above(prop.Id == PropId::GraveShard ? slabs[2] : slabs[CollapseNoise(hx, hy, 0x7A41u) < 0.5f ? 0 : 1], { local.x, LakeWaterLine + 0.02f, local.y }, prop.Yaw, scale * 0.66f, tiltA * 1.3f, tiltB * 1.3f);
			}
		const char* seracs[] = { "Abyss/Serac_A", "Abyss/Serac_B", "Abyss/Serac_C" };
		for (int32_t i = 0; i < 15; i++)
		{
			const float x = -40.0f + float(i) * 5.8f + rng.Range(-1.4f, 1.4f);
			const float bend = (x / 40.0f) * (x / 40.0f);
			const float serZ = 25.0f + rng.Range(-1.0f, 2.2f) - 7.0f * bend;
			above(seracs[i % 3], { x, m_ShoreLevel.Height(m_Center + Vec2(x, serZ)) - 1.2f, serZ }, rng.Range(-0.5f, 0.5f), rng.Range(0.62f, 1.02f), 0.0f, 0.0f, false);
		}
		above("Abyss/IceFall", { -side * 3.5f, m_ShoreLevel.Height(m_Center + Vec2(-side * 3.5f, 22.6f)) - 1.0f, 22.6f }, 0.0f, 1.25f, 0.0f, 0.0f, false);
		above("Abyss/IceFall", { -side * 18.0f, m_ShoreLevel.Height(m_Center + Vec2(-side * 18.0f, 21.5f)) - 1.0f, 21.5f }, 0.15f, 1.0f, 0.0f, 0.0f, false);

		std::vector<std::vector<uint32_t>> kinds;
		for (const char* name : { "Abyss/Floe_A", "Abyss/Floe_B", "Abyss/Floe_C", "Abyss/Floe_D" })
			kinds.push_back(MeshesNamed(renderer, name));
		if (!kinds[0].empty())
			for (int32_t i = 0; i < 74; i++)
			{
				const float a = rng.Range(0.0f, 6.28f);
				const float r = std::sqrt(rng.Range(0.05f, 1.0f));
				const Vec2 p = m_Center + Vec2(std::cos(a) * 25.5f * r, std::sin(a) * 15.2f * r);
				const Vec2 pier = m_Layout.PierRoot();
				if (Length(p - m_Center - m_Layout.Boat()) < 3.2f || (std::abs(p.x - m_Center.x - pier.x) < 2.8f && p.y - m_Center.y > pier.y - 9.0f))
					continue;
				const int32_t tx = std::clamp(int32_t(p.x), 0, m_Width - 1);
				const int32_t ty = std::clamp(int32_t(p.y), 0, m_Height - 1);
				const float pick = rng.Next();
				const std::vector<uint32_t>& kind = kinds[pick < 0.06f ? 0 : pick < 0.2f ? 1 : pick < 0.5f ? 2 : 3];
				if (!kind.empty())
					m_Floes.push_back({ kind[0], p, rng.Range(0.0f, 6.28f), rng.Range(0.42f, 0.8f), rng.Range(0.0f, 6.28f), IsBrawlFloor(level.Map.GetTerrain(tx, ty)) ? ty * m_Width + tx : -1 });
			}
	}

	void LakeShore::Build(const Level& level)
	{
		const TileMap& map = level.Map;
		m_Width = map.GetWidth();
		m_Height = map.GetHeight();
		const size_t count = size_t(m_Width) * size_t(m_Height);
		const auto isLake = [&](int32_t x, int32_t y)
		{
			const TerrainId terrain = map.GetTerrain(x, y);
			return IsBrawlFloor(terrain) || IsHazardLiquid(terrain);
		};
		const auto measure = [&](bool towardLake)
		{
			std::vector<float> field(count, 1e6f);
			for (int32_t y = 0; y < m_Height; y++)
				for (int32_t x = 0; x < m_Width; x++)
					if (isLake(x, y) == towardLake)
						field[size_t(y) * size_t(m_Width) + size_t(x)] = 0.0f;
			const auto relax = [&](int32_t x, int32_t y, int32_t dx, int32_t dy, float cost)
			{
				if (x + dx >= 0 && y + dy >= 0 && x + dx < m_Width && y + dy < m_Height)
					field[size_t(y) * size_t(m_Width) + size_t(x)] = std::min(field[size_t(y) * size_t(m_Width) + size_t(x)], field[size_t(y + dy) * size_t(m_Width) + size_t(x + dx)] + cost);
			};
			for (int32_t y = 0; y < m_Height; y++)
				for (int32_t x = 0; x < m_Width; x++)
				{
					relax(x, y, -1, 0, 1.0f);
					relax(x, y, 0, -1, 1.0f);
					relax(x, y, -1, -1, 1.414f);
					relax(x, y, 1, -1, 1.414f);
				}
			for (int32_t y = m_Height - 1; y >= 0; y--)
				for (int32_t x = m_Width - 1; x >= 0; x--)
				{
					relax(x, y, 1, 0, 1.0f);
					relax(x, y, 0, 1, 1.0f);
					relax(x, y, 1, 1, 1.414f);
					relax(x, y, -1, 1, 1.414f);
				}
			return field;
		};
		const std::vector<float> toLake = measure(true);
		const std::vector<float> toLand = measure(false);
		m_Distance.resize(count);
		for (size_t i = 0; i < count; i++)
			m_Distance[i] = toLake[i] > 0.0f ? toLake[i] - 0.5f : 0.5f - toLand[i];
		std::vector<float> blurred(count);
		for (int32_t pass = 0; pass < 4; pass++)
		{
			for (int32_t y = 0; y < m_Height; y++)
				for (int32_t x = 0; x < m_Width; x++)
				{
					float sum = 0.0f;
					for (int32_t dy = -1; dy <= 1; dy++)
						for (int32_t dx = -1; dx <= 1; dx++)
							sum += m_Distance[size_t(std::clamp(y + dy, 0, m_Height - 1)) * size_t(m_Width) + size_t(std::clamp(x + dx, 0, m_Width - 1))];
					blurred[size_t(y) * size_t(m_Width) + size_t(x)] = sum / 9.0f;
				}
			m_Distance.swap(blurred);
		}
	}

	float LakeShore::Distance(Vec2 position) const
	{
		if (m_Distance.empty())
			return 1e3f;
		const float px = std::clamp(position.x - 0.5f, 0.0f, float(m_Width - 1));
		const float py = std::clamp(position.y - 0.5f, 0.0f, float(m_Height - 1));
		const int32_t x = std::min(int32_t(px), m_Width - 2);
		const int32_t y = std::min(int32_t(py), m_Height - 2);
		const float u = px - float(x);
		const float v = py - float(y);
		const auto at = [&](int32_t ax, int32_t ay) { return m_Distance[size_t(ay) * size_t(m_Width) + size_t(ax)]; };
		const float inside = std::lerp(std::lerp(at(x, y), at(x + 1, y), u), std::lerp(at(x, y + 1), at(x + 1, y + 1), u), v);
		return inside < 0.0f ? inside : inside + Length(Vec2(position.x - 0.5f - px, position.y - 0.5f - py));
	}

	float LakeArt::BankRim(Vec2 p) const
	{
		const Vec2 offset = p - (m_Center + Vec2(m_GraveSide * LakeBankX, LakeBankZ));
		const float metric = std::sqrt(offset.x * offset.x / (LakeBankHalfX * LakeBankHalfX) + offset.y * offset.y / (LakeBankHalfZ * LakeBankHalfZ));
		const float broad = Noise2(p * 0.31f, 0xF113u);
		const float fine = Noise2(p * 1.27f, 0xF219u);
		const float crumbs = Noise2(p * 3.3f, 0xF31Du);
		return (1.0f - metric) * LakeBankHalfZ + (broad - 0.5f) * 1.4f + (fine - 0.5f) * 0.42f + (crumbs - 0.5f) * 0.12f;
	}

	float LakeShore::Edge(Vec2 p) const
	{
		const float broad = Noise2(p * 0.13f, 0xE101u);
		const float mid = Noise2(p * 0.41f, 0xE203u);
		const float fine = Noise2(p * 1.1f, 0xE307u);
		return Distance(p) + 0.1f + (broad - 0.5f) * 3.2f + (mid - 0.5f) * 1.1f + (fine - 0.5f) * 0.3f;
	}

	void LakeArt::BuildShoreIce(WorldRenderer& renderer, const Level& level)
	{
		const std::optional<uint32_t> snow = renderer.FindMaterialSlot("GroundSnow");
		if (!snow)
			return;
		constexpr float Cell = 0.3f;
		constexpr float Reach = 14.0f;
		const int32_t nx = int32_t((float(m_Width) + 2.0f * Reach) / Cell) + 1;
		const int32_t nz = int32_t((float(m_Height) + 2.0f * Reach) / Cell) + 1;
		const auto pointOf = [&](int32_t i, int32_t j) { return Vec2(float(i) * Cell - Reach, float(j) * Cell - Reach); };
		const Vec2 pier = m_Layout.PierRoot();
		const Vec2 boat = m_Layout.Boat();
		const auto plates = [&](Vec2 p)
		{
			const Vec2 scaled = p * (1.0f / 1.7f);
			const int32_t cx = int32_t(std::floor(scaled.x));
			const int32_t cy = int32_t(std::floor(scaled.y));
			float nearest = 1e9f;
			float second = 1e9f;
			for (int32_t dy = -1; dy <= 1; dy++)
				for (int32_t dx = -1; dx <= 1; dx++)
				{
					const Vec2 spot(float(cx + dx) + CollapseNoise(cx + dx, cy + dy, 0xB3C1u), float(cy + dy) + CollapseNoise(cx + dx, cy + dy, 0xB3D7u));
					const float d = Length(scaled - spot);
					if (d < nearest)
					{
						second = nearest;
						nearest = d;
					}
					else if (d < second)
						second = d;
				}
			return (second - nearest) * 1.7f;
		};
		std::vector<float> field(size_t(nx) * size_t(nz), 1.0f);
		std::vector<float> edges(size_t(nx) * size_t(nz), 0.0f);
		for (int32_t j = 0; j < nz; j++)
			for (int32_t i = 0; i < nx; i++)
			{
				const Vec2 p = pointOf(i, j);
				const float e = std::max(m_ShoreLevel.Edge(p), BankRim(p));
				edges[size_t(j) * size_t(nx) + size_t(i)] = e;
				if (e > 1.0f || e < -4.5f)
					continue;
				const Vec2 local = p - m_Center;
				const float width = 0.45f + 2.6f * std::pow(Noise2(p * 0.16f, 0xB101u), 1.7f) + 0.45f * (Noise2(p * 0.55f, 0xB203u) - 0.5f);
				float band = std::max(e - 0.5f, -(e + width));
				if (m_Trench.size() >= 2)
					band = std::max(band, GorgeHalf(local) + 1.2f - TrenchDistance(local));
				const int32_t tx = int32_t(std::floor(p.x));
				const int32_t ty = int32_t(std::floor(p.y));
				if (tx >= -1 && ty >= -1 && tx <= m_Width && ty <= m_Height)
					for (int32_t dy = -1; dy <= 1; dy++)
						for (int32_t dx = -1; dx <= 1; dx++)
						{
							const int32_t sx = std::clamp(tx + dx, 0, m_Width - 1);
							const int32_t sy = std::clamp(ty + dy, 0, m_Height - 1);
							if (IsBrawlFloor(level.Map.GetTerrain(sx, sy)))
								band = std::max(band, 0.3f);
						}
				if (std::abs(local.x - pier.x) < 1.7f && local.y > pier.y - 10.0f)
					band = std::max(band, 0.3f);
				if (Length(local - boat) < 2.8f)
					band = std::max(band, 0.3f);
				if (e < -0.3f)
					band = std::max(band, 0.035f - plates(p));
				field[size_t(j) * size_t(nx) + size_t(i)] = band;
			}
		MeshBuild ice;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		const float surface = LakeWaterLine + 0.035f;
		const auto value = [&](int32_t i, int32_t j) { return field[size_t(j) * size_t(nx) + size_t(i)]; };
		const auto vertexAt = [&](Vec2 p, float e)
		{
			const float drift = Smooth(-1.8f, 0.4f, e) * (0.32f + 0.58f * Noise2(p * 0.7f, 0xB405u));
			const float lift = 0.012f * (Noise2(p * 2.3f, 0xB507u) - 0.5f);
			return Vertex({ p.x, surface + lift, p.y }, up, { 0.0f, 0.0f }, Pack(glm::vec3(1.0f), drift));
		};
		for (int32_t j = 0; j + 1 < nz; j++)
			for (int32_t i = 0; i + 1 < nx; i++)
			{
				const int32_t ci[4] = { i, i + 1, i + 1, i };
				const int32_t cj[4] = { j, j, j + 1, j + 1 };
				float v[4];
				bool any = false;
				bool all = true;
				for (int32_t k = 0; k < 4; k++)
				{
					v[k] = value(ci[k], cj[k]);
					any = any || v[k] < 0.0f;
					all = all && v[k] < 0.0f;
				}
				if (!any)
					continue;
				std::vector<Vec2> polygon;
				std::vector<float> polyEdge;
				std::vector<bool> crossing;
				for (int32_t k = 0; k < 4; k++)
				{
					const int32_t n = (k + 1) % 4;
					const Vec2 a = pointOf(ci[k], cj[k]);
					const Vec2 b = pointOf(ci[n], cj[n]);
					const float ea = edges[size_t(cj[k]) * size_t(nx) + size_t(ci[k])];
					const float eb = edges[size_t(cj[n]) * size_t(nx) + size_t(ci[n])];
					if (v[k] < 0.0f)
					{
						polygon.push_back(a);
						polyEdge.push_back(ea);
						crossing.push_back(false);
					}
					if ((v[k] < 0.0f) != (v[n] < 0.0f))
					{
						const float t = v[k] / (v[k] - v[n]);
						polygon.push_back(a + (b - a) * t);
						polyEdge.push_back(std::lerp(ea, eb, t));
						crossing.push_back(true);
					}
				}
				const uint32_t base = uint32_t(ice.Vertices.size());
				for (size_t k = 0; k < polygon.size(); k++)
					ice.Vertices.push_back(vertexAt(polygon[k], polyEdge[k]));
				for (uint32_t k = 1; k + 1 < uint32_t(polygon.size()); k++)
					ice.Indices.insert(ice.Indices.end(), { base, base + k + 1, base + k });
				if (all)
					continue;
				const float gx = (v[1] + v[2] - v[0] - v[3]) * 0.5f;
				const float gz = (v[2] + v[3] - v[0] - v[1]) * 0.5f;
				const float length = std::sqrt(gx * gx + gz * gz);
				const glm::vec3 outward = length > 1e-6f ? glm::vec3(gx / length, 0.0f, gz / length) : glm::vec3(1.0f, 0.0f, 0.0f);
				for (size_t k = 0; k < polygon.size(); k++)
				{
					const size_t n = (k + 1) % polygon.size();
					if (!crossing[k] || !crossing[n])
						continue;
					const glm::vec3 a(polygon[k].x, surface, polygon[k].y);
					const glm::vec3 b(polygon[n].x, surface, polygon[n].y);
					const glm::vec3 drop(0.0f, -0.1f, 0.0f);
					const uint32_t color = Pack(glm::vec3(1.0f), 0.0f);
					ice.Quad(Vertex(a, outward, { 0.0f, 0.0f }, color), Vertex(b, outward, { 0.0f, 0.0f }, color), Vertex(b + drop, outward, { 0.0f, 0.0f }, color), Vertex(a + drop, outward, { 0.0f, 0.0f }, color));
				}
			}
		if (ice.Indices.empty())
			return;
		const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(ice.Indices.size()), .Material = *snow, .Layer = *snow, .Flags = MeshVertexTint | MeshIce | MeshWet };
		AddPatches(renderer, ice.Vertices, ice.Indices, desc, 12.0f, m_ShoreIce);
	}

	float LakeShore::Floor(Vec2 p) const
	{
		const float broad = Noise2(p * 0.13f, 0xE101u);
		const float mid = Noise2(p * 0.41f, 0xE203u);
		const float fine = Noise2(p * 1.1f, 0xE307u);
		const float e = Distance(p) + 0.1f + (broad - 0.5f) * 3.2f + (mid - 0.5f) * 1.1f + (fine - 0.5f) * 0.3f;
		return e < 0.0f ? LakeWaterLine + e * 0.9f : Height(p);
	}

	float LakeShore::Height(Vec2 p) const
	{
		const float broad = Noise2(p * 0.13f, 0xE101u);
		const float mid = Noise2(p * 0.41f, 0xE203u);
		const float fine = Noise2(p * 1.1f, 0xE307u);
		const float e = Distance(p) + 0.1f + (broad - 0.5f) * 3.2f + (mid - 0.5f) * 1.1f + (fine - 0.5f) * 0.3f;
		if (e < 0.0f)
			return LakeWaterLine + std::max(e * 0.8f, -2.4f);
		const float bank = Smooth(0.0f, 1.4f, e) * (0.75f + 0.7f * mid);
		const float rise = Smooth(1.0f, 16.0f, e) * (2.4f + 3.6f * broad);
		const float far = Smooth(14.0f, 40.0f, e) * (6.0f + 6.0f * broad);
		return LakeWaterLine + 0.02f + bank + rise + far + (fine - 0.5f) * 0.25f * Smooth(0.0f, 2.0f, e);
	}

	void LakeArt::BuildLand(WorldRenderer& renderer, const Level& level)
	{
		const std::optional<uint32_t> snow = renderer.FindMaterialSlot("GroundSnow");
		std::optional<uint32_t> rock = renderer.FindMaterialSlot("GroundFrostRock");
		if (!rock)
			rock = renderer.FindMaterialSlot("GroundRock");
		if (!snow || !rock)
			return;
		const LakeShore& shore = m_ShoreLevel;
		constexpr float Margin = 54.0f;
		constexpr float Spacing = 1.25f;
		const int32_t columns = int32_t((float(m_Width) + 2.0f * Margin) / Spacing) + 1;
		const int32_t rows = int32_t((float(m_Height) + 2.0f * Margin) / Spacing) + 1;
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(size_t(columns) * size_t(rows));
		const bool walled = !m_Walls.empty();
		const auto ground = [&](Vec2 p)
		{
			const float height = LandHeight(p);
			if (!walled)
				return height;
			const Vec2 local = p - m_Center;
			const float half = GorgeHalf(local);
			const float cut = 1.0f - Smooth(half + 1.45f, half + 1.85f, TrenchDistance(local));
			return std::lerp(height, std::min(height, LakeWaterLine - 1.5f), cut);
		};
		for (int32_t y = 0; y < rows; y++)
			for (int32_t x = 0; x < columns; x++)
			{
				const Vec2 p(float(x) * Spacing - Margin, float(y) * Spacing - Margin);
				const float hx = ground(p + Vec2(0.62f, 0.0f)) - ground(p - Vec2(0.62f, 0.0f));
				const float hy = ground(p + Vec2(0.0f, 0.62f)) - ground(p - Vec2(0.0f, 0.62f));
				const glm::vec3 normal = glm::normalize(glm::vec3(-hx, 1.24f, -hy));
				const float distance = shore.Distance(p);
				const float steep = Smooth(0.72f, 0.42f, normal.y);
				const float beach = (1.0f - Smooth(0.0f, 2.6f, distance + (Noise2(p * 0.37f, 0xE411u) - 0.5f) * 3.0f)) * 0.36f;
				const float outcrop = Smooth(0.62f, 0.76f, Noise2(p * 0.19f, 0xE523u)) * 0.56f;
				const Vec2 local = p - m_Center;
				const float gorgeRock = walled ? Smooth(24.0f, 31.0f, std::abs(local.x)) * (local.x * m_Layout.Side < 0.0f ? 1.0f : 0.0f) * (1.0f - Smooth(GorgeHalf(local) + 2.5f, GorgeHalf(local) + 9.0f, TrenchDistance(local))) * (0.3f + 0.45f * Noise2(p * 0.45f, 0xE83Bu)) : 0.0f;
				const float rocky = std::clamp(steep + beach + outcrop + gorgeRock, 0.0f, 1.0f);
				const float fade = 1.0f - Smooth(24.0f, 46.0f, distance);
				vertices.push_back({ .Position = { p.x, ground(p), p.y }, .Normal = normal, .Color = Pack(glm::vec3(fade, rocky, 1.0f), 1.0f) });
			}
		indices.reserve(size_t(columns) * size_t(rows) * 6);
		for (int32_t y = 0; y + 1 < rows; y++)
			for (int32_t x = 0; x + 1 < columns; x++)
			{
				const uint32_t a = uint32_t(y * columns + x);
				const uint32_t b = a + 1;
				const uint32_t c = a + uint32_t(columns);
				const uint32_t d = c + 1;
				indices.insert(indices.end(), { a, b, d, a, d, c });
			}
		const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(indices.size()), .Material = *snow, .Layer = *rock, .Flags = MeshVertexTint | MeshIce | MeshGround };
		m_Land = renderer.AddLevelGeometry(vertices, indices, { desc });
	}

	void LakeArt::BuildBank(WorldRenderer& renderer, const Level& level)
	{
		const std::optional<uint32_t> snow = renderer.FindMaterialSlot("GroundSnow");
		if (!snow)
			return;
		float side = 1.0f;
		for (const PlacedProp& prop : level.Props)
			if (prop.Id == PropId::GraveSkull)
				side = prop.Position.x < m_Center.x ? 1.0f : -1.0f;
		const Vec2 centre = m_Center + Vec2(side * LakeBankX, LakeBankZ);
		const Vec2 grave = m_Center + Vec2(side * LakeGraveX, LakeGraveZ);
		const Vec2 skull = grave + Vec2(-side * 9.0f, LakeSkullZ);
		const auto height = [&](Vec2 p)
		{
			const float broad = Noise2(p * 0.31f, 0xF113u);
			const float fine = Noise2(p * 1.27f, 0xF219u);
			const float crumbs = Noise2(p * 3.3f, 0xF31Du);
			const float rim = BankRim(p);
			if (rim < 0.0f)
				return LakeWaterLine + std::max(rim * 0.55f, -1.6f);
			float top = Smooth(0.0f, 0.6f, rim) * (0.3f + 0.2f * fine) + Smooth(0.4f, 3.0f, rim) * (0.14f + 0.3f * broad) + (crumbs - 0.5f) * 0.05f;
			const float along = std::abs(p.y - grave.y);
			const float ribs = std::abs(p.x - grave.x) < 7.2f ? std::exp(-(along - 2.7f) * (along - 2.7f) / 0.7f) * (0.16f + 0.16f * fine) : 0.0f;
			const float head = std::exp(-LengthSquared(p - skull) / 9.0f) * (0.22f + 0.2f * broad);
			return LakeWaterLine + 0.03f + top + ribs + head;
		};
		const auto drift = [&](Vec2 p)
		{
			const float rim = BankRim(p) + (Noise2(p * 0.9f, 0xF52Bu) - 0.5f) * 0.9f + (Noise2(p * 2.7f, 0xF63Du) - 0.5f) * 0.3f;
			return Smooth(0.15f, 1.3f, rim);
		};
		constexpr float Spacing = 0.36f;
		const float reachX = LakeBankHalfX + 3.0f;
		const float reachZ = LakeBankHalfZ + 3.0f;
		const int32_t columns = int32_t(2.0f * reachX / Spacing) + 1;
		const int32_t rows = int32_t(2.0f * reachZ / Spacing) + 1;
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(size_t(columns) * size_t(rows));
		for (int32_t y = 0; y < rows; y++)
			for (int32_t x = 0; x < columns; x++)
			{
				const Vec2 p = centre + Vec2(float(x) * Spacing - reachX, float(y) * Spacing - reachZ);
				const float hx = height(p + Vec2(0.18f, 0.0f)) - height(p - Vec2(0.18f, 0.0f));
				const float hy = height(p + Vec2(0.0f, 0.18f)) - height(p - Vec2(0.0f, 0.18f));
				vertices.push_back({ .Position = { p.x, height(p), p.y }, .Normal = glm::normalize(glm::vec3(-hx, 0.36f, -hy)), .Color = Pack(glm::vec3(0.86f + 0.14f * Noise2(p * 2.1f, 0xF427u)), drift(p)) });
			}
		for (int32_t y = 0; y + 1 < rows; y++)
			for (int32_t x = 0; x + 1 < columns; x++)
			{
				const uint32_t a = uint32_t(y * columns + x);
				const uint32_t b = a + 1;
				const uint32_t c = a + uint32_t(columns);
				const uint32_t d = c + 1;
				const float highest = std::max({ vertices[a].Position.y, vertices[b].Position.y, vertices[c].Position.y, vertices[d].Position.y });
				if (highest > LakeWaterLine - 0.25f)
					indices.insert(indices.end(), { a, b, d, a, d, c });
			}
		const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(indices.size()), .Material = *snow, .Layer = *snow, .Flags = MeshVertexTint | MeshIce | MeshWet };
		m_Bank = renderer.AddLevelGeometry(vertices, indices, { desc });
	}

	void LakeArt::BuildMounds(WorldRenderer& renderer, const Level& level)
	{
		const std::optional<uint32_t> snow = renderer.FindMaterialSlot("GroundSnow");
		if (!snow)
			return;
		constexpr int32_t Rings = 7;
		constexpr int32_t Sides = 26;
		for (const PlacedProp& prop : level.Props)
		{
			if (prop.Id != PropId::IcePillar)
				continue;
			const auto height = [&](float r, float a, float edge)
			{
				const Vec2 at = prop.Position + FromAngle(a) * (r * edge);
				const float lump = Noise2(at * 1.7f, 0x3D21u);
				const float fall = std::max(1.0f - r * r, 0.0f);
				return LakeIceTop + 0.012f + (0.34f * fall * std::sqrt(fall) + 0.07f * fall) * (0.62f + 0.76f * lump);
			};
			std::vector<ModelVertex> vertices;
			std::vector<uint32_t> indices;
			for (int32_t ring = 0; ring <= Rings; ring++)
				for (int32_t side = 0; side < Sides; side++)
				{
					const float r = float(ring) / float(Rings);
					const float a = 6.2832f * float(side) / float(Sides);
					const Vec2 dir = FromAngle(a);
					const float edge = 1.15f + 0.85f * Noise2(prop.Position + dir * 2.1f, 0x3E77u);
					const Vec2 at = prop.Position + dir * (r * edge);
					const float y = height(r, a, edge);
					const float dr = height(std::min(r + 0.06f, 1.0f), a, edge) - height(std::max(r - 0.06f, 0.0f), a, edge);
					const glm::vec3 normal = glm::normalize(glm::vec3(-dir.x * dr * 4.0f, 1.0f, -dir.y * dr * 4.0f));
					vertices.push_back({ .Position = { at.x, y, at.y }, .Normal = normal, .Color = Pack(glm::vec3(1.0f), 1.0f - 0.82f * r * r) });
				}
			for (int32_t ring = 0; ring < Rings; ring++)
				for (int32_t side = 0; side < Sides; side++)
				{
					const uint32_t a = uint32_t(ring * Sides + side);
					const uint32_t b = uint32_t(ring * Sides + (side + 1) % Sides);
					const uint32_t c = a + uint32_t(Sides);
					const uint32_t d = b + uint32_t(Sides);
					indices.insert(indices.end(), { a, b, d, a, d, c });
				}
			const LevelMeshDesc desc{ .FirstIndex = 0, .IndexCount = uint32_t(indices.size()), .Material = *snow, .Layer = *snow, .Flags = MeshVertexTint | MeshIce };
			m_Mounds.push_back({ renderer.AddLevelGeometry(vertices, indices, { desc }), prop.Position });
		}
	}

	void LakeArt::BuildHarbor(WorldRenderer& renderer, const Level& level)
	{
		Rng rng{ level.Seed ^ 0x4A2Bu };
		const float side = m_Layout.Side;
		const auto put = [&](std::string_view name, Vec2 local, float y, float yaw, float scale, float tiltX, float tiltZ, bool above, bool under)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_Center.x + local.x, y, m_Center.y + local.y));
			transform = glm::rotate(transform, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			transform = glm::rotate(transform, tiltX, glm::vec3(1.0f, 0.0f, 0.0f));
			transform = glm::rotate(transform, tiltZ, glm::vec3(0.0f, 0.0f, 1.0f));
			transform = glm::scale(transform, glm::vec3(scale));
			for (const uint32_t mesh : MeshesNamed(renderer, name))
			{
				const ModelInstance instance = renderer.MakeModelInstance(mesh, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, y);
				if (above)
					m_Above.push_back({ mesh, instance });
				if (under && renderer.GetModels().GetMesh(mesh).Transmission <= 0.0f)
					m_Items.push_back({ mesh, instance });
			}
		};
		const auto shore = [&](Vec2 local) { return m_ShoreLevel.Height(m_Center + local); };
		const Vec2 pier = m_Layout.PierRoot();
		const float bx = pier.x;
		const float root = pier.y;
		const float bank = root - 0.4f;
		constexpr float Bay = 1.8f;
		const float deck = LakeWaterLine + 0.9f;
		for (int32_t j = 1; j <= 5; j++)
		{
			const float z = root + Bay * float(j);
			put("Harbor/PierDeck_A", Vec2(bx, z - Bay * 0.5f), deck, HalfPi + rng.Range(-0.03f, 0.03f), 1.0f, rng.Range(-0.02f, 0.02f), 0.0f, true, true);
			for (const float across : { -0.95f, 0.95f })
				put("Harbor/Piling_A", Vec2(bx + across, z), deck + 0.05f - 6.0f, rng.Range(0.0f, 6.28f), 1.0f, rng.Range(-0.03f, 0.03f), rng.Range(-0.03f, 0.03f), true, true);
			if (std::min(shore(Vec2(bx - 0.9f, z)), shore(Vec2(bx + 0.9f, z))) > deck - 0.15f)
				break;
		}
		for (int32_t k = 0; k <= 4; k++)
		{
			const float z = root - Bay * float(k);
			const float lean = 0.02f + 0.035f * float(k);
			for (const float across : { -0.95f, 0.95f })
			{
				if (k == 4 && across > 0.0f)
					continue;
				const float drop = k == 4 ? 0.45f : 0.0f;
				put("Harbor/Piling_A", Vec2(bx + across, z), LakeWaterLine + 0.95f - 6.0f - drop, rng.Range(0.0f, 6.28f), 1.0f, rng.Range(-lean, lean), rng.Range(-lean, lean) + (k == 4 ? 0.16f : 0.0f), true, true);
			}
			if (k < 3)
			{
				const bool broken = k == 2;
				put(broken ? "Harbor/PierDeck_B" : "Harbor/PierDeck_A", Vec2(bx, z - Bay * 0.5f), LakeWaterLine + 0.9f - (broken ? 0.18f : 0.0f), HalfPi + rng.Range(-0.03f, 0.03f), 1.0f, broken ? -0.14f : rng.Range(-0.02f, 0.02f), broken ? 0.06f : 0.0f, true, true);
			}
		}
		for (const float across : { -1.7f, 1.7f })
			put("Harbor/Bollard_A", Vec2(bx + across, root + 1.0f), shore(Vec2(bx + across, root + 1.0f)) - 0.05f, rng.Range(0.0f, 6.28f), 1.0f, 0.0f, 0.0f, true, false);
		put("Harbor/Rowboat_A", m_Layout.Boat(), LakeWaterLine - 0.3f, 0.0f, 1.0f, 0.05f, 0.2f, true, true);
		const Vec2 mouth = m_Layout.Trench[0];
		const Vec2 inward = Normalize(m_Layout.Trench[0] - m_Layout.Trench[1]);
		put("Harbor/CaveArch_A", mouth + inward * 1.2f, LakeWaterLine - 2.2f, std::atan2(-inward.x, -inward.y), 0.52f, 0.0f, 0.0f, true, true);
		const Vec2 anchorAt(bx + side * 3.2f, bank - 2.2f);
		put("Harbor/Anchor_A", anchorAt, BedHeight(anchorAt) + 0.12f, rng.Range(0.0f, 6.28f), 1.1f, 1.38f, 0.1f, false, true);
		const Vec2 candidates[] = { { side * 15.0f, m_Layout.Mouth - 8.0f }, { side * 15.0f, m_Layout.Mouth + 8.5f }, { side * 9.0f, m_Layout.Mouth - 9.5f }, { side * 19.0f, m_Layout.Mouth + 6.5f }, { -side * 8.0f, -12.0f }, { -side * 14.0f, 9.0f } };
		for (const Vec2 spot : candidates)
		{
			const float ellipse = (spot.x / 27.0f) * (spot.x / 27.0f) + (spot.y / 16.5f) * (spot.y / 16.5f);
			if (TrenchDistance(spot) < LakeTrenchHalf + 4.5f || ellipse > 0.78f)
				continue;
			put("Harbor/Hull_A", spot, BedHeight(spot) + 0.35f, rng.Range(0.0f, 6.28f), 1.0f, 0.08f, 0.6f, false, true);
			break;
		}
		const Vec2 tower(side * 30.5f, bank + 5.0f);
		put("Harbor/Lighthouse_A", tower, shore(tower) - 0.3f, rng.Range(0.0f, 6.28f), 1.0f, 0.03f, -0.02f, true, false);
		const char* seracs[] = { "Abyss/Serac_A", "Abyss/Serac_B", "Abyss/Serac_C" };
		const float spots[][2] = { { 33.0f, 0.6f }, { 38.5f, -0.58f }, { 44.0f, 0.3f }, { 50.0f, -0.45f } };
		for (int32_t i = 0; i < 4; i++)
		{
			const float x = side * spots[i][0];
			const Vec2 at(x, m_Layout.Channel(x) + spots[i][1] * m_Layout.ChannelHalf(x));
			put(seracs[i % 3], at, LakeWaterLine - rng.Range(0.5f, 0.9f), rng.Range(0.0f, 6.28f), rng.Range(0.2f, 0.3f), rng.Range(-0.06f, 0.06f), rng.Range(-0.06f, 0.06f), true, true);
		}
		std::vector<std::vector<uint32_t>> kinds;
		for (const char* name : { "Abyss/Floe_B", "Abyss/Floe_C", "Abyss/Floe_D" })
			if (std::vector<uint32_t> found = MeshesNamed(renderer, name); !found.empty())
				kinds.push_back(found);
		if (!kinds.empty())
			for (int32_t i = 0; i < 18; i++)
				m_Drifters.push_back({ kinds[size_t(rng.Next() * float(kinds.size() - 1) + 0.5f)][0], rng.Range(0.0f, 40.0f), rng.Range(-0.85f, -0.42f), rng.Range(0.0f, 6.28f), rng.Range(0.45f, 0.85f), rng.Range(0.0f, 6.28f) });
	}

	void LakeArt::DrawDrifters(WorldRenderer& renderer, const LevelView& view) const
	{
		const float side = m_Layout.Side;
		for (const Drifter& drifter : m_Drifters)
		{
			const float travel = std::fmod(drifter.Start + view.Time * 0.2f, 40.0f);
			const float x = side * (19.0f + travel);
			const Vec2 p = m_Center + Vec2(x, m_Layout.Channel(x) + drifter.Lateral * m_Layout.ChannelHalf(x));
			if (p.x + 3.0f < view.Min.x || p.x - 3.0f > view.Max.x || p.y + 3.0f < view.Min.y || p.y - 3.0f > view.Max.y)
				continue;
			const float fade = Smooth(0.0f, 3.0f, travel) * (1.0f - Smooth(34.0f, 40.0f, travel));
			const float bob = 0.035f * std::sin(view.Time * 0.7f + drifter.Phase);
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(p.x, LakeWaterLine - 0.03f + bob - (1.0f - fade) * 0.9f, p.y));
			transform = glm::rotate(transform, drifter.Yaw + view.Time * 0.012f * (drifter.Phase > 3.14f ? 1.0f : -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			transform = glm::rotate(transform, 0.025f * std::sin(view.Time * 0.5f + drifter.Phase * 1.3f), glm::vec3(1.0f, 0.0f, 0.4f));
			transform = glm::scale(transform, glm::vec3(drifter.Scale));
			renderer.DrawModel(drifter.Mesh, transform, NoPalette, glm::vec4(0.66f, 0.76f, 0.8f, 0.0f), 0.0f, LakeWaterLine);
			renderer.SubmitUnder(drifter.Mesh, renderer.MakeModelInstance(drifter.Mesh, transform));
		}
	}

	void LakeArt::DrawFloes(WorldRenderer& renderer, const LevelView& view) const
	{
		const float clock = view.Brawl ? view.Brawl->Clock : 0.0f;
		for (const Floe& floe : m_Floes)
		{
			if (floe.Position.x + 3.0f < view.Min.x || floe.Position.x - 3.0f > view.Max.x || floe.Position.y + 3.0f < view.Min.y || floe.Position.y - 3.0f > view.Max.y)
				continue;
			float rise = 1.0f;
			if (floe.Tile >= 0 && size_t(floe.Tile) < m_Times.size())
			{
				const float since = clock - m_Times[size_t(floe.Tile)] - CollapseFallSeconds;
				if (since < 0.0f)
					continue;
				rise = std::min(since / 1.4f, 1.0f);
			}
			const float bob = 0.03f * std::sin(view.Time * 0.8f + floe.Phase);
			const float roll = 0.022f * std::sin(view.Time * 0.6f + floe.Phase * 1.7f);
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(floe.Position.x, LakeWaterLine - 0.03f + bob - (1.0f - rise * rise) * 0.8f, floe.Position.y));
			transform = glm::rotate(transform, floe.Yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			transform = glm::rotate(transform, roll, glm::vec3(1.0f, 0.0f, 0.4f));
			transform = glm::scale(transform, glm::vec3(floe.Scale));
			renderer.DrawModel(floe.Mesh, transform, NoPalette, glm::vec4(0.66f, 0.76f, 0.8f, 0.0f), 0.0f, LakeWaterLine);
			renderer.SubmitUnder(floe.Mesh, renderer.MakeModelInstance(floe.Mesh, transform));
		}
	}

	void LakeArt::BuildLeviathanRig(const WorldRenderer& renderer)
	{
		m_LeviathanJoints.clear();
		m_LeviathanSpine.clear();
		const ModelLibrary& models = renderer.GetModels();
		const std::optional<uint32_t> found = models.FindSkeleton("Abyssal");
		if (!found)
			return;
		const ModelSkeleton& rig = models.GetSkeleton(*found);
		const uint32_t count = std::min(rig.JointCount(), MaxModelJoints);
		m_LeviathanJoints.resize(count);
		std::vector<std::pair<int32_t, uint32_t>> spine;
		for (uint32_t i = 0; i < count; i++)
		{
			const std::string_view name = rig.Names[i];
			LeviathanJoint& joint = m_LeviathanJoints[i];
			joint.Parent = rig.Parents[i];
			joint.Rest = glm::vec3(glm::inverse(rig.InverseBind[i])[3]);
			joint.Side = joint.Rest.x < 0.0f ? -1.0f : 1.0f;
			joint.Seed = float(i) * 2.399f;
			joint.Link = name.empty() ? 0.0f : float(name.back() - '0');
			if (name == "Head")
				joint.Part = LeviathanPart::Head;
			else if (name == "Jaw")
				joint.Part = LeviathanPart::Jaw;
			else if (name.starts_with("Pec"))
				joint.Part = LeviathanPart::Pectoral;
			else if (name.starts_with("Hind"))
				joint.Part = LeviathanPart::Hind;
			else if (name.starts_with("Whip"))
				joint.Part = LeviathanPart::Whip;
			else if (name.starts_with("Ribbon"))
				joint.Part = LeviathanPart::Ribbon;
			else
				spine.push_back({ std::atoi(std::string(name.substr(1)).c_str()), i });
		}
		std::sort(spine.begin(), spine.end());
		for (const auto& [order, joint] : spine)
			m_LeviathanSpine.push_back(joint);
		if (m_LeviathanSpine.empty())
			return;
		const float front = m_LeviathanJoints[m_LeviathanSpine[0]].Rest.z;
		for (LeviathanJoint& joint : m_LeviathanJoints)
			joint.Behind = joint.Parent >= 0 && joint.Part != LeviathanPart::Spine ? m_LeviathanJoints[size_t(joint.Parent)].Behind : front - joint.Rest.z;
	}

	void LakeArt::DrawLeviathan(WorldRenderer& renderer, const LevelView& view) const
	{
		m_Leviathan.Active = false;
		if (m_LeviathanMeshes.empty() || m_LeviathanSpine.empty() || m_Route.size() < 2 || !view.Brawl)
			return;
		const uint32_t seed = view.Brawl->Plan.Seed;
		if (!view.ForceLeviathan && CollapseNoise(int32_t(seed & 0x7FFFu), int32_t(seed >> 15), 0xBEA57u) >= LeviathanRoundChance)
			return;
		Rng rng{ seed ^ 0x1E71u };
		const float start = view.ForceLeviathan ? 2.0f : LeviathanEarliest + LeviathanWindow * rng.Next();
		const float crossing = (m_RouteLength + LeviathanLength) / LeviathanSpeed;
		const float local = view.Clock - start;
		if (local < 0.0f || local >= crossing)
			return;
		const bool fromCave = rng.Next() < 0.5f;
		const float phase = rng.Range(0.0f, 6.28f);
		const float travel = LeviathanSpeed * local;
		constexpr float Scale = 1.15f;
		constexpr float Nose = 9.3f;
		constexpr float WaveRate = 0.52f;
		constexpr float WaveBeat = 2.4f;
		const auto route = [&](float u) { return m_Center + RoutePoint(fromCave ? u : m_RouteLength - u); };
		const auto direction = [&](float u) { return AngleOf(route(u + 0.6f) - route(u - 0.6f)); };
		const auto cruise = [&](float u) { return RouteDepth(fromCave ? u : m_RouteLength - u) + 0.22f * std::sin(u * 0.21f + phase); };
		const auto wave = [&](float behind, float when)
		{
			const float along = std::clamp(behind / 15.0f, 0.0f, 1.0f);
			return std::lerp(0.035f, 0.58f, along * std::sqrt(along)) * std::sin(WaveRate * behind * Scale - WaveBeat * when + phase);
		};
		const auto unwrap = [](float angle, float near) { return near + AngleBetween(near, angle); };

		const size_t bones = m_LeviathanSpine.size();
		std::array<float, MaxModelJoints> along{};
		std::array<float, MaxModelJoints> angle{};
		std::array<Vec2, MaxModelJoints> place{};
		JointMatrices palette{};
		for (size_t i = 0; i < bones; i++)
		{
			const LeviathanJoint& joint = m_LeviathanJoints[m_LeviathanSpine[i]];
			if (i == 0)
			{
				along[i] = travel - (Nose - joint.Rest.z) * Scale;
				place[i] = route(along[i]);
				angle[i] = direction(along[i]) + wave(joint.Behind, local);
				continue;
			}
			const float length = (m_LeviathanJoints[m_LeviathanSpine[i - 1]].Rest.z - joint.Rest.z) * Scale;
			along[i] = along[i - 1] - length;
			angle[i] = unwrap(direction(along[i]) + wave(joint.Behind, local), angle[i - 1]);
			place[i] = place[i - 1] - FromAngle(0.5f * (angle[i - 1] + angle[i])) * length;
		}
		for (size_t i = 0; i < bones; i++)
		{
			const uint32_t index = m_LeviathanSpine[i];
			const LeviathanJoint& joint = m_LeviathanJoints[index];
			const size_t ahead = i > 0 ? i - 1 : i;
			const size_t astern = std::min(i + 1, bones - 1);
			const float span = std::max(Length(place[ahead] - place[astern]), 0.1f);
			const float turn = (angle[ahead] - angle[astern]) / span;
			const Vec2 facing = FromAngle(angle[i]);
			const float pitch = cruise(along[i] + 0.5f) - cruise(along[i] - 0.5f);
			const glm::vec3 forward = glm::normalize(glm::vec3(facing.x, pitch, facing.y));
			const glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
			const glm::vec3 lift = glm::cross(forward, side);
			const float roll = std::clamp(turn * 1.3f, -0.24f, 0.24f);
			glm::mat4 frame(1.0f);
			frame[0] = glm::vec4(side * std::cos(roll) + lift * std::sin(roll), 0.0f) * Scale;
			frame[1] = glm::vec4(lift * std::cos(roll) - side * std::sin(roll), 0.0f) * Scale;
			frame[2] = glm::vec4(forward, 0.0f) * Scale;
			frame[3] = glm::vec4(place[i].x, cruise(along[i]) + joint.Rest.y * Scale, place[i].y, 1.0f);
			palette[index] = frame * glm::translate(glm::mat4(1.0f), -joint.Rest);
		}
		const auto pivot = [](const glm::vec3& rest, float x, float y, float z)
		{
			const glm::mat4 turn = glm::rotate(glm::rotate(glm::rotate(glm::mat4(1.0f), y, glm::vec3(0.0f, 1.0f, 0.0f)), x, glm::vec3(1.0f, 0.0f, 0.0f)), z, glm::vec3(0.0f, 0.0f, 1.0f));
			return glm::translate(glm::mat4(1.0f), rest) * turn * glm::translate(glm::mat4(1.0f), -rest);
		};
		const float rowing = local * 1.36f + phase;
		for (size_t i = 0; i < m_LeviathanJoints.size(); i++)
		{
			const LeviathanJoint& joint = m_LeviathanJoints[i];
			if (joint.Part == LeviathanPart::Spine || joint.Parent < 0)
				continue;
			const float s = joint.Side;
			const float link = joint.Link;
			glm::mat4 motion(1.0f);
			switch (joint.Part)
			{
			case LeviathanPart::Head:
				motion = pivot(joint.Rest, 0.03f * std::sin(local * 0.61f + phase), -0.5f * wave(0.0f, local) + 0.05f * std::sin(local * 0.37f + phase), 0.0f);
				break;
			case LeviathanPart::Jaw:
			{
				const float yawn = std::pow(std::max(std::sin(local * 0.31f + phase), 0.0f), 6.0f);
				motion = pivot(joint.Rest, 0.03f + 0.09f * std::sin(local * 0.47f + phase) + 0.04f * std::sin(local * 1.13f) + 0.24f * yawn, 0.0f, 0.0f);
				break;
			}
			case LeviathanPart::Pectoral:
			{
				const float stroke = rowing + (s > 0.0f ? 0.0f : Pi) - link * 0.8f;
				const float swing = 0.26f - link * 0.05f;
				motion = pivot(joint.Rest, 0.0f, s * (swing * std::sin(stroke) + 0.05f), s * ((0.17f + link * 0.05f) * std::cos(stroke)));
				break;
			}
			case LeviathanPart::Hind:
			{
				const float stroke = rowing + (s > 0.0f ? Pi : 0.0f) + 0.4f - link * 0.8f;
				motion = pivot(joint.Rest, 0.0f, s * 0.15f * std::sin(stroke), s * (0.11f + link * 0.04f) * std::cos(stroke));
				break;
			}
			case LeviathanPart::Whip:
			case LeviathanPart::Ribbon:
			{
				const bool whip = joint.Part == LeviathanPart::Whip;
				const float lag = (link + 1.0f) * 0.3f;
				const float trailing = wave(joint.Behind, local - lag) - wave(joint.Behind, local);
				const float grow = 0.055f + 0.05f * link;
				const float sway = grow * std::sin(local * 1.5f - link * 0.75f + joint.Seed);
				const float heave = grow * 0.8f * std::sin(local * 1.13f - link * 0.68f + joint.Seed * 1.7f);
				motion = pivot(joint.Rest, heave - (whip && link < 1.5f ? 0.14f : 0.02f), 0.55f * trailing + sway, 0.0f);
				break;
			}
			default:
				break;
			}
			palette[i] = palette[size_t(joint.Parent)] * motion;
		}
		const uint32_t skin = renderer.AddPalette(palette, uint32_t(m_LeviathanJoints.size()));
		for (const uint32_t mesh : m_LeviathanMeshes)
		{
			ModelInstance instance = renderer.MakeModelInstance(mesh, glm::mat4(1.0f), skin, glm::vec4(1.45f, 1.65f, 1.75f, 1.0f));
			instance.Burn.w = local + 1.0f;
			renderer.SubmitUnder(mesh, instance);
		}
		const glm::vec3 snout = glm::vec3(palette[m_LeviathanSpine[0]] * glm::vec4(0.0f, 0.4f, Nose, 1.0f));
		m_Leviathan.Active = true;
		m_Leviathan.Progress = local / crossing;
		m_Leviathan.Heading = { FromAngle(angle[0]).x, FromAngle(angle[0]).y };
		m_Leviathan.Head = snout;
		for (int32_t i = 0; i < 6; i++)
		{
			const size_t bone = std::min(size_t(3 + i * 2), bones - 1);
			const float beat = 0.7f + 0.4f * std::sin(float(i) * 1.1f - local * 1.9f);
			renderer.DrawLakeLight({ glm::vec3(place[bone].x, cruise(along[bone]) + 0.5f, place[bone].y), 9.0f, glm::vec3(0.1f, 0.85f, 0.95f) * (1.7f * beat) });
		}
		renderer.DrawLakeLight({ snout + glm::vec3(0.0f, 1.2f, 0.0f), 5.0f, glm::vec3(0.5f, 0.85f, 0.95f) * 0.2f });
		const Vec2 middle = place[bones / 2];
		m_Leviathan.Near = 1.0f - Smooth(5.0f, 24.0f, Length(middle - view.Focus));
		renderer.SetLakeBeast(glm::vec4(middle.x, middle.y, 13.0f, 1.0f));
	}

	void LakeArt::UpdateField(WorldRenderer& renderer, const LevelView& view) const
	{
		const size_t count = size_t(m_Width) * size_t(m_Height);
		const CollapsePlan plan = view.Brawl ? view.Brawl->Plan : CollapsePlan{};
		const bool fresh = !m_HavePlan || plan.Seed != m_Plan.Seed || plan.StartRadius != m_Plan.StartRadius || plan.Center != m_Plan.Center || plan.CraterCount != m_Plan.CraterCount || plan.Steady != m_Plan.Steady || plan.Shape != m_Plan.Shape;
		const auto chamfer = [&](std::vector<float>& distance)
		{
			const float diagonal = 1.414f;
			const auto relax = [&](int32_t x, int32_t y, int32_t dx, int32_t dy, float cost)
			{
				const int32_t nx = x + dx;
				const int32_t ny = y + dy;
				if (nx >= 0 && ny >= 0 && nx < m_Width && ny < m_Height)
					distance[size_t(y) * size_t(m_Width) + size_t(x)] = std::min(distance[size_t(y) * size_t(m_Width) + size_t(x)], distance[size_t(ny) * size_t(m_Width) + size_t(nx)] + cost);
			};
			for (int32_t y = 0; y < m_Height; y++)
				for (int32_t x = 0; x < m_Width; x++)
				{
					relax(x, y, -1, 0, 1.0f);
					relax(x, y, 0, -1, 1.0f);
					relax(x, y, -1, -1, diagonal);
					relax(x, y, 1, -1, diagonal);
				}
			for (int32_t y = m_Height - 1; y >= 0; y--)
				for (int32_t x = m_Width - 1; x >= 0; x--)
				{
					relax(x, y, 1, 0, 1.0f);
					relax(x, y, 0, 1, 1.0f);
					relax(x, y, 1, 1, diagonal);
					relax(x, y, -1, 1, diagonal);
				}
		};
		if (fresh)
		{
			m_Plan = plan;
			m_HavePlan = true;
			m_Times.resize(count);
			m_Shelf.assign(count, 1e6f);
			m_Holes.clear();
			for (uint32_t i = 0; i < HoleCount(plan); i++)
				m_Holes.push_back(HoleCenter(plan, i));
			for (int32_t y = 0; y < m_Height; y++)
				for (int32_t x = 0; x < m_Width; x++)
				{
					const size_t index = size_t(y) * size_t(m_Width) + size_t(x);
					const CellCoord chunk = ChunkOf({ x, y });
					m_Times[index] = view.Brawl ? ChunkCollapseTime(plan, chunk) : 1e6f;
					if (m_Times[index] <= 0.0f && HoleCollapseTime(plan, ChunkSeed(chunk)) >= 0.0f)
						m_Shelf[index] = 0.0f;
				}
			chamfer(m_Shelf);
			m_Alive.assign(count, 2);
		}
		const float clock = view.Brawl ? view.Brawl->Clock : 0.0f;
		bool changed = false;
		for (size_t i = 0; i < count; i++)
		{
			const uint8_t alive = m_Times[i] > clock ? 1 : 0;
			changed = changed || alive != m_Alive[i];
			m_Alive[i] = alive;
		}
		if (changed)
		{
			m_Thin.assign(count, 1e6f);
			for (size_t i = 0; i < count; i++)
				if (!m_Alive[i])
					m_Thin[i] = 0.0f;
			chamfer(m_Thin);
		}
		m_Field.resize(count);
		for (size_t i = 0; i < count; i++)
		{
			const float thin = std::exp(-std::max(m_Thin[i] - 0.5f, 0.0f) / 0.8f);
			const float shelf = 1.0f - Smooth(0.6f, 2.0f, m_Shelf[i]);
			const float warn = m_Alive[i] != 0 ? std::clamp((clock - (m_Times[i] - CollapseWarning)) / CollapseWarning, 0.0f, 1.0f) : 0.0f;
			m_Field[i] = glm::vec4(thin, shelf, warn, 0.0f);
		}
		renderer.SetIceField(m_Width, m_Height, m_Field, LakeWaterLine, LakeIceTop);
	}

	void LakeArt::DrawCreatures(WorldRenderer& renderer, const LevelView& view) const
	{
		const float time = view.Time;
		const Vec2 viewMin = view.Min - Vec2(5.0f, 5.0f);
		const Vec2 viewMax = view.Max + Vec2(5.0f, 5.0f);
		const auto inView = [&](Vec2 p) { return p.x >= viewMin.x && p.x <= viewMax.x && p.y >= viewMin.y && p.y <= viewMax.y; };
		if (m_Fish)
			for (const School& school : m_Schools)
			{
				const float angle = time * school.Speed + school.Phase;
				const Vec2 centre = school.Center + FromAngle(angle) * school.Radius + FromAngle(angle * 2.3f) * 1.2f;
				if (!inView(centre))
					continue;
				const float direction = school.Speed > 0.0f ? 1.0f : -1.0f;
				for (uint32_t k = 0; k < school.Count; k++)
				{
					const glm::vec4 slot = m_FishSlots[school.First + k];
					const float delay = slot.w * 0.1f;
					const float own = time * school.Speed + school.Phase - delay * direction * 0.4f;
					const Vec2 place = school.Center + FromAngle(own) * (school.Radius + slot.x * 1.3f) + FromAngle(own * 2.3f) * 1.2f + Vec2(slot.y, slot.x) * 0.9f;
					const float yaw = own + direction * HalfPi;
					const Vec2 heading = FromAngle(yaw);
					glm::mat4 transform(1.0f);
					transform[0] = glm::vec4(heading.y, 0.0f, -heading.x, 0.0f);
					transform[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
					transform[2] = glm::vec4(heading.x, 0.0f, heading.y, 0.0f);
					transform[3] = glm::vec4(place.x, -school.Depth + slot.z * 0.55f, place.y, 1.0f);
					transform = glm::scale(transform, glm::vec3(school.Size * (0.8f + 0.4f * std::abs(slot.y))));
					ModelInstance instance = renderer.MakeModelInstance(*m_Fish, transform, NoPalette, glm::vec4(school.Color, 0.9f));
					instance.Dissolve = glm::vec4(0.1f, 6.0f, 0.6f, 2.0f);
					instance.Burn = glm::vec4(time * 7.0f + slot.w, 0.0f, 0.0f, 0.0f);
					renderer.SubmitUnder(*m_Fish, instance);
				}
			}
		if (m_JellyMesh)
			for (const Jelly& jelly : m_Jellies)
			{
				const glm::vec3 p = jelly.Position + glm::vec3(std::sin(time * 0.06f + jelly.Phase) * 1.6f, 0.4f * std::sin(time * 0.2f + jelly.Phase), std::cos(time * 0.05f + jelly.Phase * 1.7f) * 1.6f);
				if (!inView(Vec2(p.x, p.z)))
					continue;
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), p);
				transform = glm::scale(transform, glm::vec3(jelly.Scale));
				ModelInstance instance = renderer.MakeModelInstance(*m_JellyMesh, transform, NoPalette, glm::vec4(jelly.Color, 0.4f));
				instance.Burn = glm::vec4(0.0f, 0.5f, time * 1.4f + jelly.Phase, 0.0f);
				renderer.SubmitUnder(*m_JellyMesh, instance);
				renderer.DrawLakeLight({ p, 3.0f * jelly.Scale, jelly.Color * 0.14f });
			}
	}

	void LakeArt::Draw(WorldRenderer& renderer, const LevelView& view) const
	{
		if (!IsBuilt())
			return;
		UpdateField(renderer, view);
		if (m_Bed != UINT32_MAX)
			renderer.SubmitUnder(m_Bed, renderer.MakeModelInstance(m_Bed, glm::mat4(1.0f)));
		if (m_Shafts != UINT32_MAX)
			renderer.SubmitUnder(m_Shafts, renderer.MakeModelInstance(m_Shafts, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f)));
		for (const Item& item : m_Items)
			renderer.SubmitUnder(item.Mesh, item.Instance);
		for (const Feature& feature : m_Features)
		{
			if (feature.Max.x < view.Min.x || feature.Min.x > view.Max.x || feature.Max.y < view.Min.y || feature.Min.y > view.Max.y)
				continue;
			if (view.Brawl && view.Brawl->Clock >= ChunkCollapseTime(view.Brawl->Plan, feature.Chunk))
				continue;
			renderer.SubmitUnder(feature.Mesh, renderer.MakeModelInstance(feature.Mesh, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f)));
		}
		if (m_Land != UINT32_MAX)
			renderer.DrawModel(m_Land, glm::mat4(1.0f));
		const auto inReach = [&](const Patch& patch) { return patch.Max.x >= view.Min.x - 8.0f && patch.Min.x <= view.Max.x + 8.0f && patch.Max.y >= view.Min.y - 12.0f && patch.Min.y <= view.Max.y + 22.0f; };
		for (const Patch& patch : m_Walls)
			if (inReach(patch))
				renderer.DrawModel(patch.Mesh, glm::mat4(1.0f));
		for (const Patch& patch : m_WallsUnder)
			if (inReach(patch))
				renderer.SubmitUnder(patch.Mesh, renderer.MakeModelInstance(patch.Mesh, glm::mat4(1.0f)));
		for (const Patch& patch : m_ShoreIce)
			if (inReach(patch))
				renderer.DrawModel(patch.Mesh, glm::mat4(1.0f));
		const glm::vec4 sway(0.0f, 0.0f, view.Time, 2.0f);
		for (const Patch& patch : m_Flora)
			if (inReach(patch))
			{
				ModelInstance flora = renderer.MakeModelInstance(patch.Mesh, glm::mat4(1.0f));
				flora.Flash = sway;
				renderer.SubmitUnder(patch.Mesh, flora);
			}
		for (const Patch& patch : m_FloraGlow)
			if (inReach(patch))
			{
				ModelInstance glowing = renderer.MakeModelInstance(patch.Mesh, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f));
				glowing.Flash = sway;
				renderer.SubmitUnder(patch.Mesh, glowing);
			}
		if (m_Bank != UINT32_MAX)
		{
			renderer.DrawModel(m_Bank, glm::mat4(1.0f));
			renderer.SubmitUnder(m_Bank, renderer.MakeModelInstance(m_Bank, glm::mat4(1.0f)));
		}
		for (const Item& item : m_Above)
			renderer.SubmitModel(item.Mesh, item.Instance);
		for (const auto& [mesh, position] : m_Mounds)
		{
			const CellCoord cell = CellAt(position);
			const float since = view.Brawl ? view.Brawl->Clock - TileCollapseTime(view.Brawl->Plan, cell.X, cell.Y) : -1.0f;
			if (since >= CollapseFallSeconds)
				continue;
			const float fall = std::max(since, 0.0f) / CollapseFallSeconds;
			renderer.DrawModel(mesh, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.8f * fall * fall, 0.0f)), NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, LakeIceTop);
		}
		DrawFloes(renderer, view);
		DrawDrifters(renderer, view);
		if (m_Growth != UINT32_MAX)
			renderer.SubmitUnder(m_Growth, renderer.MakeModelInstance(m_Growth, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f)));
		for (const auto& [place, tone] : m_Glows)
			if (place.x > view.Min.x - 8.0f && place.x < view.Max.x + 8.0f && place.z > view.Min.y - 8.0f && place.z < view.Max.y + 8.0f)
				renderer.DrawLakeLight({ place, 7.0f, tone * (0.55f + 0.15f * std::sin(view.Time * 0.7f + place.x)) });
		if (m_Snow != UINT32_MAX)
		{
			ModelInstance snow = renderer.MakeModelInstance(m_Snow, glm::mat4(1.0f), NoPalette, glm::vec4(1.0f));
			snow.Flash = glm::vec4(-4.8f, 17.7f, view.Time, 1.0f);
			renderer.SubmitUnder(m_Snow, snow);
		}
		DrawLeviathan(renderer, view);
		DrawCreatures(renderer, view);
	}
}
