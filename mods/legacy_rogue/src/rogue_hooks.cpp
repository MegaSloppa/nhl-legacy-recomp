// Legacy Rogue - guest hooks. Same strong-override pattern as src/diag_hooks.cpp:
//   REX_EXTERN(__imp__sub_X); extern "C" REX_FUNC(sub_X) { ...; __imp__sub_X(ctx, base); }
// Each hook compiles ONLY when its address is defined in addresses.h.
#include "generated/default/nhllegacy_init.h"
#include <random>
#include "rogue/addresses.h"
#include "rogue/fx.h"
#include "rogue/rogue_mod.h"
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"

#define ROGUE_CAT2(a, b) a##b
#define ROGUE_CAT(a, b) ROGUE_CAT2(a, b)
#define ROGUE_SUB(addr) ROGUE_CAT(sub_, addr)
#define ROGUE_IMP(addr) ROGUE_CAT(__imp__sub_, addr)

namespace {

inline uint32_t LoadU32(uint8_t* base, uint32_t va) {
  uint8_t* p = base + va + REX_PHYS_HOST_OFFSET(va);
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline void StoreF32(uint8_t* base, uint32_t va, float f) {
  uint32_t b; std::memcpy(&b, &f, 4);
  uint8_t* p = base + va + REX_PHYS_HOST_OFFSET(va);
  p[0] = uint8_t(b >> 24); p[1] = uint8_t(b >> 16); p[2] = uint8_t(b >> 8); p[3] = uint8_t(b);
}

// TODO(RE): decide how the BAP skater is identified (addresses.h).
bool IsBapSkater(uint8_t* base, uint32_t skater) {
#if defined(ROGUE_VA_BAP_PLAYER_PTR)
  return skater && LoadU32(base, ROGUE_VA_BAP_PLAYER_PTR) == skater;
#else
  (void)base; (void)skater; return false;
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
#if defined(ROGUE_ADDR_CHECK_RESOLVE)
REX_EXTERN(ROGUE_IMP(ROGUE_ADDR_CHECK_RESOLVE));
extern "C" REX_FUNC(ROGUE_SUB(ROGUE_ADDR_CHECK_RESOLVE)) {
  const uint32_t hitter = ctx.r3.u32, hittee = ctx.r4.u32;  // TODO(RE): confirm regs
  if (!rogue::InBapGame() || !IsBapSkater(base, hitter) || !rogue::Patcher()) {
    ROGUE_IMP(ROGUE_ADDR_CHECK_RESOLVE)(ctx, base);
    return;
  }
  // Per-player power: scale the GLOBAL hit tunables only while OUR check resolves.
  const float boost = 1.f + 1.5f * rogue::Run().Stacks("freight");
  rogue::ScopedTunable a(*rogue::Patcher(), "gHitTune_GlobalScale", boost * 2.f);
  rogue::ScopedTunable b(*rogue::Patcher(), "gMomentumPhysicsImpulse", boost);
  ROGUE_IMP(ROGUE_ADDR_CHECK_RESOLVE)(ctx, base);
  if (Roll(rogue::Run().EventParam(rogue::EventKind::kCheckAlwaysFloors))) {
    ForceFall(ctx, base, hittee);
    rogue::fx::Push(rogue::fx::Kind::kKnockdown);
  }
  // kCheckShockwave: TODO - iterate opponent skaters within p0 metres of hittee.
}
#endif

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
