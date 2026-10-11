#pragma once
#include "Rendering/Animation.h"
#include <GameShared/Content/Definitions.h>
#include <GameShared/Content/Items.h>
#include <GameShared/Core/Math.h>
#include <GameShared/Net/Protocol.h>
#include <array>
#include <cstdint>
#include <optional>
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
	};

	struct PropAttachment
	{
		uint32_t Mesh = UINT32_MAX;
		uint32_t Joint = 0;
		glm::mat4 Offset{ 1.0f };
	};

	struct CharacterLook
	{
		std::vector<uint32_t> Parts;
		std::vector<PropAttachment> Props;
		float Scale = 1.0f;
		float Height = 2.0f;
		Stance Pose = Stance::Light;
		bool Undead = false;
	};

	struct CharacterPose
	{
		glm::mat4 Model{ 1.0f };
		JointMatrices Globals;

		glm::vec3 JointPosition(uint32_t joint) const { return glm::vec3(Model * Globals[joint][3]); }
	};

	class CharacterLibrary
	{
	public:
		explicit CharacterLibrary(const ModelLibrary& models);

		const ModelLibrary& GetModels() const { return m_Models; }
		std::optional<uint32_t> Clip(Anim anim) const { return size_t(anim) < m_Clips.size() ? m_Clips[size_t(anim)] : std::nullopt; }
		const JointWeights& GetUpperBody() const { return m_UpperBody; }
		uint32_t GetRightHand() const { return m_RightHand; }
		uint32_t GetLeftHand() const { return m_LeftHand; }
		uint32_t GetHead() const { return m_Head; }

		const CharacterLook& ForPlayer(const PlayerLook& look) const;
		const CharacterLook* ForEnemy(EntityKind kind) const;
		std::optional<uint32_t> WeaponMesh(ItemBase base) const;
		std::vector<uint32_t> ArmorMeshes(ItemBase base) const;
		std::optional<uint32_t> ArrowMesh(AbilityId ability) const;

	private:
		std::optional<uint32_t> Mesh(const char* name) const;
		float TopOf(const CharacterLook& look) const;
		void AddParts(CharacterLook& look, std::initializer_list<const char*> names) const;
		void AddPrefix(CharacterLook& look, const char* prefix) const;
		void Attach(CharacterLook& look, const char* mesh, uint32_t joint, const glm::mat4& offset = glm::mat4(1.0f)) const;

		const ModelLibrary& m_Models;
		std::array<std::optional<uint32_t>, size_t(Anim::Count)> m_Clips{};
		JointWeights m_UpperBody{};
		uint32_t m_RightHand = 0;
		uint32_t m_LeftHand = 0;
		uint32_t m_Head = 0;
		std::unordered_map<EntityKind, CharacterLook> m_Enemies;
		mutable std::unordered_map<uint32_t, CharacterLook> m_Players;
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
		void Evaluate(const CharacterLibrary& library, JointMatrices& globals) const;

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

		void StartAttack(const CharacterLibrary& library, const Input& input);
		void EndAttack(const CharacterLibrary& library);
		Anim Locomotion(const Input& input, float& rate, const CharacterLibrary& library) const;
		void Sample(const CharacterLibrary& library, const Crossfade& layers, Pose& pose) const;

		Crossfade m_Move;
		Crossfade m_Action;
		Crossfade m_Override;
		Crossfade m_Flinch;
		Phase m_Phase = Phase::None;
		Anim m_ReleaseAnim = Anim::Count;
		Vec2 m_LastPosition;
		Vec2 m_Velocity;
		float m_Moving = 0.0f;
		float m_FlinchCooldown = 0.0f;
		bool m_HasPosition = false;
		bool m_WasAttacking = false;
		bool m_WasDodging = false;
		bool m_WasStunned = false;
		bool m_WasDead = false;
	};

	CharacterPose DrawCharacterModel(WorldRenderer& renderer, const CharacterLibrary& library, const CharacterLook& look, const CharacterAnimator& animator, const glm::vec3& base, float facing, float scale, const glm::vec4& tint, float emission);
	glm::mat4 ModelTransform(const glm::vec3& base, float facing, float scale);
}
