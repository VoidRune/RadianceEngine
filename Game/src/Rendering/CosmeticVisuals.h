#pragma once
#include "Rendering/Characters.h"
#include <GameShared/Content/Cosmetics.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Game
{
	class EffectSystem;
	class ModelLibrary;

	struct CosmeticWearer
	{
		uint32_t Key = 0;
		Cosmetics Style;
		glm::vec3 Base{ 0.0f };
		float Facing = 0.0f;
		float Scale = 1.0f;
		bool Grounded = true;
		const CharacterPose* Pose = nullptr;
		const CharacterRig* Rig = nullptr;
	};

	class CosmeticVisuals
	{
	public:
		explicit CosmeticVisuals(const ModelLibrary& models);

		void Draw(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, float time);
		void Forget(float time);

	private:
		struct Footprint
		{
			glm::vec3 Position{ 0.0f };
			float Born = -100.0f;
			uint32_t Seed = 0;
		};

		struct CrownPiece
		{
			uint32_t Mesh = 0;
			float Degrees = 0.0f;
			float Phase = 0.0f;
		};

		struct CrownModel
		{
			std::vector<CrownPiece> Pieces;
			std::optional<uint32_t> Core;
			float CoreCenter = 0.0f;
		};

		struct Wearer
		{
			std::array<Footprint, 8> Prints{};
			float Seen = -1.0f;
			glm::vec3 Last{ 0.0f };
			glm::vec3 Velocity{ 0.0f };
			float Stride = 0.0f;
			uint32_t Steps = 0;
			glm::vec3 Crown{ 0.0f };
			bool Placed = false;
		};

		void DrawEmber(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const;
		void DrawAstral(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const;
		void DrawFrost(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const;
		void DrawCrown(WorldRenderer& renderer, EffectSystem& effects, const CosmeticWearer& wearer, Wearer& state, float time, float dt) const;
		std::optional<glm::vec3> Step(const CosmeticWearer& wearer, Wearer& state) const;

		std::array<CrownModel, size_t(Headpiece::Count)> m_Crowns{};
		std::unordered_map<uint32_t, Wearer> m_Wearers;
	};
}
