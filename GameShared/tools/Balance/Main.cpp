#include <GameShared/Simulation/BotBrain.h>
#include <GameShared/Simulation/World.h>
#include <algorithm>
#include <bit>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace Game;

namespace
{
	constexpr uint32_t MatchTickLimit = 60u * 60u * 45u;
	constexpr double ReportSeconds = 300.0;
	constexpr float LowMana = 20.0f;
	constexpr size_t PoolSize = std::size(BrawlSpellPool);
	constexpr size_t CardCount = size_t(CardId::Count);
	constexpr uint64_t MinComboSeats = 150;

	size_t PoolIndex(AbilityId spell)
	{
		return size_t(std::find(std::begin(BrawlSpellPool), std::end(BrawlSpellPool), spell) - std::begin(BrawlSpellPool));
	}

	struct Options
	{
		uint32_t Matches = 200;
		uint32_t Players = 6;
		uint8_t Rounds = 5;
		BotSkill Skill = BotSkill::Normal;
		bool MixedSkill = false;
		uint32_t Threads = std::max(std::thread::hardware_concurrency(), 2u) - 1u;
		uint32_t Seed = 1;
		double Hours = 0.0;
		std::string Out = "balance";
		MapKind Map = MapKind::Caldera;
		BrawlMode Mode = BrawlMode::Damage;
		bool WinnerCard = true;
	};

	struct Share
	{
		uint64_t Seats = 0;
		double Expected = 0.0;
		double Variance = 0.0;
		uint64_t Wins = 0;

		void Add(double chance, bool won)
		{
			Seats++;
			Expected += chance;
			Variance += chance * (1.0 - chance);
			Wins += won ? 1u : 0u;
		}

		void Merge(const Share& other)
		{
			Seats += other.Seats;
			Expected += other.Expected;
			Variance += other.Variance;
			Wins += other.Wins;
		}

		double Ratio() const { return Expected > 0.0 ? double(Wins) / Expected : 0.0; }
		double Margin() const { return Expected > 0.0 ? 1.96 * std::sqrt(Variance) / Expected : 0.0; }
	};

	struct SpellTotals
	{
		Share Rounds;
		Share Matches;
		uint64_t Knockouts = 0;
		uint64_t Casts = 0;
		uint64_t Hits = 0;
		uint64_t Damage = 0;
		double OwnerMinutes = 0.0;
		double PointShare = 0.0;
		uint64_t PointSeats = 0;
	};

	struct Totals
	{
		std::array<SpellTotals, size_t(AbilityId::Count)> Spells{};
		std::array<Share, PoolSize * PoolSize> Pairs{};
		std::array<Share, size_t(CardId::Count)> Cards{};
		std::vector<Share> CardPairs = std::vector<Share>(CardCount * CardCount);
		std::array<uint64_t, size_t(CardId::Count)> Offered{};
		std::array<double, size_t(CardId::Count)> ApexPoints{};
		std::array<uint64_t, size_t(CardId::Count)> ApexPointSeats{};
		uint64_t LateSeats = 0;
		uint64_t ApexSeats = 0;
		uint64_t Matches = 0;
		uint64_t Unfinished = 0;
		uint64_t Rounds = 0;
		uint64_t Empty = 0;
		uint64_t Knockouts = 0;
		uint64_t Falls = 0;
		uint64_t Dodges = 0;
		uint64_t Combos = 0;
		uint64_t ComboHits = 0;
		uint64_t ComboRingOuts = 0;
		uint64_t HazardDamage = 0;
		uint64_t Assists = 0;
		double ManaShare = 0.0;
		uint64_t ManaTicks = 0;
		uint64_t LowManaTicks = 0;
		double FightMinutes = 0.0;
		double SeatMinutes = 0.0;
		std::vector<float> Lengths;
		std::vector<float> FirstFalls;
		std::vector<float> KillTimes;
		std::array<Share, size_t(BotSkill::Count)> Skills{};

		void Merge(const Totals& other)
		{
			for (size_t i = 0; i < Spells.size(); i++)
			{
				Spells[i].Rounds.Merge(other.Spells[i].Rounds);
				Spells[i].Matches.Merge(other.Spells[i].Matches);
				Spells[i].Knockouts += other.Spells[i].Knockouts;
				Spells[i].Casts += other.Spells[i].Casts;
				Spells[i].Hits += other.Spells[i].Hits;
				Spells[i].Damage += other.Spells[i].Damage;
				Spells[i].OwnerMinutes += other.Spells[i].OwnerMinutes;
				Spells[i].PointShare += other.Spells[i].PointShare;
				Spells[i].PointSeats += other.Spells[i].PointSeats;
			}
			for (size_t i = 0; i < Pairs.size(); i++)
				Pairs[i].Merge(other.Pairs[i]);
			for (size_t i = 0; i < Skills.size(); i++)
				Skills[i].Merge(other.Skills[i]);
			FirstFalls.insert(FirstFalls.end(), other.FirstFalls.begin(), other.FirstFalls.end());
			KillTimes.insert(KillTimes.end(), other.KillTimes.begin(), other.KillTimes.end());
			for (size_t i = 0; i < Cards.size(); i++)
			{
				Cards[i].Merge(other.Cards[i]);
				Offered[i] += other.Offered[i];
				ApexPoints[i] += other.ApexPoints[i];
				ApexPointSeats[i] += other.ApexPointSeats[i];
			}
			LateSeats += other.LateSeats;
			ApexSeats += other.ApexSeats;
			for (size_t i = 0; i < CardPairs.size(); i++)
				CardPairs[i].Merge(other.CardPairs[i]);
			Matches += other.Matches;
			Unfinished += other.Unfinished;
			Rounds += other.Rounds;
			Empty += other.Empty;
			Knockouts += other.Knockouts;
			Falls += other.Falls;
			Dodges += other.Dodges;
			Combos += other.Combos;
			ComboHits += other.ComboHits;
			ComboRingOuts += other.ComboRingOuts;
			HazardDamage += other.HazardDamage;
			Assists += other.Assists;
			ManaShare += other.ManaShare;
			ManaTicks += other.ManaTicks;
			LowManaTicks += other.LowManaTicks;
			FightMinutes += other.FightMinutes;
			SeatMinutes += other.SeatMinutes;
			Lengths.insert(Lengths.end(), other.Lengths.begin(), other.Lengths.end());
		}
	};

