# Reverse-engineering ledger

One row per guest function / offset / global we rely on. Nothing goes into
`include/rogue/addresses.h` until its row says CONFIRMED.

Confidence: CANDIDATE (from static search) -> OBSERVED (probe fired, args look right) ->
CONFIRMED (behavior reproduced: e.g. forcing it changes the game as predicted, 2+ runs).

## Ledger

| Key (addresses.h) | Address / offset | Confidence | Evidence | Date / agent |
|---|---|---|---|---|
| ROGUE_ADDR_HIT_CONTACT | sub_833E0BF8 (file 131); only caller sub_83429560 (file 134, call at lr 0x83429644). r3 = player reacting (victim), r4 = player hitting, r5 -> float (power?), r7 -> u32, r8 -> float out | CONFIRMED (detection) | Round 3: fired at all 5 marked checks + 7 unmarked, order matched owner notes. Round 4: 59 calls over 39 interactions, owner checked hitter -> victim on screen. One call per reacting player (1-3 per collision) | 2026-10-30 Hannah |
| ROGUE_VA_REACT_KNOCKDOWN | 0x83ABF598 = r6 of the hit contact on a knockdown reaction (normal hit 0x83ABF57C). Set by the caller (its r30); sub_833E0BF8 never reads r6 | CONFIRMED | Round 3: 3/3 knockdowns, 2/2 small hits. Round 4: 15 knockdown lines, owner: labels consistent. Goalies get it and do not fall | 2026-10-30 Hannah |
| ROGUE_VA_SKATER_CLASS | first word of a player object: skater 0x82271C10, goalie 0x8223F648 (roster slots 18-19 / 38-39) | CONFIRMED | Round 4 dumps (32 + 2), round 5 player table in exhibition and BAP (every slot 18/19/38/39 is a goalie class, all others skater); goalie took a knockdown reaction without falling | 2026-10-30 Hannah |
| ROGUE_OFF_SKATER_CONTROLLER | ? byte +0x38 REJECTED: it is a reaction / animation state (2..12 on victims after a reaction, 0 on hitters) | REJECTED | Round 5: never 2 on the owner's player; the two "YOU" knockdowns in BAP were other players. Interim: the owner claims his pro in the panel (rogue::SetBapPlayer); round 6 dumps all players at the claim | 2026-10-30 Hannah |
| (candidate) BAP marker | byte +0x22C of the player object: 0xFF on every player nobody controls; 0x00 on the pro in BAP (round 6, 21 players; also unique to him then: +0x040 = 1, +0x248 heap pointer vs 0x83B44474, +0x3D0 = 1), 0x06 on the controlled player in exhibition (round 7, 40 players at the claim; moved between the owner's skaters as he switched). Test: != 0xFF | CANDIDATE (OBSERVED in 2 modes) | round 6 + round 7 claim dumps and contact lines. Round 8 scans all players once a second in BAP | 2026-10-31 Shirley |
| (offset) player index | byte +0x39 (57) of the player object: 0-19 / 20-39 by team; also at +0xAC | OBSERVED | sub_833E0BF8 passes it as an index (static); dumps show 0..0x27, goalie = 0x13 / 0x27; byte +0x30 = team side 0/1 | 2026-10-30 Hannah |
| (data) team query | sub_834EA6D0(player, 0) -> 0 / 1 by TEAM (not human): exhibition idx 0-19 -> 0, 20-39 -> 1; BAP idx 0-19 -> 1, 20-39 -> 0 | OBSERVED | Round 5 result log, 2 sessions | 2026-10-30 Hannah |
| ROGUE_ADDR_REACTION_DISPATCH | sub_83429560(r3 victim, r4 hitter): power = sub_833CA500(victim, hitter, ...); desc = sub_833B9028(f1 = power); state = sub_83429248(victim, desc+20) | CONFIRMED | static (reaction_slice.txt) + round 6 (1836 dispatches logged, all with lr 0x834295E0) | 2026-10-30 Hannah |
| ROGUE_ADDR_REACTION_CHOICE | sub_833B9028(f1 = power) -> reaction descriptor: first table entry whose threshold (+4) >= power, else entry 4 | CONFIRMED | static (power_slices.txt) + round 6: all 1836 decisions match; raising f1 for the claimed player's hits gave hit -> knockdown and bump -> hit exactly as the thresholds predict | 2026-10-30 Hannah |
| ROGUE_VA_REACTION_TABLE | 0x83ABF528: 5 entries x 28 bytes. Entry 0 0x83ABF528 (power <= ~0.5: no hit reaction, contact not called), entries 1-2 never chosen (same threshold?), entry 3 0x83ABF57C (<= ~0.75: hit), entry 4 0x83ABF598 (above: knockdown) | CONFIRMED | round 6: entry 0 max 0.498, entry 3 0.500-0.750, entry 4 min 0.767 | 2026-10-30 Hannah |
| ROGUE_VA_BAP_PLAYER_PTR | ? | - | - | - |
| ROGUE_ADDR_FORCE_FALL / ROGUE_OFF_SKATER_BALANCE | ? (lead: sub_829380A0 runs only on real falls, r3 = 0xBEA19BE0 always, r6/r7 = per-fall body objects) | - | - | - |
| ROGUE_ADDR_PUCK_BODY_HIT | ? | - | - | - |
| ROGUE_ADDR_BAP_GAME_START / END | ? | - | - | - |
| (layout, no address) Tweak object value | name-pointer address N: tag N+8, N+20 (reset value?), min N+24, max N+28, LIVE VALUE N+32 (CONFIRMED TUN-5b), heap copy ptr N+36 (copy: tag +0, name +4, value +24) | CONFIRMED (value N+32) | TUN-4 layout; TUN-8 readers load N+32; TUN-5b: writing N+32 + H+36 changes slap shot power and LOD immediately | 2026-10-29 Victoria |

## Hit contact sub_833E0BF8 (static read, hit_slices.txt, 2026-10-30)

- Saves r3 (victim) in r29, r4 (hitter) in r31, r5 (float*) in r27, r7 in r25, r8 (float*) in r26.
  r6 is never read. Returns at once when r4 == 0.
- r28 = *( *(0x83C1D218) + 4 * (28 or 29) ), index 28 when sub_834EA6D0(hitter, 0) returns
  nonzero, else 29. HYPOTHESIS: a human vs CPU choice.
- If the bool tweak byte at 0x83C78D05 is set (and the byte at 0x83C7CC54 is clear, or
  sub_833BCA38(victim, hitter) returns nonzero) and *r5 < the float tweak at N = 0x83C73E08
  (N+32; TUN-9 says this function reads gGPFallMin), then *r8 = constant float at 0x82014BEC.
- Then three calls on per-player sub-objects at +564 with player index bytes (+57):
  sub_833DE980(victim+564, hitter idx, f1 = *r5, 30); sub_833DEA28(hitter+564, victim idx,
  f1 = *r8, *r7, game-style table entry r28 of the object at N = 0x83C74CC8 (heap table));
  sub_833DBE68(victim+564, victim idx, hitter idx, 300). HYPOTHESIS: per-player "who hit whom"
  memory (intimidation / AI awareness), durations 30 and 300 (frames?).
- Consequence: this is bookkeeping AFTER the hit is decided. Good for detection, not the place to
  change hit power. The reaction (knockdown or not) is chosen upstream in sub_83429560.
- Hit credit: sub_83589B38 then sub_8357F9A0 are called back to back from sub_8358A1D0 (file 146)
  and sub_835A7078 (file 147).

## Reaction dispatcher sub_83429560 (static read, reaction_slice.txt, 2026-10-30)

- r3 = victim (r31), r4 = hitter (r28), r5, r6 saved. Returns 0 at once unless
  sub_8341D8C8(victim) (and sub_8341D8C8(hitter) if hitter != 0) return nonzero.
- power: sub_833CA500(victim, hitter, &byte81, &byte80, &float88) returns f1, stored at r1+84
  (this is the hit contact's *r5). sub_833CA500 reads gBoardsCheckingEffectiveRating,
  gGPStumbleMin, gGlobalBalanceScale (TUN-9) and runs more often than contacts (AI estimates?).
- reaction: desc = sub_833B9028(f1 = power) -> descriptor pointer (0x83ABF57C normal, 0x83ABF598
  knockdown; 28 bytes apart). desc+8 / +12 floats, desc+20 a reaction id.
- state = sub_83429248(victim, desc+20). If state != 19: unless sub_8350C728(victim, state),
  sub_83429300(victim, state) applies it. Then the hit contact sub_833E0BF8(victim, hitter,
  &power, desc, r5, r6), sub_833FBEA0(victim, byte81, byte80, float88), and
  sub_8332D4D0(*(victim+80), 316, desc+8) / (314, desc+12). Else sub_833DBC50. Returns 1.
- Consequence (HYPOTHESIS): a knockdown is chosen from power alone in sub_833B9028; raising the
  power it sees for the BAP player's hits should turn hits into knockdowns (round 6 test).

## Reaction choice sub_833B9028 (static read, power_slices.txt, 2026-10-30)

```
r10 = 0x83ABF528 (table); r11 = r10 + 4; i = 0
loop: if f1 <= *(float*)r11 -> return r10 + 28 * i
      r11 += 28; i += 1; while r11 < r10 + 116
return r10 + 112   (entry 4)
```
A perk that raises f1 for the BAP player's hits turns them into knockdowns; lowering the
threshold floats in the table would change everyone's hits (global).

## Player object (round 4 dumps, 34 objects, 2026-10-30)

| Offset | Meaning | Confidence |
|---|---|---|
| +0x00 | class pointer: skater 0x82271C10, goalie 0x8223F648 | OBSERVED |
| +0x04 | self pointer | OBSERVED |
| +0x30 | byte: team side 0 / 1 | OBSERVED |
| +0x38 | byte: reaction / animation state (2..12 right after a reaction), NOT a human flag | REJECTED round 5 |
| +0x39 | byte: player index 0-39 (also +0xAC) | OBSERVED |
| +0x60 | pointer to another object (a player in exhibition, a shared object in training) | - |
| +0x22C | byte: != 0xFF on the player a human controls (0x00 BAP, 0x06 exhibition), 0xFF on all others (rounds 6-7) | CANDIDATE |
| +0x248 | pointer: heap object on the claimed player, 0x83B44474 on 19 of 20 others | CANDIDATE |
| +564 (0x234) | per-player sub-object the hit contact updates | static |

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
A strong override catches direct AND indirect calls (the function table registers `sub_X`).
Only ONE translation unit may override a given function: the hit contact lives in
`rogue_hooks.cpp` and forwards to the probe.

## Results log

(append newest first: date, agent, what was probed, what fired, conclusion)

- 2026-10-31, Shirley: RE-2 round 7, exhibition only (owner, ~17 min; no BAP game). No AUTO tag
  ever: the probe tested +0x22C == 0x00. Claim dump (40 players): claimed player A90C64C0 (idx 21,
  team 1) = 0x06, the 39 others 0xFF. Contact lines: 0x06 only on team 1 skaters (the owner's
  team), moving between them over time (e.g. idx 34 at 0x06, ~10 s later 0xFF); team 0 and all
  goalies always 0xFF. Power vs reaction: 11 knockdowns 0.785-1.077, normal hits 0.501-0.750
  (table holds). Conclusion: marker test = "!= 0xFF", the value differs by mode. Round 8 scans
  every player once a second in BAP.

- 2026-10-30, Hannah: round 6 (owner, 2 periods of a BAP game, 576 KB). Claim worked. Mod counted
  27 hits / 13 knockdowns for the pro (25 of 28 of his contacts boosted by the test); the game's
  hit stat said 5 (owner: probably only hits on the puck carrier); the credit pair ran 19 times for
  him (strength r7: 3 x9, 2 x7, 0 x3). Power test outcomes for his hits (raw -> boosted class):
  off 6 unchanged; x1.15 2 hit->KD, 2 bump->hit; x1.3 5 hit->KD, 4 bump->hit; x2 4 hit->KD,
  5 bump->KD, 11 bump->hit. All 1836 reaction choices match the table thresholds. Nothing
  abnormal (no flying, no freezes). Conclusion: reaction dispatcher / choice / table CONFIRMED;
  +0x22C candidate BAP marker from the claim dumps.

- 2026-10-30, Hannah: STATIC read of sub_83429560 (owner's reaction_slice.txt): see "Reaction
  dispatcher" above. The reaction is chosen by sub_833B9028 from the power of sub_833CA500.

- 2026-10-30, Hannah: RE-1 / RE-2 round 5 (owner, partial exhibition + partial BAP, 47 / 54 KB
  logs, 29 contacts each). No YOU line ever matched the owner's player; two BAP knockdowns were
  tagged YOU on other players (victims). Byte +0x38 read after the reaction: hitters always 0,
  victims 0 / 1-12. REJECTED as a human flag. sub_834EA6D0(player, 0): 0 / 1 by team, every
  player (40 per game); not a human query. Power (*r5), 58 contacts: all 17 knockdowns 0.763-0.962,
  all 41 normal hits 0.501-0.734. Clean split: the knockdown threshold is between 0.734 and
  0.763.

- 2026-10-30, Hannah: STATIC read of sub_833E0BF8 + call sites (owner's hit_slices.txt). See the
  section "Hit contact sub_833E0BF8" above. Only caller sub_83429560 (file 134); r6 is the
  caller's r30 and is not read by the callee; *r5 is compared against gGPFallMin; player byte +57
  is a player index; sub_834EA6D0(hitter, 0) selects a human/CPU table entry (CANDIDATE human
  query). Hit credit pair sub_83589B38 + sub_8357F9A0 called from sub_8358A1D0 / sub_835A7078.

- 2026-10-30, Hannah: RE-1 round 4 (hit watch: 7 hooks, on-screen labels), owner exhibition +
  training (goalie hits), ~16 min, 157 KB log. 59 contacts in 39 interactions, all from lr
  0x83429644; r6 0x83ABF598 x15, 0x83ABF57C x44. Owner: hitter -> victim always right, knockdowns
  consistently labelled. 1-3 lines per collision = one line per reacting player (two-way hits,
  both knocked down). Goalie: knockdown reaction at 927 s with no fall code (goalie unaffected);
  owner's falls at 935 s / 962 s ran sub_829380A0 (12 and 18 calls). sub_8357F9A0 r7 = the hitter
  object (2 sessions), so it is not a strength; round 3's 0..3 came from sub_83589B38 r7. Credit
  ran for 27 of 39 interactions, 60-80 ms after knockdown contacts, 330-375 ms after normal hits.
  Player dumps: see the Player object table. Conclusion: hit contact and knockdown reaction
  CONFIRMED; skater class OBSERVED; +0x38 human flag CANDIDATE. The reaction-pointer dump did not
  appear in the log (cause unknown; static read answered the question instead).

- 2026-10-30, Hannah: RE-1 round 3 sweep (141 hooks), owner training 1-on-1, 472 s, 5 marked
  open-ice checks (marks 4.1-4.7 s after each check). Fired only around contacts: sub_833E0BF8
  (all 5 marks + 7 unmarked; r3 victim, r4 hitter), sub_8357F9A0 / sub_83589B38 (~0.35 s later;
  missed mark 1), sub_829380A0 (knockdowns only: marks 1, 4, 5 + one unmarked), sub_8293A998
  (r5 0x0E knockdown / 0x11 normal), sub_835888E8, sub_8359FAF0, sub_835A47D0. sub_828CC300
  (skating snapshot): 29 bursts of ~236 calls, fresh r3 each time, callers lr 828F158C /
  82921208 / 8332AF60; not per frame. User skater A9BFB8A0, opponent A9577560 (that session).

- 2026-10-29, Victoria: STATIC search for readers of snapshot +636 / +640. sub_830E65F8 (file 107)
  is NOT balance code (constant table 0x83C0CBC0, size-320 guard, 40-step complex multiply-add
  loop). Callers of the snapshot sub_828CC300: sub_8332AF40, sub_828F1550, sub_82920828,
  sub_82933A40, sub_8293B9A8, sub_8293BF80 (files 125, 40, 42, 43, 43, 44).

- 2026-10-29, Victoria: STATIC read of nhllegacy_recomp.39.cpp. sub_828CC300 = skating settings
  snapshot into a struct at r3 (map: docs/skating_params_struct.txt). Bools are read as the byte
  at object base+29 = N+21. Ragdoll blend-out code: sub_828C75A8 (gBlendOut*ForceScale),
  sub_828C7D50 (gBehaviorPitchUprightThreshold), sub_828CB380 (gHandInfluenceDistance*).

- 2026-10-29, Victoria: RE-1 round 2. Player-event handlers PE_CHECK/CHECKED/DIVING_CHECK/GOAL/
  ASSIST/SHOT_ATTEMPT/GOODSHOT/PLUS/MINUS/WIN_GAME/LOSE_GAME/OT_LOSE_GAME/TIE_GAME/PENALTY/
  FACEOFFWIN + PE_NONE: never fired in an exhibition or a BAP game (hook method verified via
  the function table). Round 3 sweep targets (function, tunables it reads):
  sub_828B9258  gSpeedBoostRatioForMinBalanceLoss
  sub_828CC300  gAccelToProhibitForwardTurns, gBalanceCarrierFallThreshold, gBalanceStableThresholdVariation
  sub_828FEBD8  gEffectiveMinEnergy, gMinEnergyForNoEffectPK
  sub_82929FB0  gAlwaysUpdateBalanceRatio, gBalanceAdjustmentEnd, gBalanceAdjustmentStart
  sub_8292AE50  gHitBasedLengthCoefficient
  sub_8292B550  gCollisionBodyTension
  sub_8292B808  gContactCOMDelta, gDefaultCom
  sub_82936F28  gHitBasedBlacoutCoefficient, gHitBasedLengthCoefficient
  sub_829380A0  gClampDeltaAngle, gDrivenFallRelaxedModifier, gDrivenFallTenseModifier
  sub_8293A148  gCollisionVelocityCompensationValue
  sub_8293A3E8  gBalanceMax, gBalanceReactionMax, gBalanceReactionScale
  sub_8293A998  gStumbleVelocityCompensationValue
  sub_8293AB78  gStumbleBodyTension
  sub_82A3B770  gDefSkaterBalance, gDefSkaterBodyChecking
  sub_82DBEFB0  gBoardHitIntensityScale
  sub_82DBF2A0  gPlayerHitIntensityScale
  sub_83310E68  gHandlePotentialKnockedOverPenaltiesTime, gHandlePotentialNotKnockedOverPenaltiesTime
  sub_8331B8F0  gKnockDownValue
  sub_83328180  gBoardPinchNoCollisionFrames
  sub_83339558  gMinPushAwayScale
  sub_8333F9C0  gBoardCollisionStressForMaxEffect, gBoardCollisionStressForMinEffect, gBoardCollisionStressMaxEffect
  sub_833442E8  gSlowFallenGuysAfterThis
  sub_8334F930  gAugmentHipCheckYVelocity
  sub_833A1F40  gLayDownCollisionRadiusPadInches
  sub_833B90E0  gDirectHitForMax, gDirectHitForMin, gDirectHitValueAtMax
  sub_833B9730  gGPPhysicalPreparednessForShooting
  sub_833B9AE8  gGPFallMax, gGPFallMin, gGPIncidentalMin
  sub_833BF998  gMaxLowHitRatioForEffect, gMinLowHitRatioForEffect, gMinLowHitSize
  sub_833C09E8  gHitInjuryChanceMultiplier
  sub_833CA500  gBoardsCheckingEffectiveRating, gGPStumbleMin, gGlobalBalanceScale
  sub_833E0BF8  gGPFallMin
  sub_833E3D58  gBaseHitComparisonPriority
  sub_833EBD80  gHitsForIntimidatingShift
  sub_833F27E8  gBigHitterMinHitPct
  sub_833F3330  gTeamHitsMinHits
  sub_833F8AC8  gDesperationNoCollisionFrames
  sub_8340C690  gPhysicsRatingsDiffMax, gPhysicsRatingsDiffMin, gPhysicsRatingsWeight
  sub_8340C7E0  gSpecialCaseModifierShooting
  sub_8340DB90  gBodyTensionBase
  sub_83411AD0  gWeightOfRelativeSpeedToTarget, gWeightOfSpeedToTarget
  sub_83412298  gCheckeeDefaultBalance, gCheckerDefaultBalance, gMaxBalance
  sub_83427068  gBodyTensionBase
  sub_83429800  gBothBehaviorBalanceTunerMax, gBothBehaviorBalanceTunerMid, gBothBehaviorBalanceTunerMin
  sub_834299F0  gBalanceInequality, gBodyTensionBase, gBodyTensionCoeff
  sub_83429EC8  gBalanceInequality
  sub_8342A2A8  gBothBehaviorBalanceTunerMax, gBothBehaviorBalanceTunerMid, gBothBehaviorBalanceTunerMin
  sub_8342A938  gDefaultBalanceThreshold, gDefaultContactCOMRatio
  sub_8342B290  gPlayerPlayerDistanceToEnable
  sub_8342B8C8  gPlayerPlayerDistanceToEnable
  sub_8342C628  gHipCheckContactCOMRatio, gHipCheckNoVelocityUpdateFrames, gHipCheckWeightMin
  sub_8342CDD0  gPlayerPlayerDistanceToEnable
  sub_83437DF8  gDefaultBalance, gHipCheckRestartWithLowControlPenalty, gHipCheckRestartWithLowControlTime
  sub_834551C8  gBodyTensionBase
  sub_834553D8  gBodyTensionBase
  sub_83459350  gDegreesForFullLowHipSwing, gHipCheckLowIntentMinimalDistance, gHipCheckLowIntentSpeedProportion
  sub_83465C48  gMaxFatigue, gRightSickFramesForHold
  sub_83467988  gBodyTensionBase
  sub_8346E460  gBodyTensionBase
  sub_83470DF8  gBodyTensionBase
  sub_83470E48  gBodyTensionBase
  sub_83476288  gAITuneAIHittingTunePenInjuryModifier, gAITuneAIHittingTunePenPlayoffsModifier, gAITuneAIHittingTunePenSecondPeriodModifier
  sub_834859E0  gPenaltyChanceCrossChecking, gPenaltyChanceHitIntoBoards, gPenaltyChanceStumbleInterference
  sub_834BF428  gBFFallOverMaxThreatDist, gChanceToFallOver, gSpecialSaveBFFallOverMinSpeed
  sub_834D8580  gPlayPhysical_Hits, gPlayPhysical_WasHit
  sub_834E9FA8  gBalancePenalty_GlobalScale, gBalancePenalty_HitBumped
  sub_834F0A48  gHitterMaxDistToDestEffort, gHitterMaxDistToTarget, gHitterMinDistToTarget
  sub_834F24E0  gManualHipCheckWinHeightFactor, gSpeedBasedHipCheckProportion, gSpeedBasedHipCheckWinBack
  sub_835038B8  gBoardPlayFootCollisionRadius
  sub_83505630  gBodyTensionBase
  sub_8350E600  gBoardCollisionAnticipationTime
  sub_8351AE18  gAutoHitAimingAtTargetDist, gAutoHitEarlyHitFrames, gAutoHitExtraFeetToEarlyHit
  sub_8351B5A0  gAutoHitDefaultCancelActionRefractoryPeriod
  sub_8351B6D0  gMinRelSpeedForAutoCheck, gMinSpeedToCareAboutRelSpeedForAutoCheck
  sub_83522198  gAggressiveCheckButtonTap
  sub_83524F60  gAutoCheckCancelDelay, gHipCheckRefractoryTime
  sub_835334A0  gPairUpAutoHitChance, gPairUpAutoHitChanceSuperstarVictim
  sub_83552AB8  gCheckingContinueAngle
  sub_8357F748  gCheckSpeedModerate, gCheckSpeedSlow
  sub_8357F9A0  gEndorphineHitInjuryTestFrequency
  sub_8357FAC0  gMaxDistForHit
  sub_83580BF8  gHowDoIStepUpGuarding
  sub_83580CE8  gStepUp_Shove_IncludeBackAnglePies
  sub_83580E90  gHowDoIStepUpShove
  sub_83581208  gDeflectionNoCollisionFrames
  sub_83582C90  gBenchSitHitReactXOffset
  sub_835831D0  gBenchHitBoardDist, gBenchHitReactionDist
  sub_835834E0  gBenchHitReactionDist
  sub_835844B0  gHookReactTimeScale, gTripFallFrequency
  sub_83585028  gHitAssistAccelScale, gHitAssistMaxSpeedScale, gHitAssistanceNumFramesPrediction
  sub_835853A8  gAssumedHitterSpeed
  sub_835888E8  gMaxFramesToAllowCheckingLayer
  sub_83588BF8  gBiasedStrengthSizeAvg, gHitTune_GlobalScale, gHitTune_HitteeBalanceForMax
  sub_83588ED8  gMaxCheckIntentForHipCheck
  sub_83589190  gEndorphineBoardHitSizeDiff
  sub_83589570  gBackPieHitSpeedThreshold, gCheckCollisionRadiusMoving, gCheckCollisionRadiusMovingPush
  sub_83589B38  gBigHitIntimidation, gBigHitIntimidationReduction, gDecEnergyForHitScale_1Player_Hittee
  sub_8358A348  gTimeAfterReleaseForHitToCount
  sub_8358D100  gTriggerCancelDist
  sub_8358D310  gMaxDistToDestForHitCancelInMeters
  sub_8358D478  gInterceptTimeForCheckIntentTrigger, gMaxDistToDestForHitInMeters
  sub_8358DBD0  gGuardingActionEffort
  sub_8358DFC0  gIZGuardingActionEffort
  sub_8358EB88  gFinishChecks_MaxTimeBuffer, gFinishChecks_MaxUndisciplinedLateHit, gFinishChecks_MinTimeBuffer
  sub_8358EE30  gExtraShoveDistanceScale, gStepUp_Shove_DistFromBoards
  sub_83593FC8  gElbowLikelihoodScale
  sub_83594968  gDistForOpponentToBeConsideredCheckable, gMinNumFramesBetweenHit
  sub_83594BD0  gDefaultBalance
  sub_83594CB8  gBenchHitMaxCheckeeSpeed, gEndorphineBoardHitMinCheckingRating, gEndorphineHitBoardDist
  sub_83595320  gBoardplayGameEventFrame, gBoardplaySizeDiffCheckExit, gCheckForBoardplayPenaltyFrequency
  sub_83596B38  gPlayerFaceDiffMaxDegrees, gPlayerSizeDiffMax
  sub_83597360  gMomentumMaintainedWin, gMomentumPhysicsCheckingAttributeWeightBoost, gMomentumPhysicsHitterWeightBoost
  sub_83597FF0  gMaxFramesSinceLastTouchForNoPenalty
  sub_83599B68  gCheckingActionAngleToFacing, gCheckingActionEffort, gCheckingActionMaxLookAheadFrames
  sub_8359A1D0  gNearBoardsHighSlotCheckAngle
  sub_8359B378  gHowDoIStepUpPoking
  sub_8359B600  gHowDoIStepUpBoardPin, gStepUp_BoardPin_DefMaxSpeed, gStepUp_BoardPin_MinPlayers
  sub_8359B7C8  gShoveFrequency, gShoveMinScore
  sub_8359BA08  gChecking_NoSwitchTargetDist, gStepUp_SortMethod
  sub_8359BF10  gCheckingHumanPrevCarrierTicks
  sub_8359C3F8  gStepUpPossibleHitMaxDistApart, gStepUpPossibleHitPlayerReach, gStepUpPossibleHitPredictEnd
  sub_8359C700  gChecking_SlowSpeedDecline, gChecking_SlowSpeedDeclineRadiusMax, gChecking_SlowSpeedDeclineRadiusMin
  sub_8359FAF0  gDefaultBalance
  sub_8359FE68  gBracingPenalty, gFacingAngleDifferenceWeight, gIntentWeight
  sub_835A06E8  gDigAnimPctPenaltyCheckEnd, gDigAnimPctPenaltyCheckStart, gDigNoCollisionFrames
  sub_835A1640  gSlashActionEffort
  sub_835A18C0  gCheckingFromBehind_DistReq, gCheckingFromBehind_SpeedDiff, gCheckingScoreTraitFactorNonPhysical
  sub_835A2338  gHowDoIStepUpMinValidScore
  sub_835A3618  gMissedBoardhitDelayFrames
  sub_835A4258  gAllowHitFromBehindBoardDist, gHitTune_ClosingSpeedForMax, gHitTune_ClosingSpeedForMin
  sub_835A47D0  gMinStressForCoincidental
  sub_835A4BE0  gMaxForceDistBoardsDistRatio, gMinHeightForBoarding
  sub_835A5F10  gSlashHighChance
  sub_835A62E8  gChecking_SlowSpeedDeclineProbMax
  sub_835A6CE0  gSmallHitIntimidation
  sub_835A7078  gFinishChecks_MaxTimeBuffer, gInstigationChanceIntimidation, gIntimidationOnInstigationChanceThreshold
  sub_835A8690  gBenchHitResultVel, gBoardplayAngleDiff, gBoardplayAngleOpponentApproachMe
  sub_835A98D0  gMaxFramesToFindCollidingPlayer
  sub_835BD388  gDeviation_FallingRecoveryTime
  sub_835CF688  gMinPushAwayScale
  sub_835E7990  gBoardCollisionLeadTime, gHitLossChancePerFrame, gSkateIntoBoardsCollisionRetentionMultiplier
  sub_835EB920  gNoPlayerCollisionFrames, gPlayerCollisionClosestPlayerDist, gPlayerCollisionImpactDist

- 2026-10-29, Victoria: RE-1 round 1 hooks, 30 min exhibition. NEVER fired: sub_83411E90,
  sub_83411C48, sub_83429698, sub_83589230 (hip-check family + hit power). sub_8342C628: ~385/s,
  lr 0x8342D13C, r3 = ~14 distinct 0xA9xxxxxx values (skater objects, OBSERVED). sub_83588BF8:
  bursts, lr 0x835A44E4 (95%) / 0x8343832C, r3/r4 = two skater objects. Round 2: PE_* handlers.

- 2026-10-29, Victoria: TUN-9 catalog + static readers. 10,583 tweak objects, 7,779 read by code.
  Hip-check code CANDIDATES (by which tunables each function reads):
  sub_83411E90 (file 133): RadiusMax/Min, AngleMin/Max/Tuner, SpeedMin/Max/Tuner, WeightMin/Max/
  Tuner, DistanceTuner, BalanceRatioTotalThreshold, BehaviorBalanceTunerMax -> hip-check scoring.
  sub_83411C48 (133): LookAheadTime, RadiusMin, RadiusMaxest, PositionOffset -> target acquisition.
  sub_83429698 (134): BalanceThreshold, BehaviorMin/Max, BehaviorBalanceTunerMin/Mid/Max,
  ContactCOMRatio -> knockdown decision. sub_8342C628 (134): ContactCOMRatio, WeightMin,
  NoVelocityUpdateFrames -> contact. Hit: sub_83588BF8 (gHitTune_GlobalScale), sub_83589230
  (gHitPowerForPhysicsGlobalModifier), sub_83588ED8 (gMaxCheckIntentForHipCheck). Skating
  sub_828CC300 reads the fall thresholds and many hip-check control values.
  Shot code: sub_833E87E0 (slap power/accuracy scales), sub_833ECF68 (wrist power).

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
