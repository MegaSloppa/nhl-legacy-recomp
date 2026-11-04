// Legacy Rogue - TUN-4 diagnostic probe (read-only) + TUN-5 write test. See src/rogue_probe.cpp.
// Gated on NHL_ROGUE=1 AND NHL_ROGUE_PROBE=1. Driven from the panel's Diagnostics section.
#pragma once
#include <cstdint>

namespace rogue::probe {
bool Enabled();                   // both env vars set
bool Busy();                      // the tweak catalog dump is running
const char* Status();             // one-line status for the panel (catalog dump)

// TUN-9: write rogue_tweak_catalog.txt (tab-separated) (every tweak object, live value at N+32). Async.
void CatalogAsync();
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

// Static-map probe (rogue_static_probe.cpp, rogue_probe_s.txt). Round 9 additions:
void StaticNote(const char* msg);
void SprintHuntReport();                 // round 29: final sprint-hunt lines (game over)         // one "note" line (lifecycle events from rogue_mod.cpp)
// Penalty immunity behaviour test: the penalty decision sub_83487DC0 returns 0 when its offender is
// the pro (rogue::ProPlayer). Off by default, panel checkbox; every block (test or perk) is logged.
void SetPenaltyImmunityTest(bool on);
// Called by the penalty decision hook in rogue_hooks.cpp after every decision (result after any
// block; blocked = the perk or the test cancelled a call). Logs pen-decide / pen-immunity lines.
void PenaltyDecided(uint32_t offender, uint32_t victim, uint32_t type, uint32_t result, bool blocked,
                    unsigned long long lr);
bool PenaltyImmunityTest();
int PenaltyImmunityBlocked();
// Round 10 contact floor test: the pro's contacts on opposing skaters with power >= min_power (and
// below the hit threshold) become normal hits (rogue_hooks.cpp reaction choice). 0 = off.
void SetContactFloorTest(float min_power);
float ContactFloorTest();          // 0 unless the probe is enabled and the test is on
void NoteContactFloorPromoted();   // called by the hook for every promoted contact
int ContactFloorPromoted();
// Round 12 shockwave test (rogue_hooks.cpp): when the pro lands a knockdown, or at his next contact
// after Arm, opposing skaters within the radius (metres) are knocked down. 0 = off.
void SetShockwaveTest(float radius);
float ShockwaveTest();             // 0 unless the probe is enabled and the test is on
void ArmShockwave();               // panel button / F8
bool TakeShockwaveArm();           // hook: consume an armed shockwave
bool ShockwaveArmed();
void ShockwaveFired(const char* line, int knocked_down);
int ShockwaveCount();
void PollKeys();                   // rogue::Tick, every frame: F8 = arm
// Round 15: 0 = kind 0, no post step (round 12); 1 = kind 0 + the post-contact step sub_833DBD18
// (default); 2 = the kind / float learned from the pro's last real knockdown + post step.
void SetShockwaveMode(int mode);
// Round 14 push test: 0 = off, else velocity added away from the pro (1 step = ~3 m/s).
void SetShockwavePush(float strength);
// Round 16 balance test: 0 = off, 1 = call sub_834E9FA8(target, 1, amount) (default), 2 = write 100.
// Round 17: also force the stored power (round 15/16 behaviour). Default off = round 13 behaviour.
// Round 18: 0 = our own dispatcher calls (rounds 12-17), 1 = physics window (default).
void SetShockwaveMethod(int m);
int ShockwaveMethod();
void NoteWindowPromoted(uint32_t victim);  // a real contact promoted to a knockdown in the window
int WindowPromoted();
void SetShockwaveForceStored(bool on);
bool ShockwaveForceStored();
void SetShockwaveBalance(int mode);
int ShockwaveBalance();
void SetShockwaveBalanceAmount(float amount);
float ShockwaveBalanceAmount();
void BalanceSeen(uint32_t player, uint32_t kind, float amount, float before, float after);  // natural calls
void ForcedBalance(uint32_t victim, int mode, float before, float after);
float ShockwavePush();
int ShockwaveMode();
// Dispatcher arguments on real hits / knockdowns involving the pro (kind = *r5, fac = *r6 bits,
// player +40 of the victim before / after) and on every forced call.
void ContactArgs(uint32_t victim, uint32_t hitter, bool pro_hit, uint32_t desc, uint32_t applied,
                 uint32_t kind, uint32_t fac_bits, uint32_t v40_in, uint32_t v40_out);
void ForcedArgs(uint32_t victim, int mode, uint32_t kind, bool ok, uint32_t v40_in, uint32_t v40_out);
// Used by the guest hooks (rogue_probe.cpp bottom, rogue_hooks.cpp reaction hooks).
void TeamQueryResult(uint32_t player, uint32_t arg, uint32_t result, unsigned long long lr);
void ReactionBegin(uint32_t victim, uint32_t hitter);
void ReactionEnd(uint32_t applied);
double ReactionPowerIn(double power);
void ReactionChosen(double power_in, double power_used, uint32_t desc, unsigned long long lr);

// Round 20 stat finder (rogue_stat_probe.cpp, RE-5): watches game memory during a BAP game and writes
// rogue_stats_probe.txt (small integers that changed, value at every checkpoint) at game over.
void StatGameStart();               // rogue::OnBapGameStart
void StatGameOver();                // rogue::OnBapGameOver: last checkpoint + report 5 s later
void StatResume();                  // rogue::ResumeBapGame: keep watching the same game
void StatEvent(const char* label);  // goal / period end hooks: checkpoint 4 s later
void StatWriteNow();                // panel button
const char* StatStatus();
}  // namespace rogue::probe