	bool ParseNumber(std::string_view text, uint32_t& value)
	{
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		return result.ec == std::errc() && result.ptr == text.data() + text.size();
	}

	bool ParseOptions(int argc, char** argv, Options& options)
	{
		for (int i = 1; i < argc; i++)
		{
			const std::string_view argument = argv[i];
			const bool hasValue = i + 1 < argc;
			const std::string_view value = hasValue ? std::string_view(argv[i + 1]) : std::string_view();
			uint32_t number = 0;
			if (argument == "--matches" && hasValue && ParseNumber(value, number) && number > 0)
				options.Matches = number;
			else if (argument == "--players" && hasValue && ParseNumber(value, number) && number >= 2 && number <= MaxPlayers)
				options.Players = number;
			else if (argument == "--rounds" && hasValue && ParseNumber(value, number) && number >= MinBrawlRounds && number <= MaxBrawlRounds)
				options.Rounds = uint8_t(number);
			else if (argument == "--threads" && hasValue && ParseNumber(value, number) && number > 0)
				options.Threads = number;
			else if (argument == "--seed" && hasValue && ParseNumber(value, number))
				options.Seed = number;
			else if (argument == "--hours" && hasValue)
			{
				options.Hours = std::atof(argv[i + 1]);
				if (options.Hours <= 0.0)
					return false;
				options.Matches = UINT32_MAX;
			}
			else if (argument == "--map" && hasValue)
			{
				if (value != "caldera" && value != "lake" && value != "ruins" && value != "peak" && value != "all")
					return false;
				options.Map = value == "caldera" ? MapKind::Caldera : value == "lake" ? MapKind::FrozenLake : value == "ruins" ? MapKind::SkyRuins : value == "peak" ? MapKind::StormPeak : RandomBrawlMap;
			}
			else if (argument == "--winner-card" && hasValue)
			{
				if (value != "on" && value != "off")
					return false;
				options.WinnerCard = value == "on";
			}
			else if (argument == "--out" && hasValue)
				options.Out = value;
			else if (argument == "--mode" && hasValue)
			{
				const auto found = std::find(std::begin(BrawlModeNames), std::end(BrawlModeNames), value);
				if (found == std::end(BrawlModeNames))
					return false;
				options.Mode = BrawlMode(found - std::begin(BrawlModeNames));
			}
			else if (argument == "--skill" && hasValue)
			{
				if (value == "mixed")
					options.MixedSkill = true;
				else
				{
					const auto found = std::find(std::begin(BotSkillNames), std::end(BotSkillNames), value);
					if (found == std::end(BotSkillNames))
						return false;
					options.Skill = BotSkill(found - std::begin(BotSkillNames));
				}
			}
			else
				return false;
			i++;
		}
		return true;
	}

