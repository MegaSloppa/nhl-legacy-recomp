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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <random>
#include <thread>
#include <cstdarg>

#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "rogue/addresses.h"
#include "rogue/fx.h"
#include "rogue/probe.h"
#include "rogue/rogue_mod.h"
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"
#include "rogue/tweak_index.h"
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
inline void StoreU32(uint8_t* base, uint32_t va, uint32_t b) {
  uint8_t* p = HostP(base, va);
  p[0] = uint8_t(b >> 24); p[1] = uint8_t(b >> 16); p[2] = uint8_t(b >> 8); p[3] = uint8_t(b);
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

// ---- RE-5 box score (round 27, 2026-11-04) ----------------------------------------------------------
// Stat tables (20 x 86-byte records per team) are heap objects; each team's table can exist in several copies
// (round 26: 3 + 3). No fixed path to them yet. So:
//  - ONE background search per game, in the first cut-scene (STORYTELLER >= 2 s) AFTER A PERIOD END (period-end
//    hook in rogue_static_probe.cpp calls NotePeriodEnd), so it lands in the 1st intermission, not in a penalty
//    cut-scene (round 26 owner); max 3 tries, else once at game over.
//  - Round 27 speed-up: the goalie record is rejected on cheap fields first (TOI, shots against >= 1, period-1
//    shots >= 1) and VirtualQuery is skipped when the table lies inside the chunk (round 26 spent 30-41 s mostly
//    on candidates of all-zero memory); resident pages only.
//  - Copies with the same content form a group (= one team). The pro's record = the (group, slot) whose TOI ticks
//    while he is on the ice; within a group the copy with the highest TOI is read (the live one). Fallback slot
//    idx % 20.
//  - GF = the other group's goalie GA (skater goals miss simulated goals); GA = own goalie GA. Goalie slot 19 is
//    validated too (round 26: a copy's slot 19 read -9249).
namespace {
constexpr int kMaxStatTables = 8;
constexpr uint32_t kRec = ROGUE_MATCH_STATS_STRIDE;
std::atomic<uint32_t> g_tab[kMaxStatTables];  // guest address of slot 0's +/- word
std::atomic<int> g_grp[kMaxStatTables];
std::atomic<int> g_ntab{0};
std::atomic<bool> g_scanning{false};
std::atomic<uint32_t> g_scan_ms{0};
std::atomic<int> g_period_ends{0};
int g_tries = 0;
bool g_story = false;
std::chrono::steady_clock::time_point g_game_t0, g_story_t0, g_next_sample;
int g_score[kMaxStatTables][18];
int g_last_toi[kMaxStatTables][18];
bool g_have_last = false;
int g_on_samples = 0;

inline int S16(const uint8_t* h) { return int(int16_t(uint16_t((uint16_t(h[0]) << 8) | h[1]))); }
bool GoalieSig(const uint8_t* h, int min_sa) {  // h -> +/- word of goalie slot 18
  const int sa = S16(h + 2);
  if (sa < min_sa || sa > 90) return false;
  const int s1 = S16(h + 4), s2 = S16(h + 6), s3 = S16(h + 8);
  if (s1 < 0 || s1 > 40 || s2 < 0 || s2 > 40 || s3 < 0 || s3 > 40) return false;
  const int ps = s1 + s2 + s3;
  if (sa < ps || sa - ps > 20) return false;
  const int ga = S16(h - 8), g1 = S16(h - 6), g2 = S16(h - 4), g3 = S16(h - 2);
  if (ga < 0 || ga > 20 || g1 < 0 || g1 > 10 || g2 < 0 || g2 > 10 || g3 < 0 || g3 > 10) return false;
  const int pg = g1 + g2 + g3;
  return ga >= pg && ga - pg <= 3;
}
bool SkatersSig(const uint8_t* p0) {
  int with_toi = 0;
  for (uint32_t s = 0; s < 18; ++s) {
    const uint8_t* r = p0 + kRec * s;
    const int g = S16(r + ROGUE_STAT_GOALS), a = S16(r + ROGUE_STAT_ASSISTS), pim = S16(r + ROGUE_STAT_PIM);
    const int pm = S16(r), sh = S16(r + ROGUE_STAT_SOG), hi = S16(r + ROGUE_STAT_HITS), toi = S16(r + ROGUE_STAT_TOI);
    if (g < 0 || g > 10 || a < 0 || a > 10 || pim < 0 || pim > 60 || pm < -10 || pm > 10 || sh < 0 || sh > 30 ||
        hi < 0 || hi > 40 || toi < 0 || toi > 4000)
      return false;
    with_toi += toi > 0;
  }
  return with_toi >= 8;
}
bool Goalie19Ok(const uint8_t* p0) {
  const uint8_t* g = p0 + kRec * 19;
  const int ga = S16(g - 8), sa = S16(g + 2), toi = S16(g + ROGUE_STAT_TOI);
  return ga >= 0 && ga <= 20 && sa >= 0 && sa <= 90 && toi >= 0 && toi <= 5400;
}
bool TableReadable(uint8_t* base, uint32_t p0) {
  return p0 > 0x100u && HostReadable(HostP(base, p0 - 0x80u), kRec * 20u + 0x80u);
}
bool TableBodyOk(const uint8_t* h) {
  const int gt = S16(h + kRec * 18 + ROGUE_STAT_TOI);
  return gt >= 30 && gt <= 5400 && GoalieSig(h + kRec * 18, 0) && SkatersSig(h) && Goalie19Ok(h);
}
bool TableOk(uint8_t* base, uint32_t p0) { return TableReadable(base, p0) && TableBodyOk(HostP(base, p0)); }
int TotalToi(const uint8_t* h) {
  int t = 0;
  for (uint32_t s = 0; s < 20; ++s) t += S16(h + kRec * s + ROGUE_STAT_TOI);
  return t;
}
void ProbeLine(const char* fmt, ...) {
  if (!rogue::probe::Enabled()) return;
  char line[300];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  rogue::probe::StaticNote(line);
}

struct WsExInfo { void* va; uintptr_t attr; };  // PSAPI_WORKING_SET_EX_INFORMATION; attr bit 0 = valid
using QueryWsExFn = BOOL(WINAPI*)(HANDLE, void*, DWORD);
QueryWsExFn WsExFn() {
  static QueryWsExFn fn = [] {
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    return k ? reinterpret_cast<QueryWsExFn>(reinterpret_cast<void*>(GetProcAddress(k, "K32QueryWorkingSetEx"))) : nullptr;
  }();
  return fn;
}
uint32_t ResidentPages(const uint8_t* h) {  // bit i = 4 KB page i of the 64 KB chunk is resident
  QueryWsExFn fn = WsExFn();
  if (!fn) return 0xFFFFu;
  WsExInfo info[16];
  for (int p = 0; p < 16; ++p) { info[p].va = const_cast<uint8_t*>(h + 0x1000 * p); info[p].attr = 0; }
  if (!fn(GetCurrentProcess(), info, DWORD(sizeof(info)))) return 0xFFFFu;
  uint32_t m = 0;
  for (int p = 0; p < 16; ++p) m |= uint32_t(info[p].attr & 1u) << p;
  return m;
}

void ScanTables(uint8_t* base) {
  const auto t0 = std::chrono::steady_clock::now();
  uint32_t hits[kMaxStatTables];
  int grp[kMaxStatTables];
  int key[kMaxStatTables][3];
  int n = 0, ngrp = 0;
  unsigned read_kb = 0, skipped_kb = 0, deep = 0;
  const uint32_t ranges[2][2] = {{0xA0000000u, 0xC0000000u}, {0x40000000u, 0x50000000u}};
  for (int r = 0; r < 2 && !(r == 1 && n > 0); ++r)
    for (uint64_t va = ranges[r][0]; va < ranges[r][1] && n < kMaxStatTables; va += 0x10000u) {
      const uint8_t* h = HostP(base, uint32_t(va));
      if (!HostReadable(h, 0x10000u)) continue;
      const uint32_t res = ResidentPages(h);
      if (!res) { skipped_kb += 64; continue; }
      read_kb += 64;
      for (uint32_t o = 16; o + 10 <= 0x10000u && n < kMaxStatTables; o += 2) {
        if (!(res & (1u << (o >> 12))) || !(res & (1u << ((o - 16) >> 12)))) { o = ((o >> 12) + 1) * 0x1000u + 14u; continue; }
        const uint8_t* g = h + o;
        const int gt = S16(g - 16);  // goalie TOI
        if (gt < 30 || gt > 5400) continue;
        const int sa = S16(g + 2), s1 = S16(g + 4);
        if (sa < 1 || sa > 90 || s1 < 1 || s1 > 40 || !GoalieSig(g, 1)) continue;
        ++deep;
        const uint32_t p0 = uint32_t(va) + o - kRec * 18u;
        const bool inside = p0 >= uint32_t(va) + 0x80u && p0 + kRec * 20u <= uint32_t(va) + 0x10000u;
        if (!(inside ? TableBodyOk(HostP(base, p0)) : TableOk(base, p0))) continue;
        const uint8_t* ph = HostP(base, p0);
        int sog = 0;
        uint32_t hsh = 2166136261u;  // round 30: content hash (TOI + SOG of all 20 records): copies are identical,
                                     // the two teams are not (round 29: both teams had 9 shots, the old key merged them)
        for (uint32_t s = 0; s < 20; ++s) {
          sog += s < 18 ? S16(ph + kRec * s + ROGUE_STAT_SOG) : 0;
          hsh = (hsh ^ uint32_t(uint16_t(S16(ph + kRec * s + ROGUE_STAT_TOI)))) * 16777619u;
          hsh = (hsh ^ uint32_t(uint16_t(S16(ph + kRec * s + ROGUE_STAT_SOG)))) * 16777619u;
        }
        const int kk[3] = {sog, sa, int(hsh)};
        int gi = -1;
        for (int q = 0; q < n && gi < 0; ++q)
          if (key[q][0] == kk[0] && key[q][1] == kk[1] && key[q][2] == kk[2]) gi = grp[q];
        if (gi < 0) gi = ngrp++;
        key[n][0] = kk[0]; key[n][1] = kk[1]; key[n][2] = kk[2];
        grp[n] = gi;
        hits[n++] = p0;
      }
      std::this_thread::yield();
    }
  for (int q = 0; q < n; ++q) { g_tab[q].store(hits[q]); g_grp[q].store(grp[q]); }
  g_ntab.store(n);
  g_scan_ms.store(uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()));
  ProbeLine("stats-scan: %d table(s) in %d group(s), %u ms; read %u KB, skipped %u KB, %u deep checks; residency %s", n,
            ngrp, g_scan_ms.load(), read_kb, skipped_kb, deep, WsExFn() ? "on" : "UNAVAILABLE");
  for (int q = 0; q < n; ++q)
    ProbeLine("stats-table #%d group %d p0=%08X skater SOG=%d goalie SA=%d hash=%08X", q, grp[q], hits[q], key[q][0], key[q][1], unsigned(key[q][2]));
  g_scanning.store(false);
}
void StartScan(uint8_t* base) {
  if (g_scanning.exchange(true)) return;
  ++g_tries;
  for (auto& row : g_score) for (int& v : row) v = 0;
  g_have_last = false;
  g_on_samples = 0;
  std::thread(ScanTables, base).detach();
}

// Live copy of a group: the one with the highest total TOI that still passes the checks. -1 if none.
int LiveCopy(uint8_t* base, int group) {
  int best = -1, best_toi = -1;
  for (int t = 0; t < g_ntab.load(); ++t) {
    if (g_grp[t].load() != group) continue;
    const uint32_t p0 = g_tab[t].load();
    if (!TableOk(base, p0)) continue;
    const int toi = TotalToi(HostP(base, p0));
    if (toi > best_toi) { best_toi = toi; best = t; }
  }
  return best;
}

struct Pick { int group = -1, slot = -1; bool identified = false; };
Pick ChoosePro(uint8_t* base, int idx) {
  Pick p;
  const int n = g_ntab.load();
  int gs[kMaxStatTables][18];
  bool used[kMaxStatTables] = {};
  for (auto& row : gs) for (int& v : row) v = -1 << 30;
  for (int t = 0; t < n; ++t) {
    const int g = g_grp[t].load();
    if (g < 0 || g >= kMaxStatTables) continue;
    used[g] = true;
    for (int s = 0; s < 18; ++s) gs[g][s] = std::max(gs[g][s], g_score[t][s]);
  }
  int best = -1 << 30, second = -1 << 30;
  for (int g = 0; g < kMaxStatTables; ++g)
    if (used[g])
      for (int s = 0; s < 18; ++s) {
        const int v = gs[g][s];
        if (v > best) { second = best; best = v; p.group = g; p.slot = s; }
        else if (v > second) second = v;
      }
  if (p.group >= 0 && g_on_samples >= 30 && best >= g_on_samples && best - second >= 6) { p.identified = true; return p; }
  // Round 30: linemates tick together. If slot idx % 20 is among the near-best candidates, take it as identified.
  if (p.group >= 0 && g_on_samples >= 30 && best >= g_on_samples && idx % 20 < 18)
    for (int g = 0; g < kMaxStatTables; ++g)
      if (used[g] && gs[g][idx % 20] >= best - 6) { p.group = g; p.slot = idx % 20; p.identified = true; return p; }
  p = Pick{};
  const int s = idx % 20;
  if (s >= 18) return p;
  for (int g = 0; g < kMaxStatTables; ++g) {
    if (!used[g]) continue;
    const int t = LiveCopy(base, g);
    if (t >= 0 && S16(HostP(base, g_tab[t].load()) + kRec * uint32_t(s) + ROGUE_STAT_TOI) > 0) { p.group = g; p.slot = s; return p; }
  }
  return p;
}
}  // namespace

