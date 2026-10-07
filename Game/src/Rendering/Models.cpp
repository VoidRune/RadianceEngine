#include "Models.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Palette.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Game
{
	namespace
	{
		constexpr glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		constexpr glm::vec3 Steel{ 0.78f, 0.80f, 0.85f };
		constexpr glm::vec3 Wood{ 0.48f, 0.33f, 0.19f };
		constexpr glm::vec3 Leather{ 0.36f, 0.24f, 0.14f };
		constexpr glm::vec3 DarkLeather{ 0.22f, 0.16f, 0.12f };
		constexpr glm::vec3 Arcane{ 0.72f, 0.55f, 1.0f };
		constexpr glm::vec3 Skin{ 0.92f, 0.76f, 0.62f };
		constexpr glm::vec3 Cloth{ 0.28f, 0.25f, 0.22f };
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

	void DrawArc(WorldRenderer& renderer, const glm::vec3& center, float facing, float halfArc, float radius, uint32_t segments, const glm::vec3& color, float emission, float thickness)
	{
		const float halfLength = std::max(0.08f, halfArc * radius / float(segments));
		for (uint32_t i = 0; i < segments; i++)
		{
			const float angle = facing + halfArc * (2.0f * float(i) / float(segments - 1) - 1.0f);
			renderer.DrawBox(center + Heading(angle) * radius, { thickness, 0.04f, halfLength }, color, angle, emission);
		}
	}

	void DrawWeapon(WorldRenderer& renderer, ItemBase type, Rarity rarity, const glm::vec3& grip, float facing, float time)
	{
		const glm::vec3 forward = Heading(facing);
		const glm::vec3 side = Heading(facing + HalfPi);
		const float glow = GlowOf(rarity);
		const glm::vec3 accent = rarity == Rarity::Common ? Steel : glm::mix(Steel, RarityColor(rarity), 0.7f);
		const glm::vec3 focus = glm::mix(accent, Arcane, 0.5f);
		const auto part = [&](float ahead, float height, float across, const glm::vec3& halfExtents, const glm::vec3& color, float emission = 0.0f, float spin = 0.0f)
		{
			renderer.DrawBox(grip + forward * ahead + Up * height + side * across, halfExtents, color, facing + spin, emission);
		};
		switch (type)
		{
		case ItemBase::Wand:
			part(0.17f, 0.0f, 0.0f, { 0.17f, 0.022f, 0.022f }, Wood);
			part(0.37f, 0.0f, 0.0f, { 0.045f, 0.045f, 0.045f }, focus, 0.6f + 0.4f * glow, time * 3.0f);
			break;
		case ItemBase::Staff:
			part(0.0f, 0.05f, 0.0f, { 0.032f, 0.5f, 0.032f }, Wood);
			part(0.0f, 0.62f, 0.0f, { 0.08f, 0.08f, 0.08f }, focus, 0.7f + 0.3f * glow, time * 2.0f);
			break;
		case ItemBase::Sword:
			part(0.44f, 0.0f, 0.0f, { 0.36f, 0.03f, 0.05f }, accent, 0.6f * glow);
			part(0.07f, 0.0f, 0.0f, { 0.025f, 0.035f, 0.14f }, glm::mix(accent, DarkLeather, 0.4f), 0.3f * glow);
			part(-0.04f, 0.0f, 0.0f, { 0.07f, 0.025f, 0.025f }, DarkLeather);
			break;
		case ItemBase::Axe:
			part(0.28f, 0.0f, 0.0f, { 0.3f, 0.028f, 0.028f }, Wood);
			part(0.5f, 0.0f, 0.11f, { 0.075f, 0.03f, 0.12f }, accent, 0.6f * glow);
			break;
		case ItemBase::Dagger:
			part(0.22f, 0.0f, 0.0f, { 0.15f, 0.022f, 0.035f }, accent, 0.6f * glow);
			part(0.06f, 0.0f, 0.0f, { 0.02f, 0.03f, 0.08f }, glm::mix(accent, DarkLeather, 0.4f), 0.3f * glow);
			part(-0.02f, 0.0f, 0.0f, { 0.05f, 0.02f, 0.02f }, DarkLeather);
			break;
		case ItemBase::Bow:
			part(0.1f, 0.0f, 0.0f, { 0.04f, 0.12f, 0.035f }, DarkLeather);
			part(0.06f, 0.28f, 0.0f, { 0.03f, 0.18f, 0.03f }, glm::mix(Wood, accent, 0.5f), 0.5f * glow);
			part(0.06f, -0.28f, 0.0f, { 0.03f, 0.18f, 0.03f }, glm::mix(Wood, accent, 0.5f), 0.5f * glow);
			part(-0.04f, 0.0f, 0.0f, { 0.006f, 0.44f, 0.006f }, { 0.9f, 0.88f, 0.8f });
			break;
		case ItemBase::Crossbow:
			part(0.18f, 0.0f, 0.0f, { 0.26f, 0.035f, 0.035f }, Wood);
			part(0.38f, 0.0f, 0.0f, { 0.035f, 0.03f, 0.3f }, accent, 0.6f * glow);
			part(0.26f, 0.05f, 0.0f, { 0.18f, 0.012f, 0.012f }, Steel, 0.1f);
			break;
		default:
			break;
		}
	}

	void DrawCharacter(WorldRenderer& renderer, const PlayerLook& look, const glm::vec3& base, float facing, const glm::vec3& teamColor, float flash, float emission, float time)
	{
		const glm::vec3 forward = Heading(facing);
		const glm::vec3 side = Heading(facing + HalfPi);
		const auto tint = [&](const glm::vec3& color) { return glm::mix(color, glm::vec3(1.0f), flash * 0.8f); };
		const auto draw = [&](MeshKind mesh, const glm::vec3& offset, const glm::vec3& halfExtents, const glm::vec3& color, float glow = 0.0f, float spin = 0.0f)
		{
			renderer.Draw(mesh, base + forward * offset.z + side * offset.x + Up * offset.y, halfExtents, tint(color), facing + spin, std::max(glow, emission), base.y);
		};

		glm::vec3 boot = Skin * 0.7f;
		glm::vec3 bootSize{ 0.07f, 0.06f, 0.1f };
		switch (look.Feet)
		{
		case ItemBase::Sandals:
			boot = { 0.65f, 0.5f, 0.3f };
			bootSize = { 0.075f, 0.05f, 0.11f };
			break;
		case ItemBase::Boots:
			boot = Leather;
			bootSize = { 0.08f, 0.12f, 0.12f };
			break;
		case ItemBase::Greaves:
			boot = Steel;
			bootSize = { 0.085f, 0.17f, 0.12f };
			break;
		default:
			break;
		}
		for (const float across : { -0.13f, 0.13f })
		{
			draw(MeshKind::Box, { across, 0.27f, 0.0f }, { 0.075f, 0.18f, 0.075f }, Cloth);
			draw(MeshKind::Box, { across, bootSize.y, 0.03f }, bootSize, boot);
		}

		glm::vec3 sleeve = teamColor;
		switch (look.Body)
		{
		case ItemBase::Robe:
			sleeve = teamColor * 0.8f;
			draw(MeshKind::Cone, { 0.0f, 0.52f, 0.0f }, { 0.36f, 0.52f, 0.36f }, teamColor * 0.75f);
			draw(MeshKind::Cylinder, { 0.0f, 0.86f, 0.0f }, { 0.22f, 0.2f, 0.22f }, teamColor * 0.85f);
			draw(MeshKind::Cylinder, { 0.0f, 0.66f, 0.0f }, { 0.26f, 0.03f, 0.26f }, Gold, 0.15f);
			break;
		case ItemBase::Tunic:
			sleeve = glm::mix(Leather, teamColor, 0.35f);
			draw(MeshKind::Box, { 0.0f, 0.74f, 0.0f }, { 0.25f, 0.27f, 0.18f }, glm::mix(Leather, teamColor, 0.3f));
			draw(MeshKind::Box, { 0.0f, 0.56f, 0.0f }, { 0.26f, 0.04f, 0.19f }, teamColor);
			draw(MeshKind::Box, { 0.0f, 0.8f, 0.15f }, { 0.04f, 0.2f, 0.04f }, DarkLeather, 0.0f, 0.6f);
			break;
		case ItemBase::Plate:
			sleeve = Steel * 0.9f;
			draw(MeshKind::Box, { 0.0f, 0.75f, 0.0f }, { 0.3f, 0.3f, 0.21f }, Steel);
			draw(MeshKind::Box, { 0.0f, 0.66f, 0.17f }, { 0.15f, 0.22f, 0.05f }, teamColor);
			for (const float across : { -0.31f, 0.31f })
				draw(MeshKind::Sphere, { across, 0.99f, 0.0f }, { 0.14f, 0.11f, 0.14f }, Steel * 0.95f);
			break;
		default:
			draw(MeshKind::Box, { 0.0f, 0.74f, 0.0f }, { 0.24f, 0.28f, 0.17f }, teamColor);
			break;
		}
		for (const float across : { -0.32f, 0.32f })
			draw(MeshKind::Box, { across, 0.72f, 0.02f }, { 0.07f, 0.24f, 0.07f }, sleeve);

		draw(MeshKind::Sphere, { 0.0f, 1.17f, 0.0f }, { 0.17f, 0.17f, 0.17f }, Skin);
		draw(MeshKind::Box, { 0.0f, 1.2f, 0.15f }, { 0.11f, 0.025f, 0.02f }, { 0.12f, 0.1f, 0.1f });
		switch (look.Head)
		{
		case ItemBase::WizardHat:
		{
			const glm::vec3 hat = glm::mix(teamColor, glm::vec3(0.25f, 0.12f, 0.4f), 0.55f);
			draw(MeshKind::Cylinder, { 0.0f, 1.29f, 0.0f }, { 0.31f, 0.02f, 0.31f }, hat);
			draw(MeshKind::Cone, { 0.0f, 1.58f, -0.02f }, { 0.19f, 0.29f, 0.19f }, hat, 0.0f, 0.3f);
			draw(MeshKind::Sphere, { 0.0f, 1.88f, -0.04f }, { 0.04f, 0.04f, 0.04f }, Gold, 0.9f + 0.1f * std::sin(time * 4.0f));
			break;
		}
		case ItemBase::Hood:
		{
			const glm::vec3 hood = glm::mix(glm::vec3(0.18f, 0.26f, 0.17f), teamColor, 0.25f);
			draw(MeshKind::Sphere, { 0.0f, 1.21f, -0.03f }, { 0.21f, 0.2f, 0.2f }, hood);
			draw(MeshKind::Cone, { 0.0f, 1.25f, -0.2f }, { 0.08f, 0.14f, 0.08f }, hood, 0.0f, 0.4f);
			draw(MeshKind::Box, { 0.0f, 1.17f, 0.13f }, { 0.12f, 0.1f, 0.03f }, Skin * 0.85f);
			break;
		}
		case ItemBase::Helm:
			draw(MeshKind::Cylinder, { 0.0f, 1.2f, 0.0f }, { 0.2f, 0.17f, 0.2f }, Steel);
			draw(MeshKind::Box, { 0.0f, 1.2f, 0.18f }, { 0.12f, 0.02f, 0.03f }, { 0.08f, 0.08f, 0.1f });
			draw(MeshKind::Box, { 0.0f, 1.39f, 0.0f }, { 0.03f, 0.05f, 0.14f }, teamColor);
			break;
		default:
			draw(MeshKind::Box, { 0.0f, 1.3f, -0.02f }, { 0.16f, 0.05f, 0.16f }, { 0.35f, 0.22f, 0.12f });
			break;
		}

		DrawWeapon(renderer, look.Weapon, look.WeaponQuality, base + forward * 0.22f - side * 0.42f + Up * 0.6f, facing, time);
	}

	void DrawItemDrop(WorldRenderer& renderer, ItemBase base, Rarity rarity, AbilityId skill, const glm::vec3& position, float time, float phase)
	{
		const glm::vec3 color = base == ItemBase::Gem ? SkillColor(skill) : RarityColor(rarity);
		const float spin = time * 0.9f + phase;
		const float bob = 0.06f * std::sin(time * 2.2f + phase);
		const auto draw = [&](MeshKind mesh, float height, const glm::vec3& halfExtents, const glm::vec3& tone, float glow = 0.0f, float yaw = 0.0f)
		{
			renderer.Draw(mesh, position + Up * (height + bob), halfExtents, tone, spin + yaw, glow, position.y);
		};
		const glm::vec3 accent = rarity == Rarity::Common ? Steel : glm::mix(Steel, color, 0.6f);
		switch (GetEquipSlot(base))
		{
		case EquipSlot::Weapon:
		{
			const bool upright = base == ItemBase::Staff || base == ItemBase::Bow;
			DrawWeapon(renderer, base, rarity, position + Up * ((upright ? 0.62f : 0.42f) + bob) - Heading(spin) * (upright ? 0.0f : 0.25f), spin, time);
			break;
		}
		case EquipSlot::Head:
			if (base == ItemBase::WizardHat)
			{
				draw(MeshKind::Cylinder, 0.4f, { 0.26f, 0.02f, 0.26f }, glm::mix(accent, glm::vec3(0.3f, 0.15f, 0.5f), 0.5f));
				draw(MeshKind::Cone, 0.62f, { 0.16f, 0.22f, 0.16f }, glm::mix(accent, glm::vec3(0.3f, 0.15f, 0.5f), 0.5f));
			}
			else if (base == ItemBase::Helm)
				draw(MeshKind::Cylinder, 0.5f, { 0.18f, 0.15f, 0.18f }, accent);
			else
				draw(MeshKind::Sphere, 0.5f, { 0.19f, 0.17f, 0.19f }, glm::mix(glm::vec3(0.2f, 0.3f, 0.2f), color, 0.4f));
			break;
		case EquipSlot::Body:
			draw(base == ItemBase::Robe ? MeshKind::Cone : MeshKind::Box, 0.5f, { 0.24f, 0.24f, 0.14f }, base == ItemBase::Plate ? accent : glm::mix(Leather, color, 0.5f));
			break;
		case EquipSlot::Feet:
			for (const float across : { -0.1f, 0.1f })
				renderer.Draw(MeshKind::Box, position + Up * (0.45f + bob) + Heading(spin + HalfPi) * across, { 0.07f, 0.1f, 0.11f }, base == ItemBase::Greaves ? accent : glm::mix(Leather, color, 0.4f), spin, 0.0f, position.y);
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
		renderer.DrawBox(position + Up * 0.01f, { 0.3f, 0.01f, 0.3f }, color * 0.45f, spin * 0.5f, 0.3f + 0.1f * float(rarity));
		if (rarity < Rarity::Legendary)
			return;
		const float pulse = 0.75f + 0.25f * std::sin(time * 4.0f + phase);
		const float width = rarity == Rarity::Mythic ? 0.07f : 0.045f;
		renderer.DrawBox(position + Up * 2.4f, { width, 2.4f, width }, color, spin, pulse);
		DrawRing(renderer, position + Up * 0.03f, 0.55f + 0.08f * std::sin(time * 3.0f + phase), 18, color, 0.9f, 0.035f);
	}
}
