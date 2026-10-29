#include "rogue/run_state.h"
#include <algorithm>
#include <fstream>
#include <rex/filesystem.h>

namespace rogue {

RogueRun& Run() { static RogueRun r; return r; }

static std::filesystem::path P(const char* n) {
  return rex::filesystem::GetExecutableFolder() / n;
}
const char* HeatTierName(HeatTier t) {
  static const char* n[] = {"Unknown", "Rising", "Star", "Phenom", "Menace", "Legend"};
  return n[int(t)];
}
static int RarityWeight(Rarity r) {
  switch (r) { case Rarity::kCommon: return 60; case Rarity::kRare: return 28;
               case Rarity::kEpic: return 10; default: return 2; }
}
static int RarityBasePrice(Rarity r) {
  switch (r) { case Rarity::kCommon: return 40; case Rarity::kRare: return 90;
               case Rarity::kEpic: return 180; default: return 400; }
}

void RogueRun::StartCareer(const std::string& id, uint32_t seed) {
  run_ = RunState{};
  run_.active = true;
  run_.career_id = id;
  run_.seed = seed;
  rng_.seed(seed);
  free_picks_ = 1;  // starting perk
  if (meta_.unlocked.empty())
    for (auto& p : Catalog())
      if (p.rarity <= Rarity::kRare || p.is_curse) meta_.unlocked.push_back(p.id);
  Save();
}

int RogueRun::LegacyScore() const {
  return run_.tot_points * 3 + run_.tot_goals * 2 + run_.tot_knockdowns + run_.tot_crits * 2 +
         run_.games + run_.notoriety / 10;
}

void RogueRun::RetireCareer() {
  const int score = LegacyScore();
  meta_.legacy += score / 20;
  meta_.best_legacy_score = std::max(meta_.best_legacy_score, score);
  run_.active = false;
  Save();
}

HeatTier RogueRun::Tier() const {
  const int n = run_.notoriety;
  return n < 100 ? HeatTier::kUnknown : n < 250 ? HeatTier::kRising : n < 450 ? HeatTier::kStar
       : n < 650 ? HeatTier::kPhenom : n < 850 ? HeatTier::kMenace : HeatTier::kLegend;
}
float RogueRun::HypeMultiplier() const { return 1.f + 0.15f * int(Tier()); }

void RogueRun::OnGameFinished(const GameStats& g) {
  if (!run_.active) return;
  ++run_.games;
  const int points = g.goals + g.assists;
  // Hype: every good thing you do pays. Tunable in one place.
  int hype = 10 + g.goals * 15 + g.assists * 8 + g.hits * 2 + g.knockdowns * 4 +
             g.crit_slaps * 6 + std::max(0, g.plus_minus) * 3 + (g.won ? 20 : 0);
  run_.hype += int(hype * HypeMultiplier());
  // Notoriety: grows faster than linearly with dominance, decays 4%/game.
  const int dominance = points * 12 + g.knockdowns * 5 + g.crit_slaps * 6 + (points >= 3 ? 25 : 0);
  run_.notoriety = std::clamp(int(run_.notoriety * 0.96f) + dominance, 0, 1000);
  run_.tot_goals += g.goals; run_.tot_points += points;
  run_.tot_knockdowns += g.knockdowns; run_.tot_crits += g.crit_slaps;
  // Milestones grant a free pick (82-game seasons: roughly every ~20 games).
  if (run_.games % 20 == 0) ++free_picks_;
  if (run_.games % 82 == 0) { ++run_.season; ++free_picks_; }
  run_.live = GameStats{};
  run_.rerolls_this_shop = 0;
  Save();
}

int RogueRun::Stacks(const char* id) const {
  for (auto& o : run_.perks) if (o.id == id) return o.stacks;
  return 0;
}
float RogueRun::EventParam(EventKind k) const {
  float s = 0.f;
  for (auto& o : run_.perks)
    if (auto* p = FindPerk(o.id.c_str()))
      for (auto& e : p->events) if (e.kind == k) s += e.p0 * o.stacks;
  return s;
}

int RogueRun::Price(const PerkDef& p) const {
  int owned = 0;
  for (auto& o : run_.perks) owned += o.stacks;
  float inflation = 1.f + 0.08f * owned;
  return p.is_curse ? -RarityBasePrice(p.rarity) / 2  // curses PAY you
                    : int(RarityBasePrice(p.rarity) * inflation);
}

std::vector<ShopOffer> RogueRun::RollShop() {
  std::vector<const PerkDef*> pool;
  std::vector<int> w;
  for (auto& id : meta_.unlocked)
    if (auto* p = FindPerk(id.c_str()))
      if (Stacks(p->id) < p->max_stacks) { pool.push_back(p); w.push_back(RarityWeight(p->rarity)); }
  std::vector<ShopOffer> out;
  while (!pool.empty() && out.size() < 3) {
    std::discrete_distribution<int> d(w.begin(), w.end());
    int i = d(rng_);
    out.push_back({pool[i], Price(*pool[i]), free_picks_ > 0 && !pool[i]->is_curse});
    pool.erase(pool.begin() + i);
    w.erase(w.begin() + i);
  }
  return out;
}

bool RogueRun::Buy(const ShopOffer& o) {
  if (!o.perk) return false;
  if (o.free) {
    // BUG-1 (2026-10-29): stale FREE offers let every offer be taken and drove
    // free_picks_ negative. Never spend a pick that does not exist.
    if (free_picks_ <= 0 || o.perk->is_curse) return false;
    --free_picks_;
  } else if (run_.hype < o.price) {
    return false;
  } else {
    run_.hype -= o.price;  // negative price (curse) adds Hype
  }
  for (auto& p : run_.perks) if (p.id == o.perk->id) { ++p.stacks; Save(); return true; }
  run_.perks.push_back({o.perk->id, 1});
  Save();
  return true;
}

bool RogueRun::Reroll() {
  if (run_.hype < RerollCost()) return false;
  run_.hype -= RerollCost();
  ++run_.rerolls_this_shop;
  return true;
}

// Plain key=value text: hand-editable, no JSON lib in-tree. One file per career.
void RogueRun::Save() const {
  std::ofstream r(P("rogue_career.txt"));
  r << "active=" << run_.active << "\ncareer=" << run_.career_id << "\nseed=" << run_.seed
    << "\ngames=" << run_.games << "\nseason=" << run_.season << "\nhype=" << run_.hype
    << "\nnotoriety=" << run_.notoriety << "\nfree=" << free_picks_
    << "\ntot_goals=" << run_.tot_goals << "\ntot_points=" << run_.tot_points
    << "\ntot_kd=" << run_.tot_knockdowns << "\ntot_crits=" << run_.tot_crits << "\n";
  for (auto& o : run_.perks) r << "perk=" << o.id << ":" << int(o.stacks) << "\n";
  std::ofstream m(P("rogue_meta.txt"));
  m << "legacy=" << meta_.legacy << "\nbest=" << meta_.best_legacy_score << "\n";
  for (auto& u : meta_.unlocked) m << "unlock=" << u << "\n";
}

void RogueRun::Load() {
  std::string line;
  std::ifstream r(P("rogue_career.txt"));
  while (std::getline(r, line)) {
    auto eq = line.find('='); if (eq == std::string::npos) continue;
    auto k = line.substr(0, eq), v = line.substr(eq + 1);
    auto I = [&] { return std::stoi(v); };
    if (k == "active") run_.active = v == "1";
    else if (k == "career") run_.career_id = v;
    else if (k == "seed") run_.seed = std::stoul(v);
    else if (k == "games") run_.games = I();
    else if (k == "season") run_.season = I();
    else if (k == "hype") run_.hype = I();
    else if (k == "notoriety") run_.notoriety = I();
    else if (k == "free") free_picks_ = std::max(0, I());  // repairs saves hit by BUG-1
    else if (k == "tot_goals") run_.tot_goals = I();
    else if (k == "tot_points") run_.tot_points = I();
    else if (k == "tot_kd") run_.tot_knockdowns = I();
    else if (k == "tot_crits") run_.tot_crits = I();
    else if (k == "perk") { auto c = v.find(':');
      run_.perks.push_back({v.substr(0, c), uint8_t(std::stoi(v.substr(c + 1)))}); }
  }
  std::ifstream m(P("rogue_meta.txt"));
  while (std::getline(m, line)) {
    auto eq = line.find('='); if (eq == std::string::npos) continue;
    auto k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "legacy") meta_.legacy = std::stoi(v);
    else if (k == "best") meta_.best_legacy_score = std::stoi(v);
    else if (k == "unlock") meta_.unlocked.push_back(v);
  }
  rng_.seed(run_.seed + run_.games * 7919u);
}

}  // namespace rogue
