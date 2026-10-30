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
// Period end / intermission - natural point to offer a perk draft.
// #define ROGUE_ADDR_PERIOD_END       82XXXXXX
// Goal scored (scorer ptr in a register) - currency, kGoalHeal.
// #define ROGUE_ADDR_GOAL_SCORED      82XXXXXX

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
// How to tell the BAP-controlled skater apart: NOT FOUND. Byte +0x38 was a reaction state, not a
// human flag (round 5); sub_834EA6D0 is a team query. Until found, the owner claims his pro in the
// panel (rogue::SetBapPlayer). CANDIDATE (round 6, one session): byte +0x22C is 0x00 on the
// claimed player and 0xFF on all 20 others. Round 7 tests it (AUTO tag).
// #define ROGUE_OFF_SKATER_CONTROLLER 0x0000
// #define ROGUE_HUMAN_CONTROLLER_VALUE 0x00
// #define ROGUE_VA_BAP_PLAYER_PTR     0x00000000
