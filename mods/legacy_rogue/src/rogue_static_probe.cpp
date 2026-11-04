// Legacy Rogue - static-map probe round S1 (task PROBE-S1, docs/user-guide.md 4g).
//
// Checks the CANDIDATES from docs/static/static-map.md (2026-11-02, static analysis only) in one
// session. Everything here is READ-ONLY and pass-through: every hook calls the original with the
// same registers and only logs. Gated on rogue::probe::Enabled() (NHL_ROGUE=1 + NHL_ROGUE_PROBE=1).
// Output: rogue_probe_s.txt next to the exe (separate from rogue_probe.txt so the two never mix).
//
// What it answers (static-map.md section 14):
//  A. RE-2: is the word at 0x83C876D4 the Be-A-Pro player? (sampler, compared with the claim and
//     with the +0x22C marker byte)
//  B. RE-5: game state machine order (sub_8330C890), period word, game over / DONE.
//  C. Goal (sub_83480248), period end (sub_8331F408), puck contact (sub_83328180),
//     penalty decide / apply (sub_83487DC0 / sub_83485BD0): do the registers mean what we think?
//  D. The event post function: slot 1 of the event manager vtable (via sub_82FEC310).
//  E. GameEvent type ids: the 175 type slots, dumped once they are filled.
//  F. Raw ratings (+1240 + 4 * id) and energy (+360) of the cached BAP player.
//
// None of these addresses is CONFIRMED; that is what this round is for (00-START-HERE rule 4:
// nothing here goes into addresses.h). No function hooked here is hooked anywhere else
// (checked 2026-11-02 against rogue_hooks.cpp, rogue_probe.cpp and src/diag_hooks.cpp).
// NOT compiled by the agent against the real SDK (syntax-checked against stub headers only).
#include "generated/default/nhllegacy_init.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>

#include <rex/filesystem.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "rogue/probe.h"
#include "rogue/rogue_mod.h"