	Totals RunMatch(const Options& options, uint32_t seed)
	{
		Totals totals;
		WorldSettings settings;
		settings.Seed = seed;
		settings.Map = options.Map == RandomBrawlMap ? BrawlMaps[seed % std::size(BrawlMaps)] : options.Map;
		settings.Mode = GameMode::Brawl;
		settings.Rules.Rounds = options.Rounds;
		settings.Rules.Mode = options.Mode;
		settings.Rules.WinnerCard = options.WinnerCard;
		settings.Players = options.Players;
		World world(settings);
		BotSquad squad;
		Random random(uint64_t(seed) * 0x2545F4914F6CDD1DULL + 7u);
		std::array<BotSkill, MaxPlayers> skills{};
		for (uint32_t id = 0; id < options.Players; id++)
		{
			world.SpawnPlayer(id, StarterKit::Arcanist);
			skills[id] = options.MixedSkill ? BotSkill(random.Below(uint32_t(BotSkill::Count))) : options.Skill;
			squad.Add(id, BotProfile{ .Skill = skills[id], .Style = BotStyle(random.Below(uint32_t(BotStyle::Count))), .Seed = random.Next(), .RandomSpells = true, .Patient = false });
		}
		std::array<float, MaxPlayers> attackAges{};
		attackAges.fill(1e9f);
		std::array<int32_t, MaxPlayers> lives{};
		lives.fill(-1);
		std::array<float, MaxPlayers> hurtAt{};
		hurtAt.fill(-1.0f);
		bool fallen = false;
		struct Chain
		{
			uint32_t Count = 0;
			uint32_t Tick = 0;
			bool Active = false;
			bool Hit = false;
			bool RingOut = false;
		};
		std::array<Chain, MaxPlayers> chains{};
		constexpr uint32_t ChainTicks = 150;
		const auto chained = [&](uint32_t playerId) { return playerId < MaxPlayers && chains[playerId].Active && world.GetTick() - chains[playerId].Tick < ChainTicks; };

		struct Seat
		{
			uint32_t PlayerId = NoPlayer;
			BrawlSpells Spells{};
			CardMask Cards = 0;
		};
		std::vector<Seat> seats;
		std::array<BrawlSpells, MaxPlayers> matchSpells{};
		std::array<bool, MaxPlayers> started{};
		std::array<CardOffer, MaxPlayers> lastOffer{};
		std::array<uint16_t, MaxPlayers> roundPoints{};
		while (world.GetStatus().Match != MatchPhase::Ended && world.GetTick() < MatchTickLimit)
		{
			squad.Update(world);
			for (uint32_t id = 0; id < options.Players; id++)
				if (const BotTally* tally = squad.FindTally(id); tally && tally->Combos != chains[id].Count)
					chains[id] = { tally->Combos, world.GetTick(), true, false, false };
			world.Step();
			const MatchDirector& match = world.GetMatch();
			world.GetEntities().Each<PlayerController>([&](Entity entity, PlayerController& controller)
			{
				if (controller.PlayerId >= MaxPlayers)
					return;
				float& previous = attackAges[controller.PlayerId];
				const bool struck = controller.LastAttackAge < previous && controller.LastAttacker != NoPlayer && controller.LastAbility < uint8_t(AbilityId::Count);
				if (struck)
				{
					totals.Spells[controller.LastAbility].Hits++;
					if (controller.LastAbility == uint8_t(AbilityId::Comet) && chained(controller.LastAttacker) && !chains[controller.LastAttacker].Hit)
					{
						chains[controller.LastAttacker].Hit = true;
						totals.ComboHits++;
					}
				}
				previous = controller.LastAttackAge;
				if (controller.State.Alive && controller.Traits.MaxMana > 0.0f && match.GetBrawl().Phase == BrawlPhase::Fight)
				{
					totals.ManaShare += double(controller.State.Mana / controller.Traits.MaxMana);
					totals.ManaTicks++;
					totals.LowManaTicks += controller.State.Mana < LowMana ? 1u : 0u;
				}
				const Health* health = world.GetEntities().TryGet<Health>(entity);
				if (!health)
					return;
				int32_t& life = lives[controller.PlayerId];
				const int32_t current = std::max(health->Current, 0);
				if (life >= 0 && current < life)
				{
					const uint32_t lost = uint32_t(life - current);
					if (struck)
						totals.Spells[controller.LastAbility].Damage += lost;
					else
						totals.HazardDamage += lost;
					if (hurtAt[controller.PlayerId] < 0.0f)
						hurtAt[controller.PlayerId] = match.GetBrawl().Clock;
				}
				life = current;
			});
			for (uint32_t id = 0; id < options.Players; id++)
			{
				const BrawlLoadout& loadout = match.GetLoadout(id);
				if (loadout.OfferPending && loadout.Offer != lastOffer[id])
				{
					lastOffer[id] = loadout.Offer;
					for (const CardId card : loadout.Offer)
						if (card != CardId::Count)
							totals.Offered[size_t(card)]++;
				}
			}
			for (const WorldEvent& event : world.TakeEvents())
			{
				if (event.Type == WorldEventType::RoundStarted)
				{
					fallen = false;
					hurtAt.fill(-1.0f);
					seats.clear();
					for (uint32_t id = 0; id < options.Players; id++)
						roundPoints[id] = match.GetPoints(id);
					for (uint32_t id = 0; id < options.Players; id++)
						if (match.GetScore(id).Entered)
						{
							const BrawlLoadout& loadout = match.GetLoadout(id);
							seats.push_back({ id, loadout.Spells, loadout.Cards });
							if (!started[id])
							{
								started[id] = true;
								matchSpells[id] = loadout.Spells;
							}
						}
				}
				else if (event.Type == WorldEventType::RoundEnded && seats.size() >= 2)
				{
					const double chance = 1.0 / double(seats.size());
					const float seconds = std::max(match.GetBrawl().Clock, 0.0f);
					const double minutes = double(seconds) / 60.0;
					totals.Rounds++;
					totals.Empty += event.PlayerId == NoPlayer ? 1u : 0u;
					totals.FightMinutes += minutes;
					totals.SeatMinutes += minutes * double(seats.size());
					totals.Lengths.push_back(seconds);
					for (const Seat& seat : seats)
					{
						const bool won = seat.PlayerId == event.PlayerId;
						totals.Skills[size_t(skills[seat.PlayerId])].Add(chance, won);
						for (const AbilityId spell : seat.Spells)
						{
							SpellTotals& stats = totals.Spells[size_t(spell)];
							stats.Rounds.Add(chance, won);
							stats.OwnerMinutes += minutes;
						}
						for (size_t i = 0; i < seat.Spells.size(); i++)
							for (size_t j = i + 1; j < seat.Spells.size(); j++)
							{
								const size_t a = PoolIndex(seat.Spells[i]);
								const size_t b = PoolIndex(seat.Spells[j]);
								if (a < PoolSize && b < PoolSize && a != b)
									totals.Pairs[std::min(a, b) * PoolSize + std::max(a, b)].Add(chance, won);
							}
						totals.Spells[size_t(BrawlBasic)].OwnerMinutes += minutes;
						for (const CardDef& card : Cards)
							if (HasCard(seat.Cards, card.Id))
							{
								totals.Cards[size_t(card.Id)].Add(chance, won);
								for (const CardDef& other : Cards)
									if (other.Id > card.Id && HasCard(seat.Cards, other.Id))
										totals.CardPairs[size_t(card.Id) * CardCount + size_t(other.Id)].Add(chance, won);
							}
					}
					double gained = 0.0;
					for (const Seat& seat : seats)
						gained += double(match.GetPoints(seat.PlayerId) - roundPoints[seat.PlayerId]);
					const double meanGain = gained / double(seats.size());
					for (const Seat& seat : seats)
					{
						const CardId apex = HeldApex(seat.Cards);
						if (event.Detail >= ApexFromRound)
						{
							totals.LateSeats++;
							totals.ApexSeats += apex != CardId::Count ? 1u : 0u;
						}
						if (apex != CardId::Count && meanGain > 0.0)
						{
							totals.ApexPoints[size_t(apex)] += double(match.GetPoints(seat.PlayerId) - roundPoints[seat.PlayerId]) / meanGain;
							totals.ApexPointSeats[size_t(apex)]++;
						}
					}
				}
			}
			for (const KillRecord& kill : world.TakeKills())
			{
				if (!fallen)
				{
					fallen = true;
					totals.FirstFalls.push_back(match.GetBrawl().Clock);
				}
				if (kill.Victim < MaxPlayers && hurtAt[kill.Victim] >= 0.0f)
				{
					totals.KillTimes.push_back(match.GetBrawl().Clock - hurtAt[kill.Victim]);
					hurtAt[kill.Victim] = -1.0f;
				}
				if (kill.Killer == NoPlayer || kill.Ability >= uint8_t(AbilityId::Count))
				{
					totals.Falls++;
					continue;
				}
				totals.Knockouts++;
				totals.Spells[kill.Ability].Knockouts++;
				if (kill.Ability == uint8_t(AbilityId::Comet) && chained(kill.Killer) && !chains[kill.Killer].RingOut)
				{
					chains[kill.Killer].RingOut = true;
					totals.ComboRingOuts++;
				}
			}
			world.TakeHits();
			world.TakeNotices();
		}
		const GameStatus status = world.GetStatus();
		totals.Matches = 1;
		if (status.Match != MatchPhase::Ended)
		{
			totals.Unfinished = 1;
			return totals;
		}
		uint32_t entrants = 0;
		double points = 0.0;
		for (uint32_t id = 0; id < options.Players; id++)
			if (started[id])
			{
				entrants++;
				points += double(world.GetMatch().GetPoints(id));
			}
		const double meanPoints = entrants > 0 ? points / double(entrants) : 0.0;
		for (uint32_t id = 0; id < options.Players; id++)
		{
			if (const BotTally* tally = squad.FindTally(id))
			{
				for (size_t spell = 0; spell < tally->Casts.size(); spell++)
					totals.Spells[spell].Casts += tally->Casts[spell];
				totals.Dodges += tally->Dodges;
				totals.Combos += tally->Combos;
			}
			totals.Assists += world.GetMatch().GetScore(id).Assists;
			if (!started[id] || entrants < 2)
				continue;
			for (const AbilityId spell : matchSpells[id])
			{
				SpellTotals& stats = totals.Spells[size_t(spell)];
				stats.Matches.Add(1.0 / double(entrants), status.Winner == id);
				if (meanPoints > 0.0)
				{
					stats.PointShare += double(world.GetMatch().GetPoints(id)) / meanPoints;
					stats.PointSeats++;
				}
			}
		}
		return totals;
	}

