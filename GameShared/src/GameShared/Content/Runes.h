#pragma once
#include "GameShared/Content/Brawl.h"
#include <array>
#include <cstdint>
#include <iterator>

namespace Game
{
	inline constexpr uint8_t MaxSockets = 4;
	inline constexpr uint8_t BaseSockets = 2;
	inline constexpr uint8_t MaxCastDepth = 2;
	inline constexpr uint32_t RunProjectileCap = 200;

	enum class RuneId : uint8_t
	{
		Multicast,
		Pierce,
		Bounce,
		Enlarge,
		Linger,
		Quickcast,
		Split,
		Homing,
		Encore,
		Ricochet,
		Link,
		Echo,
		Combustion,
		Count,
		None = 0xFF,
	};

	enum class RuneKind : uint8_t
	{
		Modifier,
		Trigger,
	};

	enum SpellForm : uint8_t
	{
		FormBolt = 1 << 0,
		FormChain = 1 << 1,
		FormBlade = 1 << 2,
		FormStrike = 1 << 3,
		FormNova = 1 << 4,
		FormBlast = 1 << 5,
		FormUtility = 1 << 6,
	};

	using RuneMask = uint32_t;
	using RuneSockets = std::array<RuneId, MaxSockets>;

	struct RuneDef
	{
		RuneId Id;
		RuneKind Kind;
		CardRarity Rarity;
		uint8_t Fits;
		const char* Name;
		const char* Text;
		const char* Needs;
	};

	inline constexpr uint8_t FormShots = FormBolt | FormChain;
	inline constexpr uint8_t FormAttacks = FormBolt | FormChain | FormBlade | FormStrike | FormNova;

	inline constexpr RuneDef Runes[] = {
		{ RuneId::Multicast, RuneKind::Modifier, CardRarity::Common, FormShots, "Multicast", "Fires 1 more projectile in a fan. Obsidian Blades throws 2 more. Each one deals 20% less.", "Projectile spells" },
		{ RuneId::Pierce, RuneKind::Modifier, CardRarity::Common, FormBolt, "Pierce", "Projectiles pass through 2 more enemies.", "Bolt spells" },
		{ RuneId::Bounce, RuneKind::Modifier, CardRarity::Common, FormShots, "Bounce", "Projectiles bounce off walls and rocks twice, and can hit the same enemy again after each bounce.", "Projectile spells" },
		{ RuneId::Enlarge, RuneKind::Modifier, CardRarity::Common, FormStrike | FormNova | FormBlast | FormBlade, "Enlarge", "The spell's area is 40% bigger.", "Area spells" },
		{ RuneId::Linger, RuneKind::Modifier, CardRarity::Common, FormStrike | FormBlast, "Linger", "The impact leaves a zone for 2 seconds. It hits everything inside for 15% of the spell's damage every half second.", "Area spells" },
		{ RuneId::Quickcast, RuneKind::Modifier, CardRarity::Common, FormAttacks | FormUtility, "Quickcast", "The spell's windup and charge time are 40% shorter.", "Spells with a windup" },
		{ RuneId::Split, RuneKind::Modifier, CardRarity::Rare, FormBolt, "Split", "On impact, each projectile bursts into 3 shards that fly on and deal 40% of its damage.", "Bolt spells" },
		{ RuneId::Homing, RuneKind::Modifier, CardRarity::Rare, FormShots, "Homing", "Projectiles curve toward the nearest enemy ahead of them.", "Projectile spells" },
		{ RuneId::Encore, RuneKind::Modifier, CardRarity::Rare, FormStrike, "Encore", "The spell lands again on the same spot 1 second later, at half strength and 25% wider.", "Ground spells" },
		{ RuneId::Ricochet, RuneKind::Modifier, CardRarity::Rare, FormShots, "Ricochet", "After a hit, the projectile jumps to 1 more enemy nearby.", "Projectile spells" },
		{ RuneId::Link, RuneKind::Trigger, CardRarity::Rare, FormAttacks, "Link", "When this spell hits, the next spell on your bar is cast from the hit point at 40% strength. Recharges in 2 seconds.", "Attack spells" },
		{ RuneId::Echo, RuneKind::Trigger, CardRarity::Rare, FormBolt | FormChain | FormStrike, "Echo", "Every 3rd cast of this spell is cast twice.", "Projectile and ground spells" },
		{ RuneId::Combustion, RuneKind::Trigger, CardRarity::Rare, FormAttacks, "Combustion", "Enemies killed by this spell explode, hitting everything nearby for a quarter of their health. Enemies killed by the explosion explode too.", "Attack spells" },
	};
	static_assert(std::size(Runes) == size_t(RuneId::Count));

