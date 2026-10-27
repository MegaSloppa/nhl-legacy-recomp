// Legacy Rogue - career-long run + meta progression. Host-side only; never
// touches the game's own Be-A-Pro save, so a broken run cannot corrupt a career.
//
// DESIGN (see docs/design.md): one run == one whole Be-A-Pro career.
//  * Hype     - run currency, earned every game from performance. Spent in the
//               between-games Perk Shop. Prices inflate as you own more perks.
//  * Notoriety- 0..1000 "heat". Rises when you dominate, decays slowly. Tiers make
//               the LEAGUE REACT (reactions.cpp): harder CPU, goons hunting you,
//               but also bigger Hype multipliers.
//  * Legacy   - permanent meta currency across careers; unlocks perks into the pool.
#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include "rogue/perks.h"

namespace rogue {

struct OwnedPerk { std::string id; uint8_t stacks = 1; };

// Per-game line, filled by hooks (TODO RE) or the panel's manual entry.
struct GameStats {
  bool won = false;
  int goals = 0, assists = 0, hits = 0, plus_minus = 0, shots = 0;
  int team_gf = 0, team_ga = 0;
  // Filled by rogue hooks during the game:
  int knockdowns = 0, crit_slaps = 0;
};

enum class HeatTier : uint8_t { kUnknown, kRising, kStar, kPhenom, kMenace, kLegend };
const char* HeatTierName(HeatTier t);

struct RunState {
  bool active = false;
  std::string career_id;   // user-entered until the BAP save slot is RE'd
  uint32_t seed = 0;
  int games = 0, season = 1;
  int hype = 0;
  int notoriety = 0;       // 0..1000
  int rerolls_this_shop = 0;
  std::vector<OwnedPerk> perks;
  GameStats live;          // current game's counters
  // Career totals (for Legacy score + milestone rewards)
  int tot_goals = 0, tot_points = 0, tot_knockdowns = 0, tot_crits = 0;
};

struct MetaState {
  int legacy = 0;
  std::vector<std::string> unlocked;
  int best_legacy_score = 0;
};

struct ShopOffer { const PerkDef* perk; int price; bool free; };

class RogueRun {
 public:
  void StartCareer(const std::string& career_id, uint32_t seed);
  void RetireCareer();                       // converts score -> Legacy
  void OnGameFinished(const GameStats& g);   // Hype + Notoriety + milestones

  HeatTier Tier() const;
  float HypeMultiplier() const;

  // Between-games shop.
  std::vector<ShopOffer> RollShop();
  bool Buy(const ShopOffer& o);
  int RerollCost() const { return 20 + 15 * run_.rerolls_this_shop; }
  bool Reroll();
  bool PendingFreePick() const { return free_picks_ > 0; }

  int Stacks(const char* id) const;
  float EventParam(EventKind k) const;
  int LegacyScore() const;

  RunState& run() { return run_; }
  MetaState& meta() { return meta_; }
  std::mt19937& rng() { return rng_; }
  void Load();
  void Save() const;

 private:
  int Price(const PerkDef& p) const;
  RunState run_;
  MetaState meta_;
  int free_picks_ = 0;
  std::mt19937 rng_{12345};
};

RogueRun& Run();

}  // namespace rogue
