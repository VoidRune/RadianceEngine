#pragma once
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include <GameShared/Net/Protocol.h>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace Game
{
	class EffectSystem;

	inline constexpr uint32_t CrawlerLegs = 6;
	inline constexpr uint32_t CrawlerBuds = 3;

	bool IsCrawlerKind(EntityKind kind);
	glm::vec3 CrawlerTone(EntityKind kind, float time);
	float CrawlerScale(EntityKind kind);

	struct CrawlerGait
	{
		glm::vec3 Base{ 0.0f };
		glm::vec2 Velocity{ 0.0f };
		float Facing = 0.0f;
		float Turn = 0.0f;
		float Cycle = 0.0f;
		float Crouch = 0.0f;
		float Flare = 0.0f;
		float Windup = 0.0f;
		float Sprout = 1.0f;
		float Burrow = 0.0f;
		float Lunge = 0.0f;
		float Shield = 0.0f;
		float Scale = 1.0f;
		float Idle = 0.0f;
		float Seed = 0.0f;
		std::array<glm::vec3, CrawlerLegs> Feet{};
		std::array<glm::vec3, CrawlerLegs> From{};
		std::array<float, CrawlerLegs> Step{};
		bool Placed = false;
	};

	struct CrawlerPose
	{
		glm::mat4 Body{ 1.0f };
		glm::mat4 Crystal{ 1.0f };
		glm::mat4 Seat{ 1.0f };
		std::array<glm::mat4, CrawlerLegs> Femurs{};
		std::array<glm::mat4, CrawlerLegs> Tibias{};
		glm::vec3 Tip{ 0.0f };
		glm::vec3 Core{ 0.0f };
	};

	class CrawlerSystem
	{
	public:
		using GroundSampler = std::function<float(Vec2)>;

		explicit CrawlerSystem(const ModelLibrary& models);
		bool IsReady() const { return m_Ready; }
		void Clear();
		void Update(float dt, std::span<const EntityState> entities, const GroundSampler& ground);
		void Vanish(const EntityState& last, bool killed, EffectSystem& effects);
		void UpdateOrbs(float dt, std::span<const Orb> orbs, std::span<const EntityState> entities, EffectSystem& effects, const GroundSampler& ground);
		void Draw(WorldRenderer& renderer, const EntityState& entity, float time, const glm::vec4& flash) const;
		void DrawTethers(WorldRenderer& renderer, std::span<const EntityState> entities, float time) const;
		void DrawRemains(WorldRenderer& renderer, float time) const;
		void DrawOrbs(WorldRenderer& renderer, float time) const;
		void DrawShard(WorldRenderer& renderer, const glm::vec3& position, float facing, const glm::vec3& tone, float time) const;
		void DrawRift(WorldRenderer& renderer, const glm::vec3& base, float time, bool open) const;
		std::optional<glm::vec3> CrystalTip(uint32_t netId) const;

	private:
		struct Corpse
		{
			EntityKind Kind = EntityKind::QuartzCrawler;
			CrawlerGait Gait;
			std::array<glm::vec3, CrawlerLegs> Tucked{};
			float Age = 0.0f;
			uint32_t Seed = 0;
		};

		struct Debris
		{
			uint32_t Chunk = 0;
			glm::mat4 Basis{ 1.0f };
			glm::vec3 Position{ 0.0f };
			glm::vec3 Velocity{ 0.0f };
			glm::vec3 Axis{ 0.0f, 1.0f, 0.0f };
			glm::vec3 Tone{ 1.0f };
			float Angle = 0.0f;
			float Spin = 0.0f;
			float Age = 0.0f;
			float Life = 1.0f;
			float Floor = 0.0f;
		};

		struct OrbView
		{
			glm::vec3 Shown{ 0.0f };
			glm::vec3 Target{ 0.0f };
			uint16_t Value = 1;
			float Age = 0.0f;
			bool Seen = false;
		};

		CrawlerPose Pose(const CrawlerGait& gait) const;
		void DrawBody(WorldRenderer& renderer, EntityKind kind, const CrawlerGait& gait, const CrawlerPose& pose, float time, const glm::vec4& flash, const ModelDissolve& dissolve, bool crystal) const;
		void DrawEgg(WorldRenderer& renderer, const EntityState& entity, const CrawlerGait& gait, float time, const glm::vec4& flash) const;
		void Shatter(EntityKind kind, const CrawlerPose& pose, float scale, float floor, uint32_t seed);

		uint32_t m_Shell = 0;
		uint32_t m_Eyes = 0;
		std::array<uint32_t, CrawlerLegs> m_Femurs{};
		std::array<uint32_t, CrawlerLegs> m_Tibias{};
		uint32_t m_Crystal = 0;
		glm::vec3 m_CrystalTip{ 0.0f };
		uint32_t m_EggShell = 0;
		uint32_t m_EggGem = 0;
		uint32_t m_ShardMesh = 0;
		std::array<uint32_t, CrawlerBuds> m_Buds{};
		std::vector<uint32_t> m_Chunks;
		std::vector<glm::vec3> m_ChunkCentres;
		std::array<float, CrawlerLegs> m_FemurLengths{};
		std::array<float, CrawlerLegs> m_TibiaLengths{};
		bool m_Ready = false;
		std::unordered_map<uint32_t, CrawlerGait> m_Gaits;
		std::vector<Corpse> m_Corpses;
		std::vector<DiamondShard> m_Shards;
		std::vector<Debris> m_Debris;
		std::unordered_map<uint16_t, OrbView> m_Orbs;
	};
}
