#include "rogue/rogue_mod.h"
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <rex/filesystem.h>
#include "rogue/addresses.h"
#include "rogue/fx.h"
#include "rogue/perks.h"
#include "rogue/probe.h"
#include "rogue/reactions.h"
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"

namespace rogue {
namespace {
TunablePatcher* g_patcher = nullptr;
bool g_in_game = false;
bool g_pending_apply = false;  // BAP game started before the tweak index was ready
const bool g_enabled = [] { const char* e = std::getenv("NHL_ROGUE"); return e && *e == '1'; }();
constexpr uint32_t kReactionLayer = 0xFFFF0000u;
std::chrono::steady_clock::time_point g_t0;
std::atomic<uint32_t> g_last_hitter{0}, g_last_victim{0}, g_bap_player{0};
std::atomic<uint32_t> g_cached_pro{0};  // round 25
// Round 28: controller read for Jet Skates (XInput loaded at run time: no link change).
struct PadState { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, PadState*);
XInputGetStateFn XInputFn() {
  static XInputGetStateFn fn = [] {
    const wchar_t* dlls[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (const wchar_t* d : dlls)
      if (HMODULE h = LoadLibraryW(d))
        if (auto p = GetProcAddress(h, "XInputGetState")) return reinterpret_cast<XInputGetStateFn>(reinterpret_cast<void*>(p));
    return XInputGetStateFn(nullptr);
  }();
  return fn;
}
// Bits: XInput button bits; 1 << 16 = left trigger, 1 << 17 = right trigger (> 30 %).
const char* const kSprintNames[] = {"Left stick click", "Right stick click", "LB", "RB", "LT", "RT", "A", "B", "X", "Y"};
const unsigned kSprintBits[] = {0x0040u, 0x0080u, 0x0100u, 0x0200u, 1u << 16, 1u << 17, 0x1000u, 0x2000u, 0x4000u, 0x8000u};
constexpr int kNumSprint = int(sizeof(kSprintBits) / sizeof(kSprintBits[0]));
int g_sprint_button = 0;  // default: left stick click
std::atomic<bool> g_sprint_held{false};
std::atomic<unsigned> g_pad_now{0xFFFFFFFFu};
unsigned g_pad_poll_frames = 0;
void PollPad() {
  XInputGetStateFn fn = XInputFn();
  unsigned bits = 0xFFFFFFFFu;
  if (fn) {
    // Stay on a pad once found; probe the others only every 120 frames (XInputGetState on an empty slot is slow).
    static int pad = -1;
    PadState s{};
    if (pad >= 0 && fn(DWORD(pad), &s) == 0) bits = s.buttons | (s.lt > 76 ? 1u << 16 : 0u) | (s.rt > 76 ? 1u << 17 : 0u);
    else if (g_pad_poll_frames++ % 120 == 0) {
      pad = -1;
      for (int i = 0; i < 4 && pad < 0; ++i)
        if (fn(DWORD(i), &s) == 0) { pad = i; bits = s.buttons | (s.lt > 76 ? 1u << 16 : 0u) | (s.rt > 76 ? 1u << 17 : 0u); }
    }
  }
  g_pad_now.store(bits, std::memory_order_relaxed);
  g_sprint_held.store(bits != 0xFFFFFFFFu && (bits & kSprintBits[g_sprint_button]) != 0, std::memory_order_relaxed);
}
std::string SettingsPath() { return (rex::filesystem::GetExecutableFolder() / "rogue_settings.txt").string(); }
void LoadSettings() {
  std::ifstream f(SettingsPath());
  std::string line;
  while (std::getline(f, line))
    if (line.rfind("sprint_button=", 0) == 0) {
      const int v = std::atoi(line.c_str() + 14);
      if (v >= 0 && v < kNumSprint) g_sprint_button = v;
    }
}
void SaveSettings() {
  std::ofstream f(SettingsPath(), std::ios::trunc);
  f << "sprint_button=" << g_sprint_button << '\n';
}
const bool g_auto = [] { const char* e = std::getenv("NHL_ROGUE_AUTO"); return !(e && *e == '0'); }();
bool g_awaiting_result = false;  // game over seen, result not entered yet
bool g_auto_started = false;     // current / last game started by the poll
uint32_t g_auto_match = 0;       // match object the current / last game belongs to
bool g_seen_match = false;       // a live BAP match was seen during the current game
int32_t g_prev_state = -1;

// One line in rogue_probe_s.txt (only with NHL_ROGUE_PROBE=1).
void Note(const char* fmt, ...) {
  if (!probe::Enabled()) return;
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  probe::StaticNote(buf);
}

uint32_t LayerId(size_t perk_idx, int stack) { return uint32_t(perk_idx * 16 + stack); }

// Persistent layers: owned perks (non-scoped) + the league's heat reaction.
void ApplyLayers() {
  if (!g_patcher) return;
  if (!g_patcher->Ready()) { g_pending_apply = true; g_patcher->Index(); return; }
  g_pending_apply = false;
  g_patcher->RevertAll();
  auto& run = Run().run();
  for (size_t i = 0; i < run.perks.size(); ++i) {
    auto* p = FindPerk(run.perks[i].id.c_str());
    if (!p || p->scope == Scope::kBapScoped || p->scope == Scope::kProTunable) continue;  // hook-scoped perks
    for (int s = 0; s < run.perks[i].stacks; ++s) g_patcher->AddLayer(LayerId(i, s), p->mods);
  }
  g_patcher->AddLayer(kReactionLayer, ReactionMods(Run().Tier()));
}
// ---- BUG-3 (2026-11-04) ----------------------------------------------------------------------------
// Owner, first game with the probe off: the poll reported game over somewhere after the 1st period
// (noticed in the 3rd). Until then ONE poll with DONE entered, no match pointer or BAP flag 0 ended
// the game, and nothing could bring it back (the start needs a new match object). Cause not known yet
// (no log with the probe off). Now:
//  - an end condition must hold: DONE for 3 s (a real DONE lasts ~150 s before the match is cleared,
//    S1), match pointer 0 / BAP flag 0 for 10 s;
//  - a game over made by the poll is undone at the next FACEOFF of the same game (same match object or
//    same pro object); a new game always loads through PRE_GAME, which cancels that;
//  - the history of state / match / flag / pro changes is kept for the panel ("Start / end log") and,
//    with NHL_ROGUE_PROBE=1, written to rogue_probe_s.txt.
using Clock = std::chrono::steady_clock;
// FACEOFF = 3: OBSERVED (S1: image name table 0x83AA5B20 order, 44 faceoffs logged in one BAP game).
// Used only as the resume trigger; a faceoff only happens while a game is being played.
constexpr int32_t kStateFaceoff = 3;
constexpr double kDoneHoldS = 3.0;
constexpr double kGoneHoldS = 10.0;
constexpr size_t kLogCap = 48;
enum class EndKind { kNone, kDone, kMatchGone, kBapFlagOff };
const char* EndKindName(EndKind k) {
  switch (k) {
    case EndKind::kDone: return "DONE";
    case EndKind::kMatchGone: return "match gone";
    case EndKind::kBapFlagOff: return "BAP flag 0";
    default: return "none";
  }
}
std::vector<LifecycleEvent> g_log, g_log_over;
char g_over_reason[160] = "";
int g_resumes = 0;
bool g_done_entered = false;     // DONE entered during this game (a left-over DONE does not count)
EndKind g_end_kind = EndKind::kNone;
Clock::time_point g_end_since;
bool g_poll_over = false;        // the last game over came from the poll and may still be undone
uint32_t g_over_match = 0, g_game_pro = 0;
BoxScore g_box;   // RE-5 round 21: read at the last game over
int g_box_serial = 0;
GuestGame g_last_seen;
uint32_t g_last_pro = 0;
bool g_have_last = false;

float SinceInit() { return std::chrono::duration<float>(Clock::now() - g_t0).count(); }
double Secs(Clock::time_point since) { return std::chrono::duration<double>(Clock::now() - since).count(); }

void Log(const GuestGame& g, uint32_t pro, const char* fmt, ...) {
  LifecycleEvent e;
  e.t = SinceInit();
  e.state = g.state;
  e.match = g.match;
  e.bap = g.bap;
  e.pro = pro;
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(e.what, sizeof(e.what), fmt, ap);
  va_end(ap);
  if (g_log.size() >= kLogCap) g_log.erase(g_log.begin());
  g_log.push_back(e);
  Note("lifecycle: t=%.1f %s (state=%d match=%08X bap=%d pro=%08X)", double(e.t), e.what, int(g.state),
       g.match, int(g.bap), pro);
}

// Changes only. State changes are logged while a game is running or waiting for its result.
void Track(const GuestGame& g, uint32_t pro) {
  if (!g_have_last) {
    g_have_last = true;
    g_last_seen = g;
    g_last_pro = pro;
    Log(g, pro, "first read");
    return;
  }
  if (g.match != g_last_seen.match) Log(g, pro, "match %08X -> %08X", g_last_seen.match, g.match);
  if (g.bap != g_last_seen.bap) Log(g, pro, "BAP flag %d -> %d", int(g_last_seen.bap), int(g.bap));
  if (pro != g_last_pro) Log(g, pro, "pro %08X -> %08X", g_last_pro, pro);
  if (g.state != g_last_seen.state && (g_in_game || g_awaiting_result))
    Log(g, pro, "state %s -> %s", GameStateName(g_last_seen.state), GameStateName(g.state));
  g_last_seen = g;
  g_last_pro = pro;
}

// Round 9: start / end the BAP game from the game's state machine (addresses.h, CONFIRMED S1).
void PollLifecycle() {
#if defined(ROGUE_GAME_STATE_DONE)
  if (!g_auto) return;
  const GuestGame g = PollGuestGame();
  if (!g.valid) return;
  const uint32_t pro = (g.match && g.bap) ? AutoBapPlayer() : 0u;
  Track(g, pro);
  const bool entered = g.state != g_prev_state;  // this poll saw a state change
  g_prev_state = g.state;
  if (!g.match) g_auto_match = 0;  // match object gone: the next one is a new game
  if (g_in_game) {
    if (g.match && g.bap) g_seen_match = true;
    if (g.state != ROGUE_GAME_STATE_DONE) g_done_entered = false;
    else if (entered) g_done_entered = true;
    const EndKind kind = !g_seen_match    ? EndKind::kNone
                         : g_done_entered ? EndKind::kDone
                         : !g.match       ? EndKind::kMatchGone
                         : !g.bap         ? EndKind::kBapFlagOff
                                          : EndKind::kNone;
    if (kind != g_end_kind) {
      if (g_end_kind != EndKind::kNone)
        Log(g, pro, "end check %s cleared after %.1f s", EndKindName(g_end_kind), Secs(g_end_since));
      if (kind != EndKind::kNone) Log(g, pro, "end check %s started", EndKindName(kind));
      g_end_kind = kind;
      g_end_since = Clock::now();
      return;
    }
    if (kind == EndKind::kNone) return;
    const double held = Secs(g_end_since);
    if (held < (kind == EndKind::kDone ? kDoneHoldS : kGoneHoldS)) return;
    std::snprintf(g_over_reason, sizeof(g_over_reason), "%s held %.0f s, at t=%.0f s (state %s, %s, BAP flag %d)",
                  EndKindName(kind), held, double(SinceInit()), GameStateName(g.state),
                  g.match ? "match live" : "no match", int(g.bap));
    Log(g, pro, "GAME OVER (%s)", EndKindName(kind));
    g_end_kind = EndKind::kNone;
    g_over_match = g.match;
    OnBapGameOver();
    g_poll_over = true;
    g_log_over = g_log;
    return;
  }
  // A new game loads through PRE_GAME (S1, CONFIRMED): from then on the old game over is final.
  if (g_poll_over && entered && g.state == ROGUE_GAME_STATE_PRE_GAME) {
    g_poll_over = false;
    Log(g, pro, "PRE_GAME after game over: no resume");
  }
  // BUG-3 safety net: the game over came from the poll, but the same game goes on.
  if (g_poll_over && g_awaiting_result && entered && g.state == kStateFaceoff && g.match && g.bap && pro &&
      ((g_over_match && g.match == g_over_match) || pro == g_game_pro)) {
    Log(g, pro, "faceoff in the same game: resume");
    ResumeBapGame();
    return;
  }
  // Round 10: also wait for the game's own pro pointer (filled at PRE_GAME). Round 9 started ~9 s
  // early, while the state was still STORYTELLER left over from an exhibition.
  if (g.bap && g.match && g.state != ROGUE_GAME_STATE_DONE && g.match != g_auto_match &&
      Run().run().active && !g_awaiting_result && pro) {
    OnBapGameStart();
    if (g_in_game) {
      g_auto_started = true;
      if (probe::Enabled()) probe::ResetPlayers();
      Log(g, pro, "BAP game started automatically");
    }
  }
#endif
}
}  // namespace

bool Enabled() { return g_enabled; }
TunablePatcher* Patcher() { return g_patcher; }
bool InBapGame() { return g_enabled && g_in_game && Run().run().active; }

void NoteContact(uint32_t victim, uint32_t hitter) {
  g_last_victim.store(victim);
  g_last_hitter.store(hitter);
}
uint32_t LastContactHitter() { return g_last_hitter.load(); }
uint32_t LastContactVictim() { return g_last_victim.load(); }
void SetBapPlayer(uint32_t player) { g_bap_player.store(InBapGame() ? player : 0u); }
uint32_t BapPlayer() { return g_bap_player.load(); }
uint32_t CachedPro() { return g_cached_pro.load(std::memory_order_relaxed); }
bool SprintHeld() { return g_sprint_held.load(std::memory_order_relaxed); }
int SprintButton() { return g_sprint_button; }
void SetSprintButton(int i) {
  if (i < 0 || i >= kNumSprint || i == g_sprint_button) return;
  g_sprint_button = i;
  SaveSettings();
}
const char* const* SprintButtonNames(int* count) { *count = kNumSprint; return kSprintNames; }
unsigned PadButtonsNow() { return g_pad_now.load(std::memory_order_relaxed); }
uint32_t ProPlayer() {
  if (const uint32_t c = g_bap_player.load()) return c;
  return InBapGame() ? AutoBapPlayer() : 0u;
}
bool AwaitingResult() { return g_awaiting_result; }
bool AutoStarted() { return g_auto_started; }
bool AutoLifecycle() { return g_auto; }
const std::vector<LifecycleEvent>& LifecycleLog() { return g_log; }
const std::vector<LifecycleEvent>& LifecycleLogAtGameOver() { return g_log_over; }
const char* LastGameOverReason() { return g_over_reason; }
int LifecycleResumes() { return g_resumes; }
const BoxScore& LastBoxScore() { return g_box; }
int BoxScoreSerial() { return g_box_serial; }
const char* GameStateName(int32_t s) {
  static const char* n[] = {"PRE_GAME", "PRE_FACEOFF", "SYNCHING", "FACEOFF", "GAME_ON", "GOOF_OFF",
                            "PREFIGHT", "FIGHT", "POSTFIGHT", "DONE", "STORYTELLER", "NISVIEWER",
                            "PRACTICE_GOOFOFF", "TUTORIAL_GOOFOFF"};
  return (s >= 0 && s < 14) ? n[s] : "none";
}

// The parent store is no longer used for perks (it edits type tags, see tweak_index.h); the
// parameter stays so the parent wiring in nhllegacy_app.h does not change.
void Init(nhl::ui::ITunableStore* /*store*/) {
  if (!g_enabled) return;
  g_patcher = new TunablePatcher();
  g_t0 = std::chrono::steady_clock::now();
  LoadSettings();  // round 28
  Run().Load();
}

void Tick() {
  if (!g_enabled || !g_patcher) return;
  if (probe::Enabled()) {
    probe::PollKeys();     // round 12: F8 arms the shockwave test
    ShockwaveWindowTick();  // round 18: physics window
  }
  PollLifecycle();  // before the index check
  g_cached_pro.store(InBapGame() ? ProPlayer() : 0u, std::memory_order_relaxed);  // round 25 pro-only perks
  PollPad();  // round 28: Jet Skates sprint button
  if (InBapGame()) BoxScoreTick();  // RE-5: search in a cut-scene, identify the pro's record
  if (g_awaiting_result && !g_box.valid && !BoxScoreScanBusy()) {  // game-over search finished
    g_box = ReadBoxScore();
    if (g_box.valid) ++g_box_serial;
  }  // PollLifecycle runs first: ApplyLayers defers until the index is ready
  if (!g_patcher->Ready()) {
    // Build only after tweak registration (~12 s after boot) or when a game needs it.
    if (g_pending_apply || std::chrono::steady_clock::now() - g_t0 > std::chrono::seconds(20))
      g_patcher->Index();
    return;
  }
  if (g_pending_apply && g_in_game) ApplyLayers();
}

void OnBapGameStart() {
  if (!g_enabled || !Run().run().active || g_in_game) return;  // already running: keep its counters
  const GuestGame g = PollGuestGame();
  g_auto_match = g.match;  // a hand start belongs to this match too (no auto restart after a hand end)
  g_seen_match = g.valid && g.match && g.bap;
  if (g.valid) g_prev_state = g.state;
  g_awaiting_result = false;
  g_auto_started = false;
  g_done_entered = false;  // BUG-3
  g_end_kind = EndKind::kNone;
  g_poll_over = false;
  g_over_reason[0] = 0;
  g_game_pro = AutoBapPlayer();
  Log(g, g_game_pro, "game start");
  g_in_game = true;
  g_bap_player.store(0);  // player objects are re-created per game: claim again
  Run().run().live = GameStats{};
  ApplyLayers();
  probe::StatGameStart();  // round 20 stat finder (probe only)
  BoxScoreReset();         // round 22: stat records are per game
  ProTunableReset();       // round 25
}

void OnBapGameOver() {
  if (!g_enabled || !g_in_game) return;
  g_in_game = false;
  g_pending_apply = false;
  g_awaiting_result = true;
  if (g_patcher) g_patcher->RevertAll();  // rule 3, even if the result is never entered
  // RE-5 round 21: the game's box score, for the panel to fill in the result (the owner checks it).
  g_box = ReadBoxScore();
  if (!g_box.valid) BoxScoreGameOver();  // round 24: search now if nothing was found during the game
  if (g_box.valid) ++g_box_serial;
  Note("box: tables=%d identified=%d gf_from_goalie=%d toi=%d", g_box.tables, int(g_box.identified), int(g_box.gf_from_goalie), g_box.toi);
  Note("box: valid=%d idx=%d slot=%d G=%d A=%d +/-=%d PIM=%d SOG=%d hits=%d team %d-%d shots %d-%d rec=%08X",
       int(g_box.valid), g_box.index, g_box.slot, g_box.goals, g_box.assists, g_box.plus_minus, g_box.pim,
       g_box.shots, g_box.hits, g_box.team_gf, g_box.team_ga, g_box.team_shots, g_box.opp_shots, g_box.record_va);
  if (probe::Enabled() && g_box.record_va) {
    char hex[100];
    DumpBoxRecord(hex, sizeof(hex), g_box.record_va, 0, 44);
    Note("box-record 0..43: %s", hex);
    DumpBoxRecord(hex, sizeof(hex), g_box.record_va, 44, 44);
    Note("box-record 44..87: %s", hex);
  }
  fx::Push(fx::Kind::kMilestone, "GAME OVER");
  probe::StatGameOver();
  if (probe::Enabled()) probe::SprintHuntReport();  // round 29
}

void OnBapGameEnd(const GameStats& final_stats) {
  if (!g_enabled) return;
  g_poll_over = false;  // BUG-3: the result is in, this game is final
  g_in_game = false;
  g_awaiting_result = false;
  g_pending_apply = false;
  g_bap_player.store(0);
  if (g_patcher) g_patcher->RevertAll();  // never leak perks into other modes
  GameStats g = final_stats;
  g.knockdowns = Run().run().live.knockdowns;  // hook-counted
  g.crit_slaps = Run().run().live.crit_slaps;
  // Hits: the hit contact hook counts them once the BAP player is claimed in the panel;
  // without a claim live.hits stays 0 and the panel's manual entry is used.
  if (Run().run().live.hits > g.hits) g.hits = Run().run().live.hits;
  const HeatTier before = Run().Tier();
  Run().OnGameFinished(g);
  if (Run().Tier() > before) fx::Push(fx::Kind::kTierUp, HeatTierName(Run().Tier()));
  OpenShop();
}
// BUG-3: undo a game over. Perk layers come back, the game's counters (hits, knockdowns) are kept.
void ResumeBapGame() {
  if (!g_enabled || g_in_game || !g_awaiting_result || !Run().run().active) return;
  const GuestGame g = PollGuestGame();
  if (g.valid) g_prev_state = g.state;
  g_in_game = true;
  g_awaiting_result = false;
  g_poll_over = false;
  g_done_entered = false;
  g_end_kind = EndKind::kNone;
  ++g_resumes;
  Log(g, AutoBapPlayer(), "game resumed (counters kept)");
  ApplyLayers();
  fx::Push(fx::Kind::kMilestone, "GAME RESUMED");
  probe::StatResume();
}
}  // namespace rogue
