#include "ForgeArt.h"
#include "Rendering/LevelRenderer.h"
#include "Rendering/ModelLibrary.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>

namespace Game
{
	namespace
	{
		constexpr float GridStep = 0.25f;
		constexpr int32_t GridCells = int32_t(2.0f * (ForgeReach + 0.5f) / GridStep);
		constexpr float GridOrigin = -(ForgeReach + 0.5f);
		constexpr float InvSqrt2 = 0.70710678f;
		constexpr Vec2 CruciblePivot{ 0.0f, 14.0f };
		constexpr float CrucibleHeight = 3.6f;
		constexpr float CrucibleRadius = 1.55f;
		constexpr float SpoutReach = 1.99f;
		constexpr float SpoutLift = 1.12f;
		constexpr float CrucibleDepth = 2.4f;
		constexpr float CrucibleTiltMax = 1.15f;
		constexpr float CrucibleReturn = 1.5f;
		constexpr float StreamCore = 0.55f;
		constexpr float FallCore = 0.55f;
		constexpr float ChannelLip = 18.6f;
		constexpr float ChannelMelt = 0.51f;
		constexpr Vec2 TroughLip{ 4.0f, -21.0f };
		constexpr float TroughMelt = 1.05f;
		constexpr float SmokeInset = 0.6f;
		constexpr float PumpStroke = 0.22f;
		constexpr float PumpRate = 0.9f;
		constexpr float UnderlightSpan = 96.0f;
		constexpr uint32_t UnderlightSize = 128;
		const glm::vec3 PourForward(0.0f, 0.0f, -1.0f);
		constexpr float WarnRateStart = 1.2f;
		constexpr float WarnRateGain = 1.2f;
		constexpr float WarnLinger = 0.3f;
		constexpr float GrooveSheet = 0.045f;
		constexpr float OpenReach = 1e5f;
		constexpr float FloodMode = 2.0f;
		constexpr float NeverCovered = 1e9f;
		constexpr float AnchorSpacing = 2.0f;
		constexpr float AnchorLift = 0.6f;
		constexpr float MetalLightRadius = 3.4f;
		constexpr float FloodLightRadius = 6.5f;
		constexpr float FloodLightShare = 0.8f;
		constexpr float FloodJointLevel = -0.02f;
		constexpr float BedHeat = 7.0f;
		constexpr const char* JointMaterial = "ForgeJointSilver";
		const glm::vec3 JointTint(1.0f, 1.0f, 1.0f);
		constexpr const char* RockMaterial = "GroundBasalt";
		constexpr const char* WallMaterial = "castle_wall_varriation";
		constexpr uint32_t ScatterCount = 46;
		constexpr uint32_t ScatterAttempts = 2400;
		constexpr float ScatterInner = 17.0f;
		constexpr float ScatterOuter = 31.0f;
		constexpr float ScatterEdge = 20.5f;
		constexpr float ScatterSpacing = 3.4f;
		constexpr float ScatterClusterShare = 0.55f;

		struct ScatterKeepOut
		{
			Vec2 A;
			Vec2 B;
			float Radius;
		};

		constexpr ScatterKeepOut ScatterKeepOuts[] = {
			{ { -12.0f, 13.0f }, { 12.0f, 30.0f }, 4.0f },
			{ { 13.2f, 17.0f }, { 13.2f, 17.0f }, 5.0f },
			{ { -13.2f, 17.0f }, { -13.2f, 17.0f }, 5.0f },
			{ { 15.8f, 9.9f }, { 25.4f, 15.9f }, 2.8f },
			{ { -15.8f, 9.9f }, { -25.4f, 15.9f }, 2.8f },
			{ { 25.4f, 15.9f }, { 25.4f, 15.9f }, 8.5f },
			{ { -25.4f, 15.9f }, { -25.4f, 15.9f }, 8.5f },
			{ { -30.5f, -1.6f }, { -28.5f, -10.9f }, 8.0f },
			{ { 4.0f, -33.0f }, { 4.0f, -21.0f }, 3.0f },
			{ { 24.3f, -4.5f }, { 19.6f, -11.8f }, 2.8f },
			{ { 29.5f, -2.5f }, { 44.0f, -5.0f }, 3.0f },
			{ { -19.8f, -10.0f }, { -23.8f, 5.6f }, 3.0f },
			{ { -24.0f, -18.0f }, { -20.0f, -14.6f }, 2.8f },
			{ { -18.5f, 10.0f }, { -18.5f, 10.0f }, 3.2f },
			{ { -23.0f, 4.5f }, { -23.0f, 4.5f }, 3.8f },
			{ { 10.5f, -14.0f }, { 10.5f, -14.0f }, 3.0f },
			{ { 21.0f, -19.0f }, { 21.0f, -19.0f }, 4.5f },
			{ { -21.5f, -12.0f }, { -21.5f, -12.0f }, 4.5f },
		};
		constexpr float FloodLightFade = 3.0f;
		constexpr float PourLightRadius = 7.0f;
		constexpr float ChainPitch = 0.82f;
		constexpr float ChainRise = 14.0f;
		constexpr float ChainStart = 0.5f;
		constexpr float ChainReach = 4.5f;
		constexpr float ShackleInset = 0.45f;
		constexpr float ShackleLift = 0.42f;
		constexpr float RaisedShift = 0.65f;
		constexpr float BackdropLightInset = 2.0f;
		constexpr float BackdropLightRadius = 8.0f;
		constexpr float FurnaceLightShare = 1.6f;
		constexpr float VatLightShare = 1.2f;
		constexpr float TitanLightShare = 1.3f;
		constexpr float TitanLightHeight = 1.5f;
		constexpr Vec2 TitanLights[] = { { -6.9f, 13.6f }, { 6.9f, 13.6f }, { 0.0f, 17.5f } };
		constexpr float CrucibleGlowLift = 1.6f;
		constexpr float CrucibleGlowRadius = 5.0f;
		constexpr float CrucibleGlowShare = 0.7f;
		constexpr float CrucibleMode = 3.0f;
		constexpr float MeltFull = 0.8f;
		constexpr float MeltDrain = 0.35f;
		constexpr float MeltRefill = 4.0f;
		constexpr float MeltLipHeight = 1.12f;
		constexpr float MeltLipReach = 1.37f;
		constexpr float MeltLipDrop = 0.01f;
		constexpr float MeltSpan = 2.6f;
		constexpr float GateCheck = 0.1f;
		constexpr float GateLead = 1.2f;
		constexpr float GateRise = 1.4f;
		constexpr float GateTravel = 0.9f;
		constexpr float GateSkirtGap = 0.004f;
		constexpr float GateSkirtHalf = 1.36f;
		constexpr float GateSkirtDepth = 1.0f;
		constexpr float SkirtLinger = 0.6f;

		const glm::vec3 IronColor(0.1f, 0.09f, 0.085f);
		const glm::vec4 CoreTint(4.5f, 3.2f, 1.2f, 0.0f);
		const glm::vec4 WarnTint(3.0f, 1.0f, 0.2f, 0.0f);
		const glm::vec4 CrustTint(0.12f, 0.14f, 0.2f, 0.0f);
		const glm::vec3 MetalLight(2.0f, 0.75f, 0.17f);

		using Field = std::function<float(Vec2)>;
		using Arrival = std::function<float(Vec2)>;

		struct ArenaPart
		{
			const char* Name;
			bool Ground;
		};

