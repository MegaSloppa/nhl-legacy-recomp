// Legacy Rogue - public entry points the host app calls (see docs/user-guide.md "Wiring"):
//   rogue::Init(tunable_store);   // OnCreateDialogs, after the TunableRuntimeStore exists
//   rogue::Tick();                // once per frame
//   rogue::DrawPanel();           // inside the overlay window (Guide/F1)
//   rogue::fx::Draw();            // every frame, even with the overlay closed (task FX-1)
#pragma once
#include <cstdint>
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

// Round 9 lifecycle: rogue::Tick polls the game state (read in rogue_hooks.cpp). A BAP match starts
// the game; DONE (or the match vanishing) ends it: perks are reverted at once and the result entry
// waits for the panel's "BAP game ended". NHL_ROGUE_AUTO=0 turns the automatic part off.
struct GuestGame { bool valid = false; int32_t state = -1; uint32_t match = 0; bool bap = false; };
GuestGame PollGuestGame();
void OnBapGameOver();     // game over seen: revert perks, keep the counters
bool AwaitingResult();   // game over seen, result not entered yet
bool AutoStarted();      // the current / last game was started by the poll
bool AutoLifecycle();    // NHL_ROGUE_AUTO != 0
}  // namespace rogue
