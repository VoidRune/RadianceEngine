#pragma once
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/Brawl.h>
#include <GameShared/World/Level.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>
#include <vector>

namespace Game
{
	struct LevelView;

	inline constexpr float LakeIceTop = 0.06f;
	inline constexpr float LakeWaterLine = -0.62f;
	inline constexpr float LakeWading = -0.74f;
	inline constexpr float LeviathanRoundChance = 0.021f;
	inline constexpr float LeviathanEarliest = 10.0f;
	inline constexpr float LeviathanWindow = 14.0f;
	inline constexpr float LeviathanSpeed = 3.4f;
	inline constexpr float LeviathanLength = 24.0f;

	struct LeviathanPose
	{
		bool Active = false;
		glm::vec3 Head{ 0.0f };
		glm::vec2 Heading{ 1.0f, 0.0f };
		float Progress = 0.0f;
		float Near = 0.0f;
	};

	class LakeShore
	{
	public:
		void Build(const Level& level);
		float Distance(Vec2 position) const;
		float Edge(Vec2 position) const;
		float Height(Vec2 position) const;
		float Floor(Vec2 position) const;

	private:
		std::vector<float> m_Distance;
		int32_t m_Width = 0;
		int32_t m_Height = 0;
	};

	void RegisterLakeMeshes(WorldRenderer& renderer);

	class LakeArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		bool IsBuilt() const { return m_Width > 0; }
		void Draw(WorldRenderer& renderer, const LevelView& view) const;
		const LeviathanPose& GetLeviathan() const { return m_Leviathan; }
		const std::vector<Vec2>& GetHoles() const { return m_Holes; }

	private:
		struct Feature
		{
			CellCoord Chunk;
			uint32_t Mesh = 0;
			glm::vec3 Pivot{ 0.0f };
			Vec2 Min{ 1e9f, 1e9f };
			Vec2 Max{ -1e9f, -1e9f };
		};

		struct Item
		{
			uint32_t Mesh = 0;
			ModelInstance Instance{};
		};

		struct Floe
		{
			uint32_t Mesh = 0;
			Vec2 Position;
			float Yaw = 0.0f;
			float Scale = 1.0f;
			float Phase = 0.0f;
			int32_t Tile = -1;
		};

		struct School
		{
			Vec2 Center;
			float Depth = 0.0f;
			float Radius = 0.0f;
			float Speed = 0.0f;
			float Phase = 0.0f;
			float Size = 1.0f;
			uint32_t First = 0;
			uint32_t Count = 0;
			glm::vec3 Color{ 0.0f };
		};

		struct Jelly
		{
			glm::vec3 Position{ 0.0f };
			float Scale = 1.0f;
			float Phase = 0.0f;
			glm::vec3 Color{ 0.0f };
		};

		enum class LeviathanPart : uint8_t { Spine, Head, Jaw, Pectoral, Hind, Whip, Ribbon };
		struct LeviathanJoint
		{
			LeviathanPart Part = LeviathanPart::Spine;
			int32_t Parent = -1;
			float Side = 1.0f;
			float Link = 0.0f;
			float Behind = 0.0f;
			float Seed = 0.0f;
			glm::vec3 Rest{ 0.0f };
		};

		struct Drifter
		{
			uint32_t Mesh = 0;
			float Start = 0.0f;
			float Lateral = 0.0f;
			float Yaw = 0.0f;
			float Scale = 1.0f;
			float Phase = 0.0f;
		};

		struct Patch
		{
			uint32_t Mesh = 0;
			Vec2 Min;
			Vec2 Max;
		};

		struct WallSpot
		{
			glm::vec3 Position{ 0.0f };
			glm::vec3 Outward{ 0.0f };
		};