		const ArenaPart ArenaParts[] = { { "Curb", false }, { "CurbEdge", false }, { "CurbBolts", true }, { "Steps", false }, { "TrenchFloor", true }, { "Fuller", true }, { "PommelStep", true }, { "PommelRing", false }, { "PommelBand", false }, { "Anchors", false }, { "Bridges", false }, { "SluiceFrame", false } };

		glm::vec3 World(Vec2 local, float height)
		{
			return { ForgeCenter.x + local.x, height, ForgeCenter.y + local.y };
		}

		Vec2 LocalOf(const glm::vec3& point)
		{
			return { point.x - ForgeCenter.x, point.z - ForgeCenter.y };
		}

		float RingField(float outer, float inner, Vec2 local)
		{
			const float l1 = ForgeL1(local);
			return std::max(l1 - outer, inner - l1) * InvSqrt2;
		}

		float ArrivalNear(Vec2 local)
		{
			const float meters = ArrivalMeters(local);
			if (meters >= 0.0f)
				return meters;
			float best = -1.0f;
			for (int32_t y = -1; y <= 1; y++)
				for (int32_t x = -1; x <= 1; x++)
					if (const float near = ArrivalMeters(local + Vec2(float(x), float(y)) * ForgeCell); near >= 0.0f && (best < 0.0f || near < best))
						best = near;
			return std::max(best, 0.0f);
		}

		struct MeshBuilder
		{
			std::vector<ModelVertex>& Vertices;
			std::vector<uint32_t>& Indices;
			Arrival U;

			void Polygon(const std::vector<glm::vec3>& input, const glm::vec3& normal, const glm::vec3& color)
			{
				std::vector<glm::vec3> points;
				for (const glm::vec3& point : input)
					if (points.empty() || glm::distance(point, points.back()) > 1e-5f)
						points.push_back(point);
				while (points.size() > 1 && glm::distance(points.front(), points.back()) <= 1e-5f)
					points.pop_back();
				if (points.size() < 3)
					return;
				glm::vec3 winding(0.0f);
				for (size_t i = 0; i < points.size(); i++)
					winding += glm::cross(points[i], points[(i + 1) % points.size()]);
				if (glm::length(winding) < 1e-9f)
					return;
				const uint32_t first = uint32_t(Vertices.size());
				const glm::uvec3 c(glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f);
				const uint32_t packed = c.r | (c.g << 8) | (c.b << 16) | (255u << 24);
				for (const glm::vec3& point : points)
					Vertices.push_back({ .Position = point, .U = U ? U(LocalOf(point)) : 0.0f, .Normal = normal, .Color = packed });
				const bool flip = glm::dot(winding, normal) < 0.0f;
				for (uint32_t i = 1; i + 1 < uint32_t(points.size()); i++)
				{
					Indices.push_back(first);
					Indices.push_back(first + (flip ? i + 1 : i));
					Indices.push_back(first + (flip ? i : i + 1));
				}
			}
		};

		struct ClipResult
		{
			std::vector<Vec2> Points;
			bool Full = false;
			bool Empty = false;
		};

		ClipResult ClipCell(Vec2 lo, float step, const float values[4])
		{
			ClipResult result;
			const Vec2 corners[4] = { lo, lo + Vec2(step, 0.0f), lo + Vec2(step, step), lo + Vec2(0.0f, step) };
			const bool inside[4] = { values[0] < 0.0f, values[1] < 0.0f, values[2] < 0.0f, values[3] < 0.0f };
			result.Full = inside[0] && inside[1] && inside[2] && inside[3];
			result.Empty = !inside[0] && !inside[1] && !inside[2] && !inside[3];
			if (result.Empty)
				return result;
			for (int32_t i = 0; i < 4; i++)
			{
				const int32_t j = (i + 1) % 4;
				if (inside[i])
					result.Points.push_back(corners[i]);
				if (inside[i] != inside[j])
					result.Points.push_back(Lerp(corners[i], corners[j], values[i] / (values[i] - values[j])));
			}
			return result;
		}

		struct FieldGrid
		{
			std::vector<float> Values;

			explicit FieldGrid(const Field& field)
			{
				Values.resize(size_t(GridCells + 1) * size_t(GridCells + 1));
				for (int32_t y = 0; y <= GridCells; y++)
					for (int32_t x = 0; x <= GridCells; x++)
						Values[size_t(y) * size_t(GridCells + 1) + size_t(x)] = field(Corner(x, y));
			}

			static Vec2 Corner(int32_t x, int32_t y) { return { GridOrigin + float(x) * GridStep, GridOrigin + float(y) * GridStep }; }
			float At(int32_t x, int32_t y) const { return Values[size_t(y) * size_t(GridCells + 1) + size_t(x)]; }
		};

		void FillRegion(MeshBuilder& mesh, const FieldGrid& grid, float height, bool merge)
		{
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			const glm::vec3 white(1.0f);
			for (int32_t y = 0; y < GridCells; y++)
			{
				int32_t run = -1;
				for (int32_t x = 0; x <= GridCells; x++)
				{
					ClipResult clip;
					if (x < GridCells)
					{
						const float values[4] = { grid.At(x, y), grid.At(x + 1, y), grid.At(x + 1, y + 1), grid.At(x, y + 1) };
						clip = ClipCell(FieldGrid::Corner(x, y), GridStep, values);
					}
					else
						clip.Empty = true;
					if (clip.Full && merge)
					{
						if (run < 0)
							run = x;
						continue;
					}
					if (run >= 0)
					{
						const Vec2 a = FieldGrid::Corner(run, y);
						const Vec2 b = FieldGrid::Corner(x, y + 1);
						mesh.Polygon({ World(a, height), World(Vec2(b.x, a.y), height), World(b, height), World(Vec2(a.x, b.y), height) }, up, white);
						run = -1;
					}
					if (clip.Empty)
						continue;
					std::vector<glm::vec3> points;
					for (const Vec2 p : clip.Points)
						points.push_back(World(p, height));
					mesh.Polygon(points, up, white);
				}
			}
		}

		void Cylinder(MeshBuilder& mesh, float radius, float bottom, float top, uint32_t segments, const glm::vec3& color, bool inner, bool cap)
		{
			for (uint32_t i = 0; i < segments; i++)
			{
				const float a0 = 2.0f * Pi * float(i) / float(segments);
				const float a1 = 2.0f * Pi * float(i + 1) / float(segments);
				const glm::vec3 p0(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);
				const glm::vec3 p1(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);
				const glm::vec3 normal = glm::normalize(glm::vec3(std::cos(0.5f * (a0 + a1)), 0.0f, std::sin(0.5f * (a0 + a1))));
				const glm::vec3 lo(0.0f, bottom, 0.0f);
				const glm::vec3 hi(0.0f, top, 0.0f);
				mesh.Polygon({ p0 + lo, p1 + lo, p1 + hi, p0 + hi }, normal, color);
				if (inner)
					mesh.Polygon({ p0 + lo, p1 + lo, p1 + hi, p0 + hi }, -normal, color * 2.5f);
				if (cap)
					mesh.Polygon({ lo, p1 + lo, p0 + lo }, glm::vec3(0.0f, -1.0f, 0.0f), color);
			}
		}

