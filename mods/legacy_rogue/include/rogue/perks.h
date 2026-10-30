// Legacy Rogue - perk model. Two kinds of effect:
//   1. TUNABLE mods (works today, no RE): composed by TunablePatcher. GLOBAL: CPU players get
//      them too.
//   2. EVENT effects (need hooks, see addresses.h): run inside guest hooks for the BAP player
//      only. Live since 2026-10-30: kHitPowerBoost and kCheckAlwaysFloors (reaction choice hook,
//      rogue_hooks.cpp; needs the pro claimed in the panel each game).
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
  kCritSlapKnockdown,   // p0 = chance 0..1 per stack, p1 = min puck speed (needs RE-4)
  kCheckAlwaysFloors,   // p0 = chance 0..1 that a normal hit becomes a knockdown (LIVE)
  kCheckShockwave,      // p0 = radius (m): floors nearby opponents too (needs a force-fall)
  kGoalHeal,            // p0 = energy restored on goal (needs RE-5/6)
  kHitPowerBoost,       // p0 = extra hit power per stack, 0.15 = +15% (LIVE)
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