namespace {

// --- CANDIDATE addresses (static-map.md). Keep in sync with the doc, not with addresses.h.
constexpr uint32_t kVaGameState = 0x83C1D220u;        // current game state (sub_8330C890), -1 none
constexpr uint32_t kVaFlowPhase = 0x83C1D220u + 52u;  // game-flow phase (0 -> GameOver branch)
constexpr uint32_t kVaPeriod    = 0x83C876B8u;        // current period (255 / 238 special)
constexpr uint32_t kVaBapFlag   = 0x83AC5F63u;        // byte: Be-A-Pro mode
constexpr uint32_t kVaMatch     = 0x83C1E89Cu;        // match object pointer
constexpr uint32_t kVaBapCache  = 0x83C876D4u;        // cached BAP player pointer (sub_834DC8A8)
constexpr uint32_t kOffIndex = 57, kOffTeam = 48, kOffEnergy = 360, kOffFatigue = 364;
constexpr uint32_t kOffInjury = 368, kOffMarker = 0x22C, kOffRatings = 1240;
constexpr int kNumRatings = 29;  // ids 0..28 (static-map.md section 13)

const char* const kStateNames[14] = {
  "PRE_GAME", "PRE_FACEOFF", "SYNCHING", "FACEOFF", "GAME_ON", "GOOF_OFF", "PREFIGHT",
  "FIGHT", "POSTFIGHT", "DONE", "STORYTELLER", "NISVIEWER", "PRACTICE_GOOFOFF",
  "TUTORIAL_GOOFOFF"};
const char* StateName(int32_t s) { return (s >= 0 && s < 14) ? kStateNames[s] : "?"; }

// GameEvent type slots (docs/static/game_events.tsv). Filled lazily by the game.
struct Slot { uint32_t va; const char* name; };
const Slot kSlots[] = {
  {0x83B6F614u, "AIObjectInitComplete"},
  {0x83B6FAA8u, "GoalAwarded"},
  {0x83B6FAACu, "EndPeriod"},
  {0x83B6FAB0u, "EndShootout"},
  {0x83BCF190u, "Announce3Stars"},
  {0x83BD0584u, "BeAPro_Task_Assigned"},
  {0x83BD1780u, "Goal"},
  {0x83BD178Cu, "Generic"},
  {0x83BD1790u, "PenaltyCall"},
  {0x83BD1794u, "PenaltyStoppage"},
  {0x83BD1798u, "PlayerStopPlay"},
  {0x83BD179Cu, "CallPenaltyShot"},
  {0x83BD17A0u, "BeAPro_ON_ICE"},
  {0x83BD17A4u, "BeAPro_OFF_ICE"},
  {0x83BD17A8u, "BeAPro_SHIFT_START"},
  {0x83BD17ACu, "BeAPro_SHIFT_END"},
  {0x83BD17B0u, "BeAPro_REQUEST_CHANGE"},
  {0x83BD17B4u, "BeAPro_FATIGUE_LEVEL"},
  {0x83BD17B8u, "BeAPro_COACH_FEEDBACK"},
  {0x83BD17BCu, "BAPLineChangeButtonOn"},
  {0x83BD17C0u, "BAPLineChangeButtonOff"},
  {0x83BD17C4u, "BAPVisualFXStartFade"},
  {0x83BD17C8u, "BAPVisualFXFadeComplete"},
  {0x83BD17CCu, "BAPVisualFXStartReveal"},
  {0x83BD17D0u, "BeAPro_Task_Cancelled"},
  {0x83BD17D4u, "BeAPro_Task_Complete"},
  {0x83BD17D8u, "BeAPro_Task_ScoreChange"},
  {0x83BD1808u, "ProgressMessage"},
  {0x83BD180Cu, "Injury"},
  {0x83BD1810u, "FightInjury"},
  {0x83BD1814u, "ShotBlockInjury"},
  {0x83BD1818u, "EndPeriodInjuryEffectsComplete"},
  {0x83BD1820u, "Whistle"},
  {0x83BD2C90u, "EndPeriodInjuryEffectsStart"},
  {0x83BE4F74u, "FaceoffSetup"},
  {0x83BE4F78u, "FightPlayerKnockedOut"},
  {0x83BE4F7Cu, "GameOver"},
  {0x83BE4F80u, "StartPeriod"},
  {0x83BE4F84u, "SetupShootout"},
  {0x83BE87DCu, "BlockShot"},
  {0x83BE87E0u, "Boardplay"},
  {0x83BE87E4u, "BoardplayExit"},
  {0x83BE87E8u, "ClearAttempt"},
  {0x83BE87F0u, "DumpAttempt"},
  {0x83BE87F4u, "FaceoffEnd"},
  {0x83BE87F8u, "GlassShake"},
  {0x83BE87FCu, "HelmetIceCollision"},
  {0x83BE8800u, "LooseStick"},
  {0x83BE8804u, "LooseStickIceCollision"},
  {0x83BE8808u, "PassComplete"},
  {0x83BE880Cu, "PassStart"},
  {0x83BE8810u, "PickupPuck"},
  {0x83BE8818u, "PracticeTargetHit"},
  {0x83BE881Cu, "PuckHitBoards"},
  {0x83BE8820u, "PuckHitIce"},
  {0x83BE8824u, "PuckHitLooseStick"},
  {0x83BE8828u, "PuckHitPost"},
  {0x83BE882Cu, "PuckRimAroundBoards"},
  {0x83BE8830u, "Save"},
  {0x83BE8834u, "Shot"},
  {0x83BE8844u, "SkatingGlideTurn"},
  {0x83BE8848u, "SkatingStop"},
  {0x83BE884Cu, "SlapPass"},
  {0x83BE894Cu, "Chop"},
  {0x83BE8950u, "ForecheckOZEnded"},
  {0x83BE8954u, "ForecheckNZEnded"},
  {0x83BE8958u, "ForecheckOZStruggling"},
  {0x83BE895Cu, "ForecheckNZStruggling"},
  {0x83BE8960u, "ForecheckSucceeding"},
  {0x83BE8964u, "ForecheckBiasComment"},
  {0x83BE8968u, "ForecheckAdjusted"},
  {0x83BEF080u, "AnnounceGoal"},
  {0x83BEF084u, "AnnouncePenalty"},
  {0x83BEF088u, "BatDown"},
  {0x83BEF08Cu, "BeginReplay"},
  {0x83BEF090u, "Breakaway"},
  {0x83BEF094u, "CallTimeOut"},
  {0x83BEF098u, "CancelPulledGoalie"},
  {0x83BEF09Cu, "Check"},
  {0x83BEF0A0u, "ClearResult"},
  {0x83BEF0A4u, "ClockSecond"},
  {0x83BEF0A8u, "DumpResult"},
  {0x83BEF0ACu, "EndIntermissionStartPeriod"},
  {0x83BEF0B0u, "EndRush"},
  {0x83BEF0B4u, "FaceoffResult"},
  {0x83BEF0B8u, "FightEnd"},
  {0x83BEF0BCu, "FightEnding"},
  {0x83BEF0C0u, "FightPlayerPunched"},
  {0x83BEF0C4u, "FightPlayerTurtled"},
  {0x83BEF0C8u, "FightPlayerTurtlePunched"},
  {0x83BEF0CCu, "FightRejected"},
  {0x83BEF0D0u, "FightStart"},
  {0x83BEF0D4u, "FightStartSecondary"},
  {0x83BEF0D8u, "FightStepIn"},
  {0x83BEF0DCu, "FailedOneTimer"},
  {0x83BEF0E0u, "GoalieReleasePuck"},
  {0x83BEF0E4u, "GoalieGetsSnowedNonDeterministic"},
  {0x83BEF0E8u, "GoofOffIsOver"},
  {0x83BEF0ECu, "HelmetDropped"},
  {0x83BEF0F0u, "IcingBeatOut"},
  {0x83BEF0F4u, "IcedPuck"},
  {0x83BEF0F8u, "InjuryCoachDecision"},
  {0x83BEF0FCu, "Instigation"},
  {0x83BEF100u, "Interception"},
  {0x83BEF104u, "KnownSequence"},
  {0x83BEF108u, "LastMinute"},
  {0x83BEF10Cu, "LooseStickGrabBenchStick"},
  {0x83BEF110u, "LooseStickGrabDroppedStick"},
  {0x83BEF114u, "LooseStickGrabTeammateStick"},
  {0x83BEF118u, "LineChange"},
  {0x83BEF11Cu, "MissedNet"},
  {0x83BEF120u, "NetBattleInProgress"},
  {0x83BEF124u, "NotifyKnownSequenceReplay"},
  {0x83BEF128u, "PassIncomplete"},
  {0x83BEF12Cu, "Penalty"},
  {0x83BEF130u, "PullGoalie"},
  {0x83BEF134u, "RefGoalCall"},
  {0x83BEF138u, "RefPenaltyCall"},
  {0x83BEF13Cu, "RematchStartPeriod"},
  {0x83BEF140u, "Rush"},
  {0x83BEF144u, "SaveAttempt"},
  {0x83BEF148u, "ScreenedGoalie"},
  {0x83BEF14Cu, "SecondaryFightPlayerPunched"},
  {0x83BEF150u, "SkateKick"},
  {0x83BEF154u, "StartOfShootoutInRegularGame"},
  {0x83BEF158u, "StickCheck"},
  {0x83BEF15Cu, "StickLiftAttempt"},
  {0x83BEF160u, "SwapGoalie"},
  {0x83BEF164u, "TendencyDetection"},
  {0x83BEF168u, "Turnover"},
  {0x83BEF170u, "PassEnd"},
  {0x83BEF174u, "PenaltyOver"},
  {0x83BEF178u, "SlashHookResult"},
  {0x83BEF17Cu, "UsefulLoosePuckDekeFail"},
  {0x83BEF180u, "UsefulLoosePuckDekeStart"},
  {0x83BEF184u, "UsefulLoosePuckDekeSuccess"},
  {0x83BEF18Cu, "TimeLeftInPenalty"},
  {0x83BEF190u, "ZonePossessionTime"},
  {0x83BF5270u, "GlassBreak"},
  {0x83BF5280u, "FacialInjuryEffect"},
  {0x83BF5284u, "GoalLight"},
  {0x83BF528Cu, "FightPlayerGainHealth"},
  {0x83BF5290u, "GlovesDropped"},
  {0x83BF6B8Cu, "BenchPieceVisibility"},
  {0x83BF6B90u, "FreezePowerRing"},
  {0x83BF6B94u, "TriggerPowerRingEffect"},
  {0x83C22D0Cu, "StorableGameEvent"},
  {0x83C22D14u, "GoalOnMajor"},
  {0x83C22D1Cu, "PuckHitPlayer"},
  {0x83C22D20u, "ExitWarmupGame"},
  {0x83C22D24u, "PlayerInteractionEnd"},
  {0x83C22D28u, "LooseHelmetSituation"},
  {0x83C22D2Cu, "GoalOnMinor"},
  {0x83C22D30u, "PenaltyAlmostOver"},
  {0x83C22D34u, "PlayerInteraction"},
  {0x83C22D38u, "PlayerInteractionControllerRumble"},
  {0x83C22D3Cu, "PostWhistleBehaviorOver"},
  {0x83C22D40u, "PuckFrozenGoalieTraffic"},
  {0x83C7CC40u, "InjuryCoachDecisionNIS"},
  {0x83C7CC44u, "PracticeModeFaceoffSetup"},
  {0x83C7CC48u, "FightPending"},
  {0x83C7CC4Cu, "FightPlayerTugStart"},
  {0x83C84908u, "LoosePuckDekeSuccess"},
  {0x83C84910u, "Intimidation"},
  {0x83C84914u, "IntimidationReduction"},
  {0x83C88E3Cu, "ShootoutGoal"},
  {0x83C93E60u, "NetBattleAction"},
  {0x83C98D64u, "Takeaway"},
  {0x83CB23ACu, "MagnumRecordStop"},
  {0x83CB23B0u, "DekePlayer"},
  {0x83CB23B4u, "DekeGoalie"},
  {0x83CB23B8u, "TendencyObservation"},
  {0x83CB23BCu, "UserCalledForPass"},
  {0x83CB23C0u, "PuckHeadingTowardsBench"},
  {0x83CB23C4u, "IntentToHitStart"}
};

std::mutex g_mu;
std::atomic<bool> g_started{false};
std::atomic<bool> g_slots_dumped{false};
std::atomic<int> g_post_logs{0};
std::atomic<uint32_t> g_last_mgr{0};
std::atomic<bool> g_pen_immunity{false};  // round 9 behaviour test
std::atomic<int> g_pen_blocked{0};
std::atomic<float> g_floor_min{0.f};  // round 10 contact floor test
std::atomic<int> g_floor_promoted{0};
std::atomic<float> g_shock_radius{0.f};  // round 12 shockwave test
std::atomic<bool> g_shock_armed{false};
std::atomic<int> g_shock_count{0};
std::atomic<int> g_shock_mode{1};  // round 15: 0 / 1 / 2, see ForceKnockdown
std::atomic<float> g_shock_push{0.f};
std::atomic<bool> g_shock_stored{false};
std::atomic<int> g_shock_method{1};
std::atomic<int> g_window_promoted{0};
std::atomic<int> g_shock_bal{0};  // round 17: off by default (round 16: no effect)
std::atomic<float> g_shock_bal_amt{50.f};

void Log(const char* fmt, ...) {
  char buf[1024];
  int w = std::snprintf(buf, sizeof(buf), "t=%lu ", (unsigned long)GetTickCount());
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf + w, sizeof(buf) - size_t(w), fmt, ap);
  va_end(ap);
  std::lock_guard<std::mutex> lk(g_mu);
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_probe_s.txt";
  if (FILE* f = std::fopen(path.string().c_str(), "a")) {
    std::fputs(buf, f);
    std::fputc('\n', f);
    std::fclose(f);
  }
}

