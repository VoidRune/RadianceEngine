#pragma once
#include "Rendering/Animation.h"
#include "Rendering/WorldRenderer.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Core/Math.h>
#include <GameShared/Net/Protocol.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game
{
	class ModelLibrary;
	class CharacterAnimator;

	enum class Anim : uint8_t
	{
		Idle,
		SkeletonIdle,
		HeavyIdle,
		BowIdle,
		IdleLook,
		IdleLookRight,
		IdleScan,
		IdleWand,
		IdleStretch,
		IdleRub,
		IdleNod,
		Walk,
		Jog,
		Run,
		RunWithBow,
		RunWithRifle,
		SkeletonWalk,
		WalkBack,
		StrafeLeft,
		StrafeRight,
		StrafeLeftWalk,
		StrafeRightWalk,
		DodgeForward,
		DodgeBack,
		DodgeLeft,
		DodgeRight,
		HitA,
		HitB,
		Death,
		SkeletonDeath,
		Slash,
		Chop,
		Stab,
		HeavyStab,
		HeavyChop,
		Spinning,
		Punch,
		MagicShoot,
		MagicCast,
		MagicRaise,
		CastFlick,
		CastSlash,
		CastBackhand,
		CastLunge,
		CastHurl,
		CastSurge,
		CastLash,
		CastGrasp,
		CastLob,
		CastSmite,
		CastSummon,
		CastCall,
		CastPush,
		CastWard,
		CastNova,
		CastBlink,
		BowDraw,
		BowRelease,
		BowDrawUp,
		BowReleaseUp,
		RifleAim,
		RifleShoot,
		UseItem,
		Count,
	};

	inline constexpr std::array<Anim, 7> IdleFidgets = { Anim::IdleLook, Anim::IdleLookRight, Anim::IdleScan, Anim::IdleWand, Anim::IdleStretch, Anim::IdleRub, Anim::IdleNod };

	enum class Stance : uint8_t
	{
		Light,
		Heavy,
		Bow,
		Rifle,
		Skeleton,
		Shambling,
		Staff,
	};

	struct AttackAnim
	{
		Anim Windup = Anim::Count;
		Anim Release = Anim::Count;
		bool Loop = false;
		uint8_t Variants = 1;
		bool Hold = false;
		float HoldAt = 0.62f;
		float HoldReach = 0.28f;
	};

	struct PropAttachment
	{
		uint32_t Mesh = UINT32_MAX;
		uint32_t Joint = 0;
		glm::mat4 Offset{ 1.0f };
	};

	struct CharacterPart
	{
		uint32_t Mesh = UINT32_MAX;
		float Shade = 1.0f;
		uint32_t Accent = 0;
	};

	struct CharacterLook
	{
		std::vector<CharacterPart> Parts;
		std::vector<PropAttachment> Props;
		uint32_t Skeleton = 0;
		float Scale = 1.0f;
		float Height = 2.0f;
		Stance Pose = Stance::Light;
		std::optional<glm::vec3> Tip;
		uint32_t TipJoint = 0;
		bool Undead = false;
	};

	struct LegJoints
	{
		uint32_t Upper = 0;
		uint32_t Lower = 0;
		uint32_t Foot = 0;
		glm::vec3 Hinge{ -1.0f, 0.0f, 0.0f };
	};

	inline constexpr uint32_t MaxBodyCapsules = 11;

	struct SpringJoint
	{
		uint32_t Joint = 0;
		int32_t Parent = -1;
		glm::vec3 Tail{ 0.0f };
		float Stiffness = 0.0f;
		float Damping = 0.0f;
		float Drag = 0.0f;
		float Gravity = 0.0f;
		float Radius = 0.0f;
		float Bulk = 1.0f;
		float Margin = 0.9f;
		std::array<float, MaxBodyCapsules> Clear{};
	};

	struct BodyCapsule
	{
		uint32_t From = 0;
		uint32_t To = 0;
		float Radius = 0.0f;
		bool Bulky = false;
		glm::vec3 FromLocal{ 0.0f };
		glm::vec3 ToLocal{ 0.0f };
	};

	inline constexpr uint32_t MaxSpringJoints = 40;

	struct CharacterRig
	{
		std::array<std::optional<uint32_t>, size_t(Anim::Count)> Clips{};
		JointWeights UpperBody{};
		uint32_t RightHand = 0;
		uint32_t LeftHand = 0;
		uint32_t Head = 0;
		std::optional<uint32_t> Hips;
		std::optional<uint32_t> Spine;
		std::optional<uint32_t> Chest;
		std::optional<uint32_t> Neck;
		std::array<LegJoints, 2> Legs{};
		bool HasLegs = false;
		std::array<uint32_t, 3> RightArm{};
		bool HasRightArm = false;
		float HipHeight = 1.0f;
		float AnkleHeight = 0.0f;
		std::vector<SpringJoint> Springs;
		std::vector<BodyCapsule> Colliders;
	};

	struct CharacterPose
	{
		glm::mat4 Model{ 1.0f };
		JointMatrices Globals;

		glm::vec3 JointPosition(uint32_t joint) const { return glm::vec3(Model * Globals[joint][3]); }
		glm::vec3 JointPoint(uint32_t joint, const glm::vec3& local) const { return glm::vec3(Model * Globals[joint] * glm::vec4(local, 1.0f)); }
	};

	class CharacterLibrary
	{
	public:
		explicit CharacterLibrary(const ModelLibrary& models);

		const ModelLibrary& GetModels() const { return m_Models; }
		const CharacterRig& GetRig(uint32_t skeleton) const { return m_Rigs[skeleton < m_Rigs.size() ? skeleton : m_Shared]; }
		std::optional<uint32_t> Clip(Anim anim, uint32_t skeleton) const { return size_t(anim) < size_t(Anim::Count) ? GetRig(skeleton).Clips[size_t(anim)] : std::nullopt; }

		const CharacterLook& ForPlayer(const PlayerLook& look) const;
		const CharacterLook* ForEnemy(EntityKind kind) const;
		std::vector<uint32_t> WeaponMeshes(Focus focus) const;

	private:
		std::optional<uint32_t> Mesh(const char* name) const;
		float TopOf(const CharacterLook& look) const;
		void AddParts(CharacterLook& look, std::initializer_list<const char*> names) const;
		void AddPrefix(CharacterLook& look, const char* prefix) const;
		std::vector<uint32_t> NodeMeshes(const std::string& name) const;
		void AddNode(CharacterLook& look, const std::string& name, float shade = 1.0f, bool dyed = false) const;
		uint32_t AccentOf(uint32_t mesh, bool dyed) const;
		void Attach(CharacterLook& look, const char* mesh, uint32_t joint, const glm::mat4& offset = glm::mat4(1.0f)) const;
		void DressHero(CharacterLook& look, const PlayerLook& player) const;
		void DressKayKit(CharacterLook& look, const PlayerLook& player) const;

		const ModelLibrary& m_Models;
		std::vector<CharacterRig> m_Rigs;
		uint32_t m_Shared = 0;
		std::optional<uint32_t> m_Hero;
		std::array<std::optional<uint32_t>, size_t(Outfit::Count)> m_Outfits{};
		std::unordered_map<EntityKind, CharacterLook> m_Enemies;
		mutable std::unordered_map<uint64_t, CharacterLook> m_Players;
	};

	AttackAnim PlayerAttackAnim(AbilityId ability);
	AttackAnim EnemyAttackAnim(EntityKind kind);

	CharacterPose DrawCharacterModel(WorldRenderer& renderer, const CharacterLibrary& library, const CharacterLook& look, const CharacterAnimator& animator, const glm::vec3& base, float facing, float scale, const glm::vec4& tint, float emission, const glm::vec4& accent = glm::vec4(0.0f), const glm::vec4& flash = glm::vec4(0.0f), const ModelDissolve& dissolve = {});
	glm::mat4 ModelTransform(const glm::vec3& base, float facing, float scale);
}