		void BuildTrench();
		void BuildRoute();
		float GorgeHalf(Vec2 local) const;
		float GorgeHalf(Vec2 local, float along) const;
		float LandHeight(Vec2 position) const;
		float BankRim(Vec2 position) const;
		void BuildWalls(WorldRenderer& renderer, const Level& level);
		void BuildFlora(WorldRenderer& renderer, const Level& level);
		void BuildShoreIce(WorldRenderer& renderer, const Level& level);
		static void AddPatches(WorldRenderer& renderer, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices, const LevelMeshDesc& desc, float size, std::vector<Patch>& patches);
		bool Settle(const WorldRenderer& renderer, std::string_view name, glm::mat4& transform, Vec2 local, float sink, bool spanCheck) const;
		Vec2 RoutePoint(float along) const;
		float RouteDepth(float along) const;
		Vec2 TrenchPoint(float along) const;
		float TrenchDistance(Vec2 local, float* along = nullptr) const;
		float BedHeight(Vec2 local) const;
		void BuildHarbor(WorldRenderer& renderer, const Level& level);
		void BuildSnow(WorldRenderer& renderer, const Level& level);
		void DrawDrifters(WorldRenderer& renderer, const LevelView& view) const;
		void BuildLeviathanRig(const WorldRenderer& renderer);
		void BuildBed(WorldRenderer& renderer, const Level& level);
		void BuildRuins(WorldRenderer& renderer, const Level& level);
		void BuildFeatures(WorldRenderer& renderer, const Level& level);
		void BuildCreatures(const Level& level);
		void BuildShore(WorldRenderer& renderer, const Level& level);
		void DrawLeviathan(WorldRenderer& renderer, const LevelView& view) const;
		void DrawFloes(WorldRenderer& renderer, const LevelView& view) const;
		void BuildMounds(WorldRenderer& renderer, const Level& level);
		void BuildLand(WorldRenderer& renderer, const Level& level);
		void BuildBank(WorldRenderer& renderer, const Level& level);
		void AddItem(const WorldRenderer& renderer, std::string_view name, const glm::mat4& transform, const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
		void UpdateField(WorldRenderer& renderer, const LevelView& view) const;
		void DrawCreatures(WorldRenderer& renderer, const LevelView& view) const;

		int32_t m_Width = 0;
		int32_t m_Height = 0;
		Vec2 m_Center;
		uint32_t m_Seed = 0;
		float m_GraveSide = 1.0f;
		std::vector<TerrainId> m_Terrains;
		uint32_t m_Bed = UINT32_MAX;
		uint32_t m_Shafts = UINT32_MAX;
		LakeShore m_ShoreLevel;
		uint32_t m_Land = UINT32_MAX;
		uint32_t m_Bank = UINT32_MAX;
		std::optional<uint32_t> m_Fish;
		std::optional<uint32_t> m_JellyMesh;
		std::vector<uint32_t> m_LeviathanMeshes;
		LakeLayout m_Layout;
		std::vector<Vec2> m_Trench;
		std::vector<float> m_TrenchAlong;
		float m_TrenchLength = 0.0f;
		std::vector<Vec2> m_Route;
		std::vector<float> m_RouteAlong;
		std::vector<float> m_RouteDepth;
		float m_RouteLength = 0.0f;
		uint32_t m_Growth = UINT32_MAX;
		std::vector<Patch> m_Walls;
		std::vector<Patch> m_WallsUnder;
		std::vector<Patch> m_Flora;
		std::vector<Patch> m_FloraGlow;
		std::vector<Patch> m_ShoreIce;
		std::vector<WallSpot> m_WallSpots;
		std::vector<WallSpot> m_LedgeSpots;
		std::vector<std::pair<glm::vec3, glm::vec3>> m_Glows;
		uint32_t m_Snow = UINT32_MAX;
		std::vector<Drifter> m_Drifters;
		std::vector<LeviathanJoint> m_LeviathanJoints;
		std::vector<uint32_t> m_LeviathanSpine;
		std::vector<Feature> m_Features;
		std::vector<Item> m_Items;
		std::vector<Item> m_Above;
		std::vector<Floe> m_Floes;
		std::vector<std::pair<uint32_t, Vec2>> m_Mounds;
		std::vector<School> m_Schools;
		std::vector<glm::vec4> m_FishSlots;
		std::vector<Jelly> m_Jellies;
		mutable std::vector<Vec2> m_Holes;
		mutable LeviathanPose m_Leviathan;
		mutable std::vector<float> m_Times;
		mutable std::vector<float> m_Shelf;
		mutable std::vector<uint8_t> m_Alive;
		mutable std::vector<float> m_Thin;
		mutable std::vector<glm::vec4> m_Field;
		mutable CollapsePlan m_Plan{};
		mutable bool m_HavePlan = false;
	};
}
