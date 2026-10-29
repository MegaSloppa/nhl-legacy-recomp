#include "rogue/reactions.h"

namespace rogue {
// League reaction to Notoriety. Rebuilt 2026-10-29 on tunables with a static reader of the live
// value (TUN-9). Effects are UNVERIFIED in play and GLOBAL (they affect your team too) until
// per-player hooks exist. The old gTweakByGameStyle*CPU entries were removed: those tables had
// no effect in TUN-5b. gFightFreqChance is a tag-12 slider and is not supported by the patcher.
std::vector<Mod> ReactionMods(HeatTier t) {
  std::vector<Mod> m;
  const int n = int(t);
  if (n >= 1)  // Rising: opponents start paying attention
    m.push_back({"gChecking_AggressionMult", ModOp::kMul, 1.15f});         // stock 1.0
  if (n >= 2)  // Star: the league plays you physically
    m.push_back({"gChecking_AggressionMult", ModOp::kMul, 1.15f});
  if (n >= 3)  // Phenom: CPU goalies sharpen up
    m.push_back({"gAITuneuserorcpu_goalie_skill_levelCPU", ModOp::kMul, 1.08f});  // stock 1.06
  if (n >= 4) {  // Menace: hits hurt more, goalies sharper still
    m.push_back({"gHitInjuryChanceMultiplier", ModOp::kMul, 1.3f});
    m.push_back({"gAITuneuserorcpu_goalie_skill_levelCPU", ModOp::kMul, 1.08f});
  }
  if (n >= 5)  // Legend: the whole league is tuned to stop you
    m.push_back({"gChecking_AggressionMult", ModOp::kMul, 1.3f});
  return m;
}
}  // namespace rogue
