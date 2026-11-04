// Legacy Rogue - guest function addresses the EVENT perks need.
// Each one is found with the workflow in docs/re-findings.md (static search -> runtime probe ->
// confirm). A define goes in ONLY when its ledger row in re-findings.md says CONFIRMED
// (00-START-HERE rule 4). rogue_hooks.cpp only compiles hooks whose address is defined, so the
// mod builds and runs (tunable perks only) without any.
//
// Value = the 8 hex digits of the recompiled function, e.g. 82B7A8D0 for sub_82B7A8D0.
#pragma once

// --- Mode / lifecycle --------------------------------------------------------
// Be-A-Pro game begins (after the BAP player is placed on a team / game loaded).
// Leads: BAP task strings (gBAPTasks_*, BAPGenerateOffenseNewTeamTask), the
// gBeAPro_* tunable readers, "BAP_PLAYER_GROWTH" (VA 0x82035818).
// #define ROGUE_ADDR_BAP_GAME_START   82XXXXXX
// #define ROGUE_ADDR_BAP_GAME_END     82XXXXXX
// Since round 9 the mod does not hook these: rogue::Tick polls the game state below instead.
//
// Game state machine (round S1, 2026-11-03, one full BAP game, re-findings.md "Game state"):
// the word below is the current state, written by the switch sub_8330C890 (r3 = new state; names
// from the image table 0x83AA5B20). Seen: -1 -> PRE_GAME (0) once at game load, then FACEOFF /
// GAME_ON / GOOF_OFF / PRE_FACEOFF / SYNCHING per stoppage, STORYTELLER (10) at intermissions,
// DONE (9) once, 14 s after the third period end. CONFIRMED for PRE_GAME and DONE.
#define ROGUE_VA_GAME_STATE         0x83C1D220u
#define ROGUE_GAME_STATE_PRE_GAME   0
#define ROGUE_GAME_STATE_DONE       9
// Match object pointer: non-zero from game load until ~2.5 min after DONE, then 0. CONFIRMED (S1).
#define ROGUE_VA_MATCH_PTR          0x83C1E89Cu
// Byte: 1 during the BAP game, 0 once the match pointer is cleared. CONFIRMED in BAP (S1, round 9)
// and 0 for a whole exhibition period (round 9).
#define ROGUE_VA_BAP_MODE_FLAG      0x83AC5F63u
// Period end / intermission - natural point to offer a perk draft.
// #define ROGUE_ADDR_PERIOD_END       82XXXXXX
// Goal scored (scorer ptr in a register) - currency, kGoalHeal.
// #define ROGUE_ADDR_GOAL_SCORED      82XXXXXX

// --- Box score (RE-5) ------------------------------------------------------------------------
// Round 20 stat finder, 2026-11-04 (re-findings.md "Round 20 stat finder"). OBSERVED in ONE BAP game:
// every value of the owner's box score matched (pro 1 G, 0 A, +1, 6 PIM, 13 SOG, 10 hits; 4-2 loss) and the
// team sums matched (skater SOG 36, skater goals 2, goalie GA 4 with per-period GA 2/1/1 and SA 8/14/5 = 27).
// Simulated goals ARE counted here. In addresses.h for the round 21 behaviour test (auto-fill of the result,
// checked by the owner before it is used).
// Per-player stat records: 20 records of 86 bytes, one per roster slot of a team (slot = player index % 20;
// 18 / 19 = goalies). Round 21 (2026-11-04): the records are NOT at a fixed offset of the match object
// (round 20 found them right after it, round 21 read 0xDB fill there: a separate heap object that happened to
// sit next to it). Since round 22 they are FOUND by their layout (rogue_hooks.cpp, box score locator); the
// field offsets below are relative to a record's +/- word (s16, big-endian).
#define ROGUE_MATCH_STATS_STRIDE     86
#define ROGUE_STAT_GOALS             (-8)
#define ROGUE_STAT_ASSISTS           (-4)
#define ROGUE_STAT_PIM               (-2)
#define ROGUE_STAT_SOG               20
#define ROGUE_STAT_HITS              22
#define ROGUE_STAT_TOI               (-16)  // seconds (round 23: pro 784 = 13:04; goalies ~3880 in an OT game)
// Round 23: skater goals (-8) miss SIMULATED goals; the goalie's goals against (-8 of slot 18 / 19) count
// them, so a team's score = the other team's goalie GA. The pro's record is NOT always slot idx % 20
// (round 23: idx 37 in slot 16, slot 17 empty) and tables have copies: the mod identifies the pro's record
// by its TOI ticking while he is on the ice (+0x22C below).
#define ROGUE_OFF_SKATER_ONICE       0x22C  // byte: 0xFF on the bench; on the ice 0x00 (S1, round 8) OR 0x06 (round 29
                                            // BAP game; 0x06 also in exhibition): test != 0xFF (OBSERVED)