	std::string Fixed(double value, int digits)
	{
		char text[64];
		std::snprintf(text, sizeof(text), "%.*f", digits, value);
		return text;
	}

	std::string Percent(double value)
	{
		return Fixed(value * 100.0, value < 0.1 || value > 0.9 ? 1 : 0) + "%";
	}

	const char* Verdict(const Share& share)
	{
		const double ratio = share.Ratio();
		const double margin = share.Margin();
		if (share.Seats < 30)
			return "too few rounds";
		if (ratio - margin > 1.1)
			return "too strong";
		if (ratio + margin < 0.9)
			return "too weak";
		if (ratio > 1.1)
			return "leans strong";
		if (ratio < 0.9)
			return "leans weak";
		return "in the band";
	}

	const char* BandVerdict(const Share& share, double low, double high)
	{
		const double ratio = share.Ratio();
		const double margin = share.Margin();
		if (share.Seats < 30)
			return "too few rounds";
		if (ratio - margin > high)
			return "too strong";
		if (ratio + margin < low)
			return "too weak";
		if (ratio > high)
			return "leans strong";
		if (ratio < low)
			return "leans weak";
		return "in the band";
	}

	const char* CardVerdict(const Share& share)
	{
		return BandVerdict(share, 0.85, 1.15);
	}

	std::string TagList(TagMask tags)
	{
		std::string list;
		for (size_t tag = 0; tag < size_t(CardTag::Count); tag++)
			if (tags & TagBit(CardTag(tag)))
				list += (list.empty() ? "" : " ") + std::string(CardTagNames[tag]);
		return list;
	}

	float Quantile(std::vector<float> values, double q)
	{
		if (values.empty())
			return 0.0f;
		const size_t index = std::min(size_t(q * double(values.size() - 1) + 0.5), values.size() - 1);
		std::nth_element(values.begin(), values.begin() + ptrdiff_t(index), values.end());
		return values[index];
	}

	void WriteReport(const Options& options, const Totals& totals, double seconds)
	{
		char stamp[64];
		const std::time_t now = std::time(nullptr);
		std::tm local{};
		localtime_s(&local, &now);
		std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &local);
		const std::string skill = options.MixedSkill ? "mixed" : BotSkillNames[size_t(options.Skill)];
		const bool damage = options.Mode == BrawlMode::Damage;
		const char* knockout = damage ? "Kills" : "Ring-outs";

