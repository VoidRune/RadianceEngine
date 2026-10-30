#pragma once
#include "Rendering/Animation.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Content/Items.h>
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
	class WorldRenderer;

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
		IdleBracer,
		IdleScratch,
		Walk,
		Jog,
		Run,
		RunWithBow,
		RunWithRifle,
		SkeletonWalk,
		WalkBack,
		StrafeLeft,
		StrafeRight,
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

	enum class Stance : uint8_t
	{
		Light,
		Heavy,
		Bow,
		Rifle,
		Skeleton,
		Shambling,
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

	inline constexpr uint32_t MaxCapeJoints = 4;

	struct CapeJoint
	{
		uint32_t Joint = 0;
		glm::vec3 LiftAxis{ 1.0f, 0.0f, 0.0f };
		glm::vec3 SwayAxis{ 0.0f, 0.0f, 1.0f };
		float Share = 0.0f;
	};

	struct CharacterRig
	{
		std::array<std::optional<uint32_t>, size_t(Anim::Count)> Clips{};
		JointWeights UpperBody{};
		uint32_t RightHand = 0;
		uint32_t LeftHand = 0;
		uint32_t Head = 0;
		std::array<CapeJoint, MaxCapeJoints> Cape{};
		uint32_t CapeCount = 0;
		std::array<std::optional<uint32_t>, 3> Twist{};
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
		std::vector<uint32_t> WeaponMeshes(ItemBase base) const;
		std::vector<uint32_t> ArmorMeshes(ItemBase base) const;
		std::optional<uint32_t> ArrowMesh(AbilityId ability) const;

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
		std::unordered_map<EntityKind, CharacterLook> m_Enemies;
		mutable std::unordered_map<uint64_t, CharacterLook> m_Players;
	};

	AttackAnim PlayerAttackAnim(AbilityId ability);
	AttackAnim EnemyAttackAnim(EntityKind kind);
	Stance PlayerStance(ItemBase weapon);

	class CharacterAnimator
	{
	public:
		struct Input
		{
			Vec2 Position;
			float Facing = 0.0f;
			float Scale = 1.0f;
			uint32_t Skeleton = 0;
			Stance Pose = Stance::Light;
			bool Undead = false;
			bool Dead = false;
			bool Dashing = false;
			bool Stunned = false;
			bool Hurt = false;
			bool Attacking = false;
			AttackAnim Attack;
			float Windup = 0.5f;
		};

		void Update(const CharacterLibrary& library, const Input& input, float dt);
		void Evaluate(const CharacterLibrary& library, uint32_t skeleton, JointMatrices& globals) const;
		float StrikeWeight(const CharacterLibrary& library) const;

	private:
		struct Layer
		{
			int32_t Clip = -1;
			float Time = 0.0f;
			float Rate = 1.0f;
			bool Loop = true;
		};

		struct Crossfade
		{
			Layer Current;
			Layer Previous;
			float Blend = 1.0f;
			float Weight = 0.0f;

			void Start(const Layer& layer, bool fade);
			void Advance(const CharacterLibrary& library, float dt, float fadeSeconds);
		};

		enum class Phase : uint8_t
		{
			None,
			Windup,
			Release,
			FollowThrough,
		};

		struct Spring
		{
			float Value = 0.0f;
			float Rate = 0.0f;

			void Advance(float target, float dt);
		};

		void StartAttack(const CharacterLibrary& library, const Input& input);
		void EndAttack(const CharacterLibrary& library);
		void UpdateCape(const Input& input, float dt);
		void UpdateFidget(const CharacterLibrary& library, bool still, float dt);
		Anim Locomotion(const Input& input, float& rate, const CharacterLibrary& library);
		void Sample(const CharacterLibrary& library, const Crossfade& layers, Pose& pose) const;
		void Warp(const ModelSkeleton& skeleton, const CharacterRig& rig, Pose& pose, float amount) const;
		float Random();

		Crossfade m_Move;
		Crossfade m_Action;
		Crossfade m_Override;
		Crossfade m_Flinch;
		Crossfade m_Fidget;
		Spring m_CapeLift;
		Spring m_CapeSway;
		float m_CapeTime = 0.0f;
		uint32_t m_Skeleton = NoSkeleton;
		Phase m_Phase = Phase::None;
		Anim m_ReleaseAnim = Anim::Count;
		Vec2 m_LastPosition;
		Vec2 m_Velocity;
		float m_Moving = 0.0f;
		float m_FlinchCooldown = 0.0f;
		float m_StrikeTime = 0.0f;
		float m_HoldTime = 0.0f;
		float m_IdleTime = 0.0f;
		float m_FidgetDelay = 4.0f;
		int32_t m_LastFidget = -1;
		float m_Warp = 0.0f;
		uint32_t m_Seed = 0x9E3779B9u;
		uint8_t m_Variant = 0;
		bool m_Backward = false;
		bool m_DodgeHold = false;
		bool m_HasPosition = false;
		bool m_WasAttacking = false;
		bool m_WasDodging = false;
		bool m_WasStunned = false;
		bool m_WasDead = false;
	};

	CharacterPose DrawCharacterModel(WorldRenderer& renderer, const CharacterLibrary& library, const CharacterLook& look, const CharacterAnimator& animator, const glm::vec3& base, float facing, float scale, const glm::vec4& tint, float emission, const glm::vec4& accent = glm::vec4(0.0f), const glm::vec4& flash = glm::vec4(0.0f));
	glm::mat4 ModelTransform(const glm::vec3& base, float facing, float scale);
}
