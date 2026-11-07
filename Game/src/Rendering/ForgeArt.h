#pragma once
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/Brawl.h>
#include <GameShared/World/Level.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	struct LevelView;

	struct ForgeAnchor
	{
		glm::vec3 Position{ 0.0f };
		float Meters = 0.0f;
	};

	class ForgeArt
	{
	public:
		void Build(WorldRenderer& renderer, const Level& level);
		void Clear();
		bool IsBuilt() const { return m_Built; }
		void Draw(WorldRenderer& renderer, const LevelView& view) const;
		float GroundHeight(const BrawlStatus* brawl, float clock, Vec2 position) const;
		float CrucibleTilt(float clock) const;
		bool IsPouring(float clock) const;
		glm::vec3 PourLip(float clock) const;
		glm::vec3 PourFoot() const;
		const std::vector<ForgeAnchor>& GetAnchors() const { return m_Anchors; }
		float GetGateOpen() const { return m_GateOpen; }
		glm::vec3 GetGatePosition() const;
		const std::vector<glm::vec3>& GetSmokeSpots() const { return m_SmokeSpots; }
		const std::vector<glm::vec3>& GetFallSpots() const { return m_FallSpots; }

	private:
		struct KitItem
		{
			uint32_t Mesh = 0;
			ModelInstance Instance;
			Vec2 Low;
			Vec2 High;
			int32_t Band = -1;
			float Heat = 1.0f;
		};

		struct Pump
		{
			uint32_t Mesh = 0;
			float Pivot = 0.0f;
			float Phase = 0.0f;
		};

		struct FloodSheet
		{
			uint32_t Mesh = 0;
			uint32_t Band = 0;
		};

		void PlaceArena(WorldRenderer& renderer);
		void PlaceBackdrop(WorldRenderer& renderer);
		void BuildUnderlight(WorldRenderer& renderer) const;
		void PlaceScatter(WorldRenderer& renderer);
		void AddKit(const WorldRenderer& renderer, const std::vector<uint32_t>& meshes, const glm::mat4& transform, int32_t band = -1, float heat = 1.0f);
		glm::mat4 CrucibleTransform(float clock) const;
		float PourSeconds(uint32_t pour) const;
		void DrawMetal(WorldRenderer& renderer, float clock, float line) const;
		void DrawFloods(WorldRenderer& renderer, float clock, bool steady) const;
		void DrawPour(WorldRenderer& renderer, const LevelView& view, float clock) const;
		float MeltFill(float clock) const;
		void DrawMelt(WorldRenderer& renderer, float clock, const glm::mat4& crucible) const;
		float GateLift(float clock) const;

		bool m_Built = false;
		std::vector<FloodSheet> m_Floods;
		std::array<uint32_t, ForgeBands> m_Grooves{};
		uint32_t m_Metal = 0;
		uint32_t m_Crucible = 0;
		std::vector<uint32_t> m_CrucibleParts;
		std::vector<glm::vec4> m_BackdropLights;
		uint32_t m_Stream = 0;
		uint32_t m_StreamFlow = 0;
		uint32_t m_Disc = 0;
		uint32_t m_Melt = 0;
		uint32_t m_GateSkirt = 0;
		uint32_t m_FallCore = 0;
		uint32_t m_FallFlow = 0;
		std::vector<Pump> m_Pumps;
		std::vector<uint32_t> m_ScatterMeshes;
		std::vector<glm::vec3> m_SmokeSpots;
		std::vector<glm::vec3> m_FallSpots;
		std::vector<uint32_t> m_GateParts;
		float m_GateOpen = 1e9f;
		std::vector<KitItem> m_Kit;
		std::vector<ForgeAnchor> m_Anchors;
		float m_LongestPath = 0.0f;
		float m_HiltPath = 0.0f;
	};
}