void NotePeriodEnd() { g_period_ends.fetch_add(1); }

void BoxScoreReset() {
  g_ntab.store(0);
  g_tries = 0;
  g_story = false;
  g_have_last = false;
  g_on_samples = 0;
  g_period_ends.store(0);
  for (auto& row : g_score) for (int& v : row) v = 0;
  g_game_t0 = std::chrono::steady_clock::now();
}
bool BoxScoreScanBusy() { return g_scanning.load(); }

void BoxScoreTick() {
  uint8_t* base = VBase();
  if (!base || g_scanning.load()) return;
  const GuestGame g = PollGuestGame();
  const auto now = std::chrono::steady_clock::now();
  if (g.state == ROGUE_GAME_STATE_STORYTELLER) {
    if (!g_story) { g_story = true; g_story_t0 = now; }
  } else {
    g_story = false;
  }
  // Search: in the first cut-scene after a period end (the intermission), never during play.
  if (g_ntab.load() == 0 && g_tries < 3 && g_period_ends.load() > 0 && g_story && now - g_story_t0 >= std::chrono::seconds(2)) {
    StartScan(base);
    return;
  }
  const int n = g_ntab.load();
  if (!n || g.state != ROGUE_GAME_STATE_GAME_ON || now < g_next_sample) return;
  g_next_sample = now + std::chrono::seconds(1);
  const uint32_t pro = ReadAutoBapPlayer(base);
  uint8_t marker = 0xFF;
  if (!pro || !GuestU8(base, pro + ROGUE_OFF_SKATER_ONICE, &marker)) return;
  const bool on = marker != 0xFF;  // round 30: 0x06 on the ice in round 29 (0x00 earlier)
  for (int t = 0; t < n; ++t) {
    const uint32_t p0 = g_tab[t].load();
    if (!TableReadable(base, p0)) continue;
    const uint8_t* h = HostP(base, p0);
    for (int s = 0; s < 18; ++s) {
      const int v = S16(h + kRec * uint32_t(s) + ROGUE_STAT_TOI);
      if (g_have_last) {
        const bool inc = v > g_last_toi[t][s];
        g_score[t][s] += on ? (inc ? 2 : -2) : (inc ? -1 : 0);
      }
      g_last_toi[t][s] = v;
    }
  }
  if (g_have_last && on) ++g_on_samples;
  g_have_last = true;
}

