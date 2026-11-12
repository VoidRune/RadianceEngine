#pragma once
#include "Rendering/Characters.h"
#include <functional>

namespace Game
{
	using GroundSampler = std::function<float(Vec2)>;

	class CharacterAnimator
	{
	public:
		struct Input
		{
			Vec2 Position;
			float Height = 0.0f;
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
			bool Grounded = true;
			AttackAnim Attack;
			float Windup = 0.5f;
			Vec2 Push;
			const GroundSampler* Ground = nullptr;
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
			void Advance(float dt, float fadeSeconds);
		};

		enum class Phase : uint8_t
		{
			None,
			Windup,
			Release,
			FollowThrough,
		};

		enum class Course : uint8_t
		{
			Forward,
			Right,
			Back,
			Left,
			Count,
		};

		struct Foot
		{
			Vec2 Lock;
			Vec2 Offset;
			float Yaw = 0.0f;
			float Turn = 0.0f;
			float Reach = 0.0f;
			float Ground = 0.0f;
			glm::vec2 Slope{ 0.0f };
			bool Locked = false;
			bool Stepping = false;
		};

		struct Inertia
		{
			std::array<glm::vec3, MaxModelJoints> Offset{};
			std::array<glm::vec3, MaxModelJoints> Velocity{};
			std::array<glm::vec3, MaxModelJoints> Spin{};
			glm::vec3 Shift{ 0.0f };
			glm::vec3 ShiftVelocity{ 0.0f };
			glm::vec3 Drift{ 0.0f };
			Pose Last;
			bool Valid = false;
			bool Active = false;
		};

		struct Bob
		{
			glm::vec3 Tail{ 0.0f };
			glm::vec3 Velocity{ 0.0f };
			glm::vec3 Head{ 0.0f };
			glm::vec3 Rest{ 0.0f };
		};

		void Reset(const Input& input);
		void Locomote(const CharacterLibrary& library, const Input& input, float dt);
		void Steer(const Input& input, float dt);
		void Act(const CharacterLibrary& library, const Input& input, bool walking, float dt);
		void StartAttack(const CharacterLibrary& library, const Input& input);
		void EndAttack(const CharacterLibrary& library);
		void UpdateFidget(const CharacterLibrary& library, bool still, float dt);
		void Compose(const CharacterLibrary& library, const Input& input, float dt);
		void Inertialize(const CharacterLibrary& library, Pose& pose, uint32_t joints, float dt);
		void Sample(const CharacterLibrary& library, const Crossfade& layers, Pose& pose) const;
		void Twist(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose) const;
		void React(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose) const;
		void HoldStaff(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose) const;
		void Plant(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose, float dt);
		void Swing(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, const Pose& pose, float dt);
		float MoveAngle(const Input& input) const;
		float Random();

		Layer m_Move;
		Crossfade m_Action;
		Crossfade m_Override;
		Crossfade m_Flinch;
		Crossfade m_Fidget;
		Inertia m_Inertia;
		JointMatrices m_Globals;
		std::array<Foot, 2> m_Feet;
		std::array<Bob, MaxSpringJoints> m_Bobs;
		uint32_t m_Skeleton = NoSkeleton;
		Phase m_Phase = Phase::None;
		Course m_Course = Course::Forward;
		Anim m_ReleaseAnim = Anim::Count;
		Anim m_Gait = Anim::Count;
		Vec2 m_LastPosition;
		Vec2 m_Velocity;
		Vec2 m_Pace;
		Vec2 m_Drift;
		Vec2 m_Lean;
		Vec2 m_LeanRate;
		Vec2 m_Recoil;
		Vec2 m_RecoilRate;
		Vec2 m_Whip;
		float m_Moving = 0.0f;
		float m_Cycle = 0.0f;
		float m_Stride = 1.0f;
		float m_StrideAge = 0.0f;
		float m_GaitAge = 0.0f;
		float m_BodyYaw = 0.0f;
		float m_Planted = 0.0f;
		float m_Pelvis = 0.0f;
		float m_SwingTime = 0.0f;
		float m_FlinchCooldown = 0.0f;
		float m_StrikeTime = 0.0f;
		float m_HoldTime = 0.0f;
		float m_IdleTime = 0.0f;
		float m_FidgetDelay = 4.0f;
		int32_t m_LastFidget = -1;
		uint32_t m_Seed = 0x9E3779B9u;
		uint8_t m_Variant = 0;
		uint8_t m_Contacts = 3;
		bool m_Striding = false;
		bool m_Free = false;
		bool m_Switched = false;
		bool m_Turning = false;
		bool m_Posed = false;
		bool m_Swinging = false;
		bool m_DodgeHold = false;
		bool m_HasPosition = false;
		bool m_WasAttacking = false;
		bool m_WasDodging = false;
		bool m_WasStunned = false;
		bool m_WasDead = false;
	};
}
