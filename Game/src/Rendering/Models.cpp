#include "Models.h"
#include "Rendering/Characters.h"
#include "Rendering/SpellVisuals.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		constexpr glm::vec3 Arcane{ 0.72f, 0.55f, 1.0f };
		constexpr glm::vec3 Gold{ 0.95f, 0.75f, 0.3f };
		constexpr float RarityGlow[] = { 0.0f, 0.15f, 0.3f, 0.55f, 0.85f };
		static_assert(std::size(RarityGlow) == size_t(Rarity::Count));

		float GlowOf(Rarity rarity)
		{
			return RarityGlow[size_t(rarity) < std::size(RarityGlow) ? size_t(rarity) : 0];
		}
	}

	glm::vec3 Heading(float facing)
	{
		return { std::cos(facing), 0.0f, std::sin(facing) };
	}

	glm::vec3 ElementColor(Element element)
	{
		switch (element)
		{
		case Element::Fire:
			return { 1.0f, 0.55f, 0.2f };
		case Element::Cold:
			return { 0.5f, 0.85f, 1.0f };
		case Element::Lightning:
			return { 1.0f, 0.95f, 0.4f };
		case Element::Poison:
			return { 0.5f, 0.95f, 0.3f };
		default:
			return { 0.95f, 0.95f, 0.95f };
		}
	}

	glm::vec3 SkillColor(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Gust:
			return { 0.72f, 0.95f, 1.0f };
		case AbilityId::GravityWell:
			return { 0.62f, 0.36f, 1.0f };
		case AbilityId::Swap:
			return { 0.98f, 0.42f, 0.95f };
		case AbilityId::MirrorWard:
			return { 1.0f, 0.86f, 0.48f };
		default:
			break;
		}
		const AbilityDef& def = GetAbility(ability);
		if (def.Damages != Element::Physical)
			return ElementColor(def.Damages);
		switch (def.Needs)
		{
		case Requirement::Melee:
			return { 1.0f, 0.42f, 0.32f };
		case Requirement::Bow:
			return { 0.45f, 0.9f, 0.4f };
		default:
			return def.Kind == AbilityKind::Heal ? glm::vec3(0.4f, 1.0f, 0.55f) : Arcane;
		}
	}

	void DrawRing(WorldRenderer& renderer, const glm::vec3& center, float radius, uint32_t segments, const glm::vec3& color, float emission, float thickness)
	{
		const float halfLength = Pi * radius / float(segments);
		for (uint32_t i = 0; i < segments; i++)
		{
			const float angle = 2.0f * Pi * float(i) / float(segments);
			renderer.DrawBox(center + Heading(angle) * radius, { thickness, 0.03f, halfLength }, color, angle, emission);
		}
	}

	void DrawItemDrop(WorldRenderer& renderer, const CharacterLibrary& characters, ItemBase base, Rarity rarity, AbilityId skill, const glm::vec3& position, float time, float phase)
	{
		const glm::vec3 color = base == ItemBase::Gem ? SkillColor(skill) : RarityColor(rarity);
		const float spin = time * 0.9f + phase;
		const float bob = 0.06f * std::sin(time * 2.2f + phase);
		const auto draw = [&](MeshKind mesh, float height, const glm::vec3& halfExtents, const glm::vec3& tone, float glow = 0.0f, float yaw = 0.0f)
		{
			renderer.Draw(mesh, position + Up * (height + bob), halfExtents, tone, spin + yaw, glow, position.y);
		};
		const auto drawModels = [&](const std::vector<uint32_t>& meshes, float height, float size)
		{
			glm::vec3 low(1e9f);
			glm::vec3 high(-1e9f);
			for (const uint32_t mesh : meshes)
			{
				low = glm::min(low, renderer.GetModels().GetMesh(mesh).Min);
				high = glm::max(high, renderer.GetModels().GetMesh(mesh).Max);
			}
			const glm::vec3 extent = high - low;
			const float scale = size / std::max({ extent.x, extent.y, extent.z, 0.01f });
			const glm::mat4 transform = ModelTransform(position + Up * (height + bob), spin, scale) * glm::translate(glm::mat4(1.0f), -(low + high) * 0.5f);
			for (const uint32_t mesh : meshes)
				renderer.DrawModel(mesh, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.1f * GlowOf(rarity), position.y);
		};
		switch (GetEquipSlot(base))
		{
		case EquipSlot::Weapon:
			if (const std::optional<uint32_t> mesh = characters.WeaponMesh(base))
				drawModels({ *mesh }, 0.62f, 0.95f);
			break;
		case EquipSlot::Head:
		case EquipSlot::Body:
		case EquipSlot::Feet:
			drawModels(characters.ArmorMeshes(base), 0.5f, 0.62f);
			break;
		case EquipSlot::Amulet:
			DrawRing(renderer, position + Up * (0.5f + bob), 0.17f, 10, Gold, 0.3f, 0.025f);
			draw(MeshKind::Sphere, 0.36f, { 0.07f, 0.07f, 0.07f }, color, 0.8f);
			break;
		case EquipSlot::Ring:
			DrawRing(renderer, position + Up * (0.45f + bob), 0.11f, 8, Gold, 0.3f, 0.025f);
			draw(MeshKind::Box, 0.53f, { 0.05f, 0.05f, 0.05f }, color, 0.8f, 0.785f);
			break;
		case EquipSlot::Gem:
			draw(MeshKind::Cone, 0.6f, { 0.13f, 0.13f, 0.13f }, color, 0.85f);
			draw(MeshKind::Cone, 0.34f, { 0.13f, 0.13f, 0.13f }, color * 0.8f, 0.6f, 0.785f);
			break;
		default:
			break;
		}
		DrawGroundGlow(renderer, position, 0.55f, color, 0.35f + 0.12f * float(rarity));
		if (rarity < Rarity::Legendary)
			return;
		const float pulse = 0.75f + 0.25f * std::sin(time * 4.0f + phase);
		const float width = rarity == Rarity::Mythic ? 0.16f : 0.11f;
		DrawBeam(renderer, position, 4.8f, width, color * (1.8f * pulse));
		DrawRuneCircle(renderer, { .Center = position, .Radius = 0.66f + 0.05f * std::sin(time * 3.0f + phase), .Color = color, .Intensity = 0.8f, .Spin = 0.5f, .Seed = uint32_t(phase * 131.0f) });
	}
}
