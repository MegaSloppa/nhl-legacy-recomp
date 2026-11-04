// Perk catalog, rebuilt 2026-10-29 on tunables that game code is proven or likely to read.
// Sources: TUN-9 catalog (live value at N+32, stock values below) and tweak_readers (static
// readers of N+32). Each mod is applied PER STACK.
// Round 25 (2026-11-04): Cannon, Quick Release, Sniper Scope and Jet Skates are kProTunable: their tunables are
// raised only while the shot (sub_833E87E0 / sub_833ECF68) or speed boost (sub_828B9258) code runs with the pro in
// its arguments (rogue_hooks.cpp). The BAP line boosts (Iron Lungs, Captain's Aura) and curses stay global by design.
// OLD NOTE: every TUNABLE perk here was GLOBAL (CPU players got it too). Only gSlapShotPowerScale
// is CONFIRMED in play (TUN-5b); the rest have a static reader but their in-game effect is
// UNVERIFIED. Event perks (Freight Train, Wrecking Ball since 2026-10-30) act on the BAP player's
// hits only, through the reaction choice hook (rogue_hooks.cpp), once the pro is claimed.
#include "rogue/perks.h"
#include <cstring>

namespace rogue {
static const std::vector<PerkDef> kPerks = {
  // Shooting
  {"cannon", "Cannon", "Slap shots hit much harder.", Rarity::kCommon, Scope::kProTunable, 3,
   {{"gSlapShotPowerScale", ModOp::kMul, 1.6f}}, {}},                        // stock 1.0, CONFIRMED
  {"quick_release", "Quick Release", "Wrist shots fly harder.", Rarity::kCommon, Scope::kProTunable, 3,
   {{"gWristShotPowerMultiplier", ModOp::kMul, 1.4f}}, {}},                  // stock 0.78
  {"sniper", "Sniper Scope", "Slap shot, backhand and redirect accuracy up.", Rarity::kCommon,
   Scope::kProTunable, 3, {{"gSlapShotAccuracyScale", ModOp::kMul, 1.2f},          // stock 0.85
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
  // Contact perks (LIVE 2026-11-03): act on every contact the game evaluates between the pro and an
  // opposing skater (reaction choice hook). Round 10 / 17 contact floor test: 60 contact episodes a game,
  // most of them below the hit threshold.
  {"heavy_shoulders", "Heavy Shoulders", "Light bumps stagger opponents. More stacks, lighter bumps.",
   Rarity::kRare, Scope::kBapScoped, 3, {}, {{EventKind::kContactFloor, 1.0f, 0.f}}},
  {"bulldozer", "Bulldozer", "Anything you touch goes down.", Rarity::kLegendary, Scope::kBapScoped, 1,
   {}, {{EventKind::kContactKnockdown, 0.02f, 0.f}}},
  {"shockwave", "Shockwave", "Big hits knock down nearby opponents. (Not working yet: parked 2026-11-03.)", Rarity::kLegendary,
   Scope::kBapScoped, 1, {}, {{EventKind::kCheckShockwave, 2.5f, 0.f}}},
  // Rules (event perk, LIVE 2026-11-03): the penalty decision sub_83487DC0 is cancelled when the
  // offender is the pro. Round 9 test: 6 / 6 infractions uncalled, no side effects seen.
  {"untouchable", "Untouchable", "The refs never call a penalty on you.", Rarity::kLegendary,
   Scope::kBapScoped, 1, {}, {{EventKind::kPenaltyImmunity, 1.0f, 0.f}}},
  // Skating / line
  // Round 27: your skating ratings (speed id 0, acceleration id 1) through the rating override; the old speed
  // boost tunables were read by a function that never ran for the pro (round 26).
  // Round 28 (owner): sprint-only again. Applied while the sprint button is held (controller read by the mod;
  // in BAP the human controls only the pro). Numbers = the original speed boost scales per stack.
  {"jets", "Jet Skates", "Your sprint is faster and accelerates harder.", Rarity::kCommon,
   Scope::kBapScoped, 3, {}, {{EventKind::kSprintRatingBoost, 0.f, 1.12f}, {EventKind::kSprintRatingBoost, 1.f, 1.25f}}},
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
