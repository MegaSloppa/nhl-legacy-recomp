// Legacy Rogue - public entry points the host app calls (see docs/user-guide.md "Wiring"):
//   rogue::Init(tunable_store);   // OnCreateDialogs, after the TunableRuntimeStore exists
//   rogue::Tick();                // once per frame
//   rogue::DrawPanel();           // inside the overlay window (Guide/F1)
//   rogue::fx::Draw();            // every frame, even with the overlay closed (task FX-1)
#pragma once
#include <cstdint>
#include <vector>
namespace nhl::ui { class ITunableStore; }

namespace rogue {
struct GameStats;
class TunablePatcher;

void Init(nhl::ui::ITunableStore* store);
void Tick();
void DrawPanel();
bool Enabled();  // NHL_ROGUE=1. Everything user-visible must check this (rule 1).

// Lifecycle. Called by hooks once addresses are known (addresses.h), or by the
// panel's manual buttons until then.
void OnBapGameStart();
void OnBapGameEnd(const GameStats& final_stats);
void OpenShop();  // between games

// For rogue_hooks.cpp
TunablePatcher* Patcher();
bool InBapGame();

// BAP player identification. RE-2 is not solved (no field or global found yet), so the owner
// claims his pro in the panel from the latest hit contact. Guest object address; cleared at
// BAP game start and end (objects are re-created per game). 0 = not claimed.
void NoteContact(uint32_t victim, uint32_t hitter);  // hit contact hook, every contact
uint32_t LastContactHitter();
uint32_t LastContactVictim();
void SetBapPlayer(uint32_t player);
uint32_t BapPlayer();  // the panel claim only (0 = none)

// Round 9. The game's own cached BAP player (addresses.h ROGUE_VA_BAP_PLAYER_PTR); 0 unless a BAP
// match is live and the pointer is a skater. ProPlayer() = the claim if set, else this (in a BAP game).
uint32_t AutoBapPlayer();
uint32_t ProPlayer();
// Round 25: the pro, read once per frame in rogue::Tick (0 outside a BAP game). Cheap; for guest hooks.
uint32_t CachedPro();
// Round 28: sprint button for Jet Skates. The mod reads the first connected XInput controller every frame
// (rogue::Tick). SprintButton() = index into SprintButtonNames(); persisted in rogue_settings.txt next to the exe.
bool SprintHeld();
int SprintButton();
void SetSprintButton(int i);
const char* const* SprintButtonNames(int* count);
unsigned PadButtonsNow();   // raw XInput buttons (+ bit 16 = LT, bit 17 = RT pressed); 0xFFFFFFFF = no controller
// Round 25: pro-only tunable perks (rogue_hooks.cpp). hook 0 = shot (sub_833E87E0), 1 = wrist shot
// (sub_833ECF68), 2 = rating override (sub_83501A68, round 27; calls counted only during a BAP game). calls = all calls this game, mine = calls with the pro in
// r3..r10, reg = the register that matched last (3..10, 0 = none yet).
void ProTunableStats(int hook, unsigned* calls, unsigned* mine, int* reg);
void ProTunableReset();

// Round 9 lifecycle: rogue::Tick polls the game state (read in rogue_hooks.cpp). A BAP match starts
// the game; DONE (or the match vanishing) ends it: perks are reverted at once and the result entry
// waits for the panel's "BAP game ended". NHL_ROGUE_AUTO=0 turns the automatic part off.
struct GuestGame { bool valid = false; int32_t state = -1; uint32_t match = 0; bool bap = false; };
GuestGame PollGuestGame();
void OnBapGameOver();     // game over seen: revert perks, keep the counters
bool AwaitingResult();   // game over seen, result not entered yet
bool AutoStarted();      // the current / last game was started by the poll
bool AutoLifecycle();    // NHL_ROGUE_AUTO != 0
void ShockwaveWindowTick();  // round 18 physics window (rogue_hooks.cpp), every frame from Tick

// BUG-3 (2026-11-04): the poll ended a BAP game after the 1st period. An end condition must now HOLD
// (DONE 3 s, match / BAP flag gone 10 s), a poll-made game over is undone at the next FACEOFF of the
// same game, and the start / end history is kept for the panel ("Start / end log").
struct LifecycleEvent {
  float t = 0.f;  // seconds since rogue::Init
  int32_t state = -1;
  uint32_t match = 0;
  bool bap = false;
  uint32_t pro = 0;
  char what[64] = {};
};
const std::vector<LifecycleEvent>& LifecycleLog();          // newest last, capped
const std::vector<LifecycleEvent>& LifecycleLogAtGameOver();  // copy taken at the last game over
const char* LastGameOverReason();                            // "" if none
int LifecycleResumes();                                      // false game overs undone so far
void ResumeBapGame();  // undo a game over (perks back on, counters kept); panel button + auto resume
const char* GameStateName(int32_t state);

// RE-5: the game's own box score from the stat records (addresses.h). Round 22: the records are located by
// their layout in a background scan (needs a few shots on goal in the game); valid only while they exist. GF / GA = sums over the pro's team's
// records (skater goals, goalie goals against), so they include simulated goals; a shootout is not in them.
struct BoxScore {
  bool valid = false;
  bool second_team = false;  // pro index 20-39: record mapping not verified yet
  bool ambiguous = false;    // more than one record table found: the pick is not verified
  int tables = 0;            // record tables found by the locator
  int index = -1, slot = -1;
  int goals = 0, assists = 0, pim = 0, plus_minus = 0, shots = 0, hits = 0, toi = 0;
  bool identified = false;   // the pro's record was identified by TOI tracking (else: slot guess)
  bool gf_from_goalie = false;  // GF from the other team's goalie (counts simulated goals)
  int team_gf = 0, team_ga = 0, team_shots = 0, opp_shots = 0;
  uint32_t record_va = 0;  // guest address of the pro's +/- word
};
BoxScore ReadBoxScore();          // rogue_hooks.cpp
void BoxScoreTick();              // rogue::Tick during a BAP game: search in a long cut-scene, identify the pro
void BoxScoreGameOver();          // game over: search now if no table was found during the game (background)
bool BoxScoreScanBusy();
void BoxScoreReset();             // game start: forget the located tables
void NotePeriodEnd();             // period-end hook (rogue_static_probe.cpp): the search waits for the intermission
const char* BoxScoreStatus();     // one line for the panel
const BoxScore& LastBoxScore();   // read at the last game over (valid = false if it could not be read)
int BoxScoreSerial();             // +1 at every game over that read a box score (panel prefill)
// Probe only (rogue_hooks.cpp): hex of bytes [from, from + len) of the record whose +/- word is at pm_va
// (the record starts 44 bytes before it).
void DumpBoxRecord(char* out, size_t n, uint32_t pm_va, int from, int len);
}  // namespace rogue
