# Reverse-engineering ledger

One row per guest function / offset / global we rely on. Nothing goes into
`include/rogue/addresses.h` until its row says CONFIRMED.

Confidence: CANDIDATE (from static search) -> OBSERVED (probe fired, args look right) ->
CONFIRMED (behavior reproduced: e.g. forcing it changes the game as predicted, 2+ runs).

## Ledger

| Key (addresses.h) | Address / offset | Confidence | Evidence | Date / agent |
|---|---|---|---|---|
| ROGUE_ADDR_CHECK_RESOLVE | ? | - | - | - |
| ROGUE_VA_BAP_PLAYER_PTR / ROGUE_OFF_SKATER_CONTROLLER | ? | - | - | - |
| ROGUE_ADDR_FORCE_FALL / ROGUE_OFF_SKATER_BALANCE | ? | - | - | - |
| ROGUE_ADDR_PUCK_BODY_HIT | ? | - | - | - |
| ROGUE_ADDR_BAP_GAME_START / END | ? | - | - | - |

## Known facts inherited from the parent project (verified there, not re-verified here)

- Image base 0x82000000, size 0x1EA0000. Search the decompressed image
  (`default.decompressed.exe`, VA = 0x82000000 + file offset), NOT the LZX `default.xex`.
- Generated code loads addresses as DECIMAL `lis`/`addi`/`ori` pairs; hex grep finds nothing.
  `tools/franchise_re/find_franchise_code.py` reconstructs them; our `find_bap_code.py` reuses it.
- Heavy vtable dispatch: static call graphs hit `REX_CALL_INDIRECT_FUNC` walls. Runtime
  probes are the efficient way to confirm behavior.
- Tweak registry record: `record_va+0` = BE pointer to the name string, `record_va+8` = live
  value. The CONSUMER of a tunable loads `record_va+8` (or a cached copy). A hardware write/read
  watch on a tunable's `value_va` (logged by TunableRuntimeStore) is a way to find consumers.
- Strings of interest: `BAP_PLAYER_GROWTH` @ 0x82035818. BAP play-gen task
  `BAPGenerateOffenseNewTeamTask`. Difficulty ladder names Rookie_/Pro_/AllStar_/Superstar_.
- Be-A-Pro tunables (category `Beapro`, 279 entries) include `gBeAPro_LineSkillBoost`,
  `gBeAPro_LineEnergyBoost`, `gBeAPro_ScoreBonus_*`, `gBAPTasks_*`, sim-out gates.
- Hit-related tunables (category `Checking`, 527 entries): `gHitTune_*`, `gCheckMagnitude*Weight`,
  `gMomentumPhysics*`, `gHitPowerForPhysicsGlobalModifier`. Fall: `gBalanceOffpuckFallThreshold`,
  `gBalanceCarrierFallThreshold`, `gTriggerFalls`, `gTriggerStumbleFall`, `gKnockDownValue`.

## Probe recipe (copy for each candidate)

```cpp
// in a scratch TU (e.g. src/rogue_probes.cpp), env-gated, removed once CONFIRMED
REX_EXTERN(__imp__sub_82XXXXXX);
extern "C" REX_FUNC(sub_82XXXXXX) {
  static const bool on = std::getenv("NHL_ROGUE_PROBE") != nullptr;
  static std::atomic<int> n{0};
  if (on && n.fetch_add(1) < 200) {
    FILE* f = std::fopen("rogue_probe.txt", "a");
    if (f) { std::fprintf(f, "82XXXXXX t=%lu r3=%08X r4=%08X r5=%08X r6=%08X lr=%08llX\n",
        GetTickCount(), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        (unsigned long long)ctx.lr); std::fclose(f); }
  }
  __imp__sub_82XXXXXX(ctx, base);
}
```
Ask the user to do a scripted action sequence (e.g. "3 big hits, then nothing for 10 s") and
note wall-clock times, so firing counts can be correlated with events.

## Results log

(append newest first: date, agent, what was probed, what fired, conclusion)
