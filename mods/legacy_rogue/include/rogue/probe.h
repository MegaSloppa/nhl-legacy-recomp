// Legacy Rogue - TUN-4 diagnostic probe (read-only) + TUN-5 write test. See src/rogue_probe.cpp.
// Gated on NHL_ROGUE=1 AND NHL_ROGUE_PROBE=1. Driven from the panel's Diagnostics section.
#pragma once
#include <cstdint>

namespace rogue::probe {
bool Enabled();                   // both env vars set
bool Busy();                      // a snapshot is running on the worker thread
void RunAsync(const char* label); // append one snapshot to rogue_probe.txt (next to the exe)
const char* Status();             // one-line status for the panel

// TUN-5b write test: writes the field real code reads (N+32 + heap copy) for floats/ints, or
// every entry of a game-style table. 3 presets per setting. Restart or Restore to undo.
int WriteTestCount();
const char* WriteTestName(int i);
const char* WriteTestPresetLabel(int i, int p);  // p = 0..2
bool WriteTestApply(int i, int p);
void WriteTestRestore(int i);
void WriteTestRestoreAll();
void WriteTestDescribe(int i, char* buf, unsigned n);
// TUN-9: write rogue_tweak_catalog.txt (tab-separated) (every tweak object, live value at N+32). Async.
void CatalogAsync();
// TUN-10: one-click hip-check guess (several tunables at once); undo with WriteTestRestoreAll.
void WriteTestHipCombo();
// RE-1 probes: write a MARK line to rogue_probe.txt (to line up the log with what you did).
void Mark();

// RE-1 hit watch (round 6). One entry per call of the hit contact sub_833E0BF8 (newest first),
// i.e. one per player who reacts. Addresses are guest player objects (heap, change every game).
// ME = the player claimed in the panel (rogue::BapPlayer()).
struct HitEvent {
  unsigned long tick = 0;    // GetTickCount() at contact
  uint32_t hitter = 0;       // r4 of sub_833E0BF8
  uint32_t victim = 0;       // r3 of sub_833E0BF8 (the player reacting)
  uint32_t reaction = 0;     // 0x83ABF598 knockdown, 0x83ABF57C normal hit
  bool knockdown = false;
  float power = 0.f;         // *r5 = power from sub_833CA500 (static read)
  bool hitter_me = false, victim_me = false;
  bool hitter_goalie = false, victim_goalie = false;
  bool boosted = false;      // the power test changed this reaction
  bool hitter_auto = false, victim_auto = false;  // byte +0x22C != 0xFF (CANDIDATE: player a human controls)
  int strength = -1;         // r7 of sub_83589B38 for the same pair within 1.5 s
  bool credited = false;     // sub_8357F9A0 ran for the same pair within 1.5 s
  bool fall = false;         // sub_829380A0 ran within 1.5 s
  bool counted = false;      // counted as a ME hit (preview of the mod's own counter)
};
int HitEventCount();
bool HitEventGet(int i, HitEvent* out);  // i = 0 is the newest
// What the probe counts for the claimed player (ME).
struct YouStats {
  int hits = 0, knockdowns = 0, knocked_down = 0;
  // Auto marker (+0x22C != 0xFF) vs the claim, per player of each contact (round 7):
  int auto_agree = 0;   // ME and marked
  int auto_missed = 0;  // ME but not marked
  int auto_wrong = 0;   // marked but not ME
  int auto_seen = 0;    // contacts with a marked player and no claim (exhibition)
  // Marker scan (round 8), once a second over every known player object:
  int mk_secs = 0;                            // seconds scanned
  int mk_none = 0, mk_one = 0, mk_many = 0;   // seconds with 0 / exactly 1 / 2+ marked players
  int mk_one_me = 0;                          // seconds with exactly 1 marked player and it is ME
  int mk_changes = 0;                         // times the marked set changed
  int mk_count = 0;                           // marked players at the last scan
  uint32_t mk_player = 0;                     // first marked player at the last scan
  int mk_value = -1, mk_index = -1;           // its +0x22C byte and player index
};
YouStats YouCounts();
void YouReset();
unsigned long TickNow();                 // same clock as HitEvent::tick
// Called by the hit contact hook in rogue_hooks.cpp (after the original ran).
void ContactSeen(uint32_t victim, uint32_t hitter, uint32_t reaction, float power,
                 unsigned long long lr);
// Power test: the reaction choice sees power x factor for ME's hits (1 = off), on top of perks.
void SetPowerTest(float factor);
float PowerTest();
// After a claim: dump ME and every other player object once (RE-2).
void DumpPlayersForClaim(uint32_t me);
// Round 8: clear the collected player list (start of a game). Refilled by the team query.
void ResetPlayers();
// Used by the guest hooks (rogue_probe.cpp bottom, rogue_hooks.cpp reaction hooks).
void TeamQueryResult(uint32_t player, uint32_t arg, uint32_t result, unsigned long long lr);
void ReactionBegin(uint32_t victim, uint32_t hitter);
void ReactionEnd(uint32_t applied);
double ReactionPowerIn(double power);
void ReactionChosen(double power_in, double power_used, uint32_t desc, unsigned long long lr);
}  // namespace rogue::probe