// Per-hook line budget per second (the puck contact and the penalty decision can be busy).
struct Budget {
  std::atomic<unsigned> sec{0};
  std::atomic<int> n{0};
  bool Take(int max_per_sec) {
    const unsigned s = unsigned(GetTickCount() / 1000u);
    if (sec.exchange(s) != s) n.store(0);
    return n.fetch_add(1) < max_per_sec;
  }
};
Budget g_b_contact, g_b_decide, g_b_floor, g_b_args, g_b_bal;

uint8_t* VBase() {
  auto* ks = rex::runtime::current_kernel_state();
  return (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
}
const uint8_t* Host(const uint8_t* vb, uint32_t va) { return vb + va + REX_PHYS_HOST_OFFSET(va); }
bool Readable(const uint8_t* p, size_t n) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
  return p + n <= static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
}
bool InImage(uint32_t va) { return va >= 0x82000000u && va < 0x83EA0000u; }
bool HeapPtr(uint32_t va) { return va >= 0x40000000u && !InImage(va); }

// Image globals are always mapped; anything else is checked first.
uint32_t U32(const uint8_t* vb, uint32_t va) {
  if (!vb || !va) return 0;
  const uint8_t* h = Host(vb, va);
  if (!InImage(va) && !Readable(h, 4)) return 0;
  return (uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3];
}
int U8(const uint8_t* vb, uint32_t va) {
  if (!vb || !va) return -1;
  const uint8_t* h = Host(vb, va);
  if (!InImage(va) && !Readable(h, 1)) return -1;
  return h[0];
}
float F32(const uint8_t* vb, uint32_t va) {
  const uint32_t b = U32(vb, va);
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
int Idx(const uint8_t* vb, uint32_t player) {
  return HeapPtr(player) ? int(int8_t(U8(vb, player + kOffIndex))) : -1;
}
int Team(const uint8_t* vb, uint32_t player) {
  return HeapPtr(player) ? U8(vb, player + kOffTeam) : -1;
}
// "who" tag: is this player the cached BAP player (C) and / or the panel claim (M)?
const char* Who(const uint8_t* vb, uint32_t player) {
  if (!player) return "-";
  const bool c = U32(vb, kVaBapCache) == player;
  const bool m = rogue::BapPlayer() == player;
  return c ? (m ? "CM" : "C") : (m ? "M" : "-");
}

void DumpSlots(const uint8_t* vb) {
  int filled = 0;
  for (const Slot& s : kSlots) filled += U32(vb, s.va) != 0;
  if (filled < 20) return;  // not filled yet (types register lazily); try again next second
  if (g_slots_dumped.exchange(true)) return;
  Log("slots begin (%d of %d filled)", filled, int(sizeof(kSlots) / sizeof(kSlots[0])));
  for (const Slot& s : kSlots) Log("slot %08X %08X %s", s.va, U32(vb, s.va), s.name);
  Log("slots end");
}

void DumpPlayer(const uint8_t* vb, const char* why, uint32_t p) {
  if (!HeapPtr(p)) { Log("player %s %08X (not a heap pointer)", why, p); return; }
  char r[kNumRatings * 6 + 8];
  int w = 0;
  for (int i = 0; i < kNumRatings; ++i)
    w += std::snprintf(r + w, sizeof(r) - size_t(w), "%d%s",
                       int(int32_t(U32(vb, p + kOffRatings + 4u * uint32_t(i)))), i + 1 < kNumRatings ? "," : "");
  Log("player %s %08X class=%08X idx=%d team=%d marker=%02X energy=%.3f cap=%.3f injury=%08X claim=%08X ratings[0..28]=%s",
      why, p, U32(vb, p), Idx(vb, p), Team(vb, p), unsigned(U8(vb, p + kOffMarker)) & 0xFFu,
      double(F32(vb, p + kOffEnergy)), double(F32(vb, p + kOffFatigue)), U32(vb, p + kOffInjury),
      rogue::BapPlayer(), r);
}

// Once a second: log the CANDIDATE globals when they change; full player line when the cache or
// the claim changes, and every 30 s while a BAP player is cached. Also dumps the type slots once.
void Sampler() {
  uint32_t last[6] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
  uint32_t last_claim = 0;
  int secs = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const uint8_t* vb = VBase();
    if (!vb) continue;
    const uint32_t now[6] = {U32(vb, kVaGameState), U32(vb, kVaFlowPhase), U32(vb, kVaPeriod),
                             uint32_t(U8(vb, kVaBapFlag)), U32(vb, kVaMatch), U32(vb, kVaBapCache)};
    const uint32_t claim = rogue::BapPlayer();
    if (std::memcmp(now, last, sizeof(now)) != 0 || claim != last_claim) {
      Log("sample state=%d(%s) phase=%d period=%d bapflag=%d match=%08X bapcache=%08X claim=%08X cache==claim:%s",
          int32_t(now[0]), StateName(int32_t(now[0])), int32_t(now[1]), int32_t(now[2]), int32_t(now[3]),
          now[4], now[5], claim, (now[5] && now[5] == claim) ? "yes" : "no");
      if (now[5] != last[5] && now[5]) DumpPlayer(vb, "cache-changed", now[5]);
      if (claim != last_claim && claim) DumpPlayer(vb, "claim-changed", claim);
      std::memcpy(last, now, sizeof(now));
      last_claim = claim;
    }
    if (now[5] && ++secs % 30 == 0) DumpPlayer(vb, "periodic", now[5]);
    if (now[4]) DumpSlots(vb);
  }
}