void BoxScoreGameOver() {
  uint8_t* base = VBase();
  if (base && g_ntab.load() == 0) StartScan(base);
}

const char* BoxScoreStatus() {
  static char buf[180];
  if (g_scanning.load()) return "Game box score: looking for the game's stats (one search)...";
  if (g_ntab.load() == 0)
    std::snprintf(buf, sizeof(buf), "Game box score: not found yet (searches in the 1st intermission; %d tried)", g_tries);
  else
    std::snprintf(buf, sizeof(buf), "Game box score: %d table(s) found (search %.1f s), identifying your record (%d s on the ice so far)",
                  g_ntab.load(), g_scan_ms.load() / 1000.0, g_on_samples);
  return buf;
}

BoxScore ReadBoxScore() {
  BoxScore b;
#if defined(ROGUE_OFF_PLAYER_INDEX)
  uint8_t* base = VBase();
  const uint32_t pro = ReadAutoBapPlayer(base);
  if (!pro) return b;
  uint8_t idx = 0;
  if (!GuestU8(base, pro + ROGUE_OFF_PLAYER_INDEX, &idx) || idx >= 40) return b;
  b.index = idx;
  b.second_team = idx >= 20;
  b.tables = g_ntab.load();
  if (b.tables <= 0) return b;
  const Pick pk = ChoosePro(base, idx);
  if (pk.group < 0) return b;
  const int own = LiveCopy(base, pk.group);
  if (own < 0) return b;
  const uint32_t pm0 = g_tab[own].load();
  b.slot = pk.slot;
  b.identified = pk.identified;
  const uint8_t* h = HostP(base, pm0);
  auto s16 = [&](const uint8_t* t, int slot, int rel) { return S16(t + kRec * uint32_t(slot) + rel); };
  int own_sog = 0, own_sa = 0, own_goals = 0;
  for (int s = 0; s < 18; ++s) { own_sog += s16(h, s, ROGUE_STAT_SOG); own_goals += s16(h, s, ROGUE_STAT_GOALS); }
  for (int s = 18; s < 20; ++s) { own_sa += s16(h, s, ROGUE_STAT_GOALIE_SA); b.team_ga += s16(h, s, ROGUE_STAT_GOALIE_GA); }
  b.team_shots = own_sog;
  b.opp_shots = own_sa;
  // The other team: another group whose goalie faced our shots and whose skaters took the shots we faced.
  int best_d = 5, opp = -1;
  for (int g = 0; g < kMaxStatTables; ++g) {
    if (g == pk.group) continue;
    const int t = LiveCopy(base, g);
    if (t < 0) continue;
    const uint8_t* q = HostP(base, g_tab[t].load());
    int sog = 0, sa = 0;
    for (int s = 0; s < 18; ++s) sog += s16(q, s, ROGUE_STAT_SOG);
    for (int s = 18; s < 20; ++s) sa += s16(q, s, ROGUE_STAT_GOALIE_SA);
    const int d = std::abs(sa - own_sog) + std::abs(sog - own_sa);
    if (d < best_d) { best_d = d; opp = t; }
  }
  if (opp >= 0) {
    const uint8_t* q = HostP(base, g_tab[opp].load());
    for (int s = 18; s < 20; ++s) b.team_gf += s16(q, s, ROGUE_STAT_GOALIE_GA);
    b.gf_from_goalie = true;
  } else {
    b.team_gf = own_goals;  // misses simulated goals
  }
  b.goals = s16(h, b.slot, ROGUE_STAT_GOALS);
  b.assists = s16(h, b.slot, ROGUE_STAT_ASSISTS);
  b.pim = s16(h, b.slot, ROGUE_STAT_PIM);
  b.plus_minus = s16(h, b.slot, 0);
  b.shots = s16(h, b.slot, ROGUE_STAT_SOG);
  b.hits = s16(h, b.slot, ROGUE_STAT_HITS);
  b.toi = s16(h, b.slot, ROGUE_STAT_TOI);
  b.record_va = pm0 + kRec * uint32_t(b.slot);
  b.ambiguous = !b.identified;
  b.valid = b.team_gf >= 0 && b.team_gf <= 30 && b.team_ga >= 0 && b.team_ga <= 30;
#endif
  return b;
}