#define ROGUE_GAME_STATE_GAME_ON     4      // OBSERVED (S1 state order)
#define ROGUE_GAME_STATE_STORYTELLER 10     // OBSERVED (intermissions, replays)
#define ROGUE_STAT_GOALIE_GA         (-8)
#define ROGUE_STAT_GOALIE_SA         2
// Player object: roster index byte (team * 20 + slot) at +57 (S1: the pro was idx 17, team 0; round 9: idx
// 37, team 1). OBSERVED. Open: whether the records above follow the pro's team when he is idx 20-39.
#define ROGUE_OFF_PLAYER_INDEX       57

// --- Hitting -----------------------------------------------------------------
// Hit contact: called once per player who REACTS to a body contact (so one collision gives 1-3
// calls). r3 = player reacting (victim), r4 = player hitting, r5 -> float (hit power,
// HYPOTHESIS), r6 = reaction pointer set by the only caller sub_83429560 (not read by the
// function itself). CONFIRMED 2026-10-30 (RE-1 rounds 3-4, re-findings.md).
#define ROGUE_ADDR_HIT_CONTACT      833E0BF8
// r6 of the hit contact when the reaction is a knockdown (normal hit: 0x83ABF57C). Goalies get
// it too but do not fall. CONFIRMED 2026-10-30 (rounds 3-4: 3 + 15 knockdown lines, owner check).
#define ROGUE_VA_REACT_KNOCKDOWN    0x83ABF598u
// Hit outcome. The hit contact above is the bookkeeping AFTER the decision. The decision:
// sub_83429560 (reaction dispatcher, r3 victim, r4 hitter; only caller of the hit contact) gets
// power from sub_833CA500, then the reaction from sub_833B9028(f1 = power), which walks the
// reaction table (5 entries x 28 bytes, threshold float at +4; entry 3 = hit, entry 4 =
// knockdown). CONFIRMED 2026-10-30 (static read + round 6: 1836 logged decisions all match,
// raising f1 turned the BAP player's hits into knockdowns as predicted).
#define ROGUE_ADDR_REACTION_DISPATCH 83429560
#define ROGUE_ADDR_REACTION_CHOICE   833B9028
#define ROGUE_VA_REACTION_TABLE      0x83ABF528u

// --- Shooting ----------------------------------------------------------------
// Puck-vs-skater body collision (shot blocks / hits on players). Leads:
// gInjuryShotBlockPuckSpeedThreshold, gShotBlockReactionPuckSpeedThreshold readers.
// Need: struck skater ptr, puck velocity, last-touch/shooter id.
// #define ROGUE_ADDR_PUCK_BODY_HIT    82XXXXXX

// --- Actions we CALL (not hook) ----------------------------------------------
// Put a skater into a fall/knockdown. Leads: sub_829380A0 (runs only on real falls, rounds 3-4),
// the reaction choice in sub_83429560, readers of gTriggerFalls / gBalance*FallThreshold.
// #define ROGUE_ADDR_FORCE_FALL       82XXXXXX
// #define ROGUE_OFF_SKATER_BALANCE    0x0000   // float offset in skater object

// --- Data --------------------------------------------------------------------
// Player object first word = class pointer. Skater 0x82271C10, goalie 0x8223F648 (goalies are
// roster slots 18-19 / 38-39). CONFIRMED 2026-10-30 (rounds 4-5, 3 sessions).
#define ROGUE_VA_SKATER_CLASS       0x82271C10u
// The BAP-controlled skater: the game's own cached Be-A-Pro player pointer (filled by
// sub_834DC8A8). CONFIRMED 2026-11-03 (two BAP games; round 9 ran with no claim: hits, goal and
// penalty immunity all keyed on it correctly; 0 in exhibition). Round S1 + round 8: set from PRE_GAME on,
// before any claim, and equal to the owner's claim in all 70+ samples after it; the +0x22C marker
// scan agreed (only this object ever marked, never 2+). Valid ONLY while ROGUE_VA_MATCH_PTR != 0 and
// ROGUE_VA_BAP_MODE_FLAG != 0: after the match the word keeps the old address while the object is
// re-used (class changed to 0x822708B0). rogue_hooks.cpp ReadAutoBapPlayer applies that gate plus
// a skater class check.
#define ROGUE_VA_BAP_PLAYER_PTR     0x83C876D4u
// Byte +0x22C (round 8): 0x00 on the pro only while he is on the ice, 0xFF on the bench and on
// everyone else (0x06 on the controlled player in exhibition). Not used: the pointer above is better.
// #define ROGUE_OFF_SKATER_CONTROLLER 0x22C
// #define ROGUE_HUMAN_CONTROLLER_VALUE 0x00