// Round 12-14 velocity hunt removed 2026-11-04 (done: velocity at body +0x50 / +0x60 / +0x70, re-findings.md).

// Round 29 (2026-11-04): sprint flag hunt for Jet Skates (owner: DS4 via Steam, no XInput; wants the game's own
// sprint state). 20 times a second, while the game is in GAME_ON and the pro is on the ice (+0x22C == 0), read
// every byte of the pro object (0x7B0) [round 30: on the ice = +0x22C != 0xFF; round 29 collected nothing
// because the marker was 0x06 that game] and of his physics body *(pro + 32) (0x200), plus his speed (live
// position *(pro + 32) + 48 / + 56, CANDIDATE from S10, metres per second from the change) and his energy drain
// (+360 per sample). Per byte and value: samples, summed speed, summed drain. A sprint flag is a byte whose
// non-usual values come with clearly higher speed and drain. Report every 60 s: `sprint-hunt` lines.
constexpr uint32_t kHuntPro = 0x7B0u, kHuntBody = 0x200u, kHuntN = kHuntPro + kHuntBody;
struct HuntCell { uint32_t n; float speed, drain; };
HuntCell* g_hunt = nullptr;  // [kHuntN][256]
uint32_t g_hunt_toggles[kHuntN];
uint8_t g_hunt_last[kHuntN];
unsigned g_hunt_samples = 0;
double g_hunt_speed_sum = 0.0;
void HuntReport(const char* why) {
  struct Cand { uint32_t off; int mode; uint32_t on_n, off_n, tog; double sp_on, sp_off, dr_on, dr_off; int on_val; };
  Cand best[25];
  int nb = 0;
  for (uint32_t o = 0; o < kHuntN; ++o) {
    const HuntCell* c = g_hunt + size_t(o) * 256u;
    int mode = 0;
    for (int v = 1; v < 256; ++v) if (c[v].n > c[mode].n) mode = v;
    Cand x{o, mode, 0, c[mode].n, g_hunt_toggles[o], 0, 0, 0, 0, -1};
    double sp = 0, dr = 0;
    uint32_t on_top = 0;
    for (int v = 0; v < 256; ++v) {
      if (v == mode || !c[v].n) continue;
      x.on_n += c[v].n; sp += c[v].speed; dr += c[v].drain;
      if (c[v].n > on_top) { on_top = c[v].n; x.on_val = v; }
    }
    if (x.on_n < 30 || x.off_n < 30 || x.tog < 6) continue;
    x.sp_on = sp / x.on_n; x.dr_on = dr / x.on_n;
    x.sp_off = c[mode].speed / c[mode].n; x.dr_off = c[mode].drain / c[mode].n;
    const double score = x.sp_on - x.sp_off;
    int pos = nb < 25 ? nb++ : 24;
    if (nb == 25 && pos == 24 && score <= best[24].sp_on - best[24].sp_off) continue;
    best[pos] = x;
    for (int i = pos; i > 0 && (best[i].sp_on - best[i].sp_off) > (best[i - 1].sp_on - best[i - 1].sp_off); --i) std::swap(best[i], best[i - 1]);
  }
  Log("sprint-hunt %s: %u samples on the ice, mean speed %.2f m/s; top bytes by speed when not at their usual value:", why,
      g_hunt_samples, g_hunt_samples ? g_hunt_speed_sum / g_hunt_samples : 0.0);
  for (int i = 0; i < nb; ++i) {
    const Cand& x = best[i];
    Log("sprint-hunt  %s+0x%03X usual=%02X other=%02X  on %u / off %u samples, toggles %u  speed %.2f vs %.2f  drain %.4f vs %.4f",
        x.off < kHuntPro ? "pro" : "body", x.off < kHuntPro ? x.off : x.off - kHuntPro, x.mode, x.on_val & 0xFF, x.on_n,
        x.off_n, x.tog, x.sp_on, x.sp_off, x.dr_on, x.dr_off);
  }
}
void SprintHunt() {
  g_hunt = new HuntCell[size_t(kHuntN) * 256u]();
  uint8_t buf[kHuntN];
  uint32_t lp = 0;
  float lx = 0.f, lz = 0.f, le = 0.f;
  bool have = false;
  auto next_report = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (std::chrono::steady_clock::now() >= next_report) {
      next_report += std::chrono::seconds(60);
      if (g_hunt_samples) HuntReport("60s");
    }
    const uint8_t* vb = VBase();
    if (!vb || !U32(vb, kVaMatch) || U8(vb, kVaBapFlag) != 1 || U32(vb, kVaGameState) != 4u) { have = false; continue; }
    const uint32_t pl = U32(vb, kVaBapCache);
    if (!HeapPtr(pl) || U8(vb, pl + kOffMarker) == 0xFF) { have = false; continue; }  // round 30: 0x06 on the ice too
    const uint32_t body = U32(vb, pl + 32);
    if (!HeapPtr(body) || !Readable(Host(vb, pl), kHuntPro) || !Readable(Host(vb, body), kHuntBody)) { have = false; continue; }
    std::memcpy(buf, Host(vb, pl), kHuntPro);
    std::memcpy(buf + kHuntPro, Host(vb, body), kHuntBody);
    const float x = F32(vb, body + 48), zz = F32(vb, body + 56), e = F32(vb, pl + kOffEnergy);
    if (!have || pl != lp) { have = true; lp = pl; lx = x; lz = zz; le = e; std::memcpy(g_hunt_last, buf, kHuntN); continue; }
    const float dx = x - lx, dz = zz - lz;
    const float speed = std::sqrt(dx * dx + dz * dz) / 0.05f;
    const float drain = le - e;
    lx = x; lz = zz; le = e;
    if (!std::isfinite(speed) || speed > 30.f || !std::isfinite(drain) || std::fabs(drain) > 5.f) continue;
    for (uint32_t o = 0; o < kHuntN; ++o) {
      HuntCell& cell = g_hunt[size_t(o) * 256u + buf[o]];
      ++cell.n; cell.speed += speed; cell.drain += drain;
      if (buf[o] != g_hunt_last[o] && g_hunt_toggles[o] < 0xFFFFFFFFu) ++g_hunt_toggles[o];
    }
    std::memcpy(g_hunt_last, buf, kHuntN);
    ++g_hunt_samples;
    g_hunt_speed_sum += speed;
  }
}