// Probe only: raw bytes of the pro's record (+/- word at offset 44 of the dump), for the TOI hunt.
void DumpBoxRecord(char* out, size_t n, uint32_t pm_va, int from, int len) {
  if (n) out[0] = 0;
  uint8_t* base = VBase();
  if (!base || !pm_va || !HostReadable(HostP(base, pm_va - 44u), 88)) return;
  size_t w = 0;
  for (int i = from; i < from + len && w + 3 < n; ++i)
    w += size_t(std::snprintf(out + w, n - w, "%02X", LoadU8(base, pm_va - 44u + uint32_t(i))));
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
// Also the perks Heavy Shoulders (hit floor by stacks) and Bulldozer (knockdown floor), 2026-11-03.
double ContactFloor(uint8_t* base, double p) {
  float floor_min = rogue::probe::ContactFloorTest();
  if (const int hs = rogue::Run().Stacks("heavy_shoulders")) {
    static const float kFloors[3] = {0.20f, 0.10f, 0.05f};
    const float pf = kFloors[(hs > 3 ? 3 : hs) - 1];
    if (floor_min <= 0.f || pf < floor_min) floor_min = pf;
  }
  const float kd_floor = rogue::Run().EventParam(rogue::EventKind::kContactKnockdown);
  if ((floor_min <= 0.f && kd_floor <= 0.f) || !IsSkaterObject(base, t_victim)) return p;
  if (LoadU8(base, t_victim + kOffTeam) == LoadU8(base, t_hitter + kOffTeam)) return p;
  const double hit_min = TableThreshold(base, 2, 0.5f), kd_min = TableThreshold(base, 3, 0.75f);
  if (kd_floor > 0.f && p >= double(kd_floor) && p <= kd_min) return kd_min + 0.02;  // Bulldozer
  if (floor_min <= 0.f || p < double(floor_min) || p > hit_min) return p;
  rogue::probe::NoteContactFloorPromoted();
  return hit_min + 0.02;
}

// ---- Round 12 shockwave test (probe only, Diagnostics radius != off) ----
// Knock down opposing skaters near the pro by calling the reaction dispatcher ourselves, the way
// the game does for a hitter-less reaction (sub_8342B290: r5 -> u32 0, r6 -> float at 0x82014BEC),
// with the pro as hitter and the power forced above the knockdown threshold. Static read:
// slices_s9.txt (2026-11-03). Position / player table are CANDIDATES, so this stays probe-only.
constexpr uint32_t kPlayerTable = 0x83C1D080u;      // CANDIDATE (static S3): players[team * 20 + slot]
// Live position (S10, 2026-11-03): the pairwise contact check sub_8342B8C8 loads the vector at
// *(player + 32) + 48 (x, y, z, w) for its distance test. +0xD0 in the player object was a stale
// copy (round 12). CANDIDATE until round 13 shows it moving.
constexpr uint32_t kOffBody = 32, kOffBodyPos = 48;
constexpr uint32_t kVaReactionConst = 0x82014BECu;  // float the game passes through r6
thread_local double t_force_power = 0.0;            // > 0: the reaction choice uses this power
thread_local bool t_in_shock = false;               // inside a shockwave call: no chain reactions
// Round 18 "physics window": for 0.4 s the player-player physics range gPlayerPlayerDistanceToEnable
// (the radius of the pairwise contact check sub_8342B8C8) is raised to the shockwave radius, and every
// contact the game itself evaluates with the pro as hitter and an opposing skater as victim gets
// knockdown power. Rounds 12-17 (our own dispatcher calls) never produced a real fall; real contacts
// promoted by the choice hook do (contact floor test, owner 2026-11-03).
std::atomic<bool> g_window_active{false};
std::atomic<bool> g_window_req{false};
thread_local uint32_t t_last_desc = 0;              // reaction chosen by the latest dispatch
// Round 13: the contact check passes the dispatcher r5 -> interaction kind (sub_8341DA00(mgr, hitter,
// victim)) and r6 -> a per-kind float (table 0x822229A0), then calls sub_833DBD18(hitter, victim, r5,
// r6) after it. Round 12 passed kind 0 and skipped that call. Learned from the pro's real knockdowns.
std::atomic<uint32_t> g_learn_kind{0xFFFFFFFFu};
std::atomic<uint32_t> g_learn_fac{0};

bool IsSkaterSafe(uint8_t* base, uint32_t p) {
  uint32_t cls = 0;
  return p >= 0x40000000u && !(p >= 0x82000000u && p < 0x84000000u) && GuestU32(base, p, &cls) &&
         cls == ROGUE_VA_SKATER_CLASS;
}
bool ReadPos(uint8_t* base, uint32_t p, float* x, float* z) {
  uint32_t body = 0, bx = 0, bz = 0;
  if (!GuestU32(base, p + kOffBody, &body) || body < 0x40000000u) return false;
  if (!GuestU32(base, body + kOffBodyPos, &bx) || !GuestU32(base, body + kOffBodyPos + 8, &bz)) return false;
  std::memcpy(x, &bx, 4);
  std::memcpy(z, &bz, 4);
  return std::isfinite(*x) && std::isfinite(*z) && !(*x == 0.f && *z == 0.f);  // 0,0 = off the ice
}
// Round 14 push test: add a velocity away from the pro. Round 13 velocity hunt: body +0x50, +0x60 and
// +0x70 track the position change (cos 0.98, ratio 0.33 = metres per 1/30 s frame). CANDIDATE.
void Push(uint8_t* base, uint32_t p, float px, float pz, float strength) {
  uint32_t body = 0;
  float x = 0.f, z = 0.f;
  if (strength <= 0.f || !ReadPos(base, p, &x, &z) || !GuestU32(base, p + kOffBody, &body)) return;
  float dx = x - px, dz = z - pz;
  const float len = std::sqrt(dx * dx + dz * dz);
  if (len < 0.05f) { dx = 1.f; dz = 0.f; } else { dx /= len; dz /= len; }
  const float add = 0.1f * strength;  // 0.1 m per frame = ~3 m/s per step
  for (uint32_t off : {0x50u, 0x60u, 0x70u}) {
    uint32_t bx = 0, bz = 0;
    if (!GuestU32(base, body + off, &bx) || !GuestU32(base, body + off + 8, &bz)) continue;
    float vx, vz;
    std::memcpy(&vx, &bx, 4);
    std::memcpy(&vz, &bz, 4);
    if (!std::isfinite(vx) || !std::isfinite(vz)) continue;
    StoreF32(base, body + off, vx + dx * add);
    StoreF32(base, body + off + 8, vz + dz * add);
  }
}
bool WindowTarget(uint8_t* base, uint32_t hitter, uint32_t victim) {
  if (!g_window_active.load() || !hitter || !victim || hitter == victim) return false;
  if (!IsBapSkater(base, hitter) || !IsSkaterSafe(base, victim)) return false;
  uint8_t th = 0xFF, tv = 0xFF;
  return GuestU8(base, hitter + kOffTeam, &th) && GuestU8(base, victim + kOffTeam, &tv) && th != tv;
}
void MaybeShockwave(PPCContext& ctx, uint8_t* base, uint32_t victim, uint32_t hitter, uint32_t desc);
}  // namespace

