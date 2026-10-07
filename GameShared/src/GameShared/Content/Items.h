#pragma once
#include "GameShared/Content/Definitions.h"
#include "GameShared/Content/Progression.h"
#include "GameShared/Content/Stats.h"
#include "GameShared/Core/Random.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace Game
{
	enum class Rarity : uint8_t
	{
		Common,
		Uncommon,
		Rare,
		Legendary,
		Mythic,
		Count,
	};

	struct RarityDef
	{
		Rarity Id = Rarity::Common;
		const char* Name = "";
		float Weight = 0.0f;
		uint8_t Slots = 1;
	};

	inline constexpr RarityDef Rarities[] = {
		{ Rarity::Common, "Common", 0.0f, 1 },
		{ Rarity::Uncommon, "Uncommon", 58.0f, 2 },
		{ Rarity::Rare, "Rare", 30.0f, 3 },
		{ Rarity::Legendary, "Legendary", 9.5f, 4 },
		{ Rarity::Mythic, "Mythic", 2.5f, 5 },
	};
	static_assert(std::size(Rarities) == size_t(Rarity::Count));

	enum class ItemBase : uint8_t
	{
		Wand,
		Staff,
		Sword,
		Axe,
		Dagger,
		Bow,
		Crossbow,
		WizardHat,
		Hood,
		Helm,
		Robe,
		Tunic,
		Plate,
		Sandals,
		Boots,
		Greaves,
		Amulet,
		Ring,
		Gem,
		Count,
	};

	struct Affix
	{
		Stat Kind = Stat::Damage;
		float Value = 0.0f;
		uint8_t Tier = 1;
	};

	struct ItemBaseDef
	{
		ItemBase Id = ItemBase::Wand;
		const char* Name = "";
		EquipSlot Slot = EquipSlot::Weapon;
		AbilityId Attack = AbilityId::Count;
		uint8_t Affinity = 0;
		std::array<Affix, 2> Implicits{};
		uint8_t ImplicitCount = 0;
		float DropWeight = 1.0f;
	};

	inline constexpr ItemBaseDef ItemBases[] = {
		{ ItemBase::Wand, "Wand", EquipSlot::Weapon, AbilityId::Bolt, Affinity::Caster, { { { Stat::MaxMana, 10.0f } } }, 1 },
		{ ItemBase::Staff, "Staff", EquipSlot::Weapon, AbilityId::Orb, Affinity::Caster, { { { Stat::MaxMana, 25.0f } } }, 1 },
		{ ItemBase::Sword, "Sword", EquipSlot::Weapon, AbilityId::Slash, Affinity::Melee },
		{ ItemBase::Axe, "Axe", EquipSlot::Weapon, AbilityId::Chop, Affinity::Melee, { { { Stat::PhysicalDamage, 0.1f } } }, 1 },
		{ ItemBase::Dagger, "Dagger", EquipSlot::Weapon, AbilityId::Stab, Affinity::Melee | Affinity::Caster, { { { Stat::CritChance, 0.04f } } }, 1 },
		{ ItemBase::Bow, "Bow", EquipSlot::Weapon, AbilityId::Arrow, Affinity::Bow },
		{ ItemBase::Crossbow, "Crossbow", EquipSlot::Weapon, AbilityId::Quarrel, Affinity::Bow },
		{ ItemBase::WizardHat, "Wizard Hat", EquipSlot::Head, AbilityId::Count, 0, { { { Stat::MaxMana, 25.0f }, { Stat::CastSpeed, 0.04f } } }, 2 },
		{ ItemBase::Hood, "Hood", EquipSlot::Head, AbilityId::Count, 0, { { { Stat::MaxHealth, 10.0f }, { Stat::ManaRegen, 0.5f } } }, 2 },
		{ ItemBase::Helm, "Helm", EquipSlot::Head, AbilityId::Count, 0, { { { Stat::Armor, 14.0f }, { Stat::MaxHealth, 10.0f } } }, 2 },
		{ ItemBase::Robe, "Robe", EquipSlot::Body, AbilityId::Count, 0, { { { Stat::MaxMana, 30.0f }, { Stat::ManaRegen, 1.0f } } }, 2 },
		{ ItemBase::Tunic, "Tunic", EquipSlot::Body, AbilityId::Count, 0, { { { Stat::MaxHealth, 20.0f }, { Stat::Armor, 8.0f } } }, 2 },
		{ ItemBase::Plate, "Plate Armor", EquipSlot::Body, AbilityId::Count, 0, { { { Stat::Armor, 30.0f }, { Stat::MaxHealth, 20.0f } } }, 2 },
		{ ItemBase::Sandals, "Sandals", EquipSlot::Feet, AbilityId::Count, 0, { { { Stat::MoveSpeed, 0.04f }, { Stat::ManaRegen, 0.4f } } }, 2 },
		{ ItemBase::Boots, "Boots", EquipSlot::Feet, AbilityId::Count, 0, { { { Stat::MoveSpeed, 0.06f } } }, 1 },
		{ ItemBase::Greaves, "Greaves", EquipSlot::Feet, AbilityId::Count, 0, { { { Stat::Armor, 12.0f }, { Stat::MoveSpeed, 0.02f } } }, 2 },
		{ ItemBase::Amulet, "Amulet", EquipSlot::Amulet, AbilityId::Count, 0, { { { Stat::MaxHealth, 8.0f }, { Stat::MaxMana, 8.0f } } }, 2, 1.5f },
		{ ItemBase::Ring, "Ring", EquipSlot::Ring, AbilityId::Count, 0, {}, 0, 2.0f },
		{ ItemBase::Gem, "Skill Gem", EquipSlot::Gem, AbilityId::Count, 0, {}, 0, 0.0f },
	};
	static_assert(std::size(ItemBases) == size_t(ItemBase::Count));

	enum class GemMod : uint8_t
	{
		None,
		Ruthless,
		Frugal,
		Quickened,
		Restless,
		Vast,
		Split,
		Count,
	};

	struct GemModDef
	{
		GemMod Id = GemMod::None;
		const char* Name = "";
		const char* Description = "";
	};

	inline constexpr GemModDef GemMods[] = {
		{ GemMod::None, "", "" },
		{ GemMod::Ruthless, "Ruthless", "+25% damage" },
		{ GemMod::Frugal, "Frugal", "25% less mana cost" },
		{ GemMod::Quickened, "Quickened", "20% faster casting" },
		{ GemMod::Restless, "Restless", "25% shorter cooldown" },
		{ GemMod::Vast, "Vast", "+25% area" },
		{ GemMod::Split, "Split", "+1 projectile" },
	};
	static_assert(std::size(GemMods) == size_t(GemMod::Count));

	enum class GearSlot : uint8_t
	{
		Weapon,
		Head,
		Body,
		Feet,
		Amulet,
		Ring1,
		Ring2,
		Count,
	};

	constexpr size_t GearSlotCount = size_t(GearSlot::Count);
	constexpr size_t MaxAffixes = 5;
	constexpr size_t BackpackSize = 16;
	constexpr size_t StashSize = 48;

	struct Item
	{
		uint32_t Id = 0;
		ItemBase Base = ItemBase::Wand;
		Rarity Quality = Rarity::Common;
		uint8_t Level = 1;
		float Power = 1.0f;
		AbilityId Skill = AbilityId::Count;
		uint8_t AffixCount = 0;
		std::array<Affix, MaxAffixes> Affixes{};
		uint8_t GemQuality = 0;
		GemMod Mod = GemMod::None;
		bool Corrupted = false;
		uint8_t Pity = 0;

		bool IsValid() const { return Id != 0; }
		bool IsGem() const { return IsValid() && Base == ItemBase::Gem; }
	};

	using Backpack = std::array<Item, BackpackSize>;
	using Equipment = std::array<Item, GearSlotCount>;
	using GemSlots = std::array<Item, SkillSlots>;
	using StashTab = std::array<Item, StashSize>;

	struct GearSet
	{
		Equipment Gear{};
		GemSlots Gems{};
		Backpack Pack{};
		StashTab Stash{};
		MaterialStock Materials{};
	};

	enum class ItemLocation : uint8_t
	{
		Pack,
		Gear,
		Gem,
		Stash,
		Count,
	};

	struct ItemRef
	{
		ItemLocation Location = ItemLocation::Pack;
		uint16_t Index = 0;
	};

	enum class StarterKit : uint8_t
	{
		Arcanist,
		Knight,
		Ranger,
		Count,
	};

	struct KitDef
	{
		StarterKit Id = StarterKit::Arcanist;
		const char* Name = "";
		const char* Description = "";
		ItemBase Weapon = ItemBase::Wand;
		std::array<AbilityId, 2> Gems{};
		std::array<ItemBase, 3> Armor{};
	};

	inline constexpr KitDef Kits[] = {
		{ StarterKit::Arcanist, "Arcanist", "Wand, wizard hat, robe and sandals, with Fireball and Frost Nova gems.", ItemBase::Wand, { AbilityId::Fireball, AbilityId::FrostNova }, { ItemBase::WizardHat, ItemBase::Robe, ItemBase::Sandals } },
		{ StarterKit::Knight, "Knight", "Sword, helm, plate armor and greaves, with Ground Slam and Cyclone gems.", ItemBase::Sword, { AbilityId::GroundSlam, AbilityId::Cyclone }, { ItemBase::Helm, ItemBase::Plate, ItemBase::Greaves } },
		{ StarterKit::Ranger, "Ranger", "Bow, hood, tunic and boots, with Volley and Rain of Arrows gems.", ItemBase::Bow, { AbilityId::Volley, AbilityId::RainOfArrows }, { ItemBase::Hood, ItemBase::Tunic, ItemBase::Boots } },
	};
	static_assert(std::size(Kits) == size_t(StarterKit::Count));

	inline const RarityDef& GetRarity(Rarity rarity)
	{
		return Rarities[size_t(rarity) < std::size(Rarities) ? size_t(rarity) : 0];
	}

	inline const ItemBaseDef& GetItemBase(ItemBase base)
	{
		return ItemBases[size_t(base) < std::size(ItemBases) ? size_t(base) : 0];
	}

	inline const KitDef& GetKit(StarterKit kit)
	{
		return Kits[size_t(kit) < std::size(Kits) ? size_t(kit) : 0];
	}

	inline const GemModDef& GetGemMod(GemMod mod)
	{
		return GemMods[size_t(mod) < std::size(GemMods) ? size_t(mod) : 0];
	}

	inline EquipSlot GetEquipSlot(ItemBase base)
	{
		return GetItemBase(base).Slot;
	}

	inline bool IsWeapon(ItemBase base)
	{
		return GetEquipSlot(base) == EquipSlot::Weapon;
	}

	inline bool IsRangedWeapon(ItemBase base)
	{
		const ItemBaseDef& def = GetItemBase(base);
		return def.Slot == EquipSlot::Weapon && GetAbility(def.Attack).Kind == AbilityKind::Projectile;
	}

	inline EquipSlot GetGearSlotKind(GearSlot slot)
	{
		constexpr EquipSlot Kinds[] = { EquipSlot::Weapon, EquipSlot::Head, EquipSlot::Body, EquipSlot::Feet, EquipSlot::Amulet, EquipSlot::Ring, EquipSlot::Ring };
		return Kinds[size_t(slot) < std::size(Kinds) ? size_t(slot) : 0];
	}

	inline const char* GetGearSlotName(GearSlot slot)
	{
		constexpr const char* Names[] = { "Weapon", "Head", "Body", "Feet", "Amulet", "Ring", "Ring" };
		return Names[size_t(slot) < std::size(Names) ? size_t(slot) : 0];
	}

	inline uint8_t GetAffixSlots(Rarity rarity)
	{
		return GetRarity(rarity).Slots;
	}

	inline float GetItemRoll(const Item& item)
	{
		return item.Power / LevelScale(item.Level);
	}

	inline uint16_t EncodeItemTag(const Item& item)
	{
		const uint32_t detail = item.IsGem() ? uint32_t(item.Skill) : uint32_t(item.Quality);
		return uint16_t((detail << 8) | uint32_t(item.Base));
	}

	inline ItemBase ItemTagBase(uint16_t tag)
	{
		return ItemBase(std::min<uint32_t>(tag & 0xFF, uint32_t(ItemBase::Count) - 1));
	}

	inline Rarity ItemTagRarity(uint16_t tag)
	{
		return ItemTagBase(tag) == ItemBase::Gem ? Rarity::Rare : Rarity(std::min<uint32_t>(tag >> 8, uint32_t(Rarity::Count) - 1));
	}

	inline AbilityId ItemTagSkill(uint16_t tag)
	{
		return ItemTagBase(tag) == ItemBase::Gem ? AbilityId(std::min<uint32_t>(tag >> 8, uint32_t(AbilityId::Count) - 1)) : AbilityId::Count;
	}

	Item MakeItem(ItemBase base, uint32_t id, uint32_t level = 1);
	Item MakeGem(AbilityId skill, uint32_t id, uint32_t level = 1);
	Item GenerateItem(Random& random, uint32_t id, uint32_t level, float rarityBonus, Rarity minimum = Rarity::Uncommon, std::optional<ItemBase> base = {});
	Item GenerateGem(Random& random, uint32_t id, uint32_t level);
	GearSet MakeStarterGear(StarterKit kit);
	std::optional<Item> SanitizeItem(const Item& item);
	GearSet SanitizeGear(const GearSet& gear);
	bool IsAffixAllowed(Stat stat, ItemBase base, Rarity rarity);
	bool HasAffix(const Item& item, Stat stat);
	uint8_t RollAffixTier(Random& random, uint8_t maxTier);
	float RollAffixValue(Random& random, Stat stat, uint8_t tier);
	float GetImplicitValue(const Item& item, uint32_t index);
	bool CanEquip(const Item& item, GearSlot slot);
	std::optional<GearSlot> PreferredGearSlot(const Item& item, const Equipment& gear);
	std::string GetItemName(const Item& item);
	std::string DescribeAffix(const Affix& affix);
}