void Start() {
  if (g_started.exchange(true)) return;
  Log("static probe S1 started (static-map.md section 14)");
  std::thread(Sampler).detach();
  std::thread(SprintHunt).detach();  // round 29
}

}  // namespace

// ---------------------------------------------------------------- hooks (pass-through)
// Literal names on purpose (same as rogue_hooks.cpp). One TU per overridden function.

// B. Game state switch: r3 = new state; old state = word 0x83C1D220 before the call.
REX_EXTERN(__imp__sub_8330C890);
extern "C" REX_FUNC(sub_8330C890) {
  const bool on = rogue::probe::Enabled();
  int32_t from = 0;
  const int32_t to = int32_t(ctx.r3.u32);
  const unsigned long long lr = (unsigned long long)ctx.lr;
  if (on) { Start(); from = int32_t(U32(base, kVaGameState)); }
  __imp__sub_8330C890(ctx, base);
  if (on)
    Log("state %d(%s) -> %d(%s) period=%d phase=%d bapflag=%d bapcache=%08X lr=%08llX", from,
        StateName(from), to, StateName(to), int32_t(U32(base, kVaPeriod)), int32_t(U32(base, kVaFlowPhase)),
        U8(base, kVaBapFlag), U32(base, kVaBapCache), lr);
}

