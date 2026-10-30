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
uint32_t BapPlayer();
}  // namespace rogue
