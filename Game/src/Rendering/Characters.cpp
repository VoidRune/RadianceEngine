#include "Characters.h"
#include "Rendering/ModelLibrary.h"
#include "Rendering/WorldRenderer.h"
#include <RadianceEngine/Core/Log.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace Game
{
	namespace
	{
		constexpr const char* AnimNames[] = {
			"Idle_A", "Skeletons_Idle", "Melee_2H_Idle", "Ranged_Bow_Idle",
			"Hero_Idle_Look", "Hero_Idle_LookRight", "Hero_Idle_Scan", "Hero_Idle_Wand", "Hero_Idle_Bracer", "Hero_Idle_Scratch",
			"Walking_A", "Hero_Jog",
			"Running_A", "Running_HoldingBow", "Running_HoldingRifle", "Skeletons_Walking",
			"Walking_Backwards", "Running_Strafe_Left", "Running_Strafe_Right",
			"Dodge_Forward", "Dodge_Backward", "Dodge_Left", "Dodge_Right",
			"Hit_A", "Hit_B", "Death_A", "Skeletons_Death",
			"Melee_1H_Attack_Slice_Horizontal", "Melee_1H_Attack_Chop", "Melee_1H_Attack_Stab", "Melee_2H_Attack_Stab", "Melee_2H_Attack_Chop", "Melee_2H_Attack_Spinning", "Melee_Unarmed_Attack_Punch_A",
			"Ranged_Magic_Shoot", "Ranged_Magic_Spellcasting", "Ranged_Magic_Raise",
			"Hero_Cast_Flick", "Hero_Cast_Slash", "Hero_Cast_Backhand", "Hero_Cast_Lunge", "Hero_Cast_Hurl", "Hero_Cast_Surge", "Hero_Cast_Lash", "Hero_Cast_Grasp",
			"Hero_Cast_Lob", "Hero_Cast_Smite", "Hero_Cast_Summon", "Hero_Cast_Call", "Hero_Cast_Push", "Hero_Cast_Ward", "Hero_Cast_Nova", "Hero_Cast_Blink",
			"Ranged_Bow_Draw", "Ranged_Bow_Release", "Ranged_Bow_Draw_Up", "Ranged_Bow_Release_Up",
			"Ranged_2H_Aiming", "Ranged_2H_Shoot", "Use_Item",
		};
		static_assert(std::size(AnimNames) == size_t(Anim::Count));

		Anim FallbackOf(Anim anim)
		{
			switch (anim)
			{
			case Anim::Walk:
			case Anim::Jog:
				return Anim::Run;
			case Anim::CastFlick:
			case Anim::CastSlash:
			case Anim::CastBackhand:
			case Anim::CastLunge:
			case Anim::CastHurl:
			case Anim::CastSurge:
			case Anim::CastLash:
			case Anim::CastGrasp:
			case Anim::CastBlink:
				return Anim::MagicShoot;
			case Anim::CastSmite:
			case Anim::CastSummon:
			case Anim::CastCall:
				return Anim::MagicRaise;
			case Anim::CastLob:
			case Anim::CastPush:
			case Anim::CastWard:
			case Anim::CastNova:
				return Anim::MagicCast;
			default:
				return Anim::Count;
			}
		}

		constexpr Anim IdleVariants[] = { Anim::IdleLook, Anim::IdleLookRight, Anim::IdleScan, Anim::IdleWand, Anim::IdleBracer, Anim::IdleScratch };

		constexpr const char* HeroSkeleton = "Hero";
		constexpr float PlayerScale = 0.78f;
		constexpr float HeroScale = 0.9f;
		constexpr float HoodShade = 0.55f;
		constexpr float DodgeSpeed = 7.0f;
		constexpr float DodgeSeconds = 0.24f;
		constexpr float MoveFade = 0.18f;
		constexpr float ActionFade = 0.1f;
		constexpr float ActionFadeOut = 0.15f;
		constexpr float OverrideFade = 0.08f;
		constexpr float OverrideFadeOut = 0.12f;
		constexpr float MinimumWindup = 0.05f;
		constexpr float FlinchCooldown = 0.5f;
		constexpr float BackpedalEnter = 1.95f;
		constexpr float BackpedalExit = 1.55f;
		constexpr float GaitStickiness = 0.12f;
		constexpr float ForwardWarp = 1.75f;
		constexpr float BackWarp = 1.4f;
		constexpr float WarpRate = 10.0f;
		constexpr float QuickDodge = 0.8f;
		constexpr float FidgetFade = 0.35f;
		constexpr float FidgetBreak = 0.2f;
		constexpr float FidgetDelayMin = 4.0f;
		constexpr float FidgetDelaySpread = 5.0f;
		constexpr float StrikeSettle = 18.0f;
		constexpr float HoldPoint = 0.62f;
		constexpr float HoldReach = 0.28f;
		constexpr float HoldRelease = 1.7f;

		uint64_t LookKey(const PlayerLook& look)
		{
			return uint64_t(look.Weapon) | uint64_t(look.Head) << 8 | uint64_t(look.Body) << 16 | uint64_t(look.Feet) << 24 | uint64_t(PackCosmetics(look.Style)) << 32;
		}

		glm::mat4 GripOf(ItemBase base)
		{
			switch (base)
			{
			case ItemBase::Axe:
			case ItemBase::Dagger:
				return glm::rotate(glm::mat4(1.0f), Pi, glm::vec3(0.0f, 1.0f, 0.0f));
			case ItemBase::Bow:
				return glm::rotate(glm::mat4(1.0f), Pi, glm::vec3(0.0f, 0.0f, 1.0f));
			case ItemBase::Crossbow:
				return glm::rotate(glm::mat4(1.0f), -0.5f * Pi, glm::vec3(0.0f, 1.0f, 0.0f));
			default:
				return glm::mat4(1.0f);
			}
		}

		const char* OutfitOf(ItemBase base)
		{
			switch (base)
			{
			case ItemBase::Robe:
			case ItemBase::Sandals:
				return "Mage";
			case ItemBase::Tunic:
			case ItemBase::Boots:
				return "Ranger";
			case ItemBase::Plate:
			case ItemBase::Greaves:
				return "Knight";
			default:
				return "Barbarian";
			}
		}
	}

	CharacterLibrary::CharacterLibrary(const ModelLibrary& models)
		: m_Models(models)
	{
		m_Shared = models.GetSharedSkeleton();
		m_Rigs.resize(models.GetSkeletonCount());
		for (uint32_t index = 0; index < m_Rigs.size(); index++)
		{
			CharacterRig& rig = m_Rigs[index];
			for (size_t i = 0; i < size_t(Anim::Count); i++)
				rig.Clips[i] = models.FindClip(AnimNames[i], index);
			for (size_t i = 0; i < size_t(Anim::Count); i++)
			{
				const Anim fallback = FallbackOf(Anim(i));
				if (!rig.Clips[i] && fallback != Anim::Count)
					rig.Clips[i] = rig.Clips[size_t(fallback)];
				const bool optional = std::find(std::begin(IdleVariants), std::end(IdleVariants), Anim(i)) != std::end(IdleVariants);
				if (!rig.Clips[i] && index == m_Shared && !optional)
					RDN_LOG_WARNING("Animation {} is missing from the model library", AnimNames[i]);
			}
			rig.Twist = { models.FindJoint("hips", index), models.FindJoint("spine", index), models.FindJoint("chest", index) };
			const ModelSkeleton& skeleton = models.GetSkeleton(index);
			for (uint32_t joint = 0; joint < std::min(skeleton.JointCount(), MaxModelJoints); joint++)
			{
				const std::string& name = skeleton.Names[joint];
				const bool arm = name.find("arm") != std::string::npos || name.find("wrist") != std::string::npos || name.find("hand") != std::string::npos || name.find("shoulder") != std::string::npos;
				rig.UpperBody[joint] = name == "spine" ? 0.5f : name == "chest" || name == "neck" || name == "head" || arm ? 1.0f : 0.0f;
			}
			rig.RightHand = models.FindJoint("handslot.r", index).value_or(0);
			rig.LeftHand = models.FindJoint("handslot.l", index).value_or(0);
			rig.Head = models.FindJoint("head", index).value_or(0);
			Pose rest;
			RestPose(skeleton, rest);
			JointMatrices globals;
			ComputeGlobals(skeleton, rest, globals);
			for (uint32_t link = 0; link < MaxCapeJoints; link++)
			{
				const std::optional<uint32_t> joint = models.FindJoint("cape." + std::to_string(link + 1), index);
				if (!joint || skeleton.Parents[*joint] < 0)
					break;
				const glm::mat3 toParent = glm::inverse(glm::mat3(globals[size_t(skeleton.Parents[*joint])]));
				rig.Cape[rig.CapeCount++] = { .Joint = *joint, .LiftAxis = glm::normalize(toParent * glm::vec3(1.0f, 0.0f, 0.0f)), .SwayAxis = glm::normalize(toParent * glm::vec3(0.0f, 0.0f, 1.0f)) };
			}
			const float shares = float(rig.CapeCount * (rig.CapeCount + 1)) * 0.5f;
			for (uint32_t link = 0; link < rig.CapeCount; link++)
				rig.Cape[link].Share = float(rig.CapeCount - link) / shares;
		}
		m_Hero = models.FindSkeleton(HeroSkeleton);
		if (m_Hero && !models.FindMesh("Hero/Hero_Head"))
			m_Hero.reset();

		const uint32_t rightHand = m_Rigs[m_Shared].RightHand;
		const uint32_t leftHand = m_Rigs[m_Shared].LeftHand;
		const auto enemy = [&](EntityKind kind, const char* prefix, float scale, Stance stance)
		{
			CharacterLook& look = m_Enemies[kind];
			AddPrefix(look, prefix);
			look.Skeleton = m_Shared;
			look.Scale = scale;
			look.Pose = stance;
			look.Height = TopOf(look) * scale;
			look.Undead = true;
			return std::ref(look);
		};
		enemy(EntityKind::Chaser, "Skeleton_Minion_", 0.72f, Stance::Skeleton);
		Attach(enemy(EntityKind::Shooter, "Skeleton_Rogue_", 0.8f, Stance::Shambling), "Skeleton_Crossbow", rightHand, GripOf(ItemBase::Crossbow));
		Attach(enemy(EntityKind::Brute, "Skeleton_Warrior_", 1.15f, Stance::Shambling), "Skeleton_Axe", rightHand);
		Attach(enemy(EntityKind::Caster, "Skeleton_Mage_", 0.85f, Stance::Shambling), "Skeleton_Staff", rightHand);
		CharacterLook& archivist = enemy(EntityKind::Archivist, "Skeleton_Mage_", 1.7f, Stance::Shambling);
		Attach(archivist, "Skeleton_Staff", rightHand);
		Attach(archivist, "spellbook_open", leftHand);
	}

	float CharacterLibrary::TopOf(const CharacterLook& look) const
	{
		float top = 0.0f;
		for (const CharacterPart& part : look.Parts)
			top = std::max(top, m_Models.GetMesh(part.Mesh).Max.y);
		return top;
	}

	std::optional<uint32_t> CharacterLibrary::Mesh(const char* name) const
	{
		return m_Models.FindMesh(name);
	}

	void CharacterLibrary::AddParts(CharacterLook& look, std::initializer_list<const char*> names) const
	{
		for (const char* name : names)
			if (const std::optional<uint32_t> mesh = Mesh(name))
				look.Parts.push_back({ *mesh });
	}

	void CharacterLibrary::AddPrefix(CharacterLook& look, const char* prefix) const
	{
		const std::vector<ModelMesh>& meshes = m_Models.GetData().Meshes;
		for (uint32_t i = 0; i < meshes.size(); i++)
			if (meshes[i].Skinned && meshes[i].Skeleton == m_Shared && meshes[i].Name.rfind(prefix, 0) == 0)
				look.Parts.push_back({ i });
	}

	std::vector<uint32_t> CharacterLibrary::NodeMeshes(const std::string& name) const
	{
		std::vector<uint32_t> meshes;
		std::optional<uint32_t> mesh = m_Models.FindMesh(name);
		for (uint32_t part = 1; mesh; part++)
		{
			meshes.push_back(*mesh);
			mesh = m_Models.FindMesh(name + "." + std::to_string(part));
		}
		return meshes;
	}

	void CharacterLibrary::AddNode(CharacterLook& look, const std::string& name, float shade, bool dyed) const
	{
		for (const uint32_t mesh : NodeMeshes(name))
			look.Parts.push_back({ mesh, shade, AccentOf(mesh, dyed) });
	}

	uint32_t CharacterLibrary::AccentOf(uint32_t mesh, bool dyed) const
	{
		const ModelLibraryData& data = m_Models.GetData();
		const uint32_t texture = data.Meshes[mesh].Texture;
		if (texture >= data.Textures.size())
			return 0;
		const std::string& name = data.Textures[texture].Name;
		if (name.find("Lining") != std::string::npos)
			return MeshAccentDye;
		if (name.find("Garb") != std::string::npos)
			return dyed ? MeshAccentTrim | MeshAccentDye : MeshAccentTrim;
		return 0;
	}

	void CharacterLibrary::Attach(CharacterLook& look, const char* mesh, uint32_t joint, const glm::mat4& offset) const
	{
		if (const std::optional<uint32_t> index = Mesh(mesh))
			look.Props.push_back({ *index, joint, offset });
	}

	const CharacterLook& CharacterLibrary::ForPlayer(const PlayerLook& look) const
	{
		const uint64_t key = LookKey(look);
		if (const auto it = m_Players.find(key); it != m_Players.end())
			return it->second;

		CharacterLook result;
		result.Pose = PlayerStance(look.Weapon);
		if (m_Hero)
			DressHero(result, look);
		else
			DressKayKit(result, look);
		const CharacterRig& rig = m_Rigs[result.Skeleton];
		for (const uint32_t weapon : WeaponMeshes(look.Weapon))
			result.Props.push_back({ weapon, look.Weapon == ItemBase::Bow ? rig.LeftHand : rig.RightHand, GripOf(look.Weapon) });
		result.Height = TopOf(result) * result.Scale;
		return m_Players.emplace(key, std::move(result)).first->second;
	}

	void CharacterLibrary::DressHero(CharacterLook& look, const PlayerLook& player) const
	{
		look.Skeleton = *m_Hero;
		look.Scale = HeroScale;
		switch (player.Style.Body)
		{
		case Outfit::Wanderer:
		default:
			AddNode(look, "Hero/Wizard_Garb");
			AddNode(look, "Hero/Wizard_Armor");
			AddNode(look, "Hero/Wizard_Cape", 1.0f, true);
			break;
		}
		switch (player.Style.Head)
		{
		case Headwear::Hood:
			AddNode(look, "Hero/Hero_Head", HoodShade);
			AddNode(look, "Hero/Hero_Hair", HoodShade);
			AddNode(look, "Hero/Hero_EyeGlow");
			AddNode(look, "Hero/Wizard_Hood");
			break;
		case Headwear::PointedHat:
			AddNode(look, "Hero/Hero_Head");
			AddNode(look, "Hero/Hero_HatHair");
			AddNode(look, "Hero/Wizard_Cowl");
			AddNode(look, "Hero/Wizard_Hat");
			break;
		case Headwear::Bare:
		default:
			AddNode(look, "Hero/Hero_Head");
			AddNode(look, "Hero/Hero_Hair");
			AddNode(look, "Hero/Wizard_Cowl");
			break;
		}
	}

	void CharacterLibrary::DressKayKit(CharacterLook& look, const PlayerLook& player) const
	{
		look.Skeleton = m_Shared;
		look.Scale = PlayerScale;
		const std::string outfit = OutfitOf(player.Body);
		const std::string legs = OutfitOf(player.Feet);
		const std::string parts[] = { outfit + "_Body", outfit + "_ArmLeft", outfit + "_ArmRight", outfit + "_Cape", legs + "_LegLeft", legs + "_LegRight" };
		for (const std::string& part : parts)
			if (const std::optional<uint32_t> mesh = Mesh(part.c_str()))
				look.Parts.push_back({ *mesh });
		switch (player.Head)
		{
		case ItemBase::WizardHat:
			AddParts(look, { "Mage_Head", "Mage_Hat" });
			break;
		case ItemBase::Hood:
			AddParts(look, { "RogueHooded_Head", "RogueHooded_Mask" });
			break;
		case ItemBase::Helm:
			AddParts(look, { "Knight_Head", "Knight_Helmet" });
			break;
		default:
			if (const std::optional<uint32_t> mesh = Mesh((outfit + "_Head").c_str()))
				look.Parts.push_back({ *mesh });
			break;
		}
		if (player.Weapon == ItemBase::Bow || player.Weapon == ItemBase::Crossbow)
			AddParts(look, { "Ranger_Quiver" });
	}

	const CharacterLook* CharacterLibrary::ForEnemy(EntityKind kind) const
	{
		const auto it = m_Enemies.find(kind);
		return it != m_Enemies.end() && !it->second.Parts.empty() ? &it->second : nullptr;
	}

	std::vector<uint32_t> CharacterLibrary::WeaponMeshes(ItemBase base) const
	{
		const auto prefer = [&](const char* original, const char* fallback)
		{
			std::vector<uint32_t> meshes = NodeMeshes(original);
			return meshes.empty() ? NodeMeshes(fallback) : meshes;
		};
		switch (base)
		{
		case ItemBase::Wand:
			return prefer("Armory/Wand", "wand");
		case ItemBase::Staff:
			return prefer("Armory/Staff", "staff");
		case ItemBase::Sword:
			return NodeMeshes("sword_1handed");
		case ItemBase::Axe:
			return NodeMeshes("axe_1handed");
		case ItemBase::Dagger:
			return NodeMeshes("dagger");
		case ItemBase::Spear:
			return NodeMeshes("spear_A");
		case ItemBase::Bow:
			return NodeMeshes("bow_withString");
		case ItemBase::Crossbow:
			return NodeMeshes("crossbow_2handed");
		default:
			return {};
		}
	}

	std::vector<uint32_t> CharacterLibrary::ArmorMeshes(ItemBase base) const
	{
		std::vector<uint32_t> meshes;
		const auto add = [&](std::initializer_list<const char*> names)
		{
			for (const char* name : names)
				if (const std::optional<uint32_t> mesh = Mesh(name))
					meshes.push_back(*mesh);
		};
		switch (base)
		{
		case ItemBase::WizardHat:
			add({ "Mage_Hat" });
			break;
		case ItemBase::Hood:
			add({ "Skeleton_Rogue_Hood" });
			break;
		case ItemBase::Helm:
			add({ "Knight_Helmet" });
			break;
		case ItemBase::Robe:
			add({ "Mage_Body" });
			break;
		case ItemBase::Tunic:
			add({ "Ranger_Body" });
			break;
		case ItemBase::Plate:
			add({ "Knight_Body" });
			break;
		case ItemBase::Sandals:
			add({ "Mage_LegLeft", "Mage_LegRight" });
			break;
		case ItemBase::Boots:
			add({ "Ranger_LegLeft", "Ranger_LegRight" });
			break;
		case ItemBase::Greaves:
			add({ "Knight_LegLeft", "Knight_LegRight" });
			break;
		default:
			break;
		}
		return meshes;
	}

	std::optional<uint32_t> CharacterLibrary::ArrowMesh(AbilityId ability) const
	{
		switch (ability)
		{
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::PiercingShot:
			return Mesh("arrow_bow");
		case AbilityId::Quarrel:
			return Mesh("arrow_crossbow");
		default:
			return std::nullopt;
		}
	}

	Stance PlayerStance(ItemBase weapon)
	{
		switch (weapon)
		{
		case ItemBase::Spear:
		case ItemBase::Staff:
			return Stance::Heavy;
		case ItemBase::Bow:
			return Stance::Bow;
		case ItemBase::Crossbow:
			return Stance::Rifle;
		default:
			return Stance::Light;
		}
	}

	AttackAnim PlayerAttackAnim(AbilityId ability)
	{
		switch (ability)
		{
		case AbilityId::Slash:
			return { Anim::Slash };
		case AbilityId::Chop:
			return { Anim::Chop };
		case AbilityId::Stab:
			return { Anim::Stab };
		case AbilityId::Thrust:
			return { Anim::HeavyStab };
		case AbilityId::GroundSlam:
			return { Anim::HeavyChop };
		case AbilityId::Cyclone:
			return { Anim::Spinning };
		case AbilityId::Bolt:
		case AbilityId::Orb:
		case AbilityId::ChainLightning:
		case AbilityId::EmberBolt:
			return { .Windup = Anim::CastFlick, .Variants = 3 };
		case AbilityId::FrostSpear:
			return { .Windup = Anim::CastHurl, .Hold = true };
		case AbilityId::IceSpear:
			return { Anim::CastLunge };
		case AbilityId::Fireball:
		case AbilityId::ArcLightning:
			return { Anim::CastHurl };
		case AbilityId::SplittingBolt:
			return { Anim::CastSurge };
		case AbilityId::ChainHook:
			return { Anim::CastLash };
		case AbilityId::Swap:
			return { Anim::CastBackhand };
		case AbilityId::GravityWell:
			return { Anim::CastGrasp };
		case AbilityId::Firebomb:
			return { Anim::CastLob };
		case AbilityId::StormStrike:
			return { Anim::CastSmite };
		case AbilityId::Blink:
		case AbilityId::Warp:
			return { Anim::CastBlink };
		case AbilityId::Gust:
			return { Anim::CastPush };
		case AbilityId::Meteor:
		case AbilityId::Comet:
			return { Anim::CastSummon };
		case AbilityId::Shockwave:
		case AbilityId::FrostNova:
			return { Anim::CastNova };
		case AbilityId::MirrorWard:
			return { Anim::CastWard };
		case AbilityId::Rejuvenate:
			return { Anim::UseItem };
		case AbilityId::Arrow:
		case AbilityId::Volley:
		case AbilityId::PiercingShot:
			return { Anim::BowDraw, Anim::BowRelease };
		case AbilityId::RainOfArrows:
			return { Anim::BowDrawUp, Anim::BowReleaseUp };
		case AbilityId::Quarrel:
			return { Anim::RifleAim, Anim::RifleShoot };
		default:
			return {};
		}
	}

	AttackAnim EnemyAttackAnim(EntityKind kind)
	{
		switch (kind)
		{
		case EntityKind::Chaser:
			return { Anim::Punch };
		case EntityKind::Shooter:
			return { Anim::RifleAim, Anim::RifleShoot };
		case EntityKind::Brute:
			return { Anim::HeavyChop };
		case EntityKind::Caster:
			return { Anim::MagicRaise, Anim::MagicShoot };
		case EntityKind::Archivist:
			return { Anim::MagicCast, Anim::Count, true };
		default:
			return {};
		}
	}

	void CharacterAnimator::Crossfade::Start(const Layer& layer, bool fade)
	{
		if (fade && Current.Clip >= 0)
		{
			Previous = Current;
			Blend = 0.0f;
		}
		else
			Blend = 1.0f;
		Current = layer;
	}

	void CharacterAnimator::Crossfade::Advance(const CharacterLibrary&, float dt, float fadeSeconds)
	{
		Current.Time += dt * Current.Rate;
		Previous.Time += dt * Previous.Rate;
		Blend = std::min(1.0f, Blend + dt / std::max(fadeSeconds, 1e-3f));
	}

	Anim CharacterAnimator::Locomotion(const Input& input, float& rate, const CharacterLibrary& library)
	{
		const float speed = Length(m_Velocity);
		rate = 1.0f;
		if (speed < 0.35f * input.Scale)
		{
			switch (input.Pose)
			{
			case Stance::Heavy:
				return Anim::HeavyIdle;
			case Stance::Bow:
				return Anim::BowIdle;
			case Stance::Skeleton:
			case Stance::Shambling:
				return Anim::SkeletonIdle;
			default:
				return Anim::Idle;
			}
		}
		const ModelLibrary& models = library.GetModels();
		const auto rateOf = [&](Anim anim)
		{
			const std::optional<uint32_t> clip = library.Clip(anim, m_Skeleton);
			const float stride = clip ? models.GetStrideSpeed(*clip) * input.Scale : 0.0f;
			return stride > 0.1f ? speed / stride : 1.0f;
		};
		if (input.Pose == Stance::Shambling || input.Pose == Stance::Skeleton)
		{
			const Anim anim = input.Pose == Stance::Shambling ? Anim::SkeletonWalk : Anim::Run;
			rate = std::clamp(rateOf(anim), 0.6f, 1.8f);
			return anim;
		}
		const Vec2 forward = FromAngle(input.Facing);
		const float angle = std::atan2(Dot(m_Velocity, Vec2(forward.y, -forward.x)), Dot(m_Velocity, forward));
		m_Backward = std::abs(angle) > (m_Backward ? BackpedalExit : BackpedalEnter);
		Anim anim = Anim::WalkBack;
		if (!m_Backward)
		{
			std::array<Anim, 3> gaits = { Anim::Walk, Anim::Jog, Anim::Run };
			if (input.Pose == Stance::Bow || input.Pose == Stance::Rifle)
				gaits = { Anim::Walk, Anim::Jog, input.Pose == Stance::Bow ? Anim::RunWithBow : Anim::RunWithRifle };
			float best = 1e9f;
			anim = gaits.back();
			for (const Anim gait : gaits)
			{
				const std::optional<uint32_t> clip = library.Clip(gait, m_Skeleton);
				if (!clip)
					continue;
				const float score = std::abs(std::log(std::max(rateOf(gait), 1e-3f))) - (int32_t(*clip) == m_Move.Current.Clip ? GaitStickiness : 0.0f);
				if (score < best)
				{
					best = score;
					anim = gait;
				}
			}
		}
		rate = std::clamp(rateOf(anim), 0.6f, 1.8f);
		return anim;
	}

	void CharacterAnimator::StartAttack(const CharacterLibrary& library, const Input& input)
	{
		Anim windupAnim = input.Attack.Windup;
		if (input.Attack.Variants > 1)
		{
			uint8_t pick = uint8_t(Random() * float(input.Attack.Variants)) % input.Attack.Variants;
			if (pick == m_Variant)
				pick = uint8_t((pick + 1) % input.Attack.Variants);
			m_Variant = pick;
			windupAnim = Anim(uint8_t(windupAnim) + pick);
		}
		const std::optional<uint32_t> clip = library.Clip(windupAnim, m_Skeleton);
		if (!clip)
			return;
		const ModelLibrary& models = library.GetModels();
		const float duration = models.GetClip(*clip).Duration();
		const float windup = std::max(input.Windup, MinimumWindup);
		Layer layer{ int32_t(*clip), 0.0f, 1.0f, input.Attack.Loop };
		m_StrikeTime = 0.0f;
		m_HoldTime = 0.0f;
		if (input.Attack.Hold)
		{
			float strike = models.GetStrikeTime(*clip);
			if (strike < 0.05f)
				strike = duration * 0.4f;
			m_HoldTime = strike * HoldPoint;
			layer.Rate = std::clamp(m_HoldTime / HoldReach, 0.6f, 2.5f);
			layer.Loop = false;
		}
		else if (!input.Attack.Loop && input.Attack.Release != Anim::Count)
		{
			layer.Rate = std::clamp(duration / windup, 0.5f, 4.0f);
			layer.Loop = false;
		}
		else if (!input.Attack.Loop)
		{
			float strike = models.GetStrikeTime(*clip);
			if (strike < 0.05f)
				strike = duration * 0.4f;
			layer.Rate = std::clamp(strike / windup, 0.6f, 1.6f);
			layer.Time = std::max(0.0f, strike - windup * layer.Rate);
			layer.Loop = false;
			m_StrikeTime = strike;
		}
		m_IdleTime = 0.0f;
		m_Action.Start(layer, m_Phase != Phase::None && m_Action.Weight > 0.0f);
		m_Phase = Phase::Windup;
		m_ReleaseAnim = input.Attack.Loop ? Anim::Count : input.Attack.Release;
	}

	void CharacterAnimator::EndAttack(const CharacterLibrary& library)
	{
		if (m_Phase != Phase::Windup)
			return;
		if (m_Action.Current.Loop)
		{
			m_Phase = Phase::None;
			return;
		}
		if (const std::optional<uint32_t> release = library.Clip(m_ReleaseAnim, m_Skeleton))
		{
			m_Action.Start({ int32_t(*release), 0.0f, 1.0f, false }, true);
			m_Phase = Phase::Release;
			return;
		}
		m_Action.Current.Rate = std::max(m_Action.Current.Rate, m_HoldTime > 0.0f ? HoldRelease : 1.0f);
		m_HoldTime = 0.0f;
		m_Phase = Phase::FollowThrough;
	}

	void CharacterAnimator::Spring::Advance(float target, float dt)
	{
		constexpr float Frequency = 8.5f;
		constexpr float Damping = 0.55f;
		Rate += (Frequency * Frequency * (target - Value) - 2.0f * Damping * Frequency * Rate) * dt;
		Value += Rate * dt;
	}

	void CharacterAnimator::UpdateCape(const Input& input, float dt)
	{
		m_CapeTime += dt;
		const Vec2 forward = FromAngle(input.Facing);
		const float ahead = Dot(m_Velocity, forward);
		const float across = Dot(m_Velocity, Vec2(forward.y, -forward.x));
		const float breeze = 0.03f * std::sin(m_CapeTime * 1.3f) + 0.015f * std::sin(m_CapeTime * 3.1f + 1.7f);
		const float lift = input.Dead ? 0.0f : std::clamp(0.075f * ahead + 0.03f * std::abs(across), -0.1f, 0.9f) + breeze;
		const float sway = input.Dead ? 0.0f : std::clamp(-0.05f * across, -0.3f, 0.3f);
		constexpr float Step = 1.0f / 120.0f;
		for (float left = std::min(dt, 0.1f); left > 0.0f; left -= Step)
		{
			m_CapeLift.Advance(lift, std::min(left, Step));
			m_CapeSway.Advance(sway, std::min(left, Step));
		}
	}

	void CharacterAnimator::Update(const CharacterLibrary& library, const Input& input, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		if (input.Skeleton != m_Skeleton)
		{
			m_Move = {};
			m_Action = {};
			m_Override = {};
			m_Flinch = {};
			m_Fidget = {};
			m_LastFidget = -1;
			m_Phase = Phase::None;
			m_DodgeHold = false;
			m_Warp = 0.0f;
			m_WasDead = false;
			m_Skeleton = input.Skeleton;
		}
		if (!m_HasPosition)
		{
			m_LastPosition = input.Position;
			m_HasPosition = true;
			m_Seed ^= uint32_t(reinterpret_cast<uintptr_t>(this) >> 4) | 1u;
		}
		Vec2 delta = input.Position - m_LastPosition;
		m_LastPosition = input.Position;
		if (LengthSquared(delta) > 4.0f)
			delta = {};
		const Vec2 instant = dt > 1e-5f ? delta / dt : Vec2();
		m_Velocity = Lerp(m_Velocity, instant, 1.0f - std::exp(-dt * 14.0f));
		const float speed = Length(m_Velocity);
		m_Moving = std::clamp((speed - 0.3f) / (1.2f * input.Scale), 0.0f, 1.0f);
		UpdateCape(input, dt);

		float rate = 1.0f;
		const Anim move = Locomotion(input, rate, library);
		if (const std::optional<uint32_t> clip = library.Clip(move, m_Skeleton); clip && int32_t(*clip) != m_Move.Current.Clip)
		{
			float phase = 0.0f;
			if (m_Move.Current.Clip >= 0)
			{
				const float duration = models.GetClip(uint32_t(m_Move.Current.Clip)).Duration();
				phase = duration > 0.0f ? std::fmod(m_Move.Current.Time / duration, 1.0f) : 0.0f;
			}
			m_Move.Start({ int32_t(*clip), phase * models.GetClip(*clip).Duration(), rate, true }, true);
		}
		m_Move.Current.Rate = rate;
		m_Move.Weight = 1.0f;
		m_Move.Advance(library, dt, MoveFade);

		if (input.Dead && !m_WasDead)
		{
			if (const std::optional<uint32_t> clip = library.Clip(input.Undead ? Anim::SkeletonDeath : Anim::Death, m_Skeleton))
				m_Override.Start({ int32_t(*clip), 0.0f, 1.0f, false }, m_Override.Weight > 0.0f);
			m_Phase = Phase::None;
		}
		m_WasDead = input.Dead;
		if (input.Dead)
		{
			m_Action.Weight = std::max(0.0f, m_Action.Weight - dt / ActionFadeOut);
			m_Flinch.Weight = 0.0f;
			m_Override.Weight = std::min(1.0f, m_Override.Weight + dt / OverrideFade);
			m_Override.Advance(library, dt, OverrideFade);
			return;
		}

		if (input.Attacking && !m_WasAttacking)
			StartAttack(library, input);
		else if (!input.Attacking && m_WasAttacking)
			EndAttack(library);
		m_WasAttacking = input.Attacking;
		if (m_Phase != Phase::None && m_Action.Current.Clip >= 0)
		{
			m_Action.Advance(library, dt, ActionFade);
			if (m_Phase == Phase::Windup && m_HoldTime > 0.0f)
				m_Action.Current.Time = std::min(m_Action.Current.Time, m_HoldTime);
			m_Action.Weight = std::min(1.0f, m_Action.Weight + dt / ActionFade);
			const bool finished = !m_Action.Current.Loop && m_Action.Current.Time >= models.GetClip(uint32_t(m_Action.Current.Clip)).Duration();
			if ((m_Phase == Phase::Release || m_Phase == Phase::FollowThrough) && finished)
				m_Phase = Phase::None;
			if (m_StrikeTime > 0.0f && m_Action.Current.Time >= m_StrikeTime)
				m_Action.Current.Rate += (1.0f - m_Action.Current.Rate) * (1.0f - std::exp(-dt * StrikeSettle));
		}
		else
		{
			m_Phase = Phase::None;
			m_Action.Weight = std::max(0.0f, m_Action.Weight - dt / ActionFadeOut);
		}

		const bool dodging = input.Dashing && speed > DodgeSpeed;
		if (dodging && !m_WasDodging)
		{
			const Vec2 forward = FromAngle(input.Facing);
			const float along = Dot(m_Velocity, forward);
			const float across = Dot(m_Velocity, Vec2(forward.y, -forward.x));
			const Anim anim = std::abs(along) >= std::abs(across) ? (along > 0.0f ? Anim::DodgeForward : Anim::DodgeBack) : (across > 0.0f ? Anim::DodgeRight : Anim::DodgeLeft);
			if (const std::optional<uint32_t> clip = library.Clip(anim, m_Skeleton))
			{
				const float duration = models.GetClip(*clip).Duration();
				m_DodgeHold = duration <= QuickDodge;
				m_Override.Start({ int32_t(*clip), 0.0f, m_DodgeHold ? 1.0f : std::max(duration / DodgeSeconds, 1.0f), false }, m_Override.Weight > 0.0f);
			}
		}
		else if (input.Stunned && !m_WasStunned && !dodging)
		{
			m_DodgeHold = false;
			if (const std::optional<uint32_t> clip = library.Clip(Anim::HitB, m_Skeleton))
				m_Override.Start({ int32_t(*clip), 0.0f, 1.0f, false }, m_Override.Weight > 0.0f);
		}
		if (m_DodgeHold && !dodging && m_Override.Current.Clip >= 0)
		{
			const bool walking = Length(instant) > 1.2f * input.Scale;
			if (input.Attacking || walking || m_Override.Current.Time >= models.GetClip(uint32_t(m_Override.Current.Clip)).Duration() - OverrideFadeOut)
				m_DodgeHold = false;
		}
		m_WasDodging = dodging;
		m_WasStunned = input.Stunned;
		const bool overriding = dodging || input.Stunned || m_DodgeHold;
		m_Override.Weight = overriding ? std::min(1.0f, m_Override.Weight + dt / OverrideFade) : std::max(0.0f, m_Override.Weight - dt / OverrideFadeOut);
		m_Override.Advance(library, dt, OverrideFade);

		m_FlinchCooldown -= dt;
		if (input.Hurt && m_FlinchCooldown <= 0.0f && m_Phase == Phase::None && !overriding)
			if (const std::optional<uint32_t> clip = library.Clip(Anim::HitA, m_Skeleton))
			{
				m_Flinch.Start({ int32_t(*clip), 0.0f, 1.4f, false }, false);
				m_Flinch.Weight = 0.55f;
				m_FlinchCooldown = FlinchCooldown;
			}
		m_Flinch.Advance(library, dt, ActionFade);
		m_Flinch.Weight = std::max(0.0f, m_Flinch.Weight - dt * 1.1f);

		const bool still = speed < 0.35f * input.Scale && m_Phase == Phase::None && !overriding && m_Override.Weight <= 0.0f && m_Flinch.Weight < 0.1f;
		UpdateFidget(library, still, dt);

		float warp = 0.0f;
		if (speed > 0.35f * input.Scale && input.Pose != Stance::Skeleton && input.Pose != Stance::Shambling)
		{
			const Vec2 forward = FromAngle(input.Facing);
			const float angle = std::atan2(Dot(m_Velocity, Vec2(forward.y, -forward.x)), Dot(m_Velocity, forward));
			warp = m_Backward ? std::clamp(angle - std::copysign(Pi, angle), -BackWarp, BackWarp) : std::clamp(angle, -ForwardWarp, ForwardWarp);
		}
		if (std::abs(warp - m_Warp) > 2.0f)
			m_Warp = warp;
		m_Warp += (warp - m_Warp) * (1.0f - std::exp(-dt * WarpRate));
	}

	void CharacterAnimator::UpdateFidget(const CharacterLibrary& library, bool still, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		m_IdleTime = still ? m_IdleTime + dt : 0.0f;
		if (still && m_Fidget.Current.Clip < 0 && m_IdleTime > m_FidgetDelay)
		{
			std::array<uint32_t, std::size(IdleVariants)> choices{};
			uint32_t count = 0;
			for (const Anim anim : IdleVariants)
				if (const std::optional<uint32_t> clip = library.Clip(anim, m_Skeleton); clip && int32_t(*clip) != m_LastFidget)
					choices[count++] = *clip;
			if (count > 0)
			{
				m_LastFidget = int32_t(choices[std::min(uint32_t(Random() * float(count)), count - 1)]);
				m_Fidget.Start({ m_LastFidget, 0.0f, 1.0f, false }, false);
			}
			m_IdleTime = 0.0f;
			m_FidgetDelay = FidgetDelayMin + FidgetDelaySpread * Random();
		}
		if (m_Fidget.Current.Clip < 0)
			return;
		m_Fidget.Advance(library, dt, FidgetFade);
		const float duration = models.GetClip(uint32_t(m_Fidget.Current.Clip)).Duration();
		const bool ending = m_Fidget.Current.Time > duration - FidgetFade;
		if (still && !ending)
			m_Fidget.Weight = std::min(1.0f, m_Fidget.Weight + dt / FidgetFade);
		else
			m_Fidget.Weight = std::max(0.0f, m_Fidget.Weight - dt / (still ? FidgetFade : FidgetBreak));
		if (m_Fidget.Weight <= 0.0f && (ending || !still))
		{
			m_Fidget.Current.Clip = -1;
			m_IdleTime = 0.0f;
		}
	}

	float CharacterAnimator::Random()
	{
		m_Seed ^= m_Seed << 13;
		m_Seed ^= m_Seed >> 17;
		m_Seed ^= m_Seed << 5;
		return float(m_Seed >> 8) / float(1u << 24);
	}

	void CharacterAnimator::Warp(const ModelSkeleton& skeleton, const CharacterRig& rig, Pose& pose, float amount) const
	{
		if (std::abs(amount) < 1e-3f || !rig.Twist[0] || !rig.Twist[1] || !rig.Twist[2])
			return;
		const auto global = [&](int32_t joint)
		{
			glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
			for (; joint >= 0; joint = skeleton.Parents[size_t(joint)])
				rotation = pose[size_t(joint)].Rotation * rotation;
			return rotation;
		};
		const auto turn = [&](uint32_t joint, float angle)
		{
			const glm::quat parent = global(skeleton.Parents[joint]);
			pose[joint].Rotation = glm::normalize(glm::inverse(parent) * glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * parent * pose[joint].Rotation);
		};
		turn(*rig.Twist[0], amount);
		turn(*rig.Twist[1], -0.5f * amount);
		turn(*rig.Twist[2], -0.5f * amount);
	}

	void CharacterAnimator::Sample(const CharacterLibrary& library, const Crossfade& layers, Pose& pose) const
	{
		const ModelLibrary& models = library.GetModels();
		const uint32_t joints = models.GetSkeleton(m_Skeleton).JointCount();
		if (layers.Current.Clip < 0)
			return;
		if (layers.Blend < 1.0f && layers.Previous.Clip >= 0)
		{
			SampleClip(models.GetClip(uint32_t(layers.Previous.Clip)), joints, layers.Previous.Time, layers.Previous.Loop, pose);
			Pose current = pose;
			SampleClip(models.GetClip(uint32_t(layers.Current.Clip)), joints, layers.Current.Time, layers.Current.Loop, current);
			BlendPose(pose, current, layers.Blend, joints);
			return;
		}
		SampleClip(models.GetClip(uint32_t(layers.Current.Clip)), joints, layers.Current.Time, layers.Current.Loop, pose);
	}

	void CharacterAnimator::Evaluate(const CharacterLibrary& library, uint32_t skeletonIndex, JointMatrices& globals) const
	{
		const ModelSkeleton& skeleton = library.GetModels().GetSkeleton(skeletonIndex);
		const uint32_t joints = skeleton.JointCount();
		Pose pose;
		RestPose(skeleton, pose);
		if (skeletonIndex != m_Skeleton)
		{
			ComputeGlobals(skeleton, pose, globals);
			return;
		}
		Sample(library, m_Move, pose);
		if (m_Fidget.Weight > 0.0f && m_Fidget.Current.Clip >= 0)
		{
			Pose fidget = pose;
			Sample(library, m_Fidget, fidget);
			BlendPose(pose, fidget, m_Fidget.Weight, joints);
		}
		if (m_Flinch.Weight > 0.0f && m_Flinch.Current.Clip >= 0)
		{
			Pose flinch = pose;
			Sample(library, m_Flinch, flinch);
			BlendPose(pose, flinch, m_Flinch.Weight, joints, &library.GetRig(m_Skeleton).UpperBody);
		}
		if (m_Action.Weight > 0.0f && m_Action.Current.Clip >= 0)
		{
			Pose action = pose;
			Sample(library, m_Action, action);
			const JointWeights& upper = library.GetRig(m_Skeleton).UpperBody;
			JointWeights mask{};
			for (uint32_t joint = 0; joint < std::min(joints, MaxModelJoints); joint++)
				mask[joint] = upper[joint] + (1.0f - upper[joint]) * (1.0f - m_Moving);
			BlendPose(pose, action, m_Action.Weight, joints, &mask);
		}
		if (m_Override.Weight > 0.0f && m_Override.Current.Clip >= 0)
		{
			Pose over = pose;
			Sample(library, m_Override, over);
			BlendPose(pose, over, m_Override.Weight, joints);
		}
		const CharacterRig& rig = library.GetRig(m_Skeleton);
		Warp(skeleton, rig, pose, m_Warp * (1.0f - m_Override.Weight));
		for (uint32_t link = 0; link < rig.CapeCount; link++)
		{
			const CapeJoint& cape = rig.Cape[link];
			const glm::quat bend = glm::angleAxis(m_CapeLift.Value * cape.Share, cape.LiftAxis) * glm::angleAxis(m_CapeSway.Value * cape.Share, cape.SwayAxis);
			pose[cape.Joint].Rotation = glm::normalize(bend * pose[cape.Joint].Rotation);
		}
		ComputeGlobals(skeleton, pose, globals);
	}

	glm::mat4 ModelTransform(const glm::vec3& base, float facing, float scale)
	{
		const glm::vec3 forward(std::cos(facing), 0.0f, std::sin(facing));
		const glm::vec3 right(std::sin(facing), 0.0f, -std::cos(facing));
		glm::mat4 transform(1.0f);
		transform[0] = glm::vec4(right * scale, 0.0f);
		transform[1] = glm::vec4(0.0f, scale, 0.0f, 0.0f);
		transform[2] = glm::vec4(forward * scale, 0.0f);
		transform[3] = glm::vec4(base, 1.0f);
		return transform;
	}

	CharacterPose DrawCharacterModel(WorldRenderer& renderer, const CharacterLibrary& library, const CharacterLook& look, const CharacterAnimator& animator, const glm::vec3& base, float facing, float scale, const glm::vec4& tint, float emission, const glm::vec4& accent)
	{
		CharacterPose pose;
		pose.Model = ModelTransform(base, facing, scale);
		animator.Evaluate(library, look.Skeleton, pose.Globals);
		const ModelSkeleton& skeleton = library.GetModels().GetSkeleton(look.Skeleton);
		JointMatrices skin;
		ComputeSkin(skeleton, pose.Globals, skin);
		const uint32_t palette = renderer.AddPalette(skin, skeleton.JointCount());
		const bool accented = glm::vec3(accent) != glm::vec3(0.0f);
		for (const CharacterPart& part : look.Parts)
			renderer.DrawModel(part.Mesh, pose.Model, palette, glm::vec4(glm::vec3(tint) * part.Shade, tint.a), emission, base.y, accent, accented ? part.Accent : 0u);
		for (const PropAttachment& prop : look.Props)
			renderer.DrawModel(prop.Mesh, pose.Model * pose.Globals[prop.Joint] * prop.Offset, NoPalette, tint, emission, base.y, accent);
		return pose;
	}
}