		float Smooth(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		struct ForgeFall
		{
			glm::vec3 Lip;
			glm::vec3 Forward;
		};

		std::vector<ForgeFall> Falls()
		{
			std::vector<ForgeFall> falls;
			for (const float degrees : { 32.0f, 148.0f })
			{
				const Vec2 out(std::cos(glm::radians(degrees)), std::sin(glm::radians(degrees)));
				falls.push_back({ World(out * ChannelLip, ChannelMelt), glm::vec3(-out.x, 0.0f, -out.y) });
			}
			falls.push_back({ World(TroughLip, TroughMelt), glm::vec3(0.0f, 0.0f, 1.0f) });
			return falls;
		}

		glm::vec3 FullTiltLip()
		{
			const glm::mat4 tipped = glm::rotate(glm::translate(glm::mat4(1.0f), World(CruciblePivot, CrucibleHeight)), -CrucibleTiltMax, glm::vec3(1.0f, 0.0f, 0.0f));
			return glm::vec3(tipped * glm::vec4(0.0f, SpoutLift, -SpoutReach, 1.0f));
		}

		struct TubeShape
		{
			float Throw;
			float Swing;
			float CurlDepth;
			float TopRadius;
			float BottomRadius;
			float ThinDepth;
			float Spread;
			float Flatten;
			float Splash;
			float SplashHeight;
			float Width;
		};

		constexpr TubeShape PourShape{ 0.32f, 0.28f, 0.8f, 0.2f, 0.13f, 1.4f, 0.9f, 0.4f, 1.8f, 0.3f, 1.0f };
		constexpr TubeShape FallShape{ 0.55f, 0.35f, 0.9f, 0.3f, 0.24f, 2.5f, 0.5f, 0.55f, 1.5f, 0.45f, 1.7f };

		void StreamTube(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices, const glm::vec3& lip, const glm::vec3& forward, float floor, float thickness, const TubeShape& shape)
		{
			constexpr uint32_t Sides = 14;
			const float height = lip.y - floor;
			std::vector<float> drops;
			for (float d = 0.0f; d < height - 0.02f; d += d < shape.CurlDepth ? 0.07f : 0.2f)
				drops.push_back(d);
			for (float d = std::max(height - shape.SplashHeight, drops.back() + 0.02f); d < height; d += 0.05f)
				drops.push_back(d);
			drops.push_back(height);
			const uint32_t first = uint32_t(vertices.size());
			for (const float d : drops)
			{
				const float swing = std::exp(-d / shape.Swing);
				const glm::vec3 center = lip + glm::vec3(0.0f, -d, 0.0f) + forward * (shape.Throw * (1.0f - swing));
				const glm::vec3 direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f) + forward * (shape.Throw * swing / shape.Swing));
				const glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
				const glm::vec3 across = glm::normalize(glm::cross(direction, side));
				const float splash = 1.0f + shape.Splash * Smooth((d - (height - shape.SplashHeight)) / shape.SplashHeight);
				const float radius = thickness * glm::mix(shape.TopRadius, shape.BottomRadius, Smooth(d / shape.ThinDepth)) * splash;
				const float wide = radius * shape.Width * (1.0f + shape.Spread * swing);
				const float thin = radius * (1.0f - shape.Flatten * swing);
				for (uint32_t s = 0; s < Sides; s++)
				{
					const float a = 2.0f * Pi * float(s) / float(Sides);
					const glm::vec3 offset = side * (std::cos(a) * wide) + across * (std::sin(a) * thin);
					const glm::vec3 normal = glm::normalize(side * (std::cos(a) / wide) + across * (std::sin(a) / thin));
					vertices.push_back({ .Position = center + offset, .U = d, .Normal = normal });
				}
			}
			const uint32_t rings = uint32_t(drops.size());
			const auto triangle = [&](uint32_t a, uint32_t b, uint32_t c)
			{
				const glm::vec3 winding = glm::cross(vertices[b].Position - vertices[a].Position, vertices[c].Position - vertices[a].Position);
				const bool flip = glm::dot(winding, vertices[a].Normal + vertices[b].Normal + vertices[c].Normal) < 0.0f;
				indices.insert(indices.end(), { a, flip ? c : b, flip ? b : c });
			};
			for (uint32_t r = 0; r + 1 < rings; r++)
				for (uint32_t s = 0; s < Sides; s++)
				{
					const uint32_t a = first + r * Sides + s;
					const uint32_t b = first + r * Sides + (s + 1) % Sides;
					const uint32_t c = first + (r + 1) * Sides + (s + 1) % Sides;
					const uint32_t d = first + (r + 1) * Sides + s;
					triangle(a, b, c);
					triangle(a, c, d);
				}
		}

		glm::mat4 Frame(const glm::vec3& origin, Vec2 outward)
		{
			const glm::vec3 out(outward.x, 0.0f, outward.y);
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			glm::mat4 frame(1.0f);
			frame[0] = glm::vec4(glm::cross(out, up), 0.0f);
			frame[1] = glm::vec4(up, 0.0f);
			frame[2] = glm::vec4(-out, 0.0f);
			frame[3] = glm::vec4(origin, 1.0f);
			return frame;
		}

		uint32_t RingOf(Vec2 local)
		{
			const float l1 = ForgeL1(local);
			for (uint32_t ring = 0; ring < ForgeRings; ring++)
				if (l1 >= ForgeRingEdges[ring + 1])
					return ring;
			return ForgeRings - 1;
		}

