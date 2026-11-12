#include "CharacterAnimator.h"
#include "Rendering/ModelLibrary.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr float DodgeSpeed = 7.0f;
		constexpr float DodgeSeconds = 0.24f;
		constexpr float ActionFade = 0.1f;
		constexpr float ActionFadeOut = 0.15f;
		constexpr float OverrideFade = 0.08f;
		constexpr float OverrideFadeOut = 0.12f;
		constexpr float MinimumWindup = 0.05f;
		constexpr float QuickDodge = 0.8f;
		constexpr float FidgetFade = 0.35f;
		constexpr float FidgetBreak = 0.2f;
		constexpr float FidgetDelayMin = 8.0f;
		constexpr float FidgetDelaySpread = 8.0f;
		constexpr float StrikeSettle = 18.0f;
		constexpr float HoldRelease = 1.7f;
		constexpr float StrikeLead = 0.12f;
		constexpr float StrikeTail = 0.16f;

		constexpr float StillSpeed = 0.35f;
		constexpr float GaitStickiness = 0.12f;
		constexpr float SlowTierBias = 0.4f;
		constexpr float GaitDwell = 0.2f;
		constexpr float GaitSettle = 0.12f;
		constexpr float PaceRate = 60.0f;
		constexpr float StartCycle = 0.08f;
		constexpr float CourseStickiness = 0.3f;
		constexpr float StrainPenalty = 5.0f;
		constexpr float FullStrain = 1.35f;
		constexpr float CourseAngles[] = { 0.0f, 0.5f * Pi, Pi, -0.5f * Pi };
		constexpr float MinimumRate = 0.8f;
		constexpr float MaximumRate = 1.35f;
		constexpr float ShortStride = 0.85f;
		constexpr float LongStride = 1.2f;
		constexpr float MinimumStride = 0.55f;
		constexpr float MaximumStride = 1.5f;
		constexpr float StrideRate = 8.0f;
		constexpr float BlendHalfLife = 0.09f;
		constexpr float MaximumSpin = 14.0f;

		constexpr float HipTurn = 2.3f;
		constexpr float MaximumTwist = 2.4f;
		constexpr glm::vec3 StaffUpper{ -0.14f, -1.0f, 0.12f };
		constexpr glm::vec3 StaffFore{ 0.12f, -0.3f, 1.0f };
		constexpr glm::vec3 StaffShaft{ 0.0f, 1.0f, 0.1f };
		constexpr float ChestTwist = 1.2f;
		constexpr float HeadTwist = 0.75f;
		constexpr float NeckShare = 0.4f;
		constexpr float TurnStart = 0.95f;
		constexpr float TurnStop = 0.08f;
		constexpr float SteerRate = 12.0f;
		constexpr float TurnRate = 9.0f;
		constexpr float CastTurnRate = 16.0f;
		constexpr float SpineShare = 0.45f;

		constexpr float DriftRate = 3.0f;
		constexpr float LeanGain = 0.05f;
		constexpr float MaximumLean = 0.26f;
		constexpr float LeanFrequency = 9.0f;
		constexpr float LeanDamping = 0.65f;
		constexpr float LeanShift = 0.3f;
		constexpr float LooseLean = 0.6f;

		constexpr float FlinchCooldown = 0.35f;
		constexpr float FlinchWeight = 0.5f;
		constexpr float FlinchFade = 1.1f;
		constexpr float RecoilImpulse = 15.0f;
		constexpr float RecoilFrequency = 13.0f;
		constexpr float RecoilDamping = 0.4f;
		constexpr float MaximumRecoil = 1.6f;
		constexpr float WhipRate = 11.0f;
		constexpr float RecoilSpine = 0.1f;
		constexpr float RecoilChest = 0.13f;
		constexpr float RecoilNeck = 0.16f;
		constexpr float RecoilShove = 0.06f;

		constexpr float PlantRate = 12.0f;
		constexpr float LockReach = 0.3f;
		constexpr float IdleReach = 0.15f;
		constexpr float RestReach = 0.02f;
		constexpr float FootTurn = 0.5f;
		constexpr float ReleaseRate = 16.0f;
		constexpr float StepRate = 9.0f;
		constexpr float StepLift = 0.07f;
		constexpr float GroundReach = 0.45f;
		constexpr float FloorClamp = 0.9f;
		constexpr float GroundRate = 18.0f;
		constexpr float PelvisRate = 10.0f;
		constexpr float PelvisDrop = 0.4f;
		constexpr float PelvisRise = 0.2f;
		constexpr float SlopeProbe = 0.12f;
		constexpr float MaximumSlope = 0.6f;
		constexpr float SlopeRate = 12.0f;

		constexpr float SwingStep = 1.0f / 90.0f;
		constexpr uint32_t SwingSteps = 4;
		constexpr float SwingGravity = 9.8f;
		constexpr float SwingBreeze = 0.5f;
		constexpr float SwingCarry = 15.0f;

		glm::vec2 Local(Vec2 world, float facing)
		{
			const Vec2 forward = FromAngle(facing);
			return { Dot(world, Vec2(forward.y, -forward.x)), Dot(world, forward) };
		}

		glm::vec3 TiltAxis(const glm::vec2& direction)
		{
			return glm::normalize(glm::vec3(direction.y, 0.0f, -direction.x));
		}

		float Ease(float rate, float dt)
		{
			return 1.0f - std::exp(-dt * rate);
		}

		float YawOf(const ModelSkeleton& skeleton, const Pose& pose, uint32_t joint, const glm::vec3& forward)
		{
			const glm::vec3 direction = GlobalRotation(skeleton, pose, int32_t(joint)) * forward;
			return std::atan2(direction.x, direction.z);
		}

		void Oscillate(Vec2& value, Vec2& rate, Vec2 target, float frequency, float damping, float dt)
		{
			constexpr float Step = 1.0f / 120.0f;
			for (float left = std::min(dt, 0.1f); left > 0.0f; left -= Step)
			{
				const float step = std::min(left, Step);
				rate += ((target - value) * (frequency * frequency) - rate * (2.0f * damping * frequency)) * step;
				value += rate * step;
			}
		}

		Anim IdleOf(Stance stance)
		{
			switch (stance)
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

	void CharacterAnimator::Crossfade::Advance(float dt, float fadeSeconds)
	{
		Current.Time += dt * Current.Rate;
		Previous.Time += dt * Previous.Rate;
		Blend = std::min(1.0f, Blend + dt / std::max(fadeSeconds, 1e-3f));
	}

	float CharacterAnimator::MoveAngle(const Input& input) const
	{
		const glm::vec2 local = Local(m_Velocity, input.Facing);
		return std::atan2(local.x, local.y);
	}

	void CharacterAnimator::Reset(const Input& input)
	{
		m_Feet = {};
		m_Inertia.Active = false;
		m_Swinging = false;
		m_BodyYaw = input.Facing;
		m_Turning = false;
		m_Velocity = {};
		m_Pace = {};
		m_Drift = {};
		m_Lean = {};
		m_LeanRate = {};
		m_Pelvis = 0.0f;
	}

	void CharacterAnimator::Locomote(const CharacterLibrary& library, const Input& input, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		const float speed = Length(m_Pace);
		const bool striding = speed >= StillSpeed * input.Scale;
		m_Free = input.Pose != Stance::Skeleton && input.Pose != Stance::Shambling;
		const auto matchOf = [&](Anim anim)
		{
			const std::optional<uint32_t> clip = library.Clip(anim, m_Skeleton);
			const float stride = clip ? models.GetStrideSpeed(*clip) * input.Scale : 0.0f;
			return stride > 0.1f ? speed / stride : 1.0f;
		};
		Anim anim = IdleOf(input.Pose);
		float rate = 1.0f;
		float stride = 1.0f;
		if (striding && !m_Free)
		{
			anim = input.Pose == Stance::Shambling ? Anim::SkeletonWalk : Anim::Run;
			rate = std::clamp(matchOf(anim), 0.6f, 1.8f);
		}
		else if (striding)
		{
			std::array<Anim, 3> gaits = { Anim::Walk, Anim::Jog, Anim::Run };
			if (input.Pose == Stance::Bow || input.Pose == Stance::Rifle)
				gaits[2] = input.Pose == Stance::Bow ? Anim::RunWithBow : Anim::RunWithRifle;
			Anim forward = gaits.back();
			float best = 1e9f;
			for (const Anim gait : gaits)
			{
				const std::optional<uint32_t> clip = library.Clip(gait, m_Skeleton);
				if (!clip)
					continue;
				const float score = std::abs(std::log(std::max(matchOf(gait), 1e-3f))) - (int32_t(*clip) == m_Move.Clip ? GaitStickiness : 0.0f);
				if (score < best)
				{
					best = score;
					forward = gait;
				}
			}
			if (m_Striding && m_StrideAge > GaitSettle && m_GaitAge < GaitDwell && std::find(gaits.begin(), gaits.end(), m_Gait) != gaits.end() && library.Clip(m_Gait, m_Skeleton))
				forward = m_Gait;
			if (forward != m_Gait)
			{
				m_Gait = forward;
				m_GaitAge = 0.0f;
			}
			const auto tierOf = [&](Anim slow, Anim fast)
			{
				const std::optional<uint32_t> slowClip = library.Clip(slow, m_Skeleton);
				const std::optional<uint32_t> fastClip = library.Clip(fast, m_Skeleton);
				if (!slowClip || !fastClip)
					return fastClip ? fast : slow;
				const auto scoreOf = [&](Anim anim, uint32_t clip) { return std::abs(std::log(std::max(matchOf(anim), 1e-3f))) - (int32_t(clip) == m_Move.Clip ? GaitStickiness : 0.0f); };
				return scoreOf(slow, *slowClip) - SlowTierBias < scoreOf(fast, *fastClip) ? slow : fast;
			};
			const std::array<Anim, size_t(Course::Count)> courses
 = { forward, tierOf(Anim::StrafeRightWalk, Anim::StrafeRight), Anim::WalkBack, tierOf(Anim::StrafeLeftWalk, Anim::StrafeLeft) };
			const float angle = MoveAngle(input);
			Course course = Course::Forward;
			best = 1e9f;
			for (size_t option = 0; option < courses.size(); option++)
			{
				if (!library.Clip(courses[option], m_Skeleton))
					continue;
				const float strain = StrainPenalty * std::max(std::log(std::max(matchOf(courses[option]), 1e-3f) / FullStrain), 0.0f);
				const float score = std::abs(AngleBetween(CourseAngles[option], angle)) + strain - (m_Striding && Course(option) == m_Course ? CourseStickiness : 0.0f);
				if (score < best)
				{
					best = score;
					course = Course(option);
				}
			}
			m_Course = course;
			anim = courses[size_t(course)];
			const float match = matchOf(anim);
			rate = std::clamp(match / std::clamp(match, ShortStride, LongStride), MinimumRate, MaximumRate);
			stride = std::clamp(match / rate, MinimumStride, MaximumStride);
		}
		m_Stride += (stride - m_Stride) * Ease(StrideRate, dt);

		if (const std::optional<uint32_t> clip = library.Clip(anim, m_Skeleton); clip && int32_t(*clip) != m_Move.Clip)
		{
			m_Switched = m_Move.Clip >= 0;
			if (striding && !m_Striding)
				m_Cycle = StartCycle + (Local(m_Pace, input.Facing).x < 0.0f ? 0.5f : 0.0f);
			m_Move = { int32_t(*clip), 0.0f, rate, true };
		}
		m_StrideAge = striding && m_Striding ? m_StrideAge + dt : 0.0f;
		m_GaitAge += dt;
		m_Striding = striding;
		m_Move.Rate = rate;
		m_Contacts = 3;
		if (m_Move.Clip < 0)
			return;
		const uint32_t clip = uint32_t(m_Move.Clip);
		const float duration = models.GetClip(clip).Duration();
		if (!m_Striding || duration <= 0.0f)
		{
			m_Move.Time += dt * rate;
			return;
		}
		m_Cycle += dt * rate / duration;
		m_Cycle -= std::floor(m_Cycle);
		m_Move.Time = (m_Cycle + models.GetSync(clip)) * duration;
		m_Contacts = uint8_t((models.IsPlanted(clip, m_Move.Time, 0) ? 1u : 0u) | (models.IsPlanted(clip, m_Move.Time, 1) ? 2u : 0u));
	}

	void CharacterAnimator::Steer(const Input& input, float dt)
	{
		if (!m_Free || m_Override.Weight > 0.5f)
		{
			m_BodyYaw = m_Free ? LerpAngle(m_BodyYaw, input.Facing, Ease(2.0f * SteerRate, dt)) : input.Facing;
			m_Turning = false;
			return;
		}
		if (input.Dead)
			return;
		float target = m_BodyYaw;
		float rate = SteerRate;
		if (m_Striding)
		{
			target = input.Facing - std::clamp(AngleBetween(CourseAngles[size_t(m_Course)], MoveAngle(input)), -HipTurn, HipTurn);
			m_Turning = false;
		}
		else
		{
			const float away = std::abs(AngleBetween(m_BodyYaw, input.Facing));
			if (input.Attacking || away > TurnStart)
				m_Turning = true;
			else if (away < TurnStop)
				m_Turning = false;
			if (m_Turning)
			{
				target = input.Facing;
				rate = input.Attacking ? CastTurnRate : TurnRate;
			}
		}
		m_BodyYaw = LerpAngle(m_BodyYaw, target, Ease(rate, dt));
		const float twist = AngleBetween(m_BodyYaw, input.Facing);
		if (std::abs(twist) > MaximumTwist)
			m_BodyYaw = input.Facing - std::copysign(MaximumTwist, twist);
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
			m_HoldTime = strike * input.Attack.HoldAt;
			layer.Rate = std::clamp(m_HoldTime / input.Attack.HoldReach, 0.6f, 3.0f);
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

	void CharacterAnimator::Act(const CharacterLibrary& library, const Input& input, bool walking, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		const float speed = Length(m_Velocity);
		if (input.Attacking && !m_WasAttacking)
			StartAttack(library, input);
		else if (!input.Attacking && m_WasAttacking)
			EndAttack(library);
		m_WasAttacking = input.Attacking;
		if (m_Phase != Phase::None && m_Action.Current.Clip >= 0)
		{
			m_Action.Advance(dt, ActionFade);
			if (m_Phase == Phase::Windup && m_HoldTime > 0.0f)
				m_Action.Current.Time = std::min(m_Action.Current.Time, m_HoldTime);
			m_Action.Weight = std::min(1.0f, m_Action.Weight + dt / ActionFade);
			const bool finished = !m_Action.Current.Loop && m_Action.Current.Time >= models.GetClip(uint32_t(m_Action.Current.Clip)).Duration();
			if ((m_Phase == Phase::Release || m_Phase == Phase::FollowThrough) && finished)
				m_Phase = Phase::None;
			if (m_StrikeTime > 0.0f && m_Action.Current.Time >= m_StrikeTime)
				m_Action.Current.Rate += (1.0f - m_Action.Current.Rate) * Ease(StrikeSettle, dt);
		}
		else
		{
			m_Phase = Phase::None;
			m_Action.Weight = std::max(0.0f, m_Action.Weight - dt / ActionFadeOut);
		}

		const bool dodging = input.Dashing && speed > DodgeSpeed;
		if (dodging && !m_WasDodging)
		{
			const glm::vec2 local = Local(m_Velocity, input.Facing);
			const Anim anim = std::abs(local.y) >= std::abs(local.x) ? (local.y > 0.0f ? Anim::DodgeForward : Anim::DodgeBack) : (local.x > 0.0f ? Anim::DodgeRight : Anim::DodgeLeft);
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
			if (input.Attacking || walking || m_Override.Current.Time >= models.GetClip(uint32_t(m_Override.Current.Clip)).Duration() - OverrideFadeOut)
				m_DodgeHold = false;
		m_WasDodging = dodging;
		m_WasStunned = input.Stunned;
		const bool overriding = dodging || input.Stunned || m_DodgeHold;
		m_Override.Weight = overriding ? std::min(1.0f, m_Override.Weight + dt / OverrideFade) : std::max(0.0f, m_Override.Weight - dt / OverrideFadeOut);
		m_Override.Advance(dt, OverrideFade);

		m_FlinchCooldown -= dt;
		if (input.Hurt && m_FlinchCooldown <= 0.0f && !overriding)
			if (const std::optional<uint32_t> clip = library.Clip(Anim::HitA, m_Skeleton))
			{
				m_Flinch.Start({ int32_t(*clip), 0.0f, 1.4f, false }, false);
				m_Flinch.Weight = FlinchWeight;
				m_FlinchCooldown = FlinchCooldown;
			}
		m_Flinch.Advance(dt, ActionFade);
		m_Flinch.Weight = std::max(0.0f, m_Flinch.Weight - dt * FlinchFade * FlinchWeight);

		const bool still = !m_Striding && m_Phase == Phase::None && !overriding && m_Override.Weight <= 0.0f && m_Flinch.Weight < 0.1f && !m_Turning;
		UpdateFidget(library, still, dt);
	}

	void CharacterAnimator::Update(const CharacterLibrary& library, const Input& input, float dt)
	{
		if (input.Skeleton != m_Skeleton)
		{
			*this = {};
			m_Skeleton = input.Skeleton;
		}
		if (!m_HasPosition)
		{
			m_LastPosition = input.Position;
			m_BodyYaw = input.Facing;
			m_HasPosition = true;
			m_Seed ^= uint32_t(reinterpret_cast<uintptr_t>(this) >> 4) | 1u;
		}
		if (dt <= 0.0f && m_Posed)
			return;
		Vec2 delta = input.Position - m_LastPosition;
		m_LastPosition = input.Position;
		if (LengthSquared(delta) > 4.0f)
		{
			delta = {};
			Reset(input);
		}
		const Vec2 instant = dt > 1e-5f ? delta / dt : Vec2();
		m_Velocity = Lerp(m_Velocity, instant, Ease(14.0f, dt));
		m_Pace = Lerp(m_Pace, instant, Ease(PaceRate, dt));
		m_Drift = Lerp(m_Drift, m_Velocity, Ease(DriftRate, dt));
		m_Moving = std::clamp((Length(m_Velocity) - 0.3f) / (1.2f * input.Scale), 0.0f, 1.0f);

		Locomote(library, input, dt);

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
			m_Fidget.Weight = 0.0f;
			m_Override.Weight = std::min(1.0f, m_Override.Weight + dt / OverrideFade);
			m_Override.Advance(dt, OverrideFade);
		}
		else
			Act(library, input, Length(instant) > 1.2f * input.Scale, dt);

		Steer(input, dt);
		const bool upright = !input.Dead && input.Grounded && m_Override.Weight < 0.5f;
		const Vec2 lean = upright ? ClampLength((m_Velocity - m_Drift) * (LeanGain / std::max(input.Scale, 0.1f)), MaximumLean) : Vec2();
		Oscillate(m_Lean, m_LeanRate, lean, LeanFrequency, LeanDamping, dt);
		if (!input.Dead)
			m_RecoilRate += input.Push * RecoilImpulse;
		Oscillate(m_Recoil, m_RecoilRate, Vec2(), RecoilFrequency, RecoilDamping, dt);
		m_Recoil = ClampLength(m_Recoil, MaximumRecoil);
		m_Whip = Lerp(m_Whip, m_Recoil, Ease(WhipRate, dt));

		Compose(library, input, dt);
	}

	void CharacterAnimator::UpdateFidget(const CharacterLibrary& library, bool still, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		m_IdleTime = still ? m_IdleTime + dt : 0.0f;
		if (still && m_Fidget.Current.Clip < 0 && m_IdleTime > m_FidgetDelay)
		{
			std::array<uint32_t, IdleFidgets.size()> choices{};
			uint32_t count = 0;
			for (const Anim anim : IdleFidgets)
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
		m_Fidget.Advance(dt, FidgetFade);
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

	void CharacterAnimator::Inertialize(const CharacterLibrary& library, Pose& pose, uint32_t joints, float dt)
	{
		Inertia& inertia = m_Inertia;
		const std::optional<uint32_t> hips = library.GetRig(m_Skeleton).Hips;
		if (m_Switched && inertia.Valid && m_Move.Clip >= 0)
		{
			constexpr float Step = 1.0f / 60.0f;
			Pose before = pose;
			SampleClip(library.GetModels().GetClip(uint32_t(m_Move.Clip)), joints, m_Move.Time - Step * m_Move.Rate, true, before);
			for (uint32_t joint = 0; joint < joints; joint++)
			{
				const glm::vec3 spin = RotationVector(pose[joint].Rotation * glm::inverse(before[joint].Rotation)) / Step;
				inertia.Offset[joint] = RotationVector(inertia.Last[joint].Rotation * glm::inverse(pose[joint].Rotation));
				inertia.Velocity[joint] = inertia.Spin[joint] - spin;
			}
			if (hips)
			{
				inertia.Shift = inertia.Last[*hips].Translation - pose[*hips].Translation;
				inertia.ShiftVelocity = inertia.Drift - (pose[*hips].Translation - before[*hips].Translation) / Step;
			}
			inertia.Active = true;
		}
		m_Switched = false;
		if (inertia.Active)
		{
			const float damping = 2.0f * 0.69314718f / BlendHalfLife;
			const float decay = std::exp(-damping * dt);
			const auto settle = [&](glm::vec3& offset, glm::vec3& velocity)
			{
				const glm::vec3 pull = velocity + offset * damping;
				offset = decay * (offset + pull * dt);
				velocity = decay * (velocity - pull * damping * dt);
				return glm::dot(offset, offset) + glm::dot(velocity, velocity) * 1e-3f;
			};
			float left = 0.0f;
			for (uint32_t joint = 0; joint < joints; joint++)
			{
				left = std::max(left, settle(inertia.Offset[joint], inertia.Velocity[joint]));
				pose[joint].Rotation = glm::normalize(FromRotationVector(inertia.Offset[joint]) * pose[joint].Rotation);
			}
			if (hips)
			{
				left = std::max(left, settle(inertia.Shift, inertia.ShiftVelocity));
				pose[*hips].Translation += inertia.Shift;
			}
			inertia.Active = left > 1e-7f;
		}
		if (inertia.Valid && dt > 1e-4f)
		{
			for (uint32_t joint = 0; joint < joints; joint++)
			{
				glm::vec3 spin = RotationVector(pose[joint].Rotation * glm::inverse(inertia.Last[joint].Rotation)) / dt;
				if (const float amount = glm::length(spin); amount > MaximumSpin)
					spin *= MaximumSpin / amount;
				inertia.Spin[joint] = spin;
			}
			if (hips)
				inertia.Drift = (pose[*hips].Translation - inertia.Last[*hips].Translation) / dt;
		}
		inertia.Last = pose;
		inertia.Valid = true;
	}

	void CharacterAnimator::Twist(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose) const
	{
		if (!m_Free || !rig.Hips || !rig.Spine || !rig.Chest)
			return;
		const float yaw = std::clamp(AngleBetween(m_BodyYaw, input.Facing), -MaximumTwist, MaximumTwist);
		if (std::abs(yaw) < 1e-3f)
			return;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		TurnJoint(skeleton, pose, *rig.Hips, glm::angleAxis(yaw, up));
		const float torso = std::clamp(yaw, -ChestTwist, ChestTwist);
		const float gaze = std::clamp(yaw - torso, -HeadTwist, HeadTwist);
		TurnJoint(skeleton, pose, *rig.Spine, glm::angleAxis(-SpineShare * torso, up));
		TurnJoint(skeleton, pose, *rig.Chest, glm::angleAxis(-(1.0f - SpineShare) * torso, up));
		if (rig.Neck)
			TurnJoint(skeleton, pose, *rig.Neck, glm::angleAxis(-NeckShare * gaze, up));
		TurnJoint(skeleton, pose, rig.Head, glm::angleAxis(-(rig.Neck ? 1.0f - NeckShare : 1.0f) * gaze, up));
	}

	void CharacterAnimator::React(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose) const
	{
		if (!rig.Spine || !rig.Chest)
			return;
		const glm::vec2 recoil = Local(m_Recoil, input.Facing);
		if (const float amount = glm::length(recoil); amount > 1e-3f)
		{
			const glm::vec3 axis = TiltAxis(recoil);
			TurnJoint(skeleton, pose, *rig.Spine, glm::angleAxis(amount * RecoilSpine, axis));
			TurnJoint(skeleton, pose, *rig.Chest, glm::angleAxis(amount * RecoilChest, axis));
		}
		const glm::vec2 whip = Local(m_Whip - m_Recoil, input.Facing);
		if (const float amount = glm::length(whip); amount > 1e-3f)
			TurnJoint(skeleton, pose, rig.Neck.value_or(rig.Head), glm::angleAxis(amount * RecoilNeck, TiltAxis(whip)));
	}

	void CharacterAnimator::HoldStaff(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose) const
	{
		const float weight = (1.0f - m_Action.Weight) * (1.0f - m_Override.Weight);
		if (!rig.HasRightArm || !rig.Chest || input.Dead || weight < 1e-3f)
			return;
		Pose rest;
		RestPose(skeleton, rest);
		const glm::quat chest = GlobalRotation(skeleton, pose, int32_t(*rig.Chest)) * glm::inverse(GlobalRotation(skeleton, rest, int32_t(*rig.Chest)));
		const auto [upper, lower, wrist] = rig.RightArm;
		JointMatrices globals;
		const auto bend = [&](uint32_t joint, const glm::vec3& from, const glm::vec3& goal)
		{
			const glm::quat turn = RotationBetween(glm::normalize(from), glm::normalize(chest * goal));
			TurnJoint(skeleton, pose, joint, glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), turn, weight));
		};
		const auto along = [&](uint32_t from, uint32_t to)
		{
			ComputeGlobals(skeleton, pose, globals);
			return glm::vec3(globals[to][3]) - glm::vec3(globals[from][3]);
		};
		bend(upper, along(upper, lower), StaffUpper);
		bend(lower, along(lower, wrist), StaffFore);
		ComputeGlobals(skeleton, pose, globals);
		bend(wrist, glm::mat3(globals[rig.RightHand]) * glm::vec3(0.0f, 1.0f, 0.0f), StaffShaft);
	}

	void CharacterAnimator::Plant(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, Pose& pose, float dt)
	{
		const glm::vec2 lean = Local(m_Lean, input.Facing);
		const float tilt = glm::length(lean);
		const bool able = rig.HasLegs && input.Ground;
		const float goal = able && input.Grounded && !input.Dead && !input.Stunned ? 1.0f - m_Override.Weight : 0.0f;
		m_Planted += (goal - m_Planted) * Ease(PlantRate, dt);
		if (!able || m_Planted < 1e-3f)
		{
			m_Feet = {};
			m_Pelvis = 0.0f;
			if (rig.Spine && tilt > 1e-4f)
				TurnJoint(skeleton, pose, *rig.Spine, glm::angleAxis(LooseLean * tilt, TiltAxis(lean)));
			return;
		}

		const float scale = std::max(input.Scale, 1e-3f);
		const float size = rig.HipHeight * scale;
		const Vec2 forward = FromAngle(input.Facing);
		const Vec2 right(forward.y, -forward.x);
		JointMatrices globals;
		JointRotations rotations;
		ComputeGlobals(skeleton, pose, globals);
		ComputeRotations(skeleton, pose, rotations);
		const glm::vec3 hips(globals[*rig.Hips][3]);
		const float angle = MoveAngle(input);
		const glm::vec3 travel(std::sin(angle), 0.0f, std::cos(angle));
		const float stretch = (m_Stride - 1.0f) * m_Moving;

		std::array<glm::vec3, 2> targets;
		std::array<glm::quat, 2> soles;
		for (uint32_t side = 0; side < 2; side++)
		{
			const LegJoints& leg = rig.Legs[side];
			Foot& foot = m_Feet[side];
			glm::vec3 ankle(globals[leg.Foot][3]);
			ankle += travel * (glm::dot(ankle - hips, travel) * stretch);
			soles[side] = rotations[leg.Foot];
			const Vec2 point = input.Position + (right * ankle.x + forward * ankle.z) * scale;
			const bool contact = (m_Contacts >> side & 1u) != 0 && m_Planted > 0.5f;
			const float reach = (m_Striding ? LockReach : IdleReach) * size;
			if (foot.Locked)
			{
				foot.Offset = foot.Lock - point;
				foot.Turn = AngleBetween(foot.Yaw, m_BodyYaw);
				const float stretched = Length(foot.Offset);
				const bool waiting = !m_Striding && m_Feet[1 - side].Stepping && stretched < 2.0f * reach;
				if (!contact || ((stretched > reach || std::abs(foot.Turn) > FootTurn) && !waiting))
				{
					foot.Locked = false;
					foot.Reach = stretched;
					foot.Stepping = contact && !m_Striding;
				}
			}
			else
			{
				const float decay = std::exp(-dt * (foot.Stepping ? StepRate : ReleaseRate));
				foot.Offset *= decay;
				foot.Turn *= decay;
				if (contact && Length(foot.Offset) < (m_Striding ? 0.5f * reach : RestReach * size))
				{
					foot.Locked = true;
					foot.Stepping = false;
					foot.Lock = point + foot.Offset;
					foot.Yaw = m_BodyYaw - foot.Turn;
				}
			}

			const Vec2 spot = point + foot.Offset;
			const float level = (*input.Ground)(spot) - input.Height;
			const bool solid = std::abs(level) < GroundReach * size;
			foot.Ground += ((solid ? level : 0.0f) - foot.Ground) * Ease(GroundRate, dt);
			glm::vec2 slope(0.0f);
			if (solid)
			{
				const float probe = SlopeProbe * size;
				const float aside = (*input.Ground)(spot + right * probe) - input.Height - level;
				const float ahead = (*input.Ground)(spot + forward * probe) - input.Height - level;
				if (std::abs(aside) < probe && std::abs(ahead) < probe)
					slope = glm::clamp(glm::vec2(aside, ahead) / probe, -MaximumSlope, MaximumSlope);
			}
			foot.Slope += (slope - foot.Slope) * Ease(SlopeRate, dt);
			const float settled = RestReach * size;
			const float lift = foot.Stepping && foot.Reach > settled + 1e-4f ? StepLift * rig.HipHeight * std::sin(Pi * std::clamp((Length(foot.Offset) - settled) / (foot.Reach - settled), 0.0f, 1.0f)) : 0.0f;
			const Vec2 local = (spot - input.Position) / scale;
			targets[side] = glm::vec3(Dot(local, right), std::max(ankle.y, FloorClamp * rig.AnkleHeight) + foot.Ground / scale + lift, Dot(local, forward));
		}

		const float low = std::clamp(std::min(m_Feet[0].Ground, m_Feet[1].Ground), -PelvisDrop * size, PelvisRise * size);
		m_Pelvis += (low - m_Pelvis) * Ease(PelvisRate, dt);
		const glm::vec2 recoil = Local(m_Recoil, input.Facing);
		glm::vec3 shift(0.0f, m_Pelvis / scale, 0.0f);
		shift += glm::vec3(lean.x, 0.0f, lean.y) * (LeanShift * rig.HipHeight);
		shift += glm::vec3(recoil.x, 0.0f, recoil.y) * (RecoilShove * rig.HipHeight);
		const int32_t parent = skeleton.Parents[*rig.Hips];
		pose[*rig.Hips].Translation += (parent >= 0 ? glm::inverse(glm::mat3(globals[size_t(parent)])) * shift : shift) * m_Planted;
		if (tilt > 1e-4f)
			TurnJoint(skeleton, pose, *rig.Hips, glm::angleAxis(tilt * m_Planted, TiltAxis(lean)));
		ComputeGlobals(skeleton, pose, globals);
		ComputeRotations(skeleton, pose, rotations);

		for (uint32_t side = 0; side < 2; side++)
		{
			const LegJoints& leg = rig.Legs[side];
			const Foot& foot = m_Feet[side];
			const glm::vec3 thigh(globals[leg.Upper][3]);
			const glm::vec3 knee(globals[leg.Lower][3]);
			const glm::vec3 ankle(globals[leg.Foot][3]);
			const LimbSolve solve = SolveLimb(thigh, knee, ankle, glm::mix(ankle, targets[side], m_Planted), rotations[leg.Upper], rotations[leg.Lower], glm::normalize(rotations[leg.Upper] * leg.Hinge));
			pose[leg.Upper].Rotation = glm::normalize(pose[leg.Upper].Rotation * solve.Upper);
			pose[leg.Lower].Rotation = glm::normalize(pose[leg.Lower].Rotation * solve.Lower);
			const glm::quat shin = GlobalRotation(skeleton, pose, int32_t(leg.Lower));
			const glm::quat level = RotationBetween(glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(glm::vec3(-foot.Slope.x, 1.0f, -foot.Slope.y)));
			const glm::quat sole = level * glm::angleAxis(std::clamp(foot.Turn, -2.0f * FootTurn, 2.0f * FootTurn), glm::vec3(0.0f, 1.0f, 0.0f)) * soles[side];
			pose[leg.Foot].Rotation = glm::normalize(glm::slerp(pose[leg.Foot].Rotation, glm::inverse(shin) * sole, m_Planted));
		}
	}

	void CharacterAnimator::Swing(const ModelSkeleton& skeleton, const CharacterRig& rig, const Input& input, const Pose& pose, float dt)
	{
		if (rig.Springs.empty())
			return;
		const glm::mat4 model = ModelTransform({ input.Position.x, input.Height, input.Position.y }, input.Facing, input.Scale);
		const glm::mat3 toModel = glm::inverse(glm::mat3(model));
		const uint32_t steps = std::clamp(uint32_t(std::ceil(dt / SwingStep)), 1u, SwingSteps);
		const float step = std::min(dt, float(SwingSteps) * SwingStep) / float(steps);
		m_SwingTime += dt;
		const glm::vec3 pull(SwingBreeze * (0.5f * std::sin(m_SwingTime * 1.3f) + 0.3f * std::sin(m_SwingTime * 3.1f + 1.7f)), -SwingGravity, SwingBreeze * 0.4f * std::sin(m_SwingTime * 2.3f + 0.6f));

		struct Capsule
		{
			glm::vec3 From;
			glm::vec3 Along;
		};
		std::array<Capsule, MaxBodyCapsules> capsules;
		const uint32_t capsuleCount = std::min(uint32_t(rig.Colliders.size()), MaxBodyCapsules);
		for (uint32_t index = 0; index < capsuleCount; index++)
		{
			const BodyCapsule& body = rig.Colliders[index];
			const glm::vec3 from(model * m_Globals[body.From] * glm::vec4(body.FromLocal, 1.0f));
			capsules[index] = { from, glm::vec3(model * m_Globals[body.To] * glm::vec4(body.ToLocal, 1.0f)) - from };
		}

		std::array<glm::mat4, MaxSpringJoints> stills;
		const uint32_t count = std::min(uint32_t(rig.Springs.size()), MaxSpringJoints);
		for (uint32_t index = 0; index < count; index++)
		{
			const SpringJoint& spring = rig.Springs[index];
			const glm::mat4 local = JointMatrix(pose[spring.Joint]);
			const glm::mat4& carrier = m_Globals[size_t(skeleton.Parents[spring.Joint])];
			stills[index] = (spring.Parent >= 0 ? stills[size_t(spring.Parent)] : carrier) * local;
			const glm::mat4 animated = carrier * local;
			const glm::vec3 head(model * animated[3]);
			const glm::vec3 hang(model * animated * glm::vec4(spring.Tail, 1.0f));
			const glm::vec3 rest(model * stills[index] * glm::vec4(spring.Tail, 1.0f));
			const float length = glm::length(hang - head);
			Bob& bob = m_Bobs[index];
			m_Globals[spring.Joint] = animated;
			if (!m_Swinging || length < 1e-5f || step <= 0.0f)
			{
				bob = { rest, glm::vec3(0.0f), head, rest };
				continue;
			}
			const float radius = spring.Radius * input.Scale;
			const auto hold = [&](const glm::vec3& anchor, const glm::vec3& point)
			{
				const glm::vec3 reach = point - anchor;
				const float distance = glm::length(reach);
				return distance > 1e-6f ? anchor + reach * (length / distance) : anchor + (hang - head);
			};
			glm::vec3 carried = (rest - bob.Rest) / std::max(dt, 1e-4f);
			if (const float pace = glm::length(carried); pace > SwingCarry)
				carried *= SwingCarry / pace;
			const float settle = 1.0f - std::exp(-spring.Damping * step);
			for (uint32_t tick = 0; tick < steps; tick++)
			{
				const float t = float(tick + 1) / float(steps);
				const glm::vec3 anchor = glm::mix(bob.Head, head, t);
				bob.Velocity += ((glm::mix(bob.Rest, rest, t) - bob.Tail) * spring.Stiffness + pull * spring.Gravity - bob.Velocity * (spring.Drag * glm::length(bob.Velocity))) * step;
				bob.Velocity -= (bob.Velocity - carried) * settle;
				glm::vec3 next = hold(anchor, bob.Tail + bob.Velocity * step);
				bob.Velocity = (next - bob.Tail) / step;
				for (uint32_t body = 0; body < capsuleCount; body++)
				{
					const Capsule& capsule = capsules[body];
					const float span = glm::dot(capsule.Along, capsule.Along);
					const float along = span > 1e-8f ? std::clamp(glm::dot(next - capsule.From, capsule.Along) / span, 0.0f, 1.0f) : 0.0f;
					const glm::vec3 closest = capsule.From + capsule.Along * along;
					const glm::vec3 away = next - closest;
					const float distance = glm::length(away);
					const float clear = spring.Clear[body] * input.Scale + radius;
					if (distance >= clear)
						continue;
					glm::vec3 side = rest - closest;
					if (span > 1e-8f)
						side -= capsule.Along * (glm::dot(side, capsule.Along) / span);
					const float sideLength = glm::length(side);
					if (!rig.Colliders[body].Bulky && sideLength > 1e-6f && (distance < 1e-6f || glm::dot(side, away) < 0.0f))
					{
						const glm::vec3 normal = side / sideLength;
						next = closest + normal * clear;
						bob.Velocity -= normal * std::min(glm::dot(bob.Velocity, normal), 0.0f);
						continue;
					}
					if (distance < 1e-6f)
						continue;
					const glm::vec3 normal = away / distance;
					next += normal * (clear - distance);
					bob.Velocity -= normal * std::min(glm::dot(bob.Velocity, normal), 0.0f);
				}
				if (next.y < input.Height + radius)
				{
					next.y = input.Height + radius;
					bob.Velocity.y = std::max(bob.Velocity.y, 0.0f);
				}
				bob.Tail = hold(anchor, next);
			}
			bob.Head = head;
			bob.Rest = rest;
			const glm::vec3 from = glm::normalize(toModel * (hang - head));
			const glm::vec3 to = glm::normalize(toModel * (bob.Tail - head));
			const glm::vec3 pivot(animated[3]);
			m_Globals[spring.Joint] = glm::translate(glm::mat4(1.0f), pivot) * glm::mat4_cast(RotationBetween(from, to)) * glm::translate(glm::mat4(1.0f), -pivot) * animated;
		}
		m_Swinging = true;
	}

	void CharacterAnimator::Compose(const CharacterLibrary& library, const Input& input, float dt)
	{
		const ModelLibrary& models = library.GetModels();
		const ModelSkeleton& skeleton = models.GetSkeleton(m_Skeleton);
		const CharacterRig& rig = library.GetRig(m_Skeleton);
		const uint32_t joints = std::min(skeleton.JointCount(), MaxModelJoints);
		Pose pose;
		RestPose(skeleton, pose);
		const glm::vec3 chestForward = rig.Chest ? glm::inverse(GlobalRotation(skeleton, pose, *rig.Chest)) * glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
		if (m_Move.Clip >= 0)
			SampleClip(models.GetClip(uint32_t(m_Move.Clip)), joints, m_Move.Time, true, pose);
		Inertialize(library, pose, joints, dt);
		if (m_Fidget.Weight > 0.0f && m_Fidget.Current.Clip >= 0)
		{
			Pose fidget = pose;
			Sample(library, m_Fidget, fidget);
			BlendPose(pose, fidget, m_Fidget.Weight, joints);
		}
		if (m_Action.Weight > 0.0f && m_Action.Current.Clip >= 0)
		{
			Pose action = pose;
			Sample(library, m_Action, action);
			JointWeights mask{};
			for (uint32_t joint = 0; joint < joints; joint++)
				mask[joint] = rig.UpperBody[joint] + (1.0f - rig.UpperBody[joint]) * (1.0f - m_Moving);
			float drift = 0.0f;
			if (rig.Spine && rig.Chest && m_Moving > 0.0f)
			{
				Pose layered = pose;
				BlendPose(layered, action, 1.0f, joints, &mask);
				drift = std::remainder(YawOf(skeleton, action, *rig.Chest, chestForward) - YawOf(skeleton, layered, *rig.Chest, chestForward), 2.0f * Pi);
			}
			BlendPose(pose, action, m_Action.Weight, joints, &mask);
			if (drift != 0.0f)
				TurnJoint(skeleton, pose, *rig.Spine, glm::angleAxis(drift * m_Action.Weight, glm::vec3(0.0f, 1.0f, 0.0f)));
		}
		if (m_Override.Weight > 0.0f && m_Override.Current.Clip >= 0)
		{
			Pose over = pose;
			Sample(library, m_Override, over);
			BlendPose(pose, over, m_Override.Weight, joints);
		}
		Twist(skeleton, rig, input, pose);
		if (input.Pose == Stance::Staff)
			HoldStaff(skeleton, rig, input, pose);
		if (m_Flinch.Weight > 0.0f && m_Flinch.Current.Clip >= 0)
		{
			const ModelClip& clip = models.GetClip(uint32_t(m_Flinch.Current.Clip));
			Pose reference = pose;
			Pose flinch = pose;
			SampleClip(clip, joints, 0.0f, false, reference);
			SampleClip(clip, joints, m_Flinch.Current.Time, false, flinch);
			AddPose(pose, reference, flinch, m_Flinch.Weight, joints, &rig.UpperBody);
		}
		React(skeleton, rig, input, pose);
		Plant(skeleton, rig, input, pose, dt);
		ComputeGlobals(skeleton, pose, m_Globals);
		Swing(skeleton, rig, input, pose, dt);
		m_Posed = true;
	}

	float CharacterAnimator::StrikeWeight(const CharacterLibrary& library) const
	{
		if (m_Phase == Phase::None || m_Action.Current.Clip < 0 || m_Action.Weight <= 0.0f)
			return 0.0f;
		const float strike = library.GetModels().GetStrikeTime(uint32_t(m_Action.Current.Clip));
		if (strike < 0.05f)
			return 0.0f;
		const float t = m_Action.Current.Time - strike;
		return m_Action.Weight * glm::smoothstep(-StrikeLead, -0.4f * StrikeLead, t) * (1.0f - glm::smoothstep(0.3f * StrikeTail, StrikeTail, t));
	}

	void CharacterAnimator::Evaluate(const CharacterLibrary& library, uint32_t skeletonIndex, JointMatrices& globals) const
	{
		if (skeletonIndex == m_Skeleton && m_Posed)
		{
			globals = m_Globals;
			return;
		}
		const ModelSkeleton& skeleton = library.GetModels().GetSkeleton(skeletonIndex);
		Pose pose;
		RestPose(skeleton, pose);
		ComputeGlobals(skeleton, pose, globals);
	}
}
