#include "rogue/rogue_mod.h"
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
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
    if (!p || p->scope == Scope::kBapScoped) continue;
    for (int s = 0; s < run.perks[i].stacks; ++s) g_patcher->AddLayer(LayerId(i, s), p->mods);
  }
  g_patcher->AddLayer(kReactionLayer, ReactionMods(Run().Tier()));
}
// Round 9: start / end the BAP game from the game's state machine (addresses.h, CONFIRMED S1).
void PollLifecycle() {
#if defined(ROGUE_GAME_STATE_DONE)
  if (!g_auto) return;
  const GuestGame g = PollGuestGame();
  if (!g.valid) return;
  const bool done_now = g.state == ROGUE_GAME_STATE_DONE && g_prev_state != ROGUE_GAME_STATE_DONE;
  g_prev_state = g.state;
  if (!g.match) g_auto_match = 0;  // match object gone: the next one is a new game
  if (g_in_game) {
    if (g.match && g.bap) g_seen_match = true;
    if (g_seen_match && (done_now || !g.match || !g.bap)) {
      Note("lifecycle: game over (state=%d match=%08X bap=%d)", int(g.state), g.match, int(g.bap));
      OnBapGameOver();
    }
    return;
  }
  // Round 10: also wait for the game's own pro pointer (filled at PRE_GAME). Round 9 started ~9 s
  // early, while the state was still STORYTELLER left over from an exhibition.
  if (g.bap && g.match && g.state != ROGUE_GAME_STATE_DONE && g.match != g_auto_match &&
      Run().run().active && !g_awaiting_result && AutoBapPlayer()) {
    OnBapGameStart();
    if (g_in_game) {
      g_auto_started = true;
      if (probe::Enabled()) probe::ResetPlayers();
      Note("lifecycle: BAP game started automatically (state=%d match=%08X pro=%08X)", int(g.state),
           g.match, AutoBapPlayer());
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
uint32_t ProPlayer() {
  if (const uint32_t c = g_bap_player.load()) return c;
  return InBapGame() ? AutoBapPlayer() : 0u;
}
bool AwaitingResult() { return g_awaiting_result; }
bool AutoStarted() { return g_auto_started; }
bool AutoLifecycle() { return g_auto; }

// The parent store is no longer used for perks (it edits type tags, see tweak_index.h); the
// parameter stays so the parent wiring in nhllegacy_app.h does not change.
void Init(nhl::ui::ITunableStore* /*store*/) {
  if (!g_enabled) return;
  g_patcher = new TunablePatcher();
  g_t0 = std::chrono::steady_clock::now();
  Run().Load();
}

void Tick() {
  if (!g_enabled || !g_patcher) return;
  if (probe::Enabled()) {
    probe::PollKeys();     // round 12: F8 arms the shockwave test
    ShockwaveWindowTick();  // round 18: physics window
  }
  PollLifecycle();  // before the index check: ApplyLayers defers until the index is ready
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
  g_in_game = true;
  g_bap_player.store(0);  // player objects are re-created per game: claim again
  Run().run().live = GameStats{};
  ApplyLayers();
}

void OnBapGameOver() {
  if (!g_enabled || !g_in_game) return;
  g_in_game = false;
  g_pending_apply = false;
  g_awaiting_result = true;
  if (g_patcher) g_patcher->RevertAll();  // rule 3, even if the result is never entered
  fx::Push(fx::Kind::kMilestone, "GAME OVER");
}

void OnBapGameEnd(const GameStats& final_stats) {
  if (!g_enabled) return;
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
}  // namespace rogue
