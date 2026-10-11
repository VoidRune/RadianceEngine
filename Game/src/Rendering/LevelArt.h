#pragma once
#include "Rendering/WorldRenderer.h"
#include <GameShared/World/Level.h>
#include <cstdint>
#include <vector>

namespace Game
{
	struct SceneItem
	{
		uint32_t Mesh = 0;
		ModelInstance Instance{};
		Vec2 Center;
		float Reach = 0.0f;
		float Base = 0.0f;
		float Top = 0.0f;
		bool Cut = false;
	};

	struct SceneBox
	{
		MeshKind Mesh = MeshKind::Box;
		glm::vec3 Center{ 0.0f };
		glm::vec3 Half{ 0.0f };
		glm::vec3 Color{ 1.0f };
		float Yaw = 0.0f;
		float Emission = 0.0f;
		float Base = 0.0f;
		float Reach = 0.0f;
		bool Cut = false;
		bool ShadowOnly = false;
	};

	enum class FireKind : uint8_t
	{
		Campfire,
		Lantern,
		Candle,
	};

	struct LevelFire
	{
		glm::vec3 Position{ 0.0f };
		float Scale = 1.0f;
		FireKind Kind = FireKind::Campfire;
	};

	struct SceneChunk
	{
		Vec2 Min{ 1e9f, 1e9f };
		Vec2 Max{ -1e9f, -1e9f };
		std::vector<SceneItem> Items;
		std::vector<SceneBox> Boxes;
	};

	enum CellArt : uint8_t
	{
		CellTop = 1 << 0,
		CellSides = 1 << 1,
	};

	struct LevelScene
	{
		std::vector<SceneChunk> Chunks;
		std::vector<uint8_t> Cells;
		std::vector<uint8_t> PropDrawn;
		std::vector<uint8_t> WallDrawn;
		std::vector<LevelFire> Fires;
		bool LevelGeometry = false;
	};

	inline constexpr float FloorTop = 0.025f;
	inline constexpr float FloorBottom = 0.075f;

	LevelScene BuildLevelScene(const Level& level, const WorldRenderer& renderer);
	bool BuildAbbeyScene(const Level& level, WorldRenderer& renderer, LevelScene& scene);
}