// C. Goal: r3 scorer, r4 = the goalie scored on (round S1: NOT the assister), r9 kind (0 / 2).
// Simulated play ("sim to next shift") scores without calling this (round S1).
REX_EXTERN(__imp__sub_83480248);
extern "C" REX_FUNC(sub_83480248) {
  const bool on = rogue::probe::Enabled();
  const uint32_t scorer = ctx.r3.u32, goalie = ctx.r4.u32, kind = ctx.r9.u32;
  const unsigned long long lr = (unsigned long long)ctx.lr;
  if (on) {
    Start();
    Log("goal scorer=%08X idx=%d team=%d who=%s goalie=%08X idx=%d team=%d r9=%d period=%d lr=%08llX", scorer,
        Idx(base, scorer), Team(base, scorer), Who(base, scorer), goalie, Idx(base, goalie), Team(base, goalie),
        int32_t(kind), int32_t(U32(base, kVaPeriod)), lr);
    // Round 20 stat finder: checkpoint a few seconds after the goal.
    const uint32_t pro = rogue::AutoBapPlayer();
    const int st = Team(base, scorer), pt = Team(base, pro);
    char lb[64];
    std::snprintf(lb, sizeof(lb), "goal by %s (period word %d)",
                  scorer && scorer == pro ? "the PRO" : (st >= 0 && st == pt) ? "the pro's team" : "the opponent",
                  int32_t(U32(base, kVaPeriod)));
    rogue::probe::StatEvent(lb);
  }
  __imp__sub_83480248(ctx, base);
}

// C. Period end (EndPeriod raiser): r3 = flag byte.
REX_EXTERN(__imp__sub_8331F408);
extern "C" REX_FUNC(sub_8331F408) {
  const bool on = rogue::probe::Enabled();
  const uint32_t flag = ctx.r3.u32;
  if (on) {
    Start();
    Log("period-end flag=%u period=%d state=%d", flag & 0xFFu, int32_t(U32(base, kVaPeriod)),
        int32_t(U32(base, kVaGameState)));
    char lb[48];
    std::snprintf(lb, sizeof(lb), "period end (period word %d)", int32_t(U32(base, kVaPeriod)));
    rogue::probe::StatEvent(lb);  // round 20 stat finder
  }
  __imp__sub_8331F408(ctx, base);
  rogue::NotePeriodEnd();  // round 27 (not probe-gated): the box score search waits for the intermission
}

