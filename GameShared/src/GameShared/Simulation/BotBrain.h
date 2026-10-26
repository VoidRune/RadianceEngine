#pragma once
#include "GameShared/Content/Brawl.h"
#include "GameShared/Core/Random.h"
#include "GameShared/Entity/EntityRegistry.h"
#include "GameShared/Simulation/PlayerInput.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Game
{
	class World;

	enum class BotSkill : uint8_t
	{
		Easy,
		Normal,
		Hard,
		Count,
	};

	enum class BotStyle : uint8_t
	{
		Balanced,
		Bruiser,
		Sniper,
		Trickster,
		Count,
	};

	inline constexpr const char* BotSkillNames[] = { "easy", "normal", "hard" };
	inline constexpr const char* BotStyleNames[] = { "balanced", "bruiser", "sniper", "trickster" };
	static_assert(std::size(BotSkillNames) == size_t(BotSkill::Count) && std::size(BotStyleNames) == size_t(BotStyle::Count));

	struct BotProfile
	{
		BotSkill Skill = BotSkill::Normal;
		BotStyle Style = BotStyle::Balanced;
		uint32_t Seed = 1;
		bool RandomSpells = false;
		bool Patient = true;
	};

	struct BotTally
	{
		std::array<uint32_t, size_t(AbilityId::Count)> Casts{};
		uint32_t Dodges = 0;
		uint32_t Combos = 0;
	};

	class BotArena
	{
	public:
		void Update(World& world);
		bool IsReady() const { return !m_Collapse.empty(); }
		float Clearance(Vec2 position) const;
		float CollapseIn(Vec2 position) const;
		std::optional<Vec2> Route(Vec2 from, Vec2 to) const;
		float Slide(Vec2 position) const;
		float GetClock() const { return m_Clock; }

	private:
		int32_t Index(int32_t x, int32_t y) const { return y * m_Width + x; }
		void BuildCells(const World& world);
		void PlanCollapse(const World& world);
		void RefreshClearance(const World& world);

		int32_t m_Width = 0;
		int32_t m_Height = 0;
		std::vector<int32_t> m_SampleCell;
		std::vector<float> m_Floor;
		std::vector<uint8_t> m_Storm;
		std::vector<float> m_Strike;
		std::vector<float> m_CellTime;
		std::vector<float> m_Collapse;
		std::vector<float> m_Clearance;
		std::vector<float> m_ToDanger;
		std::vector<float> m_ToSafety;
		std::vector<float> m_Slide;
		std::vector<uint8_t> m_Cover;
		uint32_t m_LevelSeed = 0;
		MapKind m_LevelKind = MapKind::Count;
		uint32_t m_PlanSeed = 0;
		Vec2 m_PlanCenter;
		float m_PlanRadius = 0.0f;
		uint8_t m_Craters = 0;
		float m_Clock = 0.0f;
		float m_FieldClock = -1e9f;
	};

	class BotBrain
	{
	public:
		BotBrain(uint32_t playerId, const BotProfile& profile);

		void Update(World& world, const BotArena& arena, BotTally& tally);
		uint32_t GetPlayerId() const { return m_PlayerId; }
		const BotProfile& GetProfile() const { return m_Profile; }

	private:
		struct Rival
		{
			Entity Handle;
			uint32_t PlayerId = NoPlayer;
			Vec2 Position;
			Vec2 Velocity;
			float Fling = 1.0f;
			float Health = 1.0f;
			int32_t Life = 0;
			float Clearance = 0.0f;
			float Mobility = 1.0f;
			bool Warded = false;
			bool Chilled = false;
			bool Marked = false;
			bool Shielded = false;
			bool CanDodge = true;
			uint16_t Points = 0;
		};

		struct Missile
		{
			uint32_t NetId = 0;
			Vec2 Position;
			Vec2 Velocity;
			float Radius = 0.0f;
			float Lifetime = 0.0f;
			AbilityId Ability = AbilityId::Count;
			int32_t Damage = 0;
		};

		struct Zone
		{
			uint32_t NetId = 0;
			Vec2 Center;
			float Radius = 0.0f;
			float Remaining = 0.0f;
		};

		struct Option
		{
			int32_t Slot = -1;
			float Score = 0.0f;
			float Aim = 0.0f;
			float Distance = 0.0f;
			uint32_t Target = NoPlayer;
			Vec2 Point;
			int32_t FollowSlot = -1;
			float Charge = 0.0f;
		};

		struct Seen
		{
			uint32_t NetId = 0;
			float Since = 0.0f;
			bool Answered = false;
		};

		void Perceive(World& world, Entity self);
		void Lobby(World& world, Entity self, const BrawlStatus& brawl);
		bool Escape(const World& world, const BotArena& arena, PlayerInput& input);
		bool Recover(const BotArena& arena, PlayerInput& input);
		bool Evade(const World& world, const BotArena& arena, PlayerInput& input);
		bool Blink(const BotArena& arena, PlayerInput& input, Vec2 direction);
		bool FollowUp(const BotArena& arena, PlayerInput& input, BotTally& tally);
		bool Split(PlayerInput& input);
		bool Yank(PlayerInput& input);
		void Think(const World& world, const BotArena& arena);
		void ChooseTarget(const BotArena& arena);
		void ChooseGoal(const World& world, const BotArena& arena);
		Option PlanCast(const World& world, const BotArena& arena, uint32_t slot) const;
		float LandingValue(const BotArena& arena, Vec2 position, Vec2 impulse) const;
		float Harm(float damage, const Rival& rival, AbilityId spell = AbilityId::Count) const;
		float HitChance(float travel, float distance, const Rival& rival) const;
		Vec2 Lead(const Rival& rival, float delay, float speed) const;
		Vec2 SafePoint(const BotArena& arena, float reach, float minimum, Vec2 drift = {}) const;
		Vec2 SafeDirection(const BotArena& arena, Vec2 from, float step) const;
		int32_t ComboSlot(uint32_t slot, float castTime) const;
		bool Planted(const BotArena& arena) const;
		void Steer(PlayerInput& input) const;
		void RouteGoal(const World& world, const BotArena& arena);
		void PressDodge(const BotArena& arena, PlayerInput& input, Vec2 direction, bool path = true);
		void Cast(PlayerInput& input, uint32_t slot, float aim, float distance, float charge = 0.0f);
		bool Ready(uint32_t slot) const;
		AbilityId SpellIn(uint32_t slot) const;
		const Rival* FindRival(uint32_t playerId) const;
		float Noise(float spread);

		uint32_t m_PlayerId = NoPlayer;
		BotProfile m_Profile;
		Random m_Random;
		float m_Time = 0.0f;
		float m_Think = 0.0f;
		uint32_t m_Sequence = 0;

		Vec2 m_Position;
		Vec2 m_Push;
		Vec2 m_Wind;
		float m_Health = 1.0f;
		int32_t m_Life = 0;
		float m_SelfFling = 1.0f;
		float m_Reserve = 0.0f;
		bool m_Alive = false;
		bool m_Burning = false;
		bool m_DamageMode = false;
		float m_Knockback = 1.0f;
		const PlayerState* m_State = nullptr;
		const PlayerTraits* m_Traits = nullptr;
		BrawlLoadout m_Loadout;
		std::vector<Rival> m_Rivals;
		std::vector<Missile> m_Missiles;
		std::vector<Zone> m_Zones;
		std::vector<Seen> m_Seen;

		Vec2 m_Goal;
		bool m_HasGoal = false;
		const Level* m_Terrain = nullptr;
		Vec2 m_Route;
		Vec2 m_RouteGoal;
		float m_RouteTime = 0.0f;
		bool m_RouteValid = false;
		bool m_Routed = false;
		Vec2 m_Sidestep;
		float m_SidestepTime = 0.0f;
		uint32_t m_Target = NoPlayer;
		float m_Strafe = 1.0f;
		float m_StrafeTime = 0.0f;
		int32_t m_Casting = -1;
		float m_CastAim = 0.0f;
		float m_CastDistance = 0.0f;
		float m_CastNoise = 0.0f;
		float m_CastStretch = 1.0f;
		uint32_t m_CastTarget = NoPlayer;
		Vec2 m_CastOffset;
		float m_ChargeUntil = 0.0f;
		float m_ChargeSpeed = 0.0f;
		float m_Aim = 0.0f;
		float m_AimDistance = 0.0f;
		float m_Wander = 0.0f;
		float m_TickSeconds = 1.0f / 60.0f;
		float m_Danger = 0.0f;
		float m_Flung = 0.0f;
		bool m_Composed = true;
		bool m_FlingComposed = true;
		bool m_ComboArmed = false;
		int32_t m_FollowSlot = -1;
		float m_FollowUntil = 0.0f;
		bool m_HasWell = false;
		Vec2 m_Well;
		float m_WellRemaining = 0.0f;
		bool m_HasBolt = false;
		Vec2 m_Bolt;
		Vec2 m_BoltVelocity;
		float m_BoltLifetime = 0.0f;
		bool m_SplitArmed = false;
		Vec2 m_YankTarget;
		bool m_Yanking = false;
		bool m_Planting = false;
		float m_EchoUntil = 0.0f;
		AbilityId m_EchoSpell = AbilityId::Count;

		BrawlPhase m_Phase = BrawlPhase::Draft;
		uint8_t m_Round = 255;
		float m_PhaseTime = 0.0f;
		float m_ReadyDelay = 0.0f;
		float m_CardDelay = 0.0f;
		bool m_Drafted = false;
		bool m_Readied = false;
	};

	class BotSquad
	{
	public:
		void Add(uint32_t playerId, const BotProfile& profile);
		void Remove(uint32_t playerId);
		void Clear() { m_Brains.clear(); m_Tallies.clear(); }
		bool Contains(uint32_t playerId) const;
		size_t Count() const { return m_Brains.size(); }
		const std::vector<BotBrain>& GetBrains() const { return m_Brains; }
		const BotTally* FindTally(uint32_t playerId) const;
		void Update(World& world);

	private:
		BotArena m_Arena;
		std::vector<BotBrain> m_Brains;
		std::vector<BotTally> m_Tallies;
	};

	BrawlSpells PickBotSpells(BotStyle style, Random& random, bool uniform);
	uint32_t PickBotCard(const BrawlLoadout& loadout, BotStyle style, Random& random, BrawlMode mode);
	const char* PickBotName(uint32_t index);
}
