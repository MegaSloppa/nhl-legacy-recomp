#include "rogue/reactions.h"

namespace rogue {
// All names exist in docs/tunables_schema.tsv. Values are first guesses; tune live.
// Ladder-step *Level tunables: confirm stock ranges in docs/tunable_values_runtime.txt.
std::vector<Mod> ReactionMods(HeatTier t) {
  std::vector<Mod> m;
  const int n = int(t);
  if (n >= 1)  // Rising: opponents start paying attention
    m.push_back({"gChecking_AggressionMult", ModOp::kMul, 1.15f});
  if (n >= 2) {  // Star: CPU plays you physically
    m.push_back({"gTweakByGameStyleAggressionLevelCPU", ModOp::kAdd, 1});
    m.push_back({"gFightFreqChance", ModOp::kMul, 1.5f});
  }
  if (n >= 3) {  // Phenom: CPU hits harder, blocks more shots
    m.push_back({"gTweakByGameStyleHitPowerLevelCPU", ModOp::kAdd, 1});
    m.push_back({"gTweakByGameStyleShotBlockingLevelCPU", ModOp::kAdd, 1});
  }
  if (n >= 4) {  // Menace: goalies get sharper, enforcers come out
    m.push_back({"gTweakByGameStyleGoalieReactionLevelCPU", ModOp::kAdd, 1});
    m.push_back({"gFightFreqChance", ModOp::kMul, 1.5f});
  }
  if (n >= 5)  // Legend: the whole league is tuned to stop you
    m.push_back({"gChecking_AggressionMult", ModOp::kMul, 1.3f});
  return m;
}
}  // namespace rogue
