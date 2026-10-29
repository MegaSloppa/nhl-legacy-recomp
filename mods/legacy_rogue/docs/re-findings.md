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
| (layout, no address) Tweak object value | name-pointer address N: tag N+8, N+20 (reset value?), min N+24, max N+28, LIVE VALUE N+32 (CONFIRMED TUN-5b), heap copy ptr N+36 (copy: tag +0, name +4, value +24) | CONFIRMED (value N+32) | TUN-4 layout; TUN-8 readers load N+32; TUN-5b: writing N+32 + H+36 changes slap shot power and LOD immediately | 2026-10-29 Victoria |

## Tweak object layout (TUN-4, 2026-10-29)

Source: `rogue_probe.txt` from the owner (3 snapshots: menu, in match, after setting
`gTweakByGameStyleGameSpeedLevel` to 20 in Engine Tunables). N = address of the word that
points at the `gXxx` name string. The parent store's "record_va" is N; its "value_va" is N+8.

Image object (in .data, fixed address, e.g. N = 0x83C8430C for `gSlapShotPowerMultiplier`):

| Offset from N | Meaning | Evidence |
|---|---|---|
| -8 | type-class pointer (one per type) | 0x8234B5F0 on every float, 0x8234B62C on game-style ints, 0x8234B5E4 int, 0x8234B5D8 bool |
| -4 | category / owner string pointer | |
| +0 | name string pointer | |
| +4 | label string pointer ("power multiplier", "Game Speed Level") | |
| **+8** | **type tag**: 4 float, 2 int, 1 bool, 13 game-style table, 12 indexed float slider | Matches the class pointer on all 7 objects; the only word changed by the overlay edit (13 -> 20) |
| +12 | link to the next object | |
| **+20** | **value at stock; TUN-8 shows readers do NOT load it (probably reset/default)** (float/int); bool: byte +20 = value, byte +21 = default; table types: entry count | `gSlapShotPowerMultiplier` 1.0, `gSlapShotPowerScale` 1.0, `gBeAPro_LineEnergyBoost` 0.2, `gLODShader` 1 |
| +24 | min (scalar) / pointer to static value table (table types) | 0.0 / 0 |
| +28 | max | 3.0, 2.0, 1.0, 2 |
| **+32** | **current value read by game code** (TUN-8: base+40 loads in hit, shot, LOD code) | 1.0, 1.0, 0.2, 1 |
| +36 | pointer H to the heap copy (H+0 tag, H+4 name, H+8 label, H+24 value, H+32 max, H+36 default) | Heap copy at 0xBFxxxxxx (mirrored at 0xDF/0xFF and image+0x10000000 views) holds identical values |

Game-style objects (tag 13): +20 = 3 (entries), +24 -> static table, heap copy +28 -> heap table.
Tables: GameSpeed 5,4,3; SlapShotPower 5,4,4; WristShotAccuracy 6,5,3. Which entry is the active
style and the legal range are unknown (TUN-7). Indexed slider (tag 12, `gSlapShotPowerSlider`):
+20 = 7 entries, +24 -> float table 0.85, 0.90, 0.95, 1.03, ...; +32 = 2 (index? unproven).

Other facts from the same run:
- Nothing in these objects changed between the menu and in-match snapshots.
- The registration POOL (category, name, label, default) is a different structure in .data; the
  parent store's sanity value `gDekeRatEffect` = 0.2 is read from the pool, not a live object.
- Consequence: the parent Engine Tunables editor and Legacy Rogue's `TunablePatcher` (before
  TUN-6) write type tags. Writing tag 14 (424 other objects use it) did not crash in testing.

Unproven: which copy consumers read (TUN-5 writes both); whether consumers clamp to min/max;
whether values are cached when a match loads.

## Known facts inherited from the parent project (verified there, not re-verified here)

- Image base 0x82000000, size 0x1EA0000. Search the decompressed image
  (`default.decompressed.exe`, VA = 0x82000000 + file offset), NOT the LZX `default.xex`.
- Generated code loads addresses as DECIMAL `lis`/`addi`/`ori` pairs; hex grep finds nothing.
  `tools/franchise_re/find_franchise_code.py` reconstructs them; our `find_bap_code.py` reuses it.
- Heavy vtable dispatch: static call graphs hit `REX_CALL_INDIRECT_FUNC` walls. Runtime
  probes are the efficient way to confirm behavior.
- CORRECTED 2026-10-29: the parent docs say `record_va+8` is the live value. It is the type tag
  (see the layout above). A hardware write/read watch on N+20 (not N+8) is a way to find consumers.
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

- 2026-10-29, Victoria: TUN-5b write test on N+32 + H+36. gSlapShotPowerScale x0.2 / x10 and
  gLODShader 0 / 2 changed gameplay / rendering immediately; x10 exceeds the object's max (2.0),
  so game code does not clamp. Game-style heap + static tables set to 0 / 20: no visible effect.
  Conclusion: value field N+32 CONFIRMED.

- 2026-10-29, Victoria: TUN-8 static consumer scan. Readers load N+32 (base+40), not N+20, for
  4 objects incl. the gLODShader control; game-style tables are read via heap copy H=*(N+36),
  table=*(H+32) in sub_829F8A20. Revised hypothesis: N+32 = current value, N+20 = reset/default.
  Next: TUN-5b write test on N+32 / tables.

- 2026-10-29, Victoria: TUN-5 write test. Wrote stock x0.2 / x3 / x10 to N+20 and the heap copy
  of gSlapShotPowerMultiplier (image 0x83C84320, copy 0xBFC39108), gMomentumPhysicsImpulse
  (0x83CD9F8C / 0xBFC7D630, stock 0.8) and gHitTune_GlobalScale (0x83CDEE8C / 0xBFC7DBDC),
  in a BAP match and before an exhibition. No visible effect. Conclusion: the object value is not
  what gameplay reads during play (or is read only at boot). Layout stays OBSERVED, not CONFIRMED.

- 2026-10-29, Victoria: TUN-4 registry probe (`src/rogue_probe.cpp`, snapshots menu / in match /
  after edit). 12 names, all found once in the image; image object + heap copy each. Conclusion:
  layout table above (OBSERVED). Next: TUN-5 write test for gameplay effect.