// Round 15: force the power at its SOURCE during a shockwave call. The dispatcher stores the power of
// sub_833CA500 (victim, hitter, ...) at r1+84 BEFORE the reaction choice and hands that slot to the hit
// contact (r5) and the rest of the fall path, so forcing only the choice's f1 (rounds 12-14) left the
// stored power at the real, tiny value. Round 13/14 trace: the fall routine sub_829380A0 started for
// every natural knockdown of a skater (power >= 0.75) but for only 3 of 14 forced ones.
REX_EXTERN(__imp__sub_833CA500);
extern "C" REX_FUNC(sub_833CA500) {
  __imp__sub_833CA500(ctx, base);
  // Round 17: optional again (panel). Round 13's only falls happened WITHOUT it.
  if (!rogue::probe::ShockwaveForceStored()) return;
  if (t_force_power > 0.0) ctx.f1.f64 = t_force_power;
  else if (t_dispatch && WindowTarget(base, t_hitter, t_victim))
    ctx.f1.f64 = double(TableThreshold(base, 3, 0.75f)) + 0.15;
}

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
  uint32_t kind_in = 0xFFFFFFFFu, fac_in = 0, v40_in = 0xFFFFFFFFu;
  const bool watch = probe && !prev && !t_in_shock && (IsBapSkater(base, t_hitter) || IsBapSkater(base, t_victim));
  if (watch) {
    GuestU32(base, ctx.r5.u32, &kind_in);
    GuestU32(base, ctx.r6.u32, &fac_in);
    GuestU32(base, t_victim + 40, &v40_in);
  }
  t_last_desc = 0;
  __imp__sub_83429560(ctx, base);
  const uint32_t desc = t_last_desc, victim = t_victim, hitter = t_hitter;
  if (watch) {
    uint32_t v40_out = 0xFFFFFFFFu;
    GuestU32(base, victim + 40, &v40_out);
    const bool pro_hit = IsBapSkater(base, hitter);
    // Round 14: learn only real contact kinds. Round 13 logged a second, same-frame knockdown call
    // with kind 0 (applied 0) right after the real one, which overwrote kind 6.
    if (pro_hit && desc == ROGUE_VA_REACT_KNOCKDOWN && kind_in != 0xFFFFFFFFu && kind_in != 0u &&
        (ctx.r3.u32 & 0xFFu) != 0u) {
      g_learn_kind.store(kind_in);
      g_learn_fac.store(fac_in);
    }
    if (desc == ROGUE_VA_REACT_KNOCKDOWN || desc == 0x83ABF57Cu)
      rogue::probe::ContactArgs(victim, hitter, pro_hit, desc, ctx.r3.u32 & 0xFFu, kind_in, fac_in, v40_in, v40_out);
  }
  if (probe) rogue::probe::ReactionEnd(ctx.r3.u32);
  t_dispatch = prev;
  t_hitter = prev_h;
  t_victim = prev_v;
  if (probe && !prev && !t_in_shock) MaybeShockwave(ctx, base, victim, hitter, desc);
}

// Post-contact step of the contact check (S10): sub_833DBD18(r3 actor, r4 other, r5 -> kind, r6 ->
// float). Pass-through; defined here so ForceKnockdown can call it (round 13).
REX_EXTERN(__imp__sub_833DBD18);
extern "C" REX_FUNC(sub_833DBD18) { __imp__sub_833DBD18(ctx, base); }

