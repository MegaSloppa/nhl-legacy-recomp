// Legacy Rogue - guest function addresses the EVENT perks need.
// All are UNKNOWN today. Each one is found with the workflow in README.md
// (string anchors -> generated/default cross-ref -> runtime probe to confirm).
// Uncomment a define once confirmed; rogue_hooks.cpp only compiles hooks whose
// address is defined, so the mod builds and runs (tunable perks only) without any.
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
// Check resolution: the function that reads gHitTune_* / gCheckMagnitude*Weight /
// gMomentumPhysicsImpulse and decides big-win/small-win/glance. Need: hitter and
// hittee skater pointers (r3/r4/...?).  Find it by cross-referencing the NAME
// VAs of those tunables (the registry record) or by write-watch on their value_va.
// #define ROGUE_ADDR_CHECK_RESOLVE    82XXXXXX

// --- Shooting ----------------------------------------------------------------
// Puck-vs-skater body collision (shot blocks / hits on players). Leads:
// gInjuryShotBlockPuckSpeedThreshold, gShotBlockReactionPuckSpeedThreshold readers.
// Need: struck skater ptr, puck velocity, last-touch/shooter id.
// #define ROGUE_ADDR_PUCK_BODY_HIT    82XXXXXX

// --- Actions we CALL (not hook) ----------------------------------------------
// Put a skater into a fall/knockdown. Leads: readers of gTriggerFalls,
// gBalanceOffpuckFallThreshold, gBalanceCarrierFallThreshold, gTriggerStumbleFall.
// Cheapest alternative if no clean entry point: drive the skater's balance value
// below the fall threshold (needs the balance field offset in the skater struct).
// #define ROGUE_ADDR_FORCE_FALL       82XXXXXX
// #define ROGUE_OFF_SKATER_BALANCE    0x0000   // float offset in skater object

// --- Data --------------------------------------------------------------------
// How to tell the BAP-controlled skater apart: controller-index field on the
// skater object, or a global "BAP player" pointer. Offset/global VA here.
// #define ROGUE_OFF_SKATER_CONTROLLER 0x0000
// #define ROGUE_VA_BAP_PLAYER_PTR     0x00000000