		std::ofstream csv(options.Out + "-spells.csv");
		csv << "spell,seats,round_win_ratio,ratio_margin,match_win_ratio,points_ratio,knockouts_per_owner_minute,knockout_share,casts_per_owner_minute,hits_per_cast,damage_per_owner_minute,verdict\n";
		std::ofstream md(options.Out + ".md");
		md << "# Spellbrawl balance report\n\n";
		md << stamp << " \xC2\xB7 " << totals.Matches << " matches";
		if (totals.Unfinished > 0)
			md << " (" << totals.Unfinished << " hit the time limit)";
		md << " \xC2\xB7 " << (damage ? "damage" : "knockback") << " mode \xC2\xB7 " << (options.Map == RandomBrawlMap ? "every brawl map" : GetMapDef(options.Map).Name) << " \xC2\xB7 " << options.Players << " bots per match \xC2\xB7 " << unsigned(options.Rounds) << " rounds \xC2\xB7 " << skill << " bots \xC2\xB7 " << totals.Rounds << " rounds played \xC2\xB7 " << Fixed(totals.FightMinutes / 60.0, 1) << " hours of fighting \xC2\xB7 " << Fixed(seconds / 60.0, 1) << " minutes to simulate\n\n";
		md << "Every bot draws three spells at random, so each spell sits in about a third of the seats. The ratio compares how often a spell's owners win a round with how often they would by chance. 1.00 is fair, and the target band is 0.90 to 1.10. The range is a 95% confidence interval.\n\n";
		md << "## Spells\n\n";
		md << "| Spell | Seats | Round win ratio | 95% range | Match win ratio | " << (damage ? "Points ratio | " : "") << knockout << " per owner-minute | Share of " << (damage ? "kills" : "ring-outs") << " | Casts per owner-minute | Hits per cast | " << (damage ? "Damage per owner-minute | " : "") << "Verdict |\n";
		md << "|---|---|---|---|---|---|---|---|---|" << (damage ? "---|---|" : "") << "---|\n";
		std::vector<AbilityId> order(std::begin(BrawlSpellPool), std::end(BrawlSpellPool));
		std::sort(order.begin(), order.end(), [&](AbilityId a, AbilityId b) { return totals.Spells[size_t(a)].Rounds.Ratio() > totals.Spells[size_t(b)].Rounds.Ratio(); });
		uint64_t seatSum = 0;
		for (const AbilityId spell : order)
			seatSum += totals.Spells[size_t(spell)].Rounds.Seats;
		for (const AbilityId spell : order)
		{
			const SpellTotals& stats = totals.Spells[size_t(spell)];
			const double perMinute = stats.OwnerMinutes > 0.0 ? double(stats.Knockouts) / stats.OwnerMinutes : 0.0;
			const double share = totals.Knockouts > 0 ? double(stats.Knockouts) / double(totals.Knockouts) : 0.0;
			const double casts = stats.OwnerMinutes > 0.0 ? double(stats.Casts) / stats.OwnerMinutes : 0.0;
			const double seats = seatSum > 0 ? double(stats.Rounds.Seats) * double(SkillSlots) / double(seatSum) : 0.0;
			const double hits = stats.Casts > 0 ? double(stats.Hits) / double(stats.Casts) : 0.0;
			const double dealt = stats.OwnerMinutes > 0.0 ? double(stats.Damage) / stats.OwnerMinutes : 0.0;
			const double points = stats.PointSeats > 0 ? stats.PointShare / double(stats.PointSeats) : 0.0;
			const Share& rounds = stats.Rounds;
			md << "| " << GetAbility(spell).Name << " | " << Percent(seats) << " | " << Fixed(rounds.Ratio(), 2) << " | " << Fixed(rounds.Ratio() - rounds.Margin(), 2) << " to " << Fixed(rounds.Ratio() + rounds.Margin(), 2) << " | " << Fixed(stats.Matches.Ratio(), 2) << " | " << (damage ? Fixed(points, 2) + " | " : "") << Fixed(perMinute, 2) << " | " << Percent(share) << " | " << Fixed(casts, 1) << " | " << Fixed(hits, 2) << " | " << (damage ? Fixed(dealt, 0) + " | " : "") << Verdict(rounds) << " |\n";
			csv << GetAbility(spell).Name << "," << stats.Rounds.Seats << "," << Fixed(rounds.Ratio(), 4) << "," << Fixed(rounds.Margin(), 4) << "," << Fixed(stats.Matches.Ratio(), 4) << "," << Fixed(points, 4) << "," << Fixed(perMinute, 4) << "," << Fixed(share, 4) << "," << Fixed(casts, 3) << "," << Fixed(hits, 3) << "," << Fixed(dealt, 2) << "," << Verdict(rounds) << "\n";
		}
		{
			const SpellTotals& basic = totals.Spells[size_t(BrawlBasic)];
			const double perMinute = basic.OwnerMinutes > 0.0 ? double(basic.Knockouts) / basic.OwnerMinutes : 0.0;
			const double share = totals.Knockouts > 0 ? double(basic.Knockouts) / double(totals.Knockouts) : 0.0;
			const double casts = basic.OwnerMinutes > 0.0 ? double(basic.Casts) / basic.OwnerMinutes : 0.0;
			const double hits = basic.Casts > 0 ? double(basic.Hits) / double(basic.Casts) : 0.0;
			const double dealt = basic.OwnerMinutes > 0.0 ? double(basic.Damage) / basic.OwnerMinutes : 0.0;
			md << "| " << GetAbility(BrawlBasic).Name << " (everyone) | 100% | n/a | n/a | n/a | " << (damage ? "n/a | " : "") << Fixed(perMinute, 2) << " | " << Percent(share) << " | " << Fixed(casts, 1) << " | " << Fixed(hits, 2) << " | " << (damage ? Fixed(dealt, 0) + " | " : "") << "shared |\n";
		}
		md << "\n## Spell pairs\n\n";
		md << "Each pair of spells sits in about one seat in twelve. Synergy is the pair's ratio minus what its two spells would give if their effects simply added up (the two ratios minus 1). A positive number means the spells help each other, as Gravity Well can set up Comet.\n\n";
		md << "| Pair | Seat-rounds | Round win ratio | 95% range | Synergy | Verdict |\n";
		md << "|---|---|---|---|---|---|\n";
		std::ofstream pairCsv(options.Out + "-pairs.csv");
		pairCsv << "first,second,seats,round_win_ratio,ratio_margin,synergy,verdict\n";
		std::vector<std::pair<size_t, size_t>> pairs;
		for (size_t a = 0; a < PoolSize; a++)
			for (size_t b = a + 1; b < PoolSize; b++)
				pairs.emplace_back(a, b);
		const auto pairShare = [&](const std::pair<size_t, size_t>& pair) -> const Share& { return totals.Pairs[pair.first * PoolSize + pair.second]; };
		std::sort(pairs.begin(), pairs.end(), [&](const auto& x, const auto& y) { return pairShare(x).Ratio() > pairShare(y).Ratio(); });
		for (const auto& pair : pairs)
		{
			const Share& share = pairShare(pair);
			const AbilityId first = BrawlSpellPool[pair.first];
			const AbilityId second = BrawlSpellPool[pair.second];
			const double synergy = share.Ratio() - (totals.Spells[size_t(first)].Rounds.Ratio() + totals.Spells[size_t(second)].Rounds.Ratio() - 1.0);
			const std::string sign = synergy >= 0.0 ? "+" : "";
			md << "| " << GetAbility(first).Name << " + " << GetAbility(second).Name << " | " << share.Seats << " | " << Fixed(share.Ratio(), 2) << " | " << Fixed(share.Ratio() - share.Margin(), 2) << " to " << Fixed(share.Ratio() + share.Margin(), 2) << " | " << sign << Fixed(synergy, 2) << " | " << Verdict(share) << " |\n";
			pairCsv << GetAbility(first).Name << "," << GetAbility(second).Name << "," << share.Seats << "," << Fixed(share.Ratio(), 4) << "," << Fixed(share.Margin(), 4) << "," << Fixed(synergy, 4) << "," << Verdict(share) << "\n";
		}

