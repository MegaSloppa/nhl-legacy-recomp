// Legacy Rogue - "the league reacts to you". Notoriety tiers map to tunable
// layers that make the otherwise-realistic game push back against a superhuman
// player. Tier A uses only existing tunables (CPU-side variants, so YOUR perks
// are untouched). Tier B (needs RE) is listed in docs/design.md: shadowing
// defenders, targeted goons, media/contract/trade reactions, BAP grade bonuses.
#pragma once
#include <vector>
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"

namespace rogue {
// Tunable mods active at a given heat tier (cumulative: tier N includes 1..N).
std::vector<Mod> ReactionMods(HeatTier t);
}  // namespace rogue