		std::vector<ForgeAnchor> BuildAnchors()
		{
			struct Sum
			{
				Vec2 Position;
				float Meters = 0.0f;
				uint32_t Count = 0;
			};
			std::map<std::pair<int32_t, int32_t>, Sum> bins;
			const ForgeGrid& grid = GetForgeGrid();
			for (int32_t y = 0; y < ForgeGridSize; y++)
				for (int32_t x = 0; x < ForgeGridSize; x++)
				{
					const Vec2 center = ForgeGrid::CellCenter(x, y);
					const uint16_t cost = grid.At(x, y);
					if (cost == ForgeUnreached || !InTrench(center))
						continue;
					const float meters = float(cost) * ForgeCostMeters;
					const bool arm = std::abs(center.x) > GripHalf + 0.8f && std::abs(center.y - GuardY) < GuardSpan;
					Sum& sum = bins[{ int32_t(meters / AnchorSpacing), arm ? (center.x > 0.0f ? 1 : -1) : 0 }];
					sum.Position += center;
					sum.Meters += meters;
					sum.Count++;
				}
			std::vector<ForgeAnchor> anchors;
			for (const auto& [key, sum] : bins)
				if (sum.Count >= 4)
					anchors.push_back({ World(sum.Position / float(sum.Count), MetalHotLevel + AnchorLift), sum.Meters / float(sum.Count) });
			return anchors;
		}
	}

	void ForgeArt::Clear()
	{
		m_Built = false;
		m_Anchors.clear();
		m_Kit.clear();
		m_Floods.clear();
		m_CrucibleParts.clear();
		m_GateParts.clear();
		m_BackdropLights.clear();
		m_SmokeSpots.clear();
		m_FallSpots.clear();
		m_Pumps.clear();
		m_ScatterMeshes.clear();
	}

	void ForgeArt::Build(WorldRenderer& renderer, const Level&)
	{
		Clear();
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<LevelMeshDesc> meshes;
		MeshBuilder mesh{ vertices, indices, {} };
		const uint32_t plain = renderer.GetPlainMaterial();
		const auto close = [&](uint32_t first, uint32_t flags)
		{
			meshes.push_back({ .FirstIndex = first, .IndexCount = uint32_t(indices.size()) - first, .Material = plain, .Layer = plain, .Flags = flags });
			return uint32_t(meshes.size() - 1);
		};

		const FieldGrid trench([](Vec2 p) { return std::max(TrenchDistance(p), ForgeL1(p) - ForgeReach); });
		{
			mesh.U = ArrivalNear;
			const uint32_t first = uint32_t(indices.size());
			FillRegion(mesh, trench, 0.0f, false);
			m_Metal = close(first, MeshMetal | MeshGround);
		}
		mesh.U = [](Vec2 local) { return FloodTimeAt(local); };
		for (uint32_t band = 0; band < ForgeBands; band++)
		{
			const float outer = std::min(ForgeBandEdges[band], ForgeReach - 0.05f);
			const float inner = ForgeBandEdges[band + 1];
			uint32_t first = uint32_t(indices.size());
			for (const uint32_t groove : ForgeGrooveRings)
			{
				const float grooveOuter = std::min(ForgeRingEdges[groove], outer);
				const float grooveInner = std::max(ForgeRingEdges[groove + 1], inner);
				if (grooveOuter <= grooveInner + 1e-3f)
					continue;
				const FieldGrid channel([grooveOuter, grooveInner](Vec2 p) { return std::max(RingField(grooveOuter, grooveInner, p), CurbWidth - TrenchDistance(p)); });
				FillRegion(mesh, channel, FloodJointLevel, false);
			}
			if (indices.size() > first)
				m_Floods.push_back({ close(first, MeshMetal | MeshGround), band });
		}
		mesh.U = {};
		for (uint32_t band = 0; band < ForgeBands; band++)
		{
			const uint32_t ring = ForgeGrooveRings[band];
			const FieldGrid grid([ring](Vec2 p) { return RingField(ForgeRingEdges[ring], ForgeRingEdges[ring + 1], p); });
			const uint32_t first = uint32_t(indices.size());
			FillRegion(mesh, grid, ForgeRingHeights[ring] + GrooveSheet, true);
			m_Grooves[band] = close(first, MeshVertexTint | MeshGround);
		}
		{
			const uint32_t first = uint32_t(indices.size());
			Cylinder(mesh, CrucibleRadius, -0.5f * CrucibleDepth, 0.5f * CrucibleDepth, 16, IronColor, true, true);
			m_Crucible = close(first, MeshVertexTint);
		}
		{
			const uint32_t first = uint32_t(indices.size());
			StreamTube(vertices, indices, FullTiltLip(), PourForward, TrenchFloor, StreamCore, PourShape);
			m_Stream = close(first, MeshVertexTint);
		}
		{
			const uint32_t first = uint32_t(indices.size());
			StreamTube(vertices, indices, FullTiltLip(), PourForward, TrenchFloor, 1.0f, PourShape);
			m_StreamFlow = close(first, MeshFlow);
		}
		for (const bool core : { true, false })
		{
			const uint32_t first = uint32_t(indices.size());
			for (const ForgeFall& fall : Falls())
				StreamTube(vertices, indices, fall.Lip, fall.Forward, ForgePitSurface - 0.2f, core ? FallCore : 1.0f, FallShape);
			(core ? m_FallCore : m_FallFlow) = close(first, core ? MeshVertexTint | MeshGround : MeshFlow | MeshGround);
		}
		{
			const uint32_t first = uint32_t(indices.size());
			std::vector<glm::vec3> disc;
			for (uint32_t i = 0; i < 20; i++)
				disc.push_back({ std::cos(2.0f * Pi * float(i) / 20.0f), 0.0f, std::sin(2.0f * Pi * float(i) / 20.0f) });
			mesh.Polygon(disc, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f));
			m_Disc = close(first, MeshVertexTint | MeshGround);
		}
		{
			const uint32_t first = uint32_t(indices.size());
			std::vector<glm::vec3> disc;
			for (uint32_t i = 0; i < 24; i++)
				disc.push_back({ std::cos(2.0f * Pi * float(i) / 24.0f), 0.0f, std::sin(2.0f * Pi * float(i) / 24.0f) });
			mesh.Polygon(disc, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f));
			m_Melt = close(first, MeshMetal | MeshGround);
		}
		{
			const uint32_t first = uint32_t(indices.size());
			const float y = ForgeHiltLine + GateSkirtGap;
			mesh.U = [](Vec2) { return ArrivalMeters(Vec2(0.0f, ForgeHiltLine + GateCheck)); };
			mesh.Polygon({ World(Vec2(-GateSkirtHalf, y), 0.0f), World(Vec2(GateSkirtHalf, y), 0.0f), World(Vec2(GateSkirtHalf, y), -GateSkirtDepth), World(Vec2(-GateSkirtHalf, y), -GateSkirtDepth) }, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f));
			mesh.U = {};
			m_GateSkirt = close(first, MeshMetal | MeshGround);
		}

		const uint32_t base = renderer.AddLevelGeometry(vertices, indices, meshes);
		for (FloodSheet& sheet : m_Floods)
			sheet.Mesh += base;
		for (uint32_t& groove : m_Grooves)
			groove += base;
		for (uint32_t* id : { &m_Metal, &m_Crucible, &m_Stream, &m_StreamFlow, &m_Disc, &m_Melt, &m_GateSkirt, &m_FallCore, &m_FallFlow })
			*id += base;

		m_LongestPath = 0.0f;
		m_HiltPath = 0.0f;
		const ForgeGrid& grid = GetForgeGrid();
		for (int32_t y = 0; y < ForgeGridSize; y++)
			for (int32_t x = 0; x < ForgeGridSize; x++)
				if (const uint16_t cost = grid.At(x, y); cost != ForgeUnreached)
				{
					m_LongestPath = std::max(m_LongestPath, float(cost) * ForgeCostMeters);
					if (ForgeGrid::CellCenter(x, y).y >= ForgeHiltLine)
						m_HiltPath = std::max(m_HiltPath, float(cost) * ForgeCostMeters);
				}
		m_Anchors = BuildAnchors();
		m_FallSpots.clear();
		for (const ForgeFall& fall : Falls())
		{
			const glm::vec3 impact(fall.Lip.x + fall.Forward.x * FallShape.Throw, ForgePitSurface + 0.1f, fall.Lip.z + fall.Forward.z * FallShape.Throw);
			m_FallSpots.push_back(impact);
			m_BackdropLights.push_back({ impact + glm::vec3(0.0f, 1.2f, 0.0f), 1.0f });
		}
		BuildUnderlight(renderer);
		m_GateOpen = 1e9f;
		for (uint32_t pour = 1; pour < uint32_t(std::size(ForgePours)); pour++)
			if (ForgePourAt(pour - 1).HiltOnly && !ForgePourAt(pour).HiltOnly)
			{
				m_GateOpen = PourFrontTime(pour, Vec2(0.0f, ForgeHiltLine), ArrivalMeters(Vec2(0.0f, ForgeHiltLine + GateCheck))) - GateLead;
				break;
			}
		PlaceArena(renderer);
		PlaceBackdrop(renderer);
		PlaceScatter(renderer);
		m_Built = true;
	}

	void ForgeArt::AddKit(const WorldRenderer& renderer, const std::vector<uint32_t>& meshes, const glm::mat4& transform, int32_t band, float heat)
	{
		for (const uint32_t mesh : meshes)
		{
			const ModelMesh& info = renderer.GetModels().GetMesh(mesh);
			Vec2 low(1e9f, 1e9f);
			Vec2 high(-1e9f, -1e9f);
			for (uint32_t corner = 0; corner < 8; corner++)
			{
				const glm::vec3 local((corner & 1u) ? info.Max.x : info.Min.x, (corner & 2u) ? info.Max.y : info.Min.y, (corner & 4u) ? info.Max.z : info.Min.z);
				const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
				const Vec2 seen(world.x, world.z + std::max(world.y, 0.0f) * RaisedShift);
				low = Vec2(std::min(low.x, seen.x), std::min(low.y, std::min(seen.y, world.z)));
				high = Vec2(std::max(high.x, seen.x), std::max(high.y, std::max(seen.y, world.z)));
			}
			m_Kit.push_back({ mesh, renderer.MakeModelInstance(mesh, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface), low, high, band, heat });
		}
	}

	void ForgeArt::PlaceArena(WorldRenderer& renderer)
	{
		const ModelLibrary& models = renderer.GetModels();
		const auto group = [&](const std::string& name)
		{
			std::vector<uint32_t> found;
			if (const std::optional<uint32_t> mesh = models.FindMesh(name))
			{
				found.push_back(*mesh);
				for (uint32_t extra = 1; extra < 8; extra++)
					if (const std::optional<uint32_t> part = models.FindMesh(name + "." + std::to_string(extra)))
						found.push_back(*part);
					else
						break;
			}
			return found;
		};
		const glm::mat4 arena = Frame(World({}, 0.0f), Vec2(0.0f, -1.0f));
		const std::optional<uint32_t> bronze = renderer.FindMaterialSlot(JointMaterial);
		const auto add = [&](const std::string& name, int32_t band, bool ground, float heat = 1.0f, std::optional<uint32_t> material = std::nullopt)
		{
			const std::vector<uint32_t> meshes = group("Forge/" + name);
			for (const uint32_t mesh : meshes)
			{
				if (ground)
					renderer.AddMeshFlags(mesh, MeshGround);
				if (material)
				{
					renderer.SetMeshMaterial(mesh, *material);
					renderer.AddMeshFlags(mesh, MeshCliff | MeshPolished);
				}
			}
			AddKit(renderer, meshes, arena, band, heat);
			if (material)
				for (size_t i = m_Kit.size() - meshes.size(); i < m_Kit.size(); i++)
					m_Kit[i].Instance.Tint = glm::vec4(JointTint, 0.0f);
		};
		for (int32_t quadrant = 0; quadrant < 4; quadrant++)
		{
			add("Rim" + std::to_string(quadrant), 0, false);
			add("Fascia" + std::to_string(quadrant), -1, false);
			for (int32_t band = 0; band < int32_t(ForgeBands); band++)
			{
				add("Slabs" + std::to_string(band) + std::to_string(quadrant), band, true);
				add("Bed" + std::to_string(band) + std::to_string(quadrant), band, true, BedHeat, bronze);
			}
		}
		for (const ArenaPart& part : ArenaParts)
			add(part.Name, -1, part.Ground);
		m_GateParts = group("Forge/SluiceGate");
		const std::vector<uint32_t> link = group("Forge/ChainLink");
		if (link.empty())
			return;
		const Vec2 corners[] = { { 0.0f, ForgeReach }, { ForgeReach, 0.0f }, { 0.0f, -ForgeReach }, { -ForgeReach, 0.0f } };
		for (const Vec2 corner : corners)
		{
			const Vec2 outward = Normalize(corner);
			const glm::vec3 from = World(corner - outward * ShackleInset, AnchorTop + ShackleLift);
			const glm::vec3 to = from + glm::vec3(outward.x, 0.0f, outward.y) * ChainReach + glm::vec3(0.0f, ChainRise, 0.0f);
			const glm::vec3 t = glm::normalize(to - from);
			const glm::vec3 side = glm::normalize(glm::cross(t, glm::vec3(0.0f, 1.0f, 0.0f)));
			const glm::vec3 normal = glm::cross(side, t);
			const uint32_t links = uint32_t(glm::length(to - from) / ChainPitch);
			for (uint32_t i = 0; i < links; i++)
			{
				const glm::vec3 y = (i & 1u) != 0 ? side : normal;
				glm::mat4 frame(1.0f);
				frame[0] = glm::vec4(t, 0.0f);
				frame[1] = glm::vec4(y, 0.0f);
				frame[2] = glm::vec4(glm::cross(t, y), 0.0f);
				frame[3] = glm::vec4(from + t * (ChainStart + float(i) * ChainPitch), 1.0f);
				AddKit(renderer, link, frame);
			}
		}
	}

	void ForgeArt::PlaceScatter(WorldRenderer& renderer)
	{
		std::vector<uint32_t> spires;
		for (const char* name : { "ObsidianCluster/ObsidianCluster_A", "ObsidianCluster/ObsidianCluster_B", "ObsidianCluster/ObsidianCluster_C" })
			if (const std::optional<uint32_t> mesh = renderer.GetModels().FindMesh(name))
			{
				renderer.AddMeshFlags(*mesh, MeshGround);
				spires.push_back(*mesh);
				for (uint32_t extra = 1; extra < 4; extra++)
					if (const std::optional<uint32_t> part = renderer.GetModels().FindMesh(std::string(name) + "." + std::to_string(extra)))
					{
						renderer.AddMeshFlags(*part, MeshGround);
						spires.push_back(*part | 0x80000000u);
					}
			}
		std::vector<std::vector<uint32_t>> kinds;
		for (size_t i = 0; i < spires.size(); i++)
			if ((spires[i] & 0x80000000u) == 0)
				kinds.push_back({ spires[i] });
			else if (!kinds.empty())
				kinds.back().push_back(spires[i] & 0x7FFFFFFFu);
		const size_t clusterKinds = kinds.size();
		for (const uint32_t mesh : m_ScatterMeshes)
			kinds.push_back({ mesh });
		if (kinds.empty())
			return;

		const auto blocked = [](Vec2 p)
		{
			for (size_t i = 0; i < std::size(ForgeIslandCenters); i++)
				if (Length(p - ForgeIslandCenters[i]) < ForgeIslandRadii[i] + 1.2f)
					return true;
			for (const ScatterKeepOut& keep : ScatterKeepOuts)
			{
				const Vec2 ab = keep.B - keep.A;
				const float t = std::clamp(Dot(p - keep.A, ab) / std::max(Dot(ab, ab), 1e-4f), 0.0f, 1.0f);
				if (Length(p - (keep.A + ab * t)) < keep.Radius)
					return true;
			}
			return false;
		};
		std::vector<Vec2> placed;
		uint32_t seed = 0x5EA5EEDu;
		const auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return float(seed >> 8) / float(1u << 24); };
		for (uint32_t attempt = 0; attempt < ScatterAttempts && placed.size() < ScatterCount; attempt++)
		{
			const float angle = next() * 2.0f * Pi;
			const float radius = ScatterInner + (ScatterOuter - ScatterInner) * std::sqrt(next());
			const Vec2 local(std::cos(angle) * radius, std::sin(angle) * radius);
			if (ForgeL1(local) < ScatterEdge || blocked(local))
				continue;
			bool crowded = false;
			for (const Vec2 other : placed)
				crowded = crowded || LengthSquared(other - local) < ScatterSpacing * ScatterSpacing;
			if (crowded)
				continue;
			float headroom = 1e9f;
			if (local.y < 0.0f && std::abs(local.x) < 18.0f)
				headroom = std::max(1.4f * (-(ForgeReach - std::abs(local.x)) - local.y) - 0.5f, 0.0f) - ForgePitSurface;
			const float pick = next();
			const bool cluster = clusterKinds > 0 && (m_ScatterMeshes.empty() || pick < ScatterClusterShare);
			const size_t kind = cluster ? size_t(next() * float(clusterKinds)) % clusterKinds : clusterKinds + size_t(next() * float(kinds.size() - clusterKinds)) % (kinds.size() - clusterKinds);
			const ModelMesh& info = renderer.GetModels().GetMesh(kinds[kind].back());
			const float tall = std::max(info.Max.y - info.Min.y, 0.5f);
			float scale = cluster ? 1.4f + 2.4f * next() : 0.8f + 0.7f * next();
			scale = std::min(scale, headroom / tall);
			if (scale < 0.5f)
				continue;
			placed.push_back(local);
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), World(local, ForgePitSurface - (cluster ? 0.25f : 0.35f) * scale));
			transform = glm::rotate(transform, next() * 2.0f * Pi, glm::vec3(0.0f, 1.0f, 0.0f));
			transform = glm::rotate(transform, (next() - 0.5f) * 0.25f, glm::vec3(1.0f, 0.0f, 0.0f));
			transform = glm::scale(transform, glm::vec3(scale));
			AddKit(renderer, kinds[kind], transform);
		}
	}

	void ForgeArt::BuildUnderlight(WorldRenderer& renderer) const
	{
		const uint32_t size = UnderlightSize;
		const float cell = UnderlightSpan / float(size);
		const Vec2 origin = ForgeCenter - Vec2(0.5f * UnderlightSpan, 0.5f * UnderlightSpan);
		std::vector<float> coverage(size_t(size) * size);
		for (uint32_t y = 0; y < size; y++)
			for (uint32_t x = 0; x < size; x++)
			{
				const Vec2 local = origin + Vec2((float(x) + 0.5f) * cell, (float(y) + 0.5f) * cell) - ForgeCenter;
				bool lava = ForgeL1(local) > ForgeReach + 0.3f;
				for (size_t i = 0; i < std::size(ForgeIslandCenters) && lava; i++)
					lava = Length(local - ForgeIslandCenters[i]) > ForgeIslandRadii[i] * 0.85f;
				coverage[size_t(y) * size + x] = lava ? 1.0f : 0.0f;
			}
		for (uint32_t pass = 0; pass < 3; pass++)
		{
			std::vector<float> blurred(coverage.size());
			for (uint32_t y = 0; y < size; y++)
				for (uint32_t x = 0; x < size; x++)
				{
					float sum = 0.0f;
					for (int32_t dy = -1; dy <= 1; dy++)
						for (int32_t dx = -1; dx <= 1; dx++)
						{
							const uint32_t sx = uint32_t(std::clamp(int32_t(x) + dx, 0, int32_t(size) - 1));
							const uint32_t sy = uint32_t(std::clamp(int32_t(y) + dy, 0, int32_t(size) - 1));
							sum += coverage[size_t(sy) * size + sx];
						}
					blurred[size_t(y) * size + x] = sum / 9.0f;
				}
			coverage.swap(blurred);
		}
		std::vector<uint8_t> bytes(coverage.size());
		for (size_t i = 0; i < coverage.size(); i++)
			bytes[i] = uint8_t(std::clamp(coverage[i], 0.0f, 1.0f) * 255.0f + 0.5f);
		renderer.SetLavaLight(size, size, bytes, glm::vec2(origin.x, origin.y), glm::vec2(UnderlightSpan));
	}

	void ForgeArt::PlaceBackdrop(WorldRenderer& renderer)
	{
		for (const Vec2 spot : TitanLights)
			m_BackdropLights.push_back({ World(spot, TitanLightHeight), 2.0f });
		const std::string_view prefix = "ForgeBackdrop/";
		const std::vector<ModelMesh>& meshes = renderer.GetModels().GetData().Meshes;
		const ModelLibrary& models = renderer.GetModels();
		const auto textureOf = [&](const char* reference) { const std::optional<uint32_t> mesh = models.FindMesh(reference); return mesh ? meshes[*mesh].Texture : NoTexture; };
		const std::pair<uint32_t, std::optional<uint32_t>> retextures[] = {
			{ textureOf("ForgeBackdrop/Cliff0"), renderer.FindMaterialSlot(RockMaterial) },
			{ textureOf("ForgeBackdrop/Throne"), renderer.FindMaterialSlot(WallMaterial) },
		};
		const glm::mat4 arena = Frame(World({}, 0.0f), Vec2(0.0f, -1.0f));
		for (uint32_t mesh = 0; mesh < uint32_t(meshes.size()); mesh++)
		{
			const std::string_view name = meshes[mesh].Name;
			if (!name.starts_with(prefix))
				continue;
			const std::string_view part = name.substr(prefix.size());
			for (const auto& [texture, material] : retextures)
				if (material && texture != NoTexture && meshes[mesh].Texture == texture)
				{
					renderer.SetMeshMaterial(mesh, *material);
					renderer.AddMeshFlags(mesh, MeshCliff);
				}
			if (part.starts_with("Scatter"))
			{
				renderer.AddMeshFlags(mesh, MeshGround);
				m_ScatterMeshes.push_back(mesh);
				continue;
			}
			if (part.starts_with("Crucible"))
			{
				m_CrucibleParts.push_back(mesh);
				continue;
			}
			if (part.starts_with("BellowsPump"))
			{
				renderer.AddMeshFlags(mesh, MeshGround);
				const ModelMesh& info = meshes[mesh];
				m_Pumps.push_back({ mesh, std::min(glm::vec3(arena * glm::vec4(info.Min, 1.0f)).y, glm::vec3(arena * glm::vec4(info.Max, 1.0f)).y), float(m_Pumps.size()) * 2.1f });
				continue;
			}
			if (!part.starts_with("Cradle"))
				renderer.AddMeshFlags(mesh, MeshGround);
			AddKit(renderer, { mesh }, arena);
			if (part.starts_with("Furnace") || part.starts_with("VatMelt") || part.starts_with("ChannelMelt"))
			{
				const ModelMesh& info = meshes[mesh];
				const glm::vec3 middle = glm::vec3(arena * glm::vec4(0.5f * (info.Min + info.Max), 1.0f));
				const Vec2 inward = Normalize(Vec2(ForgeCenter.x - middle.x, ForgeCenter.y - middle.z));
				const bool vat = !part.starts_with("Furnace");
				m_BackdropLights.push_back({ middle + glm::vec3(inward.x, 0.0f, inward.y) * BackdropLightInset + glm::vec3(0.0f, vat ? 1.2f : 0.0f, 0.0f), vat ? 1.0f : 0.0f });
				if (!vat)
					m_SmokeSpots.push_back(middle + glm::vec3(inward.x, 0.0f, inward.y) * SmokeInset + glm::vec3(0.0f, 0.5f * (info.Max.y - info.Min.y) + 0.6f, 0.0f));
			}
		}
	}

	float ForgeArt::PourSeconds(uint32_t pour) const
	{
		return (ForgePourAt(pour).HiltOnly ? m_HiltPath : m_LongestPath) / FlowSpeed;
	}

	float ForgeArt::CrucibleTilt(float clock) const
	{
		const uint32_t pours = ForgePourCount(clock);
		if (pours == 0)
			return 0.0f;
		const uint32_t pour = pours - 1;
		const float t = clock - ForgePourAt(pour).Tilt;
		if (t < PourTiltSeconds)
			return Smooth(t / PourTiltSeconds);
		const float poured = t - PourTiltSeconds - PourSeconds(pour);
		return poured <= 0.0f ? 1.0f : 1.0f - Smooth(poured / CrucibleReturn);
	}

	bool ForgeArt::IsPouring(float clock) const
	{
		const uint32_t pours = ForgePourCount(clock);
		if (pours == 0)
			return false;
		const float t = clock - PourLanding(pours - 1);
		return t >= -0.35f && t <= PourSeconds(pours - 1);
	}

	glm::mat4 ForgeArt::CrucibleTransform(float clock) const
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), World(CruciblePivot, CrucibleHeight));
		return glm::rotate(transform, -CrucibleTiltMax * CrucibleTilt(clock), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 ForgeArt::PourLip(float clock) const
	{
		return glm::vec3(CrucibleTransform(clock) * glm::vec4(0.0f, SpoutLift, -SpoutReach, 1.0f));
	}

	glm::vec3 ForgeArt::PourFoot() const
	{
		const glm::vec3 lip = FullTiltLip();
		return { lip.x, TrenchFloor, lip.z - PourShape.Throw };
	}

	float ForgeArt::GroundHeight(const BrawlStatus* brawl, float clock, Vec2 position) const
	{
		const Vec2 local = ForgeLocal(position);
		const bool crust = brawl && OnCrust(brawl->Plan, clock, position);
		if (ForgeL1(local) > ForgeReach)
			return ForgePitSurface + (crust ? 0.03f : -0.1f);
		const float surface = ForgeSurfaceHeight(local);
		const bool flooded = brawl && !brawl->Plan.Steady && ForgeL1(local) >= FloodLine(clock);
		if (flooded)
			return InTrench(local) && !OnBridge(local) ? MetalHotLevel + (crust ? 0.03f : 0.0f) : surface;
		if (InTrench(local) && !OnBridge(local))
			return crust ? MetalHotLevel + 0.03f : TrenchFloor;
		return surface;
	}

	void ForgeArt::DrawMetal(WorldRenderer& renderer, float clock, float line) const
	{
		const glm::mat4 identity(1.0f);
		if (line <= ForgeReach)
			renderer.DrawModel(m_Metal, identity, NoPalette, glm::vec4(1.0f / FlowSpeed, NeverCovered, FloodMode, line), 0.0f, ForgePitSurface, glm::vec4(FloodTimes[0], OpenReach, NeverCovered, clock));
		int32_t latest = -1;
		for (uint32_t pour = 0; pour < ForgePourCount(clock); pour++)
			if (PourLanding(pour) <= clock)
				latest = int32_t(pour);
		if (latest >= 0)
		{
			const auto reach = [](int32_t pour) { return ForgePourAt(uint32_t(pour)).HiltOnly ? -OpenReach : OpenReach; };
			const glm::vec4 shape(1.0f / FlowSpeed, MetalHotSeconds, 1.0f, line);
			const auto draw = [&](int32_t pour, float coveredFrom)
			{
				const glm::vec4 accent(PourLanding(uint32_t(pour)), reach(pour), coveredFrom, clock);
				renderer.DrawModel(m_Metal, identity, NoPalette, shape, 0.0f, ForgePitSurface, accent);
				if (ForgePourAt(uint32_t(pour)).HiltOnly)
					renderer.DrawModel(m_GateSkirt, identity, NoPalette, shape, 0.0f, ForgePitSurface, accent + glm::vec4(0.0f, 0.0f, SkirtLinger, 0.0f));
			};
			const float newest = PourLanding(uint32_t(latest));
			draw(latest, NeverCovered);
			if (latest > 0)
				draw(latest - 1, newest);
		}
		for (const ForgeAnchor& anchor : m_Anchors)
		{
			const Vec2 local = LocalOf(anchor.Position);
			const float age = ForgeL1(local) >= line ? -1.0f : MetalFrontAge(local, anchor.Meters, clock);
			if (age < 0.0f || age >= MetalHotSeconds + 0.5f)
				continue;
			const float heat = (1.0f - Smooth((age - (MetalHotSeconds - MetalCoolWarning)) / (MetalCoolWarning + 0.5f))) * (1.0f + 1.5f * std::exp(-age * 3.0f));
			renderer.DrawLight({ .Position = anchor.Position, .Radius = MetalLightRadius, .Color = MetalLight * heat });
		}
	}

	void ForgeArt::DrawFloods(WorldRenderer& renderer, float clock, bool steady) const
	{
		if (steady)
			return;
		const glm::mat4 identity(1.0f);
		for (uint32_t band = 0; band < ForgeBands; band++)
		{
			const float start = FloodTimes[band] - FloodWarning;
			const float covered = FloodTimeAt(Vec2(0.0f, ForgeRingEdges[ForgeGrooveRings[band] + 1])) + WarnLinger;
			if (clock >= start && clock < covered)
			{
				const float t = std::min(clock - start, FloodWarning);
				const float warn = t / FloodWarning;
				const float blink = 0.55f + 0.45f * std::cos(2.0f * Pi * (WarnRateStart * t + 0.5f * WarnRateGain * t * t));
				const float pulse = Smooth(std::min(warn * 3.0f, 1.0f)) * (blink + (1.0f - blink) * Smooth((warn - 0.6f) / 0.4f));
				renderer.DrawModel(m_Grooves[band], identity, NoPalette, WarnTint, pulse, ForgePitSurface);
			}
		}
		for (const FloodSheet& sheet : m_Floods)
			if (clock >= FloodTimes[sheet.Band])
				renderer.DrawModel(sheet.Mesh, identity, NoPalette, glm::vec4(1.0f, 1e9f, 0.0f, 0.0f), 0.0f, ForgePitSurface, glm::vec4(0.0f, 1e9f, NeverCovered, clock));
		const float line = FloodLine(clock);
		for (uint32_t band = 0; band < ForgeBands; band++)
		{
			const float middle = 0.5f * (ForgeBandEdges[band] + ForgeBandEdges[band + 1]);
			const float since = clock - FloodTimes[band] - FloodSpreads[band];
			if (clock < FloodTimes[band] || line > middle || since >= FloodLightFade)
				continue;
			const float share = FloodLightShare * (1.0f - Smooth(since / FloodLightFade));
			const float height = ForgeRingHeights[RingOf(Vec2(0.0f, middle))] + 0.8f;
			if (band + 1 == ForgeBands)
			{
				renderer.DrawLight({ .Position = World({}, height), .Radius = FloodLightRadius, .Color = MetalLight * share });
				continue;
			}
			const Vec2 points[] = { { 0.5f * middle, 0.5f * middle }, { -0.5f * middle, 0.5f * middle }, { 0.5f * middle, -0.5f * middle }, { -0.5f * middle, -0.5f * middle } };
			for (const Vec2 point : points)
				renderer.DrawLight({ .Position = World(point, height), .Radius = FloodLightRadius, .Color = MetalLight * share });
		}
	}

	float ForgeArt::MeltFill(float clock) const
	{
		const uint32_t pours = ForgePourCount(clock);
		if (pours == 0)
			return MeltFull;
		const uint32_t pour = pours - 1;
		const float landing = PourLanding(pour);
		const float duration = PourSeconds(pour);
		const float drained = Smooth((clock - landing) / std::max(duration, 0.1f));
		const float refilled = Smooth((clock - (landing + duration + CrucibleReturn)) / MeltRefill);
		return MeltFull - MeltDrain * drained * (1.0f - refilled);
	}

	void ForgeArt::DrawMelt(WorldRenderer& renderer, float clock, const glm::mat4& crucible) const
	{
		const float tilt = CrucibleTiltMax * CrucibleTilt(clock);
		const glm::vec3 pivot = World(CruciblePivot, CrucibleHeight);
		const glm::vec3 lip = glm::vec3(crucible * glm::vec4(0.0f, MeltLipHeight, -MeltLipReach, 1.0f));
		const float level = std::min(pivot.y + MeltFill(clock) * std::cos(tilt), lip.y - MeltLipDrop);
		const glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(pivot.x, level, pivot.z)), glm::vec3(MeltSpan, 1.0f, MeltSpan));
		renderer.DrawModel(m_Melt, transform, NoPalette, glm::vec4(IsPouring(clock) ? 1.0f : 0.0f, NeverCovered, CrucibleMode, clock), 0.0f, ForgePitSurface, glm::vec4(pivot, tilt));
	}

	float ForgeArt::GateLift(float clock) const
	{
		return GateTravel * Smooth((clock - m_GateOpen) / GateRise);
	}

	glm::vec3 ForgeArt::GetGatePosition() const
	{
		return World(Vec2(0.0f, ForgeHiltLine), CurbTop + 1.0f);
	}

	void ForgeArt::DrawPour(WorldRenderer& renderer, const LevelView& view, float clock) const
	{
		const glm::mat4 identity(1.0f);
		const glm::mat4 crucible = CrucibleTransform(clock);
		if (m_CrucibleParts.empty())
			renderer.DrawModel(m_Crucible, crucible, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface);
		for (const uint32_t part : m_CrucibleParts)
			renderer.DrawModel(part, crucible, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface);
		if (!m_CrucibleParts.empty())
			renderer.DrawLight({ .Position = glm::vec3(crucible * glm::vec4(0.0f, CrucibleGlowLift, 0.0f, 1.0f)), .Radius = CrucibleGlowRadius, .Color = MetalLight * CrucibleGlowShare });
		DrawMelt(renderer, clock, crucible);
		if (!IsPouring(clock))
			return;
		const glm::vec3 lip = FullTiltLip();
		const glm::vec3 foot = PourFoot();
		const float grow = std::clamp((clock - PourLanding(ForgePourCount(clock) - 1) + 0.35f) / 0.35f, 0.05f, 1.0f);
		const glm::mat4 stream = glm::translate(glm::scale(glm::translate(identity, lip), glm::vec3(1.0f, grow, 1.0f)), -lip);
		renderer.DrawModel(m_Stream, stream, NoPalette, CoreTint, 1.0f, ForgePitSurface);
		ModelInstance sheath = renderer.MakeModelInstance(m_StreamFlow, stream, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface);
		sheath.Shading.z = view.Time;
		renderer.SubmitModel(m_StreamFlow, sheath);
		renderer.DrawLight({ .Position = 0.5f * (lip + foot), .Radius = PourLightRadius, .Color = MetalLight * 1.2f });
	}

	void ForgeArt::Draw(WorldRenderer& renderer, const LevelView& view) const
	{
		if (!m_Built)
			return;
		const glm::mat4 identity(1.0f);
		const float clock = view.Brawl ? view.Clock : -1e5f;
		const bool steady = view.Brawl && view.Brawl->Plan.Steady;
		for (const KitItem& item : m_Kit)
		{
			if (item.High.x < view.Min.x || item.Low.x > view.Max.x || item.High.y < view.Min.y || item.Low.y > view.Max.y)
				continue;
			const bool outside = item.High.x < view.CameraMin.x || item.Low.x > view.CameraMax.x || item.High.y < view.CameraMin.y || item.Low.y > view.CameraMax.y;
			if (item.Band >= 0 && view.Brawl && !steady && clock >= FloodTimes[item.Band] - FloodWarning)
			{
				ModelInstance heated = item.Instance;
				heated.Info.w |= MeshHeated;
				heated.Tint.a = item.Heat;
				heated.Accent = glm::vec4(FloodTimes[item.Band], FloodSpreads[item.Band] / (ForgeBandEdges[item.Band] - ForgeBandEdges[item.Band + 1]), clock, ForgeBandEdges[item.Band]);
				renderer.SubmitModel(item.Mesh, heated, outside);
			}
			else
				renderer.SubmitModel(item.Mesh, item.Instance, outside);
		}
		for (const glm::vec4& light : m_BackdropLights)
		{
			const Vec2 at(light.x, light.z + std::max(light.y, 0.0f) * RaisedShift);
			if (at.x + BackdropLightRadius < view.Min.x || at.x - BackdropLightRadius > view.Max.x || at.y + BackdropLightRadius < view.Min.y || at.y - BackdropLightRadius > view.Max.y)
				continue;
			const float flicker = 0.85f + 0.15f * std::sin(view.Time * 7.3f + light.x * 1.7f) * std::sin(view.Time * 3.1f + light.z);
			const float share = light.w > 1.5f ? TitanLightShare : light.w > 0.5f ? VatLightShare : FurnaceLightShare;
			renderer.DrawLight({ .Position = glm::vec3(light), .Radius = BackdropLightRadius, .Color = MetalLight * share * flicker });
		}
		renderer.DrawModel(m_FallCore, identity, NoPalette, CoreTint, 1.0f, ForgePitSurface);
		ModelInstance falls = renderer.MakeModelInstance(m_FallFlow, identity, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface);
		falls.Shading.z = view.Time;
		renderer.SubmitModel(m_FallFlow, falls);
		const glm::mat4 arena = Frame(World({}, 0.0f), Vec2(0.0f, -1.0f));
		for (const Pump& pump : m_Pumps)
		{
			const float squeeze = 1.0f - PumpStroke * (0.5f + 0.5f * std::sin(view.Time * PumpRate + pump.Phase));
			const glm::mat4 lift = glm::translate(identity, glm::vec3(0.0f, pump.Pivot, 0.0f)) * glm::scale(identity, glm::vec3(1.0f, squeeze, 1.0f)) * glm::translate(identity, glm::vec3(0.0f, -pump.Pivot, 0.0f));
			renderer.DrawModel(pump.Mesh, lift * arena, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface);
		}
		DrawMetal(renderer, clock, steady ? ForgeReach + 1.0f : FloodLine(clock));
		const glm::mat4 gate = glm::translate(identity, World(Vec2(0.0f, ForgeHiltLine - 0.5f * SluiceThickness), TrenchFloor + GateLift(clock)));
		for (const uint32_t part : m_GateParts)
			renderer.DrawModel(part, gate, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, ForgePitSurface);
		DrawFloods(renderer, clock, steady);
		DrawPour(renderer, view, clock);
		if (view.Brawl)
			for (const Crust& crust : CrustsOf(view.Brawl->Plan))
			{
				const float age = clock - crust.Time;
				if (age < 0.0f || age >= CrustSeconds)
					continue;
				const float melt = std::clamp((CrustSeconds - age) / CrustMelt, 0.0f, 1.0f);
				const Vec2 local = ForgeLocal(crust.Center);
				float height = MetalHotLevel;
				if (ForgeL1(local) > ForgeReach)
					height = ForgePitSurface;
				else if (!steady && ForgeL1(local) >= FloodLine(clock) && !(InTrench(local) && !OnBridge(local)))
					height = ForgeSurfaceHeight(local);
				const float radius = crust.Radius * (0.6f + 0.4f * melt);
				const glm::mat4 transform = glm::scale(glm::translate(identity, glm::vec3(crust.Center.x, height + 0.02f, crust.Center.y)), glm::vec3(radius, 1.0f, radius));
				renderer.DrawModel(m_Disc, transform, NoPalette, CrustTint, 0.06f, ForgePitSurface);
			}
	}
}