// Round 16: balance. sub_834E9FA8(r3 player, r4 kind (0 loose stick, 1 bumped), f1 amount) stores
// max(current, penalty) into the balance-loss float *(*(player + 80) + 64) + 368 (static S6). Fall
// thresholds gBalanceCarrierFallThreshold 23 / gBalanceOffpuckFallThreshold 25 (skating snapshot).
// Round 15: forced dispatches with power 0.9 everywhere still started no fall, so the fall probably
// comes from balance. Pass-through (logs natural calls for calibration); defined here so the
// shockwave can call it.
namespace {
uint32_t BalanceVa(uint8_t* base, uint32_t p) {
  uint32_t o80 = 0, o64 = 0;
  if (!GuestU32(base, p + 80, &o80) || o80 < 0x40000000u) return 0;
  if (!GuestU32(base, o80 + 64, &o64) || o64 < 0x40000000u) return 0;
  return o64 + 368;
}
float ReadBalance(uint8_t* base, uint32_t p) {
  uint32_t va = BalanceVa(base, p), b = 0;
  if (!va || !GuestU32(base, va, &b)) return -1.f;
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
}  // namespace
REX_EXTERN(__imp__sub_834E9FA8);
extern "C" REX_FUNC(sub_834E9FA8) {
  const bool log = rogue::probe::Enabled() && !t_in_shock;
  const uint32_t p = ctx.r3.u32, kind = ctx.r4.u32;
  const double amount = ctx.f1.f64;
  const float before = log ? ReadBalance(base, p) : 0.f;
  __imp__sub_834E9FA8(ctx, base);
  if (log) rogue::probe::BalanceSeen(p, kind, float(amount), before, ReadBalance(base, p));
}

namespace {
// One forced knockdown. Guest scratch below the caller's stack pointer; r1 / r3-r6 / lr restored,
// so the dispatcher's own caller still sees its return value (other volatiles are clobbered by any
// call anyway; the callee restores the non-volatiles itself).
bool ForceKnockdown(PPCContext& ctx, uint8_t* base, uint32_t victim, uint32_t pro) {
  const uint64_t r1 = ctx.r1.u64, r3 = ctx.r3.u64, r4 = ctx.r4.u64, r5 = ctx.r5.u64, r6 = ctx.r6.u64;
  const auto lr = ctx.lr;
  const uint32_t sp = (ctx.r1.u32 - 512u) & ~15u;
  if (!HostReadable(HostP(base, sp), 512)) return false;
  const uint32_t s_u = sp + 80u, s_f = sp + 84u;
  // 0 = round 12 (kind 0, no post step), 1 = kind 0 + post step (the round 13 calls that made players
  // fall), 2 = learned kind (default 6 / 0.25) + post step (round 14: 0 of 4 fell).
  const int mode = rogue::probe::ShockwaveMode();
  // Nothing learned yet: the values of the pro's real knockdowns in round 13 (kind 6, float 0.25;
  // his normal hits were kind 6 with 0.03-0.09).
  uint32_t kind = g_learn_kind.load(), fac = g_learn_fac.load();
  if (kind == 0xFFFFFFFFu) { kind = 6u; fac = 0x3E800000u; }
  if (mode != 2) { kind = 0u; fac = mode == 1 ? 0x3F800000u : LoadU32(base, kVaReactionConst); }
  StoreU32(base, s_u, kind);
  StoreU32(base, s_f, fac);
  uint32_t v40_in = 0xFFFFFFFFu;
  GuestU32(base, victim + 40, &v40_in);
  ctx.r1.u64 = sp;
  ctx.r3.u64 = victim;
  ctx.r4.u64 = pro;
  ctx.r5.u64 = s_u;
  ctx.r6.u64 = s_f;
  t_force_power = double(TableThreshold(base, 3, 0.75f)) + 0.15;
  t_in_shock = true;
  sub_83429560(ctx, base);  // our hook above -> the original
  t_in_shock = false;
  t_force_power = 0.0;
  const bool ok = (ctx.r3.u32 & 0xFFu) != 0u;
  if (mode >= 1) {  // the game's post-contact step, same slots (contact check order)
    ctx.r1.u64 = sp;
    ctx.r3.u64 = pro;
    ctx.r4.u64 = victim;
    ctx.r5.u64 = s_u;
    ctx.r6.u64 = s_f;
    sub_833DBD18(ctx, base);
  }
  uint32_t v40_out = 0xFFFFFFFFu;
  GuestU32(base, victim + 40, &v40_out);
  rogue::probe::ForcedArgs(victim, mode, kind, ok, v40_in, v40_out);
  // Round 16 balance test: 0 off, 1 = the game's "bumped" penalty with a big amount, 2 = write 100.
  const int bal = rogue::probe::ShockwaveBalance();
  if (bal) {
    const float b_in = ReadBalance(base, victim);
    if (bal == 1) {
      ctx.r1.u64 = sp;
      ctx.r3.u64 = victim;
      ctx.r4.u64 = 1;
      ctx.f1.f64 = double(rogue::probe::ShockwaveBalanceAmount());
      sub_834E9FA8(ctx, base);
    } else if (const uint32_t va = BalanceVa(base, victim)) {
      StoreF32(base, va, 100.f);
    }
    rogue::probe::ForcedBalance(victim, bal, b_in, ReadBalance(base, victim));
  }
  ctx.r1.u64 = r1; ctx.r3.u64 = r3; ctx.r4.u64 = r4; ctx.r5.u64 = r5; ctx.r6.u64 = r6;
  ctx.lr = lr;
  return ok;
}

// Fires when the pro lands a knockdown on a skater, or at the pro's next contact of any kind after
// "Arm" (panel / F8). Targets: opposing skaters on the ice within the test radius of the pro.
void MaybeShockwave(PPCContext& ctx, uint8_t* base, uint32_t victim, uint32_t hitter, uint32_t desc) {
  const float radius = rogue::probe::ShockwaveTest();
  if (radius <= 0.f || !rogue::InBapGame()) return;
  uint32_t pro = 0, partner = 0;
  if (hitter && IsBapSkater(base, hitter)) { pro = hitter; partner = victim; }
  else if (victim && IsBapSkater(base, victim)) { pro = victim; partner = hitter; }
  if (!pro) return;
  const bool kd = pro == hitter && desc == ROGUE_VA_REACT_KNOCKDOWN && IsSkaterSafe(base, victim);
  const bool armed = !kd && rogue::probe::TakeShockwaveArm();
  if (!kd && !armed) return;
  if (rogue::probe::ShockwaveMethod() == 1) {  // round 18: let the game's own contact check do it
    g_window_req.store(true);
    return;
  }
  static std::chrono::steady_clock::time_point last{};
  const auto now = std::chrono::steady_clock::now();
  if (!armed && now - last < std::chrono::seconds(1)) return;
  last = now;
  char line[480];
  float px = 0.f, pz = 0.f;
  if (!ReadPos(base, pro, &px, &pz)) {
    std::snprintf(line, sizeof(line), "shockwave %s: pro position unreadable", armed ? "armed" : "knockdown");
    rogue::probe::ShockwaveFired(line, 0);
    return;
  }
  uint8_t pteam = 0xFF;
  GuestU8(base, pro + kOffTeam, &pteam);
  uint32_t tg[8];
  float dist[8];
  int n = 0, seen = 0;
  for (uint32_t i = 0; i < 40 && n < 8; ++i) {
    uint32_t p = 0;
    if (!GuestU32(base, kPlayerTable + 4u * i, &p) || !p || p == pro || p == partner) continue;
    if (!IsSkaterSafe(base, p)) continue;
    uint8_t team = 0xFF;
    if (!GuestU8(base, p + kOffTeam, &team) || team == pteam) continue;
    float x = 0.f, z = 0.f;
    // On the ice: |x| < 12.8 m. Benches sit just outside the side boards (pro on the bench at
    // x = 13.35 in round 13). The +0xD0 filter of round 14 was stale and dropped most opponents.
    if (!ReadPos(base, p, &x, &z) || std::fabs(x) > 12.8f) continue;
    ++seen;
    const float d = std::sqrt((x - px) * (x - px) + (z - pz) * (z - pz));
    if (d <= radius) { tg[n] = p; dist[n] = d; ++n; }
  }
  int w = std::snprintf(line, sizeof(line), "shockwave %s: pro=%08X at (%.2f, %.2f) r=%.1f push=%.0f opponents on ice=%d hit=%d:",
                        armed ? "armed" : "knockdown", pro, double(px), double(pz), double(radius),
                        double(rogue::probe::ShockwavePush()), seen, n);
  int ok_n = 0;
  for (int k = 0; k < n; ++k) {
    const bool ok = ForceKnockdown(ctx, base, tg[k], pro);
    ok_n += ok;
    if (ok) Push(base, tg[k], px, pz, rogue::probe::ShockwavePush());
    if (w > 0 && w < int(sizeof(line)))
      w += std::snprintf(line + w, sizeof(line) - size_t(w), " %08X d=%.1f %s", tg[k], double(dist[k]), ok ? "ok" : "refused");
  }
  rogue::probe::ShockwaveFired(line, ok_n);
  if (ok_n) rogue::fx::Push(rogue::fx::Kind::kShockwave);
}
}  // namespace

REX_EXTERN(__imp__sub_833B9028);
extern "C" REX_FUNC(sub_833B9028) {
  const double in = ctx.f1.f64;
  const unsigned long long lr = (unsigned long long)ctx.lr;
  double used = in;
  if (t_force_power > 0.0)
    used = t_force_power;  // shockwave call (round 12)
  else if (t_dispatch && WindowTarget(base, t_hitter, t_victim)) {
    used = double(TableThreshold(base, 3, 0.75f)) + 0.15;  // physics window (round 18)
    rogue::probe::NoteWindowPromoted(t_victim);
  } else if (t_dispatch && rogue::InBapGame() && t_hitter && t_victim != t_hitter &&
           IsBapSkater(base, t_hitter) && !IsBapSkater(base, t_victim))
    used = ContactFloor(base, BapHitPower(base, used));
  const bool probe = rogue::probe::Enabled();
  if (probe && t_force_power <= 0.0) used = rogue::probe::ReactionPowerIn(used);  // power test, on top
  ctx.f1.f64 = used;
  __imp__sub_833B9028(ctx, base);
  if (t_dispatch) t_last_desc = ctx.r3.u32;
  if (probe) rogue::probe::ReactionChosen(in, used, ctx.r3.u32, lr);
}
#endif

// ---------------------------------------------------------------- pro-only tunable perks (round 25)
// Owner 2026-11-04: with shot perks owned, EVERY player shot harder (the perks wrote global tunables).
// Now kProTunable perks are never written globally. Their tunables are raised only for the duration of a call
// of the game function that reads them, and only when the pro is one of its arguments (r3..r10):
//   sub_833E87E0  shot: gSlapShotPowerScale, gSlapShotAccuracyScale, gBackhand / gRedirect accuracy (static
//                 readers, tunable_readers.tsv; it also reads rating 27 for the shooter)
//   sub_833ECF68  wrist shot power (re-findings.md TUN-8 note; gWristShotPowerMultiplier has no static reader,
//                 so Quick Release may still do nothing)
//   (round 27: Jet Skates moved to the rating override below; the speed boost hook never saw the pro)
// HYPOTHESIS: the shooter / skater is passed in a register. The panel shows calls and "yours" per hook: if
// "yours" stays 0 while you shoot, the pro is not an argument and the perk does nothing for anyone.
// Caveat: another thread reading the tunable during the pro's call would see the raised value (none known).
namespace {
std::atomic<unsigned> g_pt_calls[3], g_pt_mine[3];
std::atomic<int> g_pt_reg[3];
class ProScope {
 public:
  ProScope(int hook, const PPCContext& ctx) {
    g_pt_calls[hook].fetch_add(1, std::memory_order_relaxed);
    const uint32_t pro = rogue::CachedPro();
    if (!pro) return;
    const uint32_t regs[8] = {ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
                              ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32};
    int hit = -1;
    for (int i = 0; i < 8 && hit < 0; ++i) if (regs[i] == pro) hit = i;
    if (hit < 0) return;
    g_pt_mine[hook].fetch_add(1, std::memory_order_relaxed);
    g_pt_reg[hook].store(3 + hit, std::memory_order_relaxed);
    rogue::TweakIndex& t = rogue::TweakIndex::Get();
    if (!t.Ready()) return;
    for (const rogue::OwnedPerk& op : rogue::Run().run().perks) {
      const rogue::PerkDef* p = rogue::FindPerk(op.id.c_str());
      if (!p || p->scope != rogue::Scope::kProTunable) continue;
      for (const rogue::Mod& mo : p->mods) {
        const rogue::TweakRef* r = t.Find(mo.tunable);
        if (!r || !rogue::ScalarTag(r->tag)) continue;
        int k = -1;
        for (int j = 0; j < n_ && k < 0; ++j) if (saved_[j].r == r) k = j;
        if (k < 0) {
          if (n_ >= kMax) continue;
          saved_[n_] = Saved{r, t.Read(*r)};
          k = n_++;
        }
        float v = rogue::BitsToF(t.Read(*r), r->tag);
        for (int s = 0; s < op.stacks; ++s)
          v = mo.op == rogue::ModOp::kMul ? v * mo.value : mo.op == rogue::ModOp::kAdd ? v + mo.value : mo.value;
        t.Write(*r, rogue::FToBits(v, r->tag));
      }
    }
  }
  ~ProScope() {
    rogue::TweakIndex& t = rogue::TweakIndex::Get();
    for (int i = n_ - 1; i >= 0; --i) t.Write(*saved_[i].r, saved_[i].bits);
  }
  ProScope(const ProScope&) = delete;
  ProScope& operator=(const ProScope&) = delete;