		if (options.MixedSkill)
		{
			md << "\n## Bot skill\n\n";
			md << "| Skill | Seat-rounds | Round win ratio | 95% range |\n";
			md << "|---|---|---|---|\n";
			for (size_t skill = 0; skill < totals.Skills.size(); skill++)
			{
				const Share& share = totals.Skills[skill];
				md << "| " << BotSkillNames[skill] << " | " << share.Seats << " | " << Fixed(share.Ratio(), 2) << " | " << Fixed(share.Ratio() - share.Margin(), 2) << " to " << Fixed(share.Ratio() + share.Margin(), 2) << " |\n";
			}
		}

		md << "\n## Cards\n\n";
		md << "Cards are taken by the bots' own picks, so these numbers are rougher: a card that is often taken by players who are already behind looks weaker than it is. The target band for cards is 0.85 to 1.15.\n\n";
		md << "| Card | Tags | Offered | Seat-rounds | Round win ratio | 95% range | Verdict |\n";
		md << "|---|---|---|---|---|---|---|\n";
		std::ofstream cardCsv(options.Out + "-cards.csv");
		cardCsv << "card,tags,offered,seats,round_win_ratio,ratio_margin,verdict\n";
		const uint8_t mode = damage ? InDamage : InKnockback;
		std::vector<CardId> cards;
		for (const CardDef& card : Cards)
			if ((card.Modes & mode) != 0 && card.Rarity != CardRarity::Apex)
				cards.push_back(card.Id);
		std::sort(cards.begin(), cards.end(), [&](CardId a, CardId b) { return totals.Cards[size_t(a)].Ratio() > totals.Cards[size_t(b)].Ratio(); });
		for (const CardId card : cards)
		{
			const Share& share = totals.Cards[size_t(card)];
			const std::string tags = TagList(GetCard(card).Tags);
			md << "| " << GetCard(card).Name << " | " << tags << " | " << totals.Offered[size_t(card)] << " | " << share.Seats << " | " << Fixed(share.Ratio(), 2) << " | " << Fixed(share.Ratio() - share.Margin(), 2) << " to " << Fixed(share.Ratio() + share.Margin(), 2) << " | " << CardVerdict(share) << " |\n";
			cardCsv << GetCard(card).Name << "," << tags << "," << totals.Offered[size_t(card)] << "," << share.Seats << "," << Fixed(share.Ratio(), 4) << "," << Fixed(share.Margin(), 4) << "," << CardVerdict(share) << "\n";
		}