	inline constexpr float MulticastCost = 0.2f;
	inline constexpr uint8_t RunePierce = 2;
	inline constexpr uint8_t RuneBounces = 2;
	inline constexpr float EnlargeArea = 1.4f;
	inline constexpr float QuickcastScale = 0.6f;
	inline constexpr uint32_t LingerPulses = 4;
	inline constexpr float LingerInterval = 0.5f;
	inline constexpr float LingerShare = 0.15f;
	inline constexpr float LingerRadius = 0.85f;
	inline constexpr float RuneSplitShare = 0.4f;
	inline constexpr float RuneSplitSpread = 0.5f;
	inline constexpr float RuneSplitRange = 3.5f;
	inline constexpr float EncoreDelay = 1.0f;
	inline constexpr float EncoreShare = 0.5f;
	inline constexpr float EncoreArea = 1.25f;
	inline constexpr float LinkShare = 0.4f;
	inline constexpr float LinkCooldown = 2.0f;
	inline constexpr float LinkReach = 7.0f;
	inline constexpr uint8_t EchoEvery = 3;
	inline constexpr float EchoRuneDelay = 0.3f;
	inline constexpr float CombustionShare = 0.25f;
	inline constexpr int32_t CombustionFloor = 8;
	inline constexpr float CombustionRadius = 2.4f;
	inline constexpr float CombustionDelay = 0.14f;
	inline constexpr uint16_t RunePrices[] = { 45, 70, 95 };

	inline constexpr const RuneDef& GetRune(RuneId id)
	{
		return Runes[size_t(id) < std::size(Runes) ? size_t(id) : 0];
	}

	inline constexpr RuneMask RuneBit(RuneId id)
	{
		return id < RuneId::Count ? RuneMask(1) << uint32_t(id) : 0;
	}

	inline constexpr bool HasRune(RuneMask runes, RuneId id)
	{
		return (runes & RuneBit(id)) != 0;
	}

	inline constexpr RuneMask TriggerRunes = RuneBit(RuneId::Link) | RuneBit(RuneId::Echo) | RuneBit(RuneId::Combustion);

	inline constexpr RuneSockets EmptySockets = { RuneId::None, RuneId::None, RuneId::None, RuneId::None };

	inline constexpr std::array<RuneSockets, AbilitySlots> EmptyRuneBoard = { EmptySockets, EmptySockets, EmptySockets, EmptySockets, EmptySockets };

	inline uint8_t SpellForms(AbilityId id)
	{
		if (id >= AbilityId::Count)
			return 0;
		const AbilityDef& def = GetAbility(id);
		switch (def.Kind)
		{
		case AbilityKind::Projectile:
			return uint8_t((def.Chain > 0 ? FormChain : FormBolt) | (def.ImpactRadius > 0.0f ? FormBlast : 0));
		case AbilityKind::Ground:
			return FormStrike;
		case AbilityKind::Melee:
			return id == AbilityId::CrystalSword ? FormBlade : FormNova;
		case AbilityKind::Nova:
			return FormNova;
		default:
			return FormUtility;
		}
	}

	inline bool RuneFits(RuneId rune, AbilityId spell)
	{
		if (rune >= RuneId::Count || spell >= AbilityId::Count)
			return false;
		if ((GetRune(rune).Fits & SpellForms(spell)) == 0)
			return false;
		if (rune == RuneId::Quickcast)
			return GetAbility(spell).CastTime > 0.0f || IsChargedShot(spell);
		return true;
	}

	inline uint8_t SocketsAt(uint8_t rank)
	{
		return uint8_t(std::min<uint32_t>(BaseSockets + (rank >= 2 ? 1u : 0u) + (rank >= 4 ? 1u : 0u), MaxSockets));
	}

	inline RuneMask SocketMask(const RuneSockets& sockets)
	{
		RuneMask mask = 0;
		for (const RuneId rune : sockets)
			mask |= RuneBit(rune);
		return mask;
	}

	inline bool IsLinkTarget(AbilityId spell)
	{
		return spell < AbilityId::Count && (SpellForms(spell) & (FormBolt | FormChain | FormStrike | FormNova)) != 0;
	}
}
