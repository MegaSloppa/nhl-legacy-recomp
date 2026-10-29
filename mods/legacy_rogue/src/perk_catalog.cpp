// Perk catalog, rebuilt 2026-10-29 on tunables that game code is proven or likely to read.
// Sources: TUN-9 catalog (live value at N+32, stock values below) and tweak_readers (static
// readers of N+32). Each mod is applied PER STACK.
// IMPORTANT: every tunable here is GLOBAL (CPU players get it too) until per-player hooks
// exist (RE-1). Scope says so honestly. Only gSlapShotPowerScale is CONFIRMED in play (TUN-5b);
// the rest have a static reader but their in-game effect is UNVERIFIED (TUN-10).
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
  // Hitting
  {"freight", "Freight Train", "Checks hit much harder.", Rarity::kRare, Scope::kGlobal, 3,
   {{"gHitTune_GlobalScale", ModOp::kMul, 1.5f},                              // stock 1.0
    {"gHitPowerForPhysicsGlobalModifier", ModOp::kMul, 1.6f}}, {}},           // stock 0.5
  {"wrecking_ball", "Wrecking Ball", "Your checks always floor the target.",
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