		if (damage)
		{
			md << "\n## Apex cards\n\n";
			md << "Apex cards are offered from round " << unsigned(ApexFromRound) << ", at most one per player. The target band is 1.2 to 1.6: an apex should be a big edge without deciding the match on its own.\n\n";
			md << "| Apex | Tags | Offered | Seat-rounds | Round win ratio | 95% range | Points ratio | Verdict |\n";
			md << "|---|---|---|---|---|---|---|---|\n";
			std::vector<CardId> apexes;
			for (const CardDef& card : Cards)
				if (card.Rarity == CardRarity::Apex)
					apexes.push_back(card.Id);
			std::sort(apexes.begin(), apexes.end(), [&](CardId a, CardId b) { return totals.Cards[size_t(a)].Ratio() > totals.Cards[size_t(b)].Ratio(); });
			for (const CardId card : apexes)
			{
				const Share& share = totals.Cards[size_t(card)];
				const double points = totals.ApexPointSeats[size_t(card)] > 0 ? totals.ApexPoints[size_t(card)] / double(totals.ApexPointSeats[size_t(card)]) : 0.0;
				md << "| " << GetCard(card).Name << " | " << TagList(GetCard(card).Tags) << " | " << totals.Offered[size_t(card)] << " | " << share.Seats << " | " << Fixed(share.Ratio(), 2) << " | " << Fixed(share.Ratio() - share.Margin(), 2) << " to " << Fixed(share.Ratio() + share.Margin(), 2) << " | " << Fixed(points, 2) << " | " << BandVerdict(share, 1.2, 1.6) << " |\n";
				cardCsv << GetCard(card).Name << ",apex," << totals.Offered[size_t(card)] << "," << share.Seats << "," << Fixed(share.Ratio(), 4) << "," << Fixed(share.Margin(), 4) << "," << BandVerdict(share, 1.2, 1.6) << "\n";
			}
			md << "\n" << Fixed(100.0 * double(totals.ApexSeats) / double(std::max<uint64_t>(totals.LateSeats, 1)), 1) << "% of seats from round " << unsigned(ApexFromRound) << " on held an apex, " << Fixed(double(totals.ApexSeats) / double(std::max<uint64_t>(totals.Matches, 1)), 2) << " apex seat-rounds per match.\n";
		}

		md << "\n## Card combinations\n\n";
		std::vector<std::pair<CardId, CardId>> combos;
		for (size_t a = 0; a < CardCount; a++)
			for (size_t b = a + 1; b < CardCount; b++)
				if (totals.CardPairs[a * CardCount + b].Seats >= MinComboSeats)
					combos.emplace_back(CardId(a), CardId(b));
		const auto combo = [&](const std::pair<CardId, CardId>& pair) -> const Share& { return totals.CardPairs[size_t(pair.first) * CardCount + size_t(pair.second)]; };
		std::sort(combos.begin(), combos.end(), [&](const auto& x, const auto& y) { return combo(x).Ratio() > combo(y).Ratio(); });
		md << "The ten pairs of cards whose holders win rounds most often, among pairs held together for at least " << MinComboSeats << " seat-rounds.\n\n";
		md << "| Pair | Tags | Seat-rounds | Round win ratio | 95% range |\n";
		md << "|---|---|---|---|---|\n";
		TagMask spanned = 0;
		for (size_t i = 0; i < combos.size() && i < 10; i++)
		{
			const Share& share = combo(combos[i]);
			const TagMask tags = TagMask(GetCard(combos[i].first).Tags | GetCard(combos[i].second).Tags);
			spanned |= tags;
			md << "| " << GetCard(combos[i].first).Name << " + " << GetCard(combos[i].second).Name << " | " << TagList(tags) << " | " << share.Seats << " | " << Fixed(share.Ratio(), 2) << " | " << Fixed(share.Ratio() - share.Margin(), 2) << " to " << Fixed(share.Ratio() + share.Margin(), 2) << " |\n";
		}
		md << "\nThe top ten span " << std::popcount(spanned) << " of the six tags.\n";

		md << "\n## Rounds\n\n";
		if (!totals.Lengths.empty())
		{
			const auto longest = std::max_element(totals.Lengths.begin(), totals.Lengths.end());
			md << "- Length: median " << Fixed(Quantile(totals.Lengths, 0.5), 0) << " s, middle half " << Fixed(Quantile(totals.Lengths, 0.25), 0) << " to " << Fixed(Quantile(totals.Lengths, 0.75), 0) << " s, 90% under " << Fixed(Quantile(totals.Lengths, 0.9), 0) << " s, longest " << Fixed(*longest, 0) << " s.\n";
		}
		if (!totals.FirstFalls.empty())
			md << "- The first mage falls after a median of " << Fixed(Quantile(totals.FirstFalls, 0.5), 0) << " s (middle half " << Fixed(Quantile(totals.FirstFalls, 0.25), 0) << " to " << Fixed(Quantile(totals.FirstFalls, 0.75), 0) << " s).\n";
		const double rounds = double(std::max<uint64_t>(totals.Rounds, 1));
		md << "- " << Percent(double(totals.Empty) / rounds) << " of rounds ended with nobody standing.\n";
		const double deaths = double(std::max<uint64_t>(totals.Knockouts + totals.Falls, 1));
		if (damage)
		{
			if (!totals.KillTimes.empty())
				md << "- Time to kill, from a mage's first wound to their death: median " << Fixed(Quantile(totals.KillTimes, 0.5), 1) << " s, middle half " << Fixed(Quantile(totals.KillTimes, 0.25), 1) << " to " << Fixed(Quantile(totals.KillTimes, 0.75), 1) << " s.\n";
			md << "- " << Percent(double(totals.Knockouts) / deaths) << " of deaths were credited to the rival with the most damage over the last " << Fixed(DamageCreditSeconds, 0) << " s. The other " << Percent(double(totals.Falls) / deaths) << " had no rival damage in that window. " << Fixed(double(totals.Assists) / double(std::max<uint64_t>(totals.Knockouts, 1)), 2) << " assists per kill.\n";
			uint64_t dealt = totals.HazardDamage;
			for (const SpellTotals& stats : totals.Spells)
				dealt += stats.Damage;
			md << "- " << Fixed(double(dealt - totals.HazardDamage) / std::max(totals.SeatMinutes, 1e-9), 0) << " spell damage per seat-minute. " << Percent(double(totals.HazardDamage) / double(std::max<uint64_t>(dealt, 1))) << " of all damage came from the " << (options.Map == MapKind::FrozenLake ? "water" : options.Map == MapKind::SkyRuins ? "sky" : options.Map == MapKind::StormPeak ? "storm and the lightning" : options.Map == RandomBrawlMap ? "hazards" : "lava") << ".\n";
		}
		else
			md << "- " << Percent(double(totals.Knockouts) / deaths) << " of falls into the lava were credited to a spell. The other " << Percent(double(totals.Falls) / deaths) << " were walked, crumbled or burned without a recent shove.\n";
		md << "- " << Fixed(double(totals.Knockouts) / std::max(totals.SeatMinutes, 1e-9), 2) << " " << (damage ? "kills" : "ring-outs") << " per seat-minute, " << Fixed(double(totals.Dodges) / std::max(totals.SeatMinutes, 1e-9), 1) << " dodges per seat-minute.\n";
		if (totals.ManaTicks > 0)
			md << "- Mana while fighting: " << Percent(totals.ManaShare / double(totals.ManaTicks)) << " of the pool on average, below " << Fixed(LowMana, 0) << " for " << Percent(double(totals.LowManaTicks) / double(totals.ManaTicks)) << " of the time.\n";
		{
			const size_t well = PoolIndex(AbilityId::GravityWell);
			const size_t comet = PoolIndex(AbilityId::Comet);
			if (well < PoolSize && comet < PoolSize)
			{
				const Share& both = totals.Pairs[std::min(well, comet) * PoolSize + std::max(well, comet)];
				const double chains = double(std::max<uint64_t>(totals.Combos, 1));
				md << "- Bots holding both Gravity Well and Comet chained the comet onto their own well " << Fixed(both.Seats > 0 ? double(totals.Combos) / double(both.Seats) : 0.0, 2) << " times per round. " << Percent(double(totals.ComboHits) / chains) << " of chains landed the comet on a rival and " << Percent(double(totals.ComboRingOuts) / chains) << (damage ? " killed someone.\n" : " rang someone out.\n");
			}
		}
		md << "\nRun with: Balance --mode " << BrawlModeNames[size_t(options.Mode)] << " --matches " << (options.Matches == UINT32_MAX ? totals.Matches : options.Matches) << " --players " << options.Players << " --rounds " << unsigned(options.Rounds) << " --skill " << skill << " --map " << (options.Map == MapKind::Caldera ? "caldera" : options.Map == MapKind::FrozenLake ? "lake" : options.Map == MapKind::SkyRuins ? "ruins" : options.Map == MapKind::StormPeak ? "peak" : "all") << " --seed " << options.Seed << "\n";
	}
}

