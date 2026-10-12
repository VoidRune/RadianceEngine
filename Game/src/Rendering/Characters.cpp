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
			"Running_A", "Running_HoldingBow", "Running_HoldingRifle", "Skeletons_Walking",
			"Walking_Backwards", "Running_Strafe_Left", "Running_Strafe_Right",
			"Dodge_Forward", "Dodge_Backward", "Dodge_Left", "Dodge_Right",
			"Hit_A", "Hit_B", "Death_A", "Skeletons_Death",
			"Melee_1H_Attack_Slice_Horizontal", "Melee_1H_Attack_Chop", "Melee_1H_Attack_Stab", "Melee_2H_Attack_Stab", "Melee_2H_Attack_Chop", "Melee_2H_Attack_Spinning", "Melee_Unarmed_Attack_Punch_A",
			"Ranged_Magic_Shoot", "Ranged_Magic_Spellcasting", "Ranged_Magic_Raise",
			"Ranged_Bow_Draw", "Ranged_Bow_Release", "Ranged_Bow_Draw_Up", "Ranged_Bow_Release_Up",
			"Ranged_2H_Aiming", "Ranged_2H_Shoot", "Use_Item",
		};
		static_assert(std::size(AnimNames) == size_t(Anim::Count));

		constexpr float PlayerScale = 0.78f;
		constexpr float DodgeSpeed = 7.0f;
		constexpr float DodgeSeconds = 0.24f;
		constexpr float MoveFade = 0.18f;
		constexpr float ActionFade = 0.1f;
		constexpr float ActionFadeOut = 0.15f;
		constexpr float OverrideFade = 0.08f;
		constexpr float OverrideFadeOut = 0.12f;
		constexpr float MinimumWindup = 0.05f;
		constexpr float FlinchCooldown = 0.5f;

		uint32_t LookKey(const PlayerLook& look)
		{
			return uint32_t(look.Weapon) | uint32_t(look.Head) << 8 | uint32_t(look.Body) << 16 | uint32_t(look.Feet) << 24;
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
		for (size_t i = 0; i < size_t(Anim::Count); i++)
		{
			m_Clips[i] = models.FindClip(AnimNames[i]);
			if (!m_Clips[i])
				RDN_LOG_WARNING("Animation {} is missing from the model library", AnimNames[i]);
		}
		const ModelSkeleton& skeleton = models.GetSkeleton();
		for (uint32_t joint = 0; joint < std::min(skeleton.JointCount(), MaxModelJoints); joint++)
		{
			const std::string& name = skeleton.Names[joint];
			const bool arm = name.find("arm") != std::string::npos || name.find("wrist") != std::string::npos || name.find("hand") != std::string::npos;
			m_UpperBody[joint] = name == "spine" ? 0.5f : name == "chest" || name == "head" || arm ? 1.0f : 0.0f;
		}
		m_RightHand = models.FindJoint("handslot.r").value_or(0);
		m_LeftHand = models.FindJoint("handslot.l").value_or(0);
		m_Head = models.FindJoint("head").value_or(0);

		const auto enemy = [&](EntityKind kind, const char* prefix, float scale, Stance stance)
		{
			CharacterLook& look = m_Enemies[kind];
			AddPrefix(look, prefix);
			look.Scale = scale;
			look.Pose = stance;
			look.Height = TopOf(look) * scale;
			look.Undead = true;
			return std::ref(look);
		};
		enemy(EntityKind::Chaser, "Skeleton_Minion_", 0.72f, Stance::Skeleton);
		Attach(enemy(EntityKind::Shooter, "Skeleton_Rogue_", 0.8f, Stance::Shambling), "Skeleton_Crossbow", m_RightHand, GripOf(ItemBase::Crossbow));
		Attach(enemy(EntityKind::Brute, "Skeleton_Warrior_", 1.15f, Stance::Shambling), "Skeleton_Axe", m_RightHand);
		Attach(enemy(EntityKind::Caster, "Skeleton_Mage_", 0.85f, Stance::Shambling), "Skeleton_Staff", m_RightHand);
		CharacterLook& archivist = enemy(EntityKind::Archivist, "Skeleton_Mage_", 1.7f, Stance::Shambling);
		Attach(archivist, "Skeleton_Staff", m_RightHand);
		Attach(archivist, "spellbook_open", m_LeftHand);
	}

	float CharacterLibrary::TopOf(const CharacterLook& look) const
	{
		float top = 0.0f;
		for (const uint32_t part : look.Parts)
			top = std::max(top, m_Models.GetMesh(part).Max.y);
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
				look.Parts.push_back(*mesh);
	}

	void CharacterLibrary::AddPrefix(CharacterLook& look, const char* prefix) const
	{
		const std::vector<ModelMesh>& meshes = m_Models.GetData().Meshes;
		for (uint32_t i = 0; i < meshes.size(); i++)
			if (meshes[i].Skinned && meshes[i].Name.rfind(prefix, 0) == 0)
				look.Parts.push_back(i);
	}

	void CharacterLibrary::Attach(CharacterLook& look, const char* mesh, uint32_t joint, const glm::mat4& offset) const
	{
		if (const std::optional<uint32_t> index = Mesh(mesh))
			look.Props.push_back({ *index, joint, offset });
	}

	const CharacterLook& CharacterLibrary::ForPlayer(const PlayerLook& look) const
	{
		const uint32_t key = LookKey(look);
		if (const auto it = m_Players.find(key); it != m_Players.end())
			return it->second;

		CharacterLook result;
		result.Scale = PlayerScale;
		result.Pose = PlayerStance(look.Weapon);
		const std::string outfit = OutfitOf(look.Body);
		const std::string legs = OutfitOf(look.Feet);
		const std::string parts[] = { outfit + "_Body", outfit + "_ArmLeft", outfit + "_ArmRight", outfit + "_Cape", legs + "_LegLeft", legs + "_LegRight" };
		for (const std::string& part : parts)
			if (const std::optional<uint32_t> mesh = Mesh(part.c_str()))
				result.Parts.push_back(*mesh);
		switch (look.Head)
		{
		case ItemBase::WizardHat:
			AddParts(result, { "Mage_Head", "Mage_Hat" });
			break;
		case ItemBase::Hood:
			AddParts(result, { "RogueHooded_Head", "RogueHooded_Mask" });
			break;
		case ItemBase::Helm:
			AddParts(result, { "Knight_Head", "Knight_Helmet" });
			break;
		default:
			if (const std::optional<uint32_t> mesh = Mesh((outfit + "_Head").c_str()))
				result.Parts.push_back(*mesh);
			break;
		}
		if (look.Weapon == ItemBase::Bow || look.Weapon == ItemBase::Crossbow)
			AddParts(result, { "Ranger_Quiver" });
		if (const std::optional<uint32_t> weapon = WeaponMesh(look.Weapon))
			result.Props.push_back({ *weapon, look.Weapon == ItemBase::Bow ? m_LeftHand : m_RightHand, GripOf(look.Weapon) });
		result.Height = TopOf(result) * result.Scale;
		return m_Players.emplace(key, std::move(result)).first->second;
	}

	const CharacterLook* CharacterLibrary::ForEnemy(EntityKind kind) const
	{
		const auto it = m_Enemies.find(kind);
		return it != m_Enemies.end() && !it->second.Parts.empty() ? &it->second : nullptr;
	}

	std::optional<uint32_t> CharacterLibrary::WeaponMesh(ItemBase base) const
	{
		switch (base)
		{
		case ItemBase::Wand:
			return Mesh("wand");
		case ItemBase::Staff:
			return Mesh("staff");
		case ItemBase::Sword:
			return Mesh("sword_1handed");
		case ItemBase::Axe:
			return Mesh("axe_1handed");
		case ItemBase::Dagger:
			return Mesh("dagger");
		case ItemBase::Spear:
			return Mesh("spear_A");
		case ItemBase::Bow:
			return Mesh("bow_withString");
		case ItemBase::Crossbow:
			return Mesh("crossbow_2handed");
		default:
			return std::nullopt;
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
		case AbilityId::Fireball:
		case AbilityId::IceSpear:
		case AbilityId::ChainLightning:
		case AbilityId::Blink:
		case AbilityId::EmberBolt:
		case AbilityId::ArcLightning:
		case AbilityId::Frostbite:
		case AbilityId::Swap:
		case AbilityId::Warp:
		case AbilityId::Gust:
			return { Anim::MagicShoot };
		case AbilityId::Meteor:
		case AbilityId::Comet:
		case AbilityId::GravityWell:
			return { Anim::MagicRaise, Anim::MagicShoot };
		case AbilityId::Shockwave:
		case AbilityId::MirrorWard:
		case AbilityId::FrostNova:
			return { Anim::MagicCast };
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

	Anim CharacterAnimator::Locomotion(const Input& input, float& rate, const CharacterLibrary& library) const
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
		const Vec2 forward = FromAngle(input.Facing);
		const Vec2 right(forward.y, -forward.x);
		const float along = Dot(m_Velocity, forward) / speed;
		const float across = Dot(m_Velocity, right) / speed;
		Anim anim = Anim::Run;
		if (input.Pose == Stance::Shambling)
			anim = Anim::SkeletonWalk;
		else if (input.Pose == Stance::Skeleton)
			anim = Anim::Run;
		else if (along > 0.6f)
			anim = input.Pose == Stance::Bow ? Anim::RunWithBow : input.Pose == Stance::Rifle ? Anim::RunWithRifle : Anim::Run;
		else if (along < -0.6f)
			anim = Anim::WalkBack;
		else
			anim = across > 0.0f ? Anim::StrafeRight : Anim::StrafeLeft;
		const std::optional<uint32_t> clip = library.Clip(anim);
		const float stride = clip ? library.GetModels().GetStrideSpeed(*clip) * input.Scale : 0.0f;
		rate = stride > 0.1f ? std::clamp(speed / stride, 0.6f, 1.8f) : 1.0f;
		return anim;
	}

	void CharacterAnimator::StartAttack(const CharacterLibrary& library, const Input& input)
	{
		const std::optional<uint32_t> clip = library.Clip(input.Attack.Windup);
		if (!clip)
			return;
		const ModelLibrary& models = library.GetModels();
		const float duration = models.GetClip(*clip).Duration();
		const float windup = std::max(input.Windup, MinimumWindup);
		Layer layer{ int32_t(*clip), 0.0f, 1.0f, input.Attack.Loop };
		if (!input.Attack.Loop && input.Attack.Release != Anim::Count)
		{
			layer.Rate = std::clamp(duration / windup, 0.5f, 4.0f);
			layer.Loop = false;
		}
		else if (!input.Attack.Loop)
		{
			float strike = models.GetStrikeTime(*clip);
			if (strike < 0.05f)
				strike = duration * 0.4f;
			layer.Rate = std::clamp(strike / windup, 0.6f, 3.0f);
			layer.Time = std::max(0.0f, strike - windup * layer.Rate);
			layer.Loop = false;
		}
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
		if (const std::optional<uint32_t> release = library.Clip(m_ReleaseAnim))
		{
			m_Action.Start({ int32_t(*release), 0.0f, 1.0f, false }, true);
			m_Phase = Phase::Release;
			return;
		}
		m_Action.Current.Rate = std::max(m_Action.Current.Rate, 1.0f);
		m_Phase = Phase::FollowThrough;
	}

	void CharacterAnimator::Update(const CharacterLibrary& library, const Input& input, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		if (!m_HasPosition)
		{
			m_LastPosition = input.Position;
			m_HasPosition = true;
		}
		Vec2 delta = input.Position - m_LastPosition;
		m_LastPosition = input.Position;
		if (LengthSquared(delta) > 4.0f)
			delta = {};
		const Vec2 instant = dt > 1e-5f ? delta / dt : Vec2();
		m_Velocity = Lerp(m_Velocity, instant, 1.0f - std::exp(-dt * 14.0f));
		const float speed = Length(m_Velocity);
		m_Moving = std::clamp((speed - 0.3f) / (1.2f * input.Scale), 0.0f, 1.0f);

		float rate = 1.0f;
		const Anim move = Locomotion(input, rate, library);
		if (const std::optional<uint32_t> clip = library.Clip(move); clip && int32_t(*clip) != m_Move.Current.Clip)
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
			if (const std::optional<uint32_t> clip = library.Clip(input.Undead ? Anim::SkeletonDeath : Anim::Death))
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
			m_Action.Weight = std::min(1.0f, m_Action.Weight + dt / ActionFade);
			const bool finished = !m_Action.Current.Loop && m_Action.Current.Time >= models.GetClip(uint32_t(m_Action.Current.Clip)).Duration();
			if ((m_Phase == Phase::Release || m_Phase == Phase::FollowThrough) && finished)
				m_Phase = Phase::None;
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
			if (const std::optional<uint32_t> clip = library.Clip(anim))
				m_Override.Start({ int32_t(*clip), 0.0f, std::max(models.GetClip(*clip).Duration() / DodgeSeconds, 1.0f), false }, m_Override.Weight > 0.0f);
		}
		else if (input.Stunned && !m_WasStunned && !dodging)
		{
			if (const std::optional<uint32_t> clip = library.Clip(Anim::HitB))
				m_Override.Start({ int32_t(*clip), 0.0f, 1.0f, false }, m_Override.Weight > 0.0f);
		}
		m_WasDodging = dodging;
		m_WasStunned = input.Stunned;
		const bool overriding = dodging || input.Stunned;
		m_Override.Weight = overriding ? std::min(1.0f, m_Override.Weight + dt / OverrideFade) : std::max(0.0f, m_Override.Weight - dt / OverrideFadeOut);
		m_Override.Advance(library, dt, OverrideFade);

		m_FlinchCooldown -= dt;
		if (input.Hurt && m_FlinchCooldown <= 0.0f && m_Phase == Phase::None && !overriding)
			if (const std::optional<uint32_t> clip = library.Clip(Anim::HitA))
			{
				m_Flinch.Start({ int32_t(*clip), 0.0f, 1.4f, false }, false);
				m_Flinch.Weight = 0.55f;
				m_FlinchCooldown = FlinchCooldown;
			}
		m_Flinch.Advance(library, dt, ActionFade);
		m_Flinch.Weight = std::max(0.0f, m_Flinch.Weight - dt * 1.1f);
	}

	void CharacterAnimator::Sample(const CharacterLibrary& library, const Crossfade& layers, Pose& pose) const
	{
		const ModelLibrary& models = library.GetModels();
		const uint32_t joints = models.GetSkeleton().JointCount();
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

	void CharacterAnimator::Evaluate(const CharacterLibrary& library, JointMatrices& globals) const
	{
		const ModelSkeleton& skeleton = library.GetModels().GetSkeleton();
		const uint32_t joints = skeleton.JointCount();
		Pose pose;
		RestPose(skeleton, pose);
		Sample(library, m_Move, pose);
		if (m_Flinch.Weight > 0.0f && m_Flinch.Current.Clip >= 0)
		{
			Pose flinch = pose;
			Sample(library, m_Flinch, flinch);
			BlendPose(pose, flinch, m_Flinch.Weight, joints, &library.GetUpperBody());
		}
		if (m_Action.Weight > 0.0f && m_Action.Current.Clip >= 0)
		{
			Pose action = pose;
			Sample(library, m_Action, action);
			const JointWeights& upper = library.GetUpperBody();
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

	CharacterPose DrawCharacterModel(WorldRenderer& renderer, const CharacterLibrary& library, const CharacterLook& look, const CharacterAnimator& animator, const glm::vec3& base, float facing, float scale, const glm::vec4& tint, float emission)
	{
		CharacterPose pose;
		pose.Model = ModelTransform(base, facing, scale);
		animator.Evaluate(library, pose.Globals);
		JointMatrices skin;
		ComputeSkin(library.GetModels().GetSkeleton(), pose.Globals, skin);
		const uint32_t palette = renderer.AddPalette(skin);
		for (const uint32_t part : look.Parts)
			renderer.DrawModel(part, pose.Model, palette, tint, emission, base.y);
		for (const PropAttachment& prop : look.Props)
			renderer.DrawModel(prop.Mesh, pose.Model * pose.Globals[prop.Joint] * prop.Offset, NoPalette, tint, emission, base.y);
		return pose;
	}
}
