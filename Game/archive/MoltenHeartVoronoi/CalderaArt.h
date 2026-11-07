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
	class IslandArt;

	struct CalderaVent
	{
		Vec2 Position;
		glm::vec3 Mouth{ 0.0f };
		bool Alive = true;
	};

	struct CalderaChain
	{
		glm::vec3 Wall{ 0.0f };
		glm::vec3 Anchor{ 0.0f };
		float Snapped = -1.0f;
	};

	class CalderaArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		bool IsBuilt() const { return m_Built; }
		void Draw(WorldRenderer& renderer, const LevelView& view, const IslandArt& island) const;
		const std::vector<CalderaVent>& GetVents() const { return m_VentState; }
		const std::vector<CalderaChain>& GetChains() const { return m_ChainState; }
		glm::vec3 GetConeMouth() const { return m_ConeMouth; }
		const std::vector<glm::vec3>& GetFallFeet() const { return m_FallFeet; }
		float WallRadius(float angle) const;

	private:
		struct Patch
		{
			uint32_t Mesh = 0;
			Vec2 Min{ 1e9f, 1e9f };
			Vec2 Max{ -1e9f, -1e9f };
		};

		struct Piece
		{
			PropId Id = PropId::Count;
			Vec2 Position;
			float Yaw = 0.0f;
			std::vector<uint32_t> Meshes;
			float Scale = 1.0f;
			float Stretch = 1.0f;
		};

		struct Anchor
		{
			Vec2 Position;
			glm::vec3 Ring{ 0.0f };
			glm::vec3 Wall{ 0.0f };
			Vec2 Inward;
		};

		void BuildWall(WorldRenderer& renderer);
		void BuildFalls(WorldRenderer& renderer);
		void UpdateLavaLight(WorldRenderer& renderer, const IslandArt& island) const;
		void DrawChain(WorldRenderer& renderer, const glm::vec3& from, const glm::vec3& to, float sag, float scale) const;

		bool m_Built = false;
		Vec2 m_Center;
		uint32_t m_Seed = 0;
		std::vector<Patch> m_Wall;
		std::vector<Patch> m_Falls;
		std::vector<glm::vec3> m_FallFeet;
		std::vector<Piece> m_Pieces;
		std::vector<Anchor> m_Anchors;
		std::vector<uint32_t> m_VentMeshes;
		std::vector<uint32_t> m_AnchorMeshes;
		uint32_t m_WallPlate = 0;
		uint32_t m_ConeMesh = 0;
		uint32_t m_CrustMesh = 0;
		Vec2 m_Cone;
		glm::vec3 m_ConeMouth{ 0.0f };
		std::optional<uint32_t> m_ChainLink;
		std::vector<uint32_t> m_Obsidian;
		std::vector<uint32_t> m_Spires;
		mutable std::vector<CalderaVent> m_VentState;
		mutable std::vector<CalderaChain> m_ChainState;
		mutable std::vector<int32_t> m_LightChunks;
		mutable uint64_t m_LightKey = 0;
	};
}