// C. Puck contact: r3 puck, r4 player hit, r5 contact code; last-touch player index at puck+180.
REX_EXTERN(__imp__sub_83328180);
extern "C" REX_FUNC(sub_83328180) {
  const bool on = rogue::probe::Enabled();
  const uint32_t puck = ctx.r3.u32, victim = ctx.r4.u32, code = ctx.r5.u32;
  if (on && g_b_contact.Take(20)) {
    Start();
    const int32_t last_touch = HeapPtr(puck) ? int32_t(U32(base, puck + 180)) : -99;
    Log("puck-contact puck=%08X victim=%08X idx=%d who=%s code=%d last_touch_idx=%d", puck, victim,
        Idx(base, victim), Who(base, victim), int32_t(code), last_touch);
  }
  __imp__sub_83328180(ctx, base);
}

// C. Penalty decision sub_83487DC0: hooked in rogue_hooks.cpp since 2026-11-03 (perk Untouchable);
// it calls rogue::probe::PenaltyDecided below for the pen-decide / pen-immunity lines.

// C. Penalty applied: r4 offender, r5 victim, r6 flags, r7 type.
REX_EXTERN(__imp__sub_83485BD0);
extern "C" REX_FUNC(sub_83485BD0) {
  const bool on = rogue::probe::Enabled();
  const uint32_t off = ctx.r4.u32, vic = ctx.r5.u32, flags = ctx.r6.u32, type = ctx.r7.u32;
  const unsigned long long lr = (unsigned long long)ctx.lr;
  if (on) {
    Start();
    Log("pen-apply offender=%08X idx=%d team=%d who=%s victim=%08X idx=%d who=%s flags=%08X type=%d lr=%08llX", off,
        Idx(base, off), Team(base, off), Who(base, off), vic, Idx(base, vic), Who(base, vic), flags, int32_t(type), lr);
  }
  __imp__sub_83485BD0(ctx, base);
}

// D. Event manager getter: log the manager, its vtable and vtable slot 1 (= the post function).
REX_EXTERN(__imp__sub_82FEC310);
extern "C" REX_FUNC(sub_82FEC310) {
  __imp__sub_82FEC310(ctx, base);
  if (!rogue::probe::Enabled()) return;
  const uint32_t mgr = ctx.r3.u32;
  if (!mgr || g_last_mgr.exchange(mgr) == mgr || g_post_logs.fetch_add(1) >= 6) return;
  Start();
  const uint32_t vt = U32(base, mgr);
  Log("event-manager mgr=%08X vtable=%08X slot0=%08X post(slot1)=%08X slot2=%08X", mgr, vt,
      InImage(vt) ? U32(base, vt) : 0u, InImage(vt) ? U32(base, vt + 4) : 0u, InImage(vt) ? U32(base, vt + 8) : 0u);
}

