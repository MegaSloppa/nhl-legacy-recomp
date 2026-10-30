// Perk catalog, rebuilt 2026-10-29 on tunables that game code is proven or likely to read.
// Sources: TUN-9 catalog (live value at N+32, stock values below) and tweak_readers (static
// readers of N+32). Each mod is applied PER STACK.
// IMPORTANT: every TUNABLE perk here is GLOBAL (CPU players get it too). Only gSlapShotPowerScale
// is CONFIRMED in play (TUN-5b); the rest have a static reader but their in-game effect is
// UNVERIFIED. Event perks (Freight Train, Wrecking Ball since 2026-10-30) act on the BAP player's
// hits only, through the reaction choice hook (rogue_hooks.cpp), once the pro is claimed.
#include "rogue/perks.h"
#include <cstring>

namespace rogue {
static const std::vector<PerkDef> kPerks = {
  // Shooting
  {"cannon", "Cannon", "Slap shots hit much harder.", Rarity::kCommon, Scope::kGlobal, 3,
   {{"gSlapShotPowerScale", ModOp::kMul, 1.6f}}, {}},                        // stock 1.0, CONFIRMED
  {"quick_release", "Quick Release", "Wrist shots fly harder.", Rarity::kCommon, Scope::kGlobal, 3,
   {{"gWristShotPowerMultiplier", ModOp::kMul, 1.4f}}, {}},                  // stock 0.78
  {"sniper", "Sniper Scope", "Slap shot, backhand and redirect accuracy up.", Rarity::kCommon,
   Scope::kGlobal, 3, {{"gSlapShotAccuracyScale", ModOp::kMul, 1.2f},          // stock 0.85
                       {"gBackhandShotAccuracyMultiplier", ModOp::kMul, 1.3f}, // stock 0.7
                       {"gRedirectShotAccuracyMultiplier", ModOp::kMul, 1.2f}}, {}},  // stock 1.0
  {"crit_slap", "Critical Slapshot", "Slap shots that hit a skater can floor them.",
   Rarity::kEpic, Scope::kBapScoped, 3, {}, {{EventKind::kCritSlapKnockdown, 0.25f, 0.f}}},
  // Hitting (event perks: your hits only, LIVE 2026-10-30). Round 6 power test: x1.15 turned 2 of
  // 10 hits into knockdowns, x1.3 5 of 6 normal hits; knockdown above power 0.75, hit above 0.5.
  {"freight", "Freight Train", "Your checks hit 15% harder per stack.", Rarity::kRare,
   Scope::kBapScoped, 3, {}, {{EventKind::kHitPowerBoost, 0.15f, 0.f}}},
  {"wrecking_ball", "Wrecking Ball", "Every check you land floors the target.",
   Rarity::kLegendary, Scope::kBapScoped, 1,
   {}, {{EventKind::kCheckAlwaysFloors, 1.0f, 0.f}}},
  {"shockwave", "Shockwave", "Big hits knock down nearby opponents.", Rarity::kLegendary,
   Scope::kBapScoped, 1, {}, {{EventKind::kCheckShockwave, 2.5f, 0.f}}},
  // Skating / line
  {"jets", "Jet Skates", "Speed burst is faster and accelerates harder.", Rarity::kCommon,
   Scope::kGlobal, 3, {{"gSpeedBoostEnergyMaxSpeedScale", ModOp::kMul, 1.12f},  // stock 1.0
                       {"gSpeedBoostEnergyMaxAccelScale", ModOp::kMul, 1.25f}}, {}},  // stock 1.0
  {"iron_lungs", "Iron Lungs", "Your line gets bonus energy.", Rarity::kRare,
   Scope::kGlobal, 2, {{"gBeAPro_LineEnergyBoost", ModOp::kAdd, 0.2f}}, {}},  // stock 0.2, range 0..1
  {"captain", "Captain's Aura", "Linemates play above their ratings.", Rarity::kEpic,
   Scope::kGlobal, 2, {{"gBeAPro_LineSkillBoost", ModOp::kAdd, 0.5f}}, {}},   // stock 1.0, meaning unproven
  // Curses (trade-offs for currency / better drafts)
  {"glass_bones", "Glass Bones", "Injuries from hits far more likely, for everyone.",
   Rarity::kCommon, Scope::kGlobal, 1, {{"gHitInjuryChanceMultiplier", ModOp::kMul, 2.5f}}, {},  // stock 1.0
   true},
  {"goon_squad", "Goon Squad", "Everyone checks far more aggressively.", Rarity::kCommon,
   Scope::kGlobal, 2, {{"gChecking_AggressionMult", ModOp::kMul, 1.5f}}, {}, true},  // stock 1.0
};

const std::vector<PerkDef>& Catalog() { return kPerks; }
const PerkDef* FindPerk(const char* id) {
  for (auto& p : kPerks) if (!std::strcmp(p.id, id)) return &p;
  return nullptr;
}
}  // namespace rogue
