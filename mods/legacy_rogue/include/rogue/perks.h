// Legacy Rogue - perk model. Two kinds of effect:
//   1. TUNABLE mods (works today, no RE): composed by TunablePatcher.
//   2. EVENT effects (need hooks, see addresses.h): run inside guest hooks,
//      e.g. "crit slapshot" = on puck-hits-body, if shooter==BAP player and
//      roll < chance -> force the struck skater into a fall.
#pragma once
#include <cstdint>
#include <vector>
#include "rogue/tunable_patch.h"

namespace rogue {

enum class Rarity : uint8_t { kCommon, kRare, kEpic, kLegendary };
enum class Scope : uint8_t {
  kHumanOnly,   // uses the gTweakByGameStyle*Level (human) vs *LevelCPU split
  kGlobal,      // affects every skater - arena/rule perks and curses
  kBapScoped,   // applied via ScopedTunable / event code inside a BAP-player hook
};

enum class EventKind : uint8_t {
  kNone,
  kCritSlapKnockdown,   // p0 = chance 0..1 per stack, p1 = min puck speed
  kCheckAlwaysFloors,   // p0 = chance 0..1
  kCheckShockwave,      // p0 = radius (m): floors nearby opponents too
  kGoalHeal,            // p0 = energy restored on goal
};

struct EventEffect { EventKind kind = EventKind::kNone; float p0 = 0, p1 = 0; };

struct PerkDef {
  const char* id;
  const char* name;
  const char* blurb;
  Rarity rarity;
  Scope scope;
  uint8_t max_stacks;
  std::vector<Mod> mods;          // persistent tunable layer (per stack)
  std::vector<EventEffect> events;
  bool is_curse = false;
};

const std::vector<PerkDef>& Catalog();
const PerkDef* FindPerk(const char* id);

}  // namespace rogue