 private:
  struct Saved { const rogue::TweakRef* r; uint32_t bits; };
  static constexpr int kMax = 16;
  Saved saved_[kMax];
  int n_ = 0;
};
}  // namespace

namespace rogue {
void ProTunableStats(int hook, unsigned* calls, unsigned* mine, int* reg) {
  if (hook < 0 || hook > 2) return;
  *calls = g_pt_calls[hook].load();
  *mine = g_pt_mine[hook].load();
  *reg = g_pt_reg[hook].load();
}
void ProTunableReset() {
  for (int i = 0; i < 3; ++i) { g_pt_calls[i] = 0; g_pt_mine[i] = 0; g_pt_reg[i] = 0; }
}
}  // namespace rogue

// Literal names (one override per function in the whole program; none of these is overridden elsewhere,
// checked 2026-11-04 against the mod and the parent src/).
REX_EXTERN(__imp__sub_833E87E0);
extern "C" REX_FUNC(sub_833E87E0) {
  ProScope scope(0, ctx);
  __imp__sub_833E87E0(ctx, base);
}
REX_EXTERN(__imp__sub_833ECF68);
extern "C" REX_FUNC(sub_833ECF68) {
  ProScope scope(1, ctx);
  __imp__sub_833ECF68(ctx, base);
}
// Round 27: the speed boost function sub_828B9258 never had the pro in its arguments (round 26: tens of
// thousands of calls, 0 yours, mostly around cut-scenes), so its hook is gone. Jet Skates now boosts the pro's
// own skating ratings through the effective-rating function sub_83501A68(player r3, rating id r4) -> f1
// (static-map.md 12 / 13: 115 callers, per-player by construction; id 0 speed, 1 acceleration, CONFIRMED map).
// Behaviour test: the float's scale is not known; perk numbers are moderate on purpose. Counter slot 2.
float RatingScale(uint32_t id) {
  float k = 1.f;
  for (const rogue::OwnedPerk& op : rogue::Run().run().perks) {
    const rogue::PerkDef* p = rogue::FindPerk(op.id.c_str());
    if (!p) continue;
    for (const rogue::EventEffect& e : p->events)
      if ((e.kind == rogue::EventKind::kRatingBoost ||
           (e.kind == rogue::EventKind::kSprintRatingBoost && rogue::SprintHeld())) &&
          uint32_t(e.p0) == id)
        for (int s = 0; s < op.stacks; ++s) k *= e.p1;
  }
  return k;
}
REX_EXTERN(__imp__sub_83501A68);
extern "C" REX_FUNC(sub_83501A68) {
  const uint32_t player = ctx.r3.u32, id = ctx.r4.u32;
  __imp__sub_83501A68(ctx, base);
  const uint32_t pro = rogue::CachedPro();
  if (!pro) return;
  g_pt_calls[2].fetch_add(1, std::memory_order_relaxed);
  if (player != pro) return;
  g_pt_mine[2].fetch_add(1, std::memory_order_relaxed);
  g_pt_reg[2].store(3, std::memory_order_relaxed);
  const float k = RatingScale(id);
  if (k != 1.f) ctx.f1.f64 *= double(k);
}

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

// ---------------------------------------------------------------- round 18 physics window (Tick)
namespace rogue {
void ShockwaveWindowTick() {
#if defined(ROGUE_ADDR_REACTION_DISPATCH) && defined(ROGUE_ADDR_REACTION_CHOICE)
  using clock = std::chrono::steady_clock;
  static const TweakRef* ref = nullptr;
  static uint32_t saved = 0;
  static bool active = false;
  static clock::time_point end{};
  auto& ti = TweakIndex::Get();
  if (!ti.Ready()) return;
  if (!ref && !(ref = ti.Find("gPlayerPlayerDistanceToEnable"))) return;
  const float radius = probe::ShockwaveTest();
  bool want = g_window_req.exchange(false);
  if (probe::ShockwaveMethod() == 1 && probe::ShockwaveArmed()) want = probe::TakeShockwaveArm() || want;  // F8: now
  const auto now = clock::now();
  if (want && radius > 0.f && InBapGame()) {
    if (!active) saved = ti.Read(*ref);
    float stock;
    std::memcpy(&stock, &saved, 4);
    const float ft = std::max(stock, radius * 3.281f);  // the tweak is in feet
    uint32_t bits;
    std::memcpy(&bits, &ft, 4);
    ti.Write(*ref, bits);
    active = true;
    g_window_active.store(true);
    end = now + std::chrono::milliseconds(400);
    char msg[160];
    std::snprintf(msg, sizeof(msg), "shockwave window ON: gPlayerPlayerDistanceToEnable %.2f -> %.2f ft for 0.4 s",
                  double(stock), double(ft));
    probe::StaticNote(msg);
    probe::ShockwaveFired("shockwave window", 0);
    fx::Push(fx::Kind::kShockwave);
  }
  if (active && (now >= end || !InBapGame())) {
    ti.Write(*ref, saved);
    active = false;
    g_window_active.store(false);
    char msg[96];
    std::snprintf(msg, sizeof(msg), "shockwave window OFF: promoted %d contacts", probe::WindowPromoted());
    probe::StaticNote(msg);
  }
#endif
}
}  // namespace rogue