// ---------------------------------------------------------------- round 9 public helpers
namespace rogue::probe {
void SprintHuntReport() { if (g_hunt && g_hunt_samples) HuntReport("game over"); }  // round 29
void StaticNote(const char* msg) {
  if (!Enabled() || !msg) return;
  Start();
  Log("note %s", msg);
}
void PenaltyDecided(uint32_t off, uint32_t vic, uint32_t type, uint32_t result, bool blocked,
                    unsigned long long lr) {
  if (!Enabled()) return;
  const uint8_t* vb = VBase();
  if (!vb) return;
  if (blocked) {
    const int n = g_pen_blocked.fetch_add(1) + 1;
    Start();
    Log("pen-immunity BLOCKED #%d type=%d offender=%08X idx=%d victim=%08X idx=%d lr=%08llX", n, int32_t(type),
        off, Idx(vb, off), vic, Idx(vb, vic), lr);
  }
  if (g_b_decide.Take(10)) {
    Start();
    Log("pen-decide result=%u type=%d offender=%08X idx=%d who=%s victim=%08X idx=%d who=%s lr=%08llX", result,
        int32_t(type), off, Idx(vb, off), Who(vb, off), vic, Idx(vb, vic), Who(vb, vic), lr);
  }
}
void SetPenaltyImmunityTest(bool on) {
  if (g_pen_immunity.exchange(on) != on && Enabled()) {
    Start();
    Log("pen-immunity test %s", on ? "ON" : "OFF");
  }
}
bool PenaltyImmunityTest() { return g_pen_immunity.load(); }
int PenaltyImmunityBlocked() { return g_pen_blocked.load(); }
void SetContactFloorTest(float min_power) {
  if (g_floor_min.exchange(min_power) != min_power && Enabled()) {
    Start();
    Log("contact-floor test min=%.2f", double(min_power));
  }
}
float ContactFloorTest() { return Enabled() ? g_floor_min.load() : 0.f; }
void NoteContactFloorPromoted() {
  const int n = g_floor_promoted.fetch_add(1) + 1;
  if (Enabled() && g_b_floor.Take(5)) {
    Start();
    Log("contact-floor PROMOTED #%d (min=%.2f)", n, double(g_floor_min.load()));
  }
}
int ContactFloorPromoted() { return g_floor_promoted.load(); }
void SetShockwaveTest(float radius) {
  if (g_shock_radius.exchange(radius) != radius && Enabled()) {
    Start();
    Log("shockwave test radius=%.1f", double(radius));
  }
  if (radius <= 0.f) g_shock_armed.store(false);
}
float ShockwaveTest() { return Enabled() ? g_shock_radius.load() : 0.f; }
void ArmShockwave() {
  if (ShockwaveTest() <= 0.f) return;
  if (!g_shock_armed.exchange(true)) { Start(); Log("shockwave ARMED"); }
}
bool TakeShockwaveArm() { return g_shock_armed.exchange(false); }
bool ShockwaveArmed() { return g_shock_armed.load(); }
void ShockwaveFired(const char* line, int knocked_down) {
  g_shock_count.fetch_add(1);
  if (!Enabled()) return;
  Start();
  Log("%s (knocked down %d)", line ? line : "shockwave", knocked_down);
}
int ShockwaveCount() { return g_shock_count.load(); }
void SetShockwaveMode(int mode) {
  if (g_shock_mode.exchange(mode) != mode && Enabled()) { Start(); Log("shockwave mode=%d", mode); }
}
int ShockwaveMode() { return g_shock_mode.load(); }
void SetShockwavePush(float s) {
  if (g_shock_push.exchange(s) != s && Enabled()) { Start(); Log("shockwave push=%.0f", double(s)); }
}
float ShockwavePush() { return g_shock_push.load(); }
void SetShockwaveBalance(int m) {
  if (g_shock_bal.exchange(m) != m && Enabled()) { Start(); Log("shockwave balance mode=%d", m); }
}
int ShockwaveBalance() { return g_shock_bal.load(); }
void SetShockwaveForceStored(bool on) {
  if (g_shock_stored.exchange(on) != on && Enabled()) { Start(); Log("shockwave force-stored=%d", int(on)); }
}
bool ShockwaveForceStored() { return g_shock_stored.load(); }
void SetShockwaveMethod(int m) {
  if (g_shock_method.exchange(m) != m && Enabled()) { Start(); Log("shockwave method=%d", m); }
}
int ShockwaveMethod() { return g_shock_method.load(); }
void NoteWindowPromoted(uint32_t victim) {
  const int n = g_window_promoted.fetch_add(1) + 1;
  if (!Enabled() || !g_b_floor.Take(10)) return;
  const uint8_t* vb = VBase();
  Start();
  Log("window PROMOTED #%d victim=%08X idx=%d", n, victim, vb ? Idx(vb, victim) : -1);
}
int WindowPromoted() { return g_window_promoted.load(); }
void SetShockwaveBalanceAmount(float a) {
  if (g_shock_bal_amt.exchange(a) != a && Enabled()) { Start(); Log("shockwave balance amount=%.0f", double(a)); }
}
float ShockwaveBalanceAmount() { return g_shock_bal_amt.load(); }
void BalanceSeen(uint32_t player, uint32_t kind, float amount, float before, float after) {
  if (!Enabled() || !g_b_bal.Take(4)) return;
  const uint8_t* vb = VBase();
  Start();
  Log("balance-penalty player=%08X idx=%d kind=%u amount=%.3f balance %.2f -> %.2f", player,
      vb ? Idx(vb, player) : -1, kind, double(amount), double(before), double(after));
}
void ForcedBalance(uint32_t victim, int mode, float before, float after) {
  if (!Enabled()) return;
  const uint8_t* vb = VBase();
  Start();
  Log("forced-balance victim=%08X idx=%d mode=%d balance %.2f -> %.2f", victim, vb ? Idx(vb, victim) : -1, mode,
      double(before), double(after));
}
void ContactArgs(uint32_t victim, uint32_t hitter, bool pro_hit, uint32_t desc, uint32_t applied,
                 uint32_t kind, uint32_t fac_bits, uint32_t v40_in, uint32_t v40_out) {
  if (!Enabled() || !g_b_args.Take(10)) return;
  float fac;
  std::memcpy(&fac, &fac_bits, 4);
  const uint8_t* vb = VBase();
  Start();
  Log("contact-args %s %s victim=%08X idx=%d hitter=%08X idx=%d applied=%u kind=%d fac=%.3f v40 %d->%d",
      pro_hit ? "PRO-HITS" : "PRO-HIT", desc == 0x83ABF598u ? "KNOCKDOWN" : "hit", victim, vb ? Idx(vb, victim) : -1,
      hitter, vb ? Idx(vb, hitter) : -1, applied, int32_t(kind), double(fac), int32_t(v40_in), int32_t(v40_out));
}
void ForcedArgs(uint32_t victim, int mode, uint32_t kind, bool ok, uint32_t v40_in, uint32_t v40_out) {
  if (!Enabled()) return;
  const uint8_t* vb = VBase();
  Start();
  Log("forced victim=%08X idx=%d mode=%d kind=%d result=%s v40 %d->%d", victim, vb ? Idx(vb, victim) : -1, mode,
      int32_t(kind), ok ? "ok" : "refused", int32_t(v40_in), int32_t(v40_out));
}
void PollKeys() {
  static bool prev = false;
  const bool down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
  if (down && !prev) ArmShockwave();
  prev = down;
}
}  // namespace rogue::probe
