#pragma once
#include "GameShared/Core/Math.h"
#include "GameShared/Entity/EntityRegistry.h"
#include <array>
#include <cstdint>

namespace Game
{
	constexpr uint32_t NoPlayer = UINT32_MAX;
	constexpr uint32_t MaxPlayers = 12;
	constexpr uint8_t NoTeam = 255;
	constexpr uint32_t HistoryLength = 32;
	constexpr float MaxRewindTicks = 28.0f;
	constexpr uint32_t SkillSlots = 3;
	constexpr uint32_t AbilitySlots = SkillSlots + 1;

	enum class EntityKind : uint8_t
	{
		Player,
		Chaser,
		Shooter,
		Brute,
		Caster,
		Archivist,
		Wyrm,
		Projectile,
		Effect,
		Loot,
		Chest,
		Gateway,
		Count,
	};

	enum class Team : uint8_t
	{
		Players,
		Enemies,
	};

	enum class Element : uint8_t
	{
		Physical,
		Fire,
		Cold,
		Lightning,
		Poison,
		Count,
	};

	enum class LootKind : uint8_t
	{
		Health,
		Might,
		Haste,
		Swiftness,
		Vitality,
		Mana,
		Item,
		Count,
	};

	constexpr size_t BuffKinds = size_t(LootKind::Mana);
	using BuffStacks = std::array<uint8_t, BuffKinds>;

	struct Transform
	{
		Vec2 Position;
		float Facing = 0.0f;
	};

	struct Collider
	{
		float Radius = 0.35f;
	};

	struct Replicated
	{
		uint32_t NetId = 0;
		EntityKind Kind = EntityKind::Player;
		uint32_t Owner = NoPlayer;
		uint16_t Tag = 0;
		uint8_t Variant = 0;
		uint8_t Scale = 16;
		uint32_t Visibility = NoPlayer;
	};

	struct Health
	{
		int32_t Current = 1;
		int32_t Max = 1;
		Team Side = Team::Players;

		bool IsDead() const { return Current <= 0; }
	};

	struct PlayerState
	{
		Vec2 DashDirection;
		float DashDistance = 0.0f;
		float DashSpeed = 0.0f;
		Vec2 Push;
		float Ward = 0.0f;
		float Invulnerable = 0.0f;
		float PortalCooldown = 0.0f;
		std::array<float, AbilitySlots> Cooldowns{};
		float DodgeCooldown = 0.0f;
		float Mana = 0.0f;
		float CastTimer = 0.0f;
		float Stun = 0.0f;
		float Chill = 0.0f;
		float StunGuard = 0.0f;
		int8_t CastSlot = -1;
		uint8_t HeldButtons = 0;
		bool Alive = true;

		bool IsCasting() const { return CastSlot >= 0; }
		bool IsStunned() const { return Stun > 0.0f; }
	};

	struct Enemy
	{
		EntityKind Type = EntityKind::Chaser;
		Entity Target;
		float RetargetTimer = 0.0f;
		float AttackCooldown = 0.0f;
		float Windup = 0.0f;
		Vec2 AimDirection;
		Vec2 AimPoint;
		float DamageScale = 1.0f;
		float SpeedScale = 1.0f;
		float CooldownScale = 1.0f;
		float PortalCooldown = 0.0f;
		float Chill = 0.0f;
		float Stun = 0.0f;
		Vec2 Anchor;
		float Stuck = 0.0f;
		bool Elite = false;
		bool Awake = true;
		uint16_t Pack = 0;
		Vec2 Home;
		float Wander = 0.0f;
		uint32_t Level = 1;
		uint8_t ExtraProjectiles = 0;
	};

	struct Projectile
	{
		Vec2 Velocity;
		float Lifetime = 0.0f;
		float Radius = 0.1f;
		int32_t Damage = 0;
		Team Side = Team::Players;
		uint8_t Pierce = 0;
		uint8_t Chain = 0;
		uint8_t HitCount = 0;
		std::array<Entity, 8> Hits{};
		float Rewind = 0.0f;
		float CritChance = 0.0f;
		float CritMultiplier = 1.5f;
		float LifeSteal = 0.0f;
		float ImpactRadius = 0.0f;
		float Chill = 0.0f;
		float Stun = 0.0f;
		float PvpScale = 1.0f;
		float Push = 0.0f;
		bool Swap = false;
		Element Damages = Element::Physical;
		uint8_t Ability = 0;
		Entity Source;
	};

	struct Effect
	{
		float Remaining = 0.0f;
	};

	enum class StrikeShape : uint8_t
	{
		Circle,
		Line,
		Ring,
		Cone,
	};

	struct PendingStrike
	{
		float Remaining = 0.0f;
		float Radius = 0.0f;
		StrikeShape Shape = StrikeShape::Circle;
		float Length = 0.0f;
		float Width = 0.0f;
		float Inner = 0.0f;
		float HalfArc = 0.0f;
		int32_t Damage = 0;
		Team Side = Team::Players;
		uint32_t Owner = NoPlayer;
		Entity Source;
		Element Damages = Element::Physical;
		float CritChance = 0.0f;
		float CritMultiplier = 1.5f;
		float LifeSteal = 0.0f;
		float Chill = 0.0f;
		float Stun = 0.0f;
		float PvpScale = 1.0f;
		float Push = 0.0f;
		uint8_t Ability = 0;
	};

	struct Boss
	{
		uint8_t Phase = 0;
		int8_t Move = -1;
		uint8_t Stage = 0;
		float StageTimer = 0.0f;
		uint8_t Step = 0;
		float StepTimer = 0.0f;
		float Idle = 2.0f;
		std::array<float, 8> Cooldowns{};
		Vec2 Aim;
		Vec2 Goal;
		bool Burrowed = false;
		Vec2 Arena;
		float ArenaRadius = 10.0f;
		uint32_t Level = 1;
		float Leash = 0.0f;
	};

	enum class ChestKind : uint8_t
	{
		Boss,
		Common,
		Vault,
		Cache,
		Count,
	};

	struct Chest
	{
		uint16_t Opened = 0;
		uint32_t Level = 1;
		float RarityBonus = 0.0f;
		float Quantity = 1.0f;
		uint8_t Items = 2;
		ChestKind Kind = ChestKind::Boss;
	};

	struct Gateway
	{
		float Radius = 1.8f;
	};

	struct MotionHistory
	{
		std::array<Vec2, HistoryLength> Positions{};
		uint32_t FirstTick = 0;
	};
}
