// Legacy Rogue - guest hooks. Same strong-override pattern as src/diag_hooks.cpp:
//   REX_EXTERN(__imp__sub_X); extern "C" REX_FUNC(sub_X) { ...; __imp__sub_X(ctx, base); }
// Each hook compiles ONLY when its address is defined in addresses.h.
#include "generated/default/nhllegacy_init.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <cstring>
#include <random>

#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "rogue/addresses.h"
#include "rogue/fx.h"
#include "rogue/probe.h"
#include "rogue/rogue_mod.h"
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"
#include "rogue/perks.h"

#define ROGUE_CAT2(a, b) a##b
#define ROGUE_CAT(a, b) ROGUE_CAT2(a, b)
#define ROGUE_SUB(addr) ROGUE_CAT(sub_, addr)
#define ROGUE_IMP(addr) ROGUE_CAT(__imp__sub_, addr)

namespace {

inline uint8_t* HostP(uint8_t* base, uint32_t va) { return base + va + REX_PHYS_HOST_OFFSET(va); }
inline uint32_t LoadU32(uint8_t* base, uint32_t va) {
  uint8_t* p = HostP(base, va);
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline uint8_t LoadU8(uint8_t* base, uint32_t va) { return *HostP(base, va); }
inline float LoadF32(uint8_t* base, uint32_t va) {
  const uint32_t b = LoadU32(base, va);
  float f; std::memcpy(&f, &b, 4); return f;
}
inline void StoreF32(uint8_t* base, uint32_t va, float f) {
  uint32_t b; std::memcpy(&b, &f, 4);
  uint8_t* p = HostP(base, va);
  p[0] = uint8_t(b >> 24); p[1] = uint8_t(b >> 16); p[2] = uint8_t(b >> 8); p[3] = uint8_t(b);
}

uint8_t* VBase() {
  auto* ks = rex::runtime::current_kernel_state();
  return (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
}
// Host-side check before reading guest memory that may not be mapped (image before load, freed heap).
bool HostReadable(const uint8_t* p, size_t n) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
  return p + n <= static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
}
bool GuestU32(uint8_t* base, uint32_t va, uint32_t* out) {
  if (!base || !HostReadable(HostP(base, va), 4)) return false;
  *out = LoadU32(base, va);
  return true;
}
bool GuestU8(uint8_t* base, uint32_t va, uint8_t* out) {
  if (!base || !HostReadable(HostP(base, va), 1)) return false;
  *out = LoadU8(base, va);
  return true;
}

// Round 9: the game's cached BAP player, gated (addresses.h ROGUE_VA_BAP_PLAYER_PTR): only while a
// BAP match is live, only a heap pointer to a skater object. 0 otherwise.
uint32_t ReadAutoBapPlayer(uint8_t* base) {
#if defined(ROGUE_VA_BAP_PLAYER_PTR) && defined(ROGUE_VA_MATCH_PTR) && defined(ROGUE_VA_BAP_MODE_FLAG) && \
    defined(ROGUE_VA_SKATER_CLASS)
  uint32_t match = 0, p = 0, cls = 0;
  uint8_t bap = 0;
  if (!GuestU32(base, ROGUE_VA_MATCH_PTR, &match) || !match) return 0;
  if (!GuestU8(base, ROGUE_VA_BAP_MODE_FLAG, &bap) || !bap) return 0;
  if (!GuestU32(base, ROGUE_VA_BAP_PLAYER_PTR, &p) || p < 0x40000000u) return 0;
  if (p >= 0x82000000u && p < 0x84000000u) return 0;  // image, not heap
  if (!GuestU32(base, p, &cls) || cls != ROGUE_VA_SKATER_CLASS) return 0;
  return p;
#else
  (void)base;
  return 0;
#endif
}

// The BAP skater: the player claimed in the panel this game (rogue::BapPlayer, an override), else
// the game's cached BAP player (round 9).
bool IsBapSkater(uint8_t* base, uint32_t skater) {
  if (const uint32_t me = rogue::BapPlayer()) return skater && skater == me;
#if defined(ROGUE_VA_BAP_PLAYER_PTR)
  return skater && ReadAutoBapPlayer(base) == skater;
#elif defined(ROGUE_OFF_SKATER_CONTROLLER)
  return skater >= 0x40000000u &&
         LoadU8(base, skater + ROGUE_OFF_SKATER_CONTROLLER) == ROGUE_HUMAN_CONTROLLER_VALUE;
#else
  (void)base; (void)skater; return false;
#endif
}

// Skater (not goalie): goalies take the knockdown reaction but do not fall.
bool IsSkaterObject(uint8_t* base, uint32_t player) {
#if defined(ROGUE_VA_SKATER_CLASS)
  return player >= 0x40000000u && LoadU32(base, player) == ROGUE_VA_SKATER_CLASS;
#else
  (void)base; (void)player; return false;
#endif
}

bool Roll(float chance) {
  std::uniform_real_distribution<float> d(0.f, 1.f);
  return d(rogue::Run().rng()) < chance;
}

// Knock a skater down. Preferred: call the game's own fall entry point with a
// saved/restored context. Fallback: crush its balance below the fall threshold.
void ForceFall(PPCContext& ctx, uint8_t* base, uint32_t skater) {
#if defined(ROGUE_ADDR_FORCE_FALL)
  PPCContext saved = ctx;
  ctx.r3.u64 = skater;            // TODO(RE): confirm calling convention / args
  ROGUE_SUB(ROGUE_ADDR_FORCE_FALL)(ctx, base);
  ctx = saved;
#elif defined(ROGUE_OFF_SKATER_BALANCE)
  (void)ctx;
  StoreF32(base, skater + ROGUE_OFF_SKATER_BALANCE, -1.0f);
#else
  (void)ctx; (void)base; (void)skater;
#endif
  ++rogue::Run().run().live.knockdowns;
}
}  // namespace

// ---------------------------------------------------------------- reads for rogue_mod.cpp (round 9)
namespace rogue {
uint32_t AutoBapPlayer() { return ReadAutoBapPlayer(VBase()); }

GuestGame PollGuestGame() {
  GuestGame g;
#if defined(ROGUE_VA_GAME_STATE) && defined(ROGUE_VA_MATCH_PTR) && defined(ROGUE_VA_BAP_MODE_FLAG)
  uint8_t* b = VBase();
  uint32_t st = 0, match = 0;
  uint8_t bap = 0;
  if (!GuestU32(b, ROGUE_VA_GAME_STATE, &st) || !GuestU32(b, ROGUE_VA_MATCH_PTR, &match) ||
      !GuestU8(b, ROGUE_VA_BAP_MODE_FLAG, &bap))
    return g;
  g.valid = true;
  g.state = int32_t(st);
  g.match = match;
  g.bap = bap != 0;
#endif
  return g;
}
}  // namespace rogue

// ---------------------------------------------------------------- lifecycle
#if defined(ROGUE_ADDR_BAP_GAME_START)
REX_EXTERN(ROGUE_IMP(ROGUE_ADDR_BAP_GAME_START));
extern "C" REX_FUNC(ROGUE_SUB(ROGUE_ADDR_BAP_GAME_START)) {
  ROGUE_IMP(ROGUE_ADDR_BAP_GAME_START)(ctx, base);
  rogue::OnBapGameStart();
}
#endif

#if defined(ROGUE_ADDR_BAP_GAME_END)
REX_EXTERN(ROGUE_IMP(ROGUE_ADDR_BAP_GAME_END));
extern "C" REX_FUNC(ROGUE_SUB(ROGUE_ADDR_BAP_GAME_END)) {
  ROGUE_IMP(ROGUE_ADDR_BAP_GAME_END)(ctx, base);
  rogue::GameStats g;  // TODO(RE): read final score + BAP player line from game state
  rogue::OnBapGameEnd(g);
}
#endif

// ---------------------------------------------------------------- hitting
// Hit contact (CONFIRMED, addresses.h). Pass-through. Every call is forwarded to the probe hit
// watch (NHL_ROGUE_PROBE=1). During a BAP game it counts the BAP player's hits and knockdowns of
// skaters (not goalies) into run().live, once the player is claimed in the panel.
#if defined(ROGUE_ADDR_HIT_CONTACT)
// Literal names (not ROGUE_SUB) so REX_FUNC never sees an unexpanded macro argument. Keep in
// sync with ROGUE_ADDR_HIT_CONTACT.
REX_EXTERN(__imp__sub_833E0BF8);
extern "C" REX_FUNC(sub_833E0BF8) {
  // Read the arguments first: the original clobbers the registers.
  const uint32_t victim = ctx.r3.u32, hitter = ctx.r4.u32, reaction = ctx.r6.u32;
  const float power = ctx.r5.u32 >= 0x40000000u ? LoadF32(base, ctx.r5.u32) : 0.f;
  const unsigned long long lr = (unsigned long long)ctx.lr;
  __imp__sub_833E0BF8(ctx, base);
  rogue::NoteContact(victim, hitter);  // for the panel claim buttons
  if (rogue::probe::Enabled()) rogue::probe::ContactSeen(victim, hitter, reaction, power, lr);
  if (!rogue::InBapGame() || !IsBapSkater(base, hitter) || IsBapSkater(base, victim)) return;
  // One collision can call this 1-3 times; the same victim within 0.5 s is the same hit.
  static uint32_t last_victim = 0;
  static std::chrono::steady_clock::time_point last_time{};
  const auto now = std::chrono::steady_clock::now();
  auto& live = rogue::Run().run().live;
  if (!(victim == last_victim && now - last_time < std::chrono::milliseconds(500))) ++live.hits;
  last_victim = victim;
  last_time = now;
  if (reaction == ROGUE_VA_REACT_KNOCKDOWN && IsSkaterObject(base, victim)) {
    ++live.knockdowns;
    rogue::fx::Push(rogue::fx::Kind::kKnockdown);
  }
}
#endif

// ---------------------------------------------------------------- hit outcome (perks)
// The reaction of a body contact is chosen from its POWER (CONFIRMED round 6, re-findings.md):
// sub_83429560 (dispatcher: r3 victim, r4 hitter) computes power, then sub_833B9028(f1 = power)
// returns the first entry of the reaction table whose threshold (+4) is >= power:
// entry 0..2 = no hit reaction, entry 3 = hit, entry 4 (above entry 3's threshold) = knockdown.
// Hit perks change the power that sub_833B9028 sees, only for the BAP player's hits on others,
// only during a BAP game. Nothing else changes (physics, ratings, the other players' hits).
#if defined(ROGUE_ADDR_REACTION_DISPATCH) && defined(ROGUE_ADDR_REACTION_CHOICE)
namespace {
thread_local bool t_dispatch = false;  // inside sub_83429560 on this thread
thread_local uint32_t t_hitter = 0, t_victim = 0;

// Thresholds read from the live table (falls back to the round 6 values if unreadable).
float TableThreshold(uint8_t* base, int entry, float fallback) {
  const uint32_t va = ROGUE_VA_REACTION_TABLE + 28u * uint32_t(entry) + 4u;
  const float f = LoadF32(base, va);
  return (f > 0.f && f < 10.f) ? f : fallback;
}

// Perks on the BAP player's hit. p = power the game computed.
double BapHitPower(uint8_t* base, double p) {
  // Freight Train: +p0 per stack (0.15 = +15%).
  const float boost = rogue::Run().EventParam(rogue::EventKind::kHitPowerBoost);
  if (boost > 0.f) p *= 1.0 + double(boost);
  // Wrecking Ball: a hit that would be a normal hit becomes a knockdown (chance p0 per stack).
  const float floors = rogue::Run().EventParam(rogue::EventKind::kCheckAlwaysFloors);
  if (floors > 0.f) {
    const double hit_min = TableThreshold(base, 2, 0.5f), kd_min = TableThreshold(base, 3, 0.75f);
    if (p > hit_min && p <= kd_min && Roll(floors)) p = kd_min + 0.02;
  }
  return p;
}

// Round 10 behaviour test (probe panel): the pro's light contacts on OPPOSING skaters (power in
// [floor, hit threshold]) become normal hits. Round 9: of 60 contact episodes the pro had with
// opponents, 11 became hits; 27 peaked between 0.2 and 0.5. Team byte = player +48 (S1).
constexpr uint32_t kOffTeam = 48;
double ContactFloor(uint8_t* base, double p) {
  const float floor_min = rogue::probe::ContactFloorTest();
  if (floor_min <= 0.f || !IsSkaterObject(base, t_victim)) return p;
  if (LoadU8(base, t_victim + kOffTeam) == LoadU8(base, t_hitter + kOffTeam)) return p;
  const double hit_min = TableThreshold(base, 2, 0.5f);
  if (p < double(floor_min) || p > hit_min) return p;
  rogue::probe::NoteContactFloorPromoted();
  return hit_min + 0.02;
}
}  // namespace

// Literal names (not ROGUE_SUB) on purpose; keep in sync with addresses.h.
REX_EXTERN(__imp__sub_83429560);
extern "C" REX_FUNC(sub_83429560) {
  const bool prev = t_dispatch;
  const uint32_t prev_h = t_hitter, prev_v = t_victim;
  t_dispatch = true;
  t_victim = ctx.r3.u32;
  t_hitter = ctx.r4.u32;
  const bool probe = rogue::probe::Enabled();
  if (probe) rogue::probe::ReactionBegin(t_victim, t_hitter);
  __imp__sub_83429560(ctx, base);
  if (probe) rogue::probe::ReactionEnd(ctx.r3.u32);
  t_dispatch = prev;
  t_hitter = prev_h;
  t_victim = prev_v;
}

REX_EXTERN(__imp__sub_833B9028);
extern "C" REX_FUNC(sub_833B9028) {
  const double in = ctx.f1.f64;
  const unsigned long long lr = (unsigned long long)ctx.lr;
  double used = in;
  if (t_dispatch && rogue::InBapGame() && t_hitter && t_victim != t_hitter &&
      IsBapSkater(base, t_hitter) && !IsBapSkater(base, t_victim))
    used = ContactFloor(base, BapHitPower(base, used));
  const bool probe = rogue::probe::Enabled();
  if (probe) used = rogue::probe::ReactionPowerIn(used);  // diagnostic power test, on top
  ctx.f1.f64 = used;
  __imp__sub_833B9028(ctx, base);
  if (probe) rogue::probe::ReactionChosen(in, used, ctx.r3.u32, lr);
}
#endif

// ---------------------------------------------------------------- penalties (perk Untouchable)
// Penalty decision sub_83487DC0(official, rec, contact): rec +4 offender, +8 victim, +12 contact type;
// returns r3 = 1 when the infraction is called. Runs at the infraction, before the delayed-penalty
// arm and the whistle. Forcing 0 for the pro cancels the call (CONFIRMED round 9, re-findings.md).
// Moved here from rogue_static_probe.cpp 2026-11-03; the probe logs through probe::PenaltyDecided.
// Literal name on purpose (one override per function in the whole program).
REX_EXTERN(__imp__sub_83487DC0);
extern "C" REX_FUNC(sub_83487DC0) {
  const uint32_t rec = ctx.r4.u32;
  uint32_t off = 0, vic = 0, type = 0;
  if (rec >= 0x40000000u && !(rec >= 0x82000000u && rec < 0x84000000u) && GuestU32(base, rec + 4, &off)) {
    GuestU32(base, rec + 8, &vic);
    GuestU32(base, rec + 12, &type);
  }
  const unsigned long long lr = (unsigned long long)ctx.lr;
  __imp__sub_83487DC0(ctx, base);
  bool blocked = false;
  if ((ctx.r3.u32 & 0xFFu) == 1u && off) {
    bool perk = false;
    if (rogue::InBapGame() && IsBapSkater(base, off)) {
      const float chance = rogue::Run().EventParam(rogue::EventKind::kPenaltyImmunity);
      perk = chance > 0.f && (chance >= 1.f || Roll(chance));
    }
    const bool test = rogue::probe::PenaltyImmunityTest() && off == rogue::ProPlayer();
    if (perk || test) {
      ctx.r3.u64 = 0;
      blocked = true;
      if (perk) {  // feedback, at most once a second (stick infractions are decided twice)
        static std::chrono::steady_clock::time_point last{};
        const auto now = std::chrono::steady_clock::now();
        if (now - last > std::chrono::seconds(1)) rogue::fx::Push(rogue::fx::Kind::kMilestone, "NO CALL");
        last = now;
      }
    }
  }
  if (rogue::probe::Enabled())
    rogue::probe::PenaltyDecided(off, vic, type, ctx.r3.u32 & 0xFFu, blocked, lr);
}

// ---------------------------------------------------------------- shooting
#if defined(ROGUE_ADDR_PUCK_BODY_HIT)
REX_EXTERN(ROGUE_IMP(ROGUE_ADDR_PUCK_BODY_HIT));
extern "C" REX_FUNC(ROGUE_SUB(ROGUE_ADDR_PUCK_BODY_HIT)) {
  const uint32_t struck = ctx.r4.u32;   // TODO(RE): confirm which reg is the skater
  const uint32_t shooter = 0;           // TODO(RE): puck last-touch skater
  ROGUE_IMP(ROGUE_ADDR_PUCK_BODY_HIT)(ctx, base);
  if (!rogue::InBapGame() || !IsBapSkater(base, shooter)) return;
  // TODO(RE): only on slap shots + puck speed >= p1 of the perk.
  if (Roll(rogue::Run().EventParam(rogue::EventKind::kCritSlapKnockdown))) {
    ForceFall(ctx, base, struck);
    ++rogue::Run().run().live.crit_slaps;
    rogue::fx::Push(rogue::fx::Kind::kCritSlap);
  }
}
#endif