int main(int argc, char** argv)
{
	Options options;
	if (!ParseOptions(argc, argv, options))
	{
		std::printf("usage: Balance [--mode damage|knockback] [--matches n] [--hours h] [--players 2..12] [--rounds 3..15] [--skill easy|normal|hard|mixed] [--map caldera|lake|ruins|peak|all] [--winner-card on|off] [--threads n] [--seed n] [--out path]\n");
		return 1;
	}
	{
		World warmup(WorldSettings{ .Seed = 1, .Map = MapKind::Caldera, .Mode = GameMode::Brawl });
		warmup.SpawnPlayer(0, StarterKit::Arcanist);
		BotSquad squad;
		squad.Add(0, {});
		for (int tick = 0; tick < 4; tick++)
		{
			squad.Update(warmup);
			warmup.Step();
		}
	}
	const auto start = std::chrono::steady_clock::now();
	const auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
	const double deadline = options.Hours > 0.0 ? options.Hours * 3600.0 : 1e300;
	std::atomic<uint32_t> next{ 0 };
	std::atomic<uint32_t> done{ 0 };
	std::mutex lock;
	Totals totals;
	std::printf("Simulating %s%s %s matches of %u %s bots on %u threads\n", options.Matches == UINT32_MAX ? "" : std::to_string(options.Matches).c_str(), options.Matches == UINT32_MAX ? ("up to " + Fixed(options.Hours, 1) + " hours of").c_str() : "", BrawlModeNames[size_t(options.Mode)], options.Players, options.MixedSkill ? "mixed" : BotSkillNames[size_t(options.Skill)], options.Threads);
	std::vector<std::thread> workers;
	for (uint32_t t = 0; t < options.Threads; t++)
		workers.emplace_back([&]
		{
			for (;;)
			{
				const uint32_t index = next.fetch_add(1);
				if (index >= options.Matches || elapsed() >= deadline)
					return;
				const Totals result = RunMatch(options, options.Seed * 1000003u + index * 7919u + 1u);
				std::lock_guard guard(lock);
				totals.Merge(result);
				done++;
			}
		});
	double lastReport = 0.0;
	uint32_t lastPrinted = 0;
	while (done.load() < options.Matches && elapsed() < deadline + 120.0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		const uint32_t finished = done.load();
		if (finished >= lastPrinted + std::max(options.Matches == UINT32_MAX ? 100u : options.Matches / 20u, 1u))
		{
			lastPrinted = finished;
			std::printf("  %u matches after %.0f s\n", finished, elapsed());
		}
		if (elapsed() - lastReport >= ReportSeconds)
		{
			lastReport = elapsed();
			std::lock_guard guard(lock);
			WriteReport(options, totals, elapsed());
		}
		if (options.Matches == UINT32_MAX && elapsed() >= deadline && next.load() > 0)
			break;
	}
	for (std::thread& worker : workers)
		worker.join();
	WriteReport(options, totals, elapsed());
	std::printf("Wrote %s.md, %s-spells.csv and %s-pairs.csv: %llu matches, %llu rounds in %.0f s\n", options.Out.c_str(), options.Out.c_str(), options.Out.c_str(), (unsigned long long)totals.Matches, (unsigned long long)totals.Rounds, elapsed());
	return 0;
}
