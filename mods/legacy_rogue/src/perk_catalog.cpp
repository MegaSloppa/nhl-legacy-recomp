// Starter catalog. Every tunable name below exists in docs/tunables_schema.tsv.
// Values are first guesses - tune live in the Engine Tunables overlay, then
// copy the numbers here. GameStyle *Level tunables are ints (ladder steps):
// check their stock range in docs/tunable_values_runtime.txt first.
#include "rogue/perks.h"
#include <cstring>

namespace rogue {
static const std::vector<PerkDef> kPerks = {
  // Shooting
  {"cannon", "Cannon", "Slap shots hit harder.", Rarity::kCommon, Scope::kHumanOnly, 3,
   {{"gTweakByGameStyleSlapShotPowerLevel", ModOp::kAdd, 1}}, {}},
  {"sniper", "Sniper Scope", "Wrist shot and one-timer accuracy up.", Rarity::kCommon,
   Scope::kHumanOnly, 3, {{"gTweakByGameStyleWristShotAccuracyLevel", ModOp::kAdd, 1},
                          {"gTweakByGameStyleOneTimerAccuracyLevel", ModOp::kAdd, 1}}, {}},
  {"crit_slap", "Critical Slapshot", "Slap shots that hit a skater can floor them.",
   Rarity::kEpic, Scope::kBapScoped, 3, {}, {{EventKind::kCritSlapKnockdown, 0.25f, 0.f}}},
  // Hitting
  {"freight", "Freight Train", "Your hit power goes up a tier.", Rarity::kRare,
   Scope::kHumanOnly, 3, {{"gTweakByGameStyleHitPowerLevel", ModOp::kAdd, 1}}, {}},
  {"wrecking_ball", "Wrecking Ball", "Your checks always floor the target.",
   Rarity::kLegendary, Scope::kBapScoped, 1,
   {}, {{EventKind::kCheckAlwaysFloors, 1.0f, 0.f}}},
  {"shockwave", "Shockwave", "Big hits knock down nearby opponents.", Rarity::kLegendary,
   Scope::kBapScoped, 1, {}, {{EventKind::kCheckShockwave, 2.5f, 0.f}}},
  // Skating / line
  {"jets", "Jet Skates", "Acceleration up.", Rarity::kCommon, Scope::kHumanOnly, 3,
   {{"gTweakByGameStyleAccelerationLevel", ModOp::kAdd, 1}}, {}},
  {"iron_lungs", "Iron Lungs", "Your line gets bonus energy.", Rarity::kRare,
   Scope::kHumanOnly, 2, {{"gBeAPro_LineEnergyBoost", ModOp::kAdd, 10}}, {}},
  {"captain", "Captain's Aura", "Linemates play above their ratings.", Rarity::kEpic,
   Scope::kHumanOnly, 2, {{"gBeAPro_LineSkillBoost", ModOp::kAdd, 5}}, {}},
  // Curses (trade-offs for currency / better drafts)
  {"glass_bones", "Glass Bones", "Injuries far more likely, for everyone.", Rarity::kCommon,
   Scope::kGlobal, 1, {{"gTweakByGameStyleInjuryOccurrenceLevel", ModOp::kAdd, 2},
                       {"gTweakByGameStyleInjuryOccurrenceLevelCPU", ModOp::kAdd, 2}}, {}, true},
  {"goon_squad", "Goon Squad", "CPU hits harder and hunts you.", Rarity::kCommon,
   Scope::kGlobal, 2, {{"gTweakByGameStyleHitPowerLevelCPU", ModOp::kAdd, 1},
                       {"gTweakByGameStyleAggressionLevelCPU", ModOp::kAdd, 1}}, {}, true},
};

const std::vector<PerkDef>& Catalog() { return kPerks; }
const PerkDef* FindPerk(const char* id) {
  for (auto& p : kPerks) if (!std::strcmp(p.id, id)) return &p;
  return nullptr;
}
}  // namespace rogue
