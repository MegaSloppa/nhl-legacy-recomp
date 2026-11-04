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

## Static candidates (Amy, 2026-11-02; details in docs/static/static-map.md)

Round S1 results: see the next section (2026-11-03). Below: the original static rows. Probe round S1 (user-guide 4g) tested them; promote a row here
only with log evidence. Reaction table entry names (image strings): 0 incidental, 1 nudge,
2 stumble, 3 stumble fall (= "normal hit" above, 0x83ABF57C), 4 fall (= "knockdown", 0x83ABF598).

| Key (proposed) | Address / offset | Confidence | Static evidence (static-map.md section) |
|---|---|---|---|
| ROGUE_VA_BAP_PLAYER_PTR | 0x83C876D4 | CANDIDATE | cache filled by `sub_834DC8A8`, gated by BAP flag 0x83AC5F63 and match 0x83C1E89C (7) |
| (game state) | word 0x83C1D220, switch `sub_8330C890`, table 0x83AA5B20 | CANDIDATE | 14 named states (9) |
| ROGUE_ADDR_BAP_GAME_END | state DONE (9) via `sub_8330C890` | CANDIDATE | (9) |
| ROGUE_ADDR_GOAL_SCORED | `sub_83480248` (r3 scorer, r4 assister) | CANDIDATE | posts Goal / GoalAwarded (6) |
| ROGUE_ADDR_PERIOD_END | `sub_8331F408` | CANDIDATE | posts EndPeriod, one caller (7) |
| (period) | word 0x83C876B8 | CANDIDATE | (8) |
| ROGUE_ADDR_PUCK_BODY_HIT | `sub_83328180` (r3 puck, r4 victim, r5 code; puck+180 last touch) | CANDIDATE | posts PuckHitPlayer / BlockShot (7) |
| (penalty decide / apply) | `sub_83487DC0` / `sub_83485BD0` | CANDIDATE | (10) |
| (ratings) | raw +1240 + 4 * id, effective `sub_83501A68(player, id)` | CANDIDATE | (12, 13) |
| (energy / injury) | player +360 energy, +368 injury pointer | CANDIDATE | (11, 13) |
| (players) | 0x83C1D080 players[team * 20 + slot] | CANDIDATE | (8) |

## Round 29 result (2026-11-04, owner + Theresa)

| Key | Result | Confidence | Evidence |
|---|---|---|---|
| BUG-3 root cause | the BAP flag 0x83AC5F63 drops to 0 for ONE poll mid-game (3 times: t=490 in period 1, 687, 987); the round 9 code ended the game on the first one | CONFIRMED | lifecycle lines "end check BAP flag 0 started ... cleared after 0.0 s"; the round 20 hold time absorbs them |
| On-ice marker +0x22C | 0x06 on the ice in this BAP game (0x00 in S1 / round 8); 0xFF off | OBSERVED | player periodic lines; test != 0xFF |
| Effect on round 27-29 code | TOI identification never counted an on-ice second (identified=0); sprint hunt collected 0 samples | OBSERVED | fixed in round 30 |
| Stat search | 1.8 s (was 30-41 s), 83 deep checks, after the 1st period end | OBSERVED | owner noticed no stutter |
| Grouping | both teams had 9 shots at the intermission: the (SOG, SA, TOI) key merged 2 + 2 copies into one group -> no opponent -> GF from skater goals (3, real 4) | OBSERVED | round 30 groups by a hash of all 20 records' TOI + SOG |
| Pro's line | correct (fallback slot idx % 20 = 17) | OBSERVED | owner |

## Round 26 result (2026-11-04, owner + Theresa)

| Key | Result | Confidence | Evidence |
|---|---|---|---|
| Shot hooks sub_833E87E0 / sub_833ECF68 (pro in r3..r10) | pro-only shot perks WORK: pro shoots harder, others at baseline; wrist shot power (Quick Release x3) obvious | CONFIRMED (behaviour, owner) | |
| Speed boost sub_828B9258 | tens of thousands of calls, 0 with the pro; counts rise when skipping cut-scenes to a faceoff | REJECTED as a per-player hook | Jet Skates moved to the rating override sub_83501A68 (round 27) |
| Table copies | 6 tables = 3 identical copies per team; pair distance 0x6F0 this game (0x79C in round 23): NOT fixed | OBSERVED | stats-table lines |
| Identification | identified=0 (copies tie); fallback slot idx % 20 = 17 was right this game | OBSERVED | round 23: slot 16 |
| Score | GF read -9249: an opponent copy's goalie slot 19 was garbage at game over | OBSERVED | round 27 validates slot 19 + reads the live copy |
| Search | 30-41 s, read 190-245 MB (residency halves it); most time = deep checks on all-zero memory | OBSERVED | owner: stutter in a penalty cut-scene |

## Round 24 result (2026-11-04, owner)

Values CORRECT (filled-in result matched the box score; pro identified). The single intermission search caused
~30 s of stutter (round 23's game-over search stopped after 4 s at 8 garbage hits; with the strict check it reads
the whole range). Working hypothesis: first reads of never-touched committed guest pages make Windows allocate
them. Round 25: resident pages only (K32QueryWorkingSetEx), stop at the first team pair, A-range first.
Layout + identification: promoted to CONFIRMED for the box score fields G, A, +/-, PIM, SOG, hits, TOI (two games:
round 20 slot 17 / team 0, round 23-24 values matched); GF from the opponent goalie OBSERVED.

## Round 23 result: both teams found; TOI field; pro not in slot idx % 20 (2026-11-04, owner + Theresa)

Owner's game: 3-4 loss after a shootout (last goal hook: period 255), shots 27 (pro's team) / 30; pro 0 G, 0 A,
0, 6 SOG, 10 PIM, 14 hits, TOI 13:04; pro idx 37 (team 1). Game-over probe: 8 layout hits in 4.0 s (pointer
search afterwards: 110 s, in the background, owner noticed no freeze).
| Key | Result | Confidence | Evidence |
|---|---|---|---|
| Pro's record | table p0 A276CFE2 slot 16: G 0, A 0, +/- 0, PIM 10, SOG 6, hits 14 = box score | OBSERVED | slot 17 (= idx % 20) empty, all zero |
| TOI | s16 at P - 16, seconds: pro 0x0310 = 784 = 13:04; goalies 3877 / 3888 (OT game) | OBSERVED | exact match |
| Two team tables | p0 A276C846 (team 0) and A276CFE2 (team 1), 0x79C apart; team 0 first | OBSERVED (1 game) | skater SOG 30 / 26, goalie SA 28 / 30, GA 3 / 3 (3-3 + shootout) |
| Goals | skater goals (-8) miss simulated goals (skater sums 1 / 0 for 3 / 3); goalie GA counts them | OBSERVED | round 20: GA 4 with 2 sim goals |
| False hits | 6 of 8 layout hits were garbage; goalie TOI >= 30 and >= 8 skaters with TOI reject all 6 | OBSERVED (offline check on the dumps) | |
| Pointer path | owners of the table object are other heap objects (no image / match / pro word) | OBSERVED | stats-ptr / stats-ptr2 lines |

## Round 22 result: locator found the other team only; scan too slow (2026-11-04, owner + Theresa)

Owner's game: pro idx 37 (TEAM 1, first time since round 9), game misconduct in period 1, rest of the game in
the box and simulated (game start to DONE ~2 min real time). Box score: 0 G, 0 A, 0, 0 SOG, 15 PIM, 1 hit,
TOI 0:41.
- Scan: 2.7 s, then ~6.8 s per pass, every 20 s until found: EXTREME stutter (owner). REJECTED for play.
- Found 2 identical tables (p0 AA8CA7DC and AB260694: skater SOG 4, goalie GA 2 / SA 6) only after period 1:
  copies of ONE team. Slot 17 of the picked copy: all zero (not the pro: 15 PIM) -> the other team (team 0).
  The pro's team table was missed: its goalie had < 5 shots against (signature minimum).
- At game over the picked copy's goalie words read 0xDBDB (being freed): copies can die before the result.
- No word in match (16 KB), pro (8 KB) or globals 0x83C00000-0x83CC0000 points into [p0 - 0x400, p0].
- Team block at p0 - 0x78 was 0 in both copies (round 20's team block is not part of these copies).
- Round 23: no scan during play; probe-only diagnostic at game over (relaxed signature, full table dumps,
  pro object dump, two-level pointer search over 0xA0000000-0xC0000000, 0x40000000-0x50000000, image
  0x83000000-0x84000000).

## Round 21 result: the stat records are a separate object (2026-11-04, owner + Theresa)

Owner's game: 2-1 OT win, shots 23 / 35, pro 0 G, 1 A, 0, 9 SOG, 2 PIM, 14 hits, TOI 18:22. Pro idx 17 team 0
again, match pointer AC7CC080 the whole game. The auto-fill showed nothing: at game over match + 0x286 + 86 * 17
held only 0xDB bytes (`box-record` lines), i.e. memory that is not the stat records.
- REJECTED: "stat records at match + 0x286". In round 20 they sat right after the match object by chance.
- Still OBSERVED: the record layout (86 bytes, field offsets) from round 20.
- Round 22: records found by layout (goalie SA / GA = sum of period values, plausible skater records), any
  heap address; the probe logs every table found and every word in the match / pro / globals that points
  just before it (pointer path, for a cheaper reader later).
- Lifecycle: clean again (DONE 30 s after the OT goal; game over 3 s later; no resume). OT goal = period-end
  flag 1, period word 3.

## Round 20 stat finder: the game's box score in the match object (2026-11-04, owner + Theresa)

Source: `rogue_stats_probe.txt` (one BAP game, 1,641 s, 22 checkpoints, 146 regions) + the owner's box score:
4-2 loss, shots 36 (pro's team) / 27, pro 1 G, 0 A, +1, 6 PIM, 13 SOG, 10 hits, TOI 17:59.

| Key | Result | Confidence | Evidence |
|---|---|---|---|
| Stat records | (location REJECTED in round 21, see above) match + 0x286 + 86 * slot = +/- (s16 BE) of roster slot 0..19 of the pro's team; G at -8, A at -4, PIM at -2, SOG at +20, hits at +22 (all s16) | OBSERVED (1 game), in addresses.h for the round 21 behaviour test | pro = slot 17: G 1, A 0, PIM 6 (2 / 4 / 6 by period), +/- -1 -> 0 -> +1, SOG 13, hits 10, all = box score. Skater SOG sum 36 = team shots; skater G sum 2 = team goals; 3 assists on 2 goals |
| Goalie records (slots 18 / 19) | GA at -8, per-period GA at -6 / -4 / -2; SA at +2, per-period SA at +4 / +6 / +8 | OBSERVED | slot 18: GA 4 (2 / 1 / 1), SA 27 (8 / 14 / 5) = opponent goals / shots |
| Team block | match + 0x1F4 = team goals (2); hi16 of match + 0x208 = goals against (4); match + 0x20C = shots (36), per-period shots at +0x210 / +0x212 / +0x214 (7 / 11 / 18) | OBSERVED | not used: the code sums the records instead |
| Simulated goals | counted in the records (GA 4, goal hook saw 2 of the 4 opponent goals) | OBSERVED | goal hook fired 4 times for 6 goals |
| Score update timing | records / team block change some time after the goal call (not within 4 s) | OBSERVED | the pro's goal at t=1479 showed at the t=1559 checkpoint |
| Globals 0x83C36C30.. | index / permutation tables (0..174), NOT stats; they matched 13 / 10 / 6 / 27 / 36 by chance | REJECTED | values set once at t<=120 and never changed |
| TOI | not found (changes every second, filtered out). Round 21 dumps the pro's whole record at game over | OPEN | |
| Record mapping when the pro is idx 20-39 | unknown (this game: slot 17, the S1 pro index) | OPEN | round 21 panel flags it |
| Lifecycle (BUG-3 build) | game over 7 s after the third period end, no false game over, no resume | OBSERVED (1 game) | checkpoint "game over" t=1641, period end t=1634 |

## Round 18 result: physics window (2026-11-03 17:00, owner + Kathleen)

- gPlayerPlayerDistanceToEnable stock = **9.00 ft** (2.7 m); the window raised it to 16.4 ft. 30+ windows
  opened (F8): **0 contacts at range**. Promotions happened only when the pro was really in contact
  with an opponent. So that tweak does not make the contact check pair far players (the real
  gate is elsewhere, e.g. body overlap). Shockwave PARKED (design.md section 8).
- **Promoting a real light contact to a knockdown makes the target fall:** at t=43639500 a 0.233 contact
  promoted in the window -> knockdown -> fall routine started 15 ms later (choice-only forcing, stored
  power not forced). This is the mechanism for the Bulldozer perk.
- Bug (probe only): a promoted knockdown re-opened the window every frame (same victim promoted 15-22
  times in 0.5 s). Harmless; left as is while the shockwave is parked.
- Contact floor 0.02 felt about the same as 0.10 (owner).

## Round 17 result + contact floor (2026-11-03 16:30, owner + Kathleen)

- Shockwave (round 13 setup, 2 m / 5 m / 8 m, stored power on and off): no visible effect on anyone but
  the primary target of a check. (Design note: the shockwave never targets the primary - on a natural
  knockdown he already falls; on Arm the contact partner is excluded.) 12 forced targets, 0 falls.
  Conclusion after 6 rounds: **our own dispatcher calls do not produce falls**, near or far. Parked in
  favour of the physics window (round 18).
- **Contact floor test (round 10) WORKS:** at 0.10, 73 contacts promoted in one game; opponents stumbled
  much more easily, and one big hit (probably a heavy stumble) became a fall with an abrupt transition
  from a stumble start to flat on the back. Owner: halve the floor again (0.05) for even smaller
  contacts; good for hitting perks. So promoting REAL contacts through the reaction choice produces
  visible reactions; only the fake contacts do not.

## Round 16 result + why round 13 was different (2026-11-03 15:15, owner + Kathleen)

Owner: no falls with balance hit 50 / 500 or balance write 100, in kind 0 and learned modes.
- **Balance lead REJECTED:** `sub_834E9FA8` was never called by the game during the whole session (all
  20 logged calls were ours), and the balance-loss float was 0.00 before every forced call. Natural
  collisions do not go through it, so it is not what drops players.
- **What round 13 had:** the only forced falls (3) were the targets CLOSEST to the pro: d = 1.2, 2.0 and
  1.6 m. Every round-13 target at 2.6 m or more stayed up (7). Round 13 also left the stored power at
  the natural value (forced only the reaction choice) and passed kind 0 / float 1.0 + the post step.
- Rounds 14-16 changed that setup: round 14 kind 6 (and a bench filter that dropped close players),
  rounds 15-16 forced the stored power too. Round 15's close targets (1.2, 1.4 and 2.0 m) did not fall
  either, so the stored-power change is the main suspect.
- HYPOTHESIS: a forced knockdown only becomes a real fall when the target is inside the player-player
  physics range (gPlayerPlayerDistanceToEnable, "Physics/Ragdoll/Balance/Modifiers", feet) of the pro,
  so the game's own physics interaction between them is live, and with the round-13 power handling.
  Round 17 restores round 13 (stored power not forced, kind 0 + post, balance off) with a 2 m radius.

## Round 15 result: the reaction path alone never drops a player (2026-11-03 15:00, owner + Kathleen)

Owner: no knockdowns in either mode; push did nothing visible.
- The source hook worked: every forced call now shows power 0.900 in the reaction choice AND in the hit
  contact (the stored slot), with the knockdown descriptor, exactly like a natural knockdown.
- Still **0 fall-routine starts for 14 forced targets**, while both natural knockdowns in the session
  started one (0.847, 0.930). One target (idx 29) was forced at t=36812890 without falling, then fell
  0.9 s later from a natural hit.
- Conclusion (OBSERVED, 4 rounds): the dispatcher path (power, descriptor, reaction state, hit
  contact, post-contact step) is not what makes a skater fall. The 3 round-13 falls were probably
  natural contacts at the same moment.
- The pairwise check is the ITK player-player physics (radius tweak gPlayerPlayerDistanceToEnable,
  "balance-magnet forces"). HYPOTHESIS: the fall comes from the balance system (balance-loss float
  *(*(player+80)+64)+368, thresholds 23 / 25), fed by physical contact. Round 16 tests balance.
- Corrected addresses in static-map.md 16-17 (hex conversion slips): manager *(0x83BB8288), mode byte
  0x83BBC230+29, radius tweak 0x83C8F25C, controller managers 0x83C29790.

## Round 14 result + fall trace (2026-11-03 14:45, owner + Kathleen)

Owner: with kind 6 (round 14 default) no shockwave target fell; push not tried.
- **Fall detector:** the probe's `call 829380A0` lines (the routine that runs only on real falls) start
  a burst (r6 = heap pointer) within 0-50 ms of EVERY natural knockdown of a skater (8 / 8 across rounds
  13-14; none for the goalie). For forced knockdowns: 3 bursts for 14 targets, all in round 13 with kind 0
  (round 14, kind 6: 0 of 4). This matches what the owner saw.
- **Likely cause:** the dispatcher stores the power from `sub_833CA500` at r1+84 BEFORE the reaction
  choice and passes that slot to the hit contact and the fall path. Rounds 12-14 forced only the choice's
  f1, so downstream code saw the real power (0.09-0.6). Round 15 forces it at the source.
- Round 14's on-ice filter (+0xD0 != 0) dropped most opponents (0 -> 11 counted over the game): +0xD0 is
  filled only after some event. Round 15 uses |x| < 12.8 m on the live position instead.
- Contact kinds again: the pro's hits kind 6 (0.0-0.25), hits on the pro kind 0 (1.0).

## Round 13 result: shockwave with a learned contact kind (2026-11-03 14:00, owner + Kathleen)

Owner: some forced knockdowns did make players fall, but inconsistently. Best case: on a strong real
hit, a second nearby opponent clearly fell from the shockwave while a third, equally close, did not
react. Mode "kind 0" (round 12): no reactions (only 2 armed fires there, both with no target in range).
- **Learning bug:** the first real knockdown learned kind 6 / float 0.25, but the game calls the
  dispatcher a second time in the same frame for the same pair (applied 0, kind 0, still the knockdown
  descriptor), which overwrote it with kind 0. So 1 forced call used kind 6 and the other 9 used kind 0.
  Fixed in round 14 (learn only kind != 0 with applied 1; default kind 6 / 0.25 until something is learned).
- **Contact kinds seen (`contact-args`):** the pro's knockdowns kind 6 with float 0.247-0.25; his normal
  hits kind 6 with 0.034-0.09; hits on the pro kind 6 (0.06-0.09), kind 1 (0.0) or kind 0 (1.0).
  HYPOTHESIS: kind 6 = body check, float = contact strength.
- Player +40 stayed 0 before / after every call: not the fall state.
- **Live position CONFIRMED-ish:** *(player + 32) + 48 moved with the pro all game (1,700 moving samples).
- **Velocity:** physics body +0x50, +0x60, +0x70 follow the position change (cos 0.98 over 1,700
  samples, ratio 0.33 = metres per 1/30 s frame). OBSERVED; which copy the integrator uses is unknown.
- "opponents on ice" counted 17: body positions exist for the bench too. Round 14 filters on +0xD0 != 0.

## Round 12 result: shockwave test (2026-11-03 13:30, owner + Kathleen)

Owner: the shockwave fired (log lines) but nobody fell, stumbled or reacted visibly. No crash.
- 7 fires (5 armed, 1 on a knockdown, 1 "pro position unreadable" = on the bench). 28 forced
  dispatches: every one ran the full pipeline exactly like a real knockdown in rogue_probe.txt
  (power forced to 0.90 -> knockdown descriptor 0x83ABF598, hit contact called, dispatcher returned
  1) for 20; 8 returned 0 (gate / state 19), always on players already hit by an earlier shockwave.
- So the reaction STATE path is not what makes a skater fall. Note the dispatcher also returns 1 when
  the victim is already in the chosen state (it then skips `sub_83429300`). HYPOTHESIS: the visible
  fall comes from the contact / interaction step that runs before the dispatcher (`sub_8342B8C8`,
  the pairwise contact check, calls `sub_8350FB88` first) or from physics data it leaves behind.
- **+0xD0 is NOT the live position**: the pro's value was identical (3.39, -24.82) in two fires 32 s
  apart during play, and the 10 Hz velocity hunt never collected 300 moving samples. It changes only
  sometimes (snapshot / last-event position?). Distances in the shockwave lines are unreliable.
Next: slices of the contact check and its helpers (S10), a live-position finder in the probe.

## Fall path (static S9, 2026-11-03, Kathleen; static-map.md section 16)

| Key | Address / offset | Confidence | Evidence |
|---|---|---|---|
| dispatcher inputs | `sub_83429560(r3 victim, r4 hitter or 0, r5 -> u32, r6 -> float)` | CANDIDATE (static, 3 callers agree) | `sub_8342B290` calls with r4 = 0, r5 -> 0, r6 -> *(0x82014BEC) |
| dispatcher gate | `sub_8341D8C8(player)` -> 1 = may react | CANDIDATE | checks player+40, controller state, mode bytes |
| state transition table | 0x82222AE8, 19 x 19 words [state * 19 + reaction id] | CANDIDATE | `sub_83429248` |
| live position | *(player + 32) + 48: x, y, z, w (physics body) | OBSERVED (S10 static + round 13: moves with the pro) | velocity candidates body +0x50 / +0x60 / +0x70 (m per frame) |
| player position | +0xD0 x, +0xD4 y, +0xD8 z (metres); facing +0xE0 | REJECTED as live position (round 12: stale for 32 s); a snapshot of some event | bench = 0, goalies at z +/-26 |

## Round 9 results (Kathleen, 2026-11-03 11:30; exhibition period + one full BAP game, no claim)

| Key | Result | Confidence | Evidence |
|---|---|---|---|
| ROGUE_VA_BAP_MODE_FLAG | 0 for a whole exhibition period; 1 in BAP | CONFIRMED | exhibition: match non-zero, flag 0, cache 0, no auto start |
| ROGUE_VA_BAP_PLAYER_PTR | the pro (this time idx 37, team 1) | CONFIRMED (2 games) | no claim all game; filled at PRE_GAME; goal hook tagged the pro's own goal `who=C`; the immunity test keyed on it |
| Lifecycle poll | started ~9 s early: at bapflag 1 while the state was still STORYTELLER left over from the exhibition (PRE_GAME came 9 s later); game over at DONE as designed | OBSERVED | round 10 also waits for the pro pointer (filled at PRE_GAME) |
| Penalty immunity (decide forced to 0 for the pro) | works | CONFIRMED (behaviour) | 6 infractions blocked (8 decide calls: stick infractions, type 6, are decided twice), no call, no arm, play went on (owner). Control with the test off: the pro's next infraction was called normally |
| After a game | the front end makes a short-lived match (flag 0, state GAME_ON) and leaves state GAME_ON / no match; after an exhibition the state stays STORYTELLER until the next game's PRE_GAME | OBSERVED | harmless: the poll needs flag 1 + a pro |

**Rating id map (CONFIRMED 2026-11-03):** the pro's attribute screens matched the 29 raw values at
+1240 + 4 * id one to one, consistent with every static hint (section 13 of static-map.md):
0 speed 82, 1 acceleration 81, 2 agility 83, 3 balance 74, 4 wrist shot power 81, 5 wrist shot accuracy
72, 6 slap shot power 82, 7 slap shot accuracy 70, 8 passing 82, 9 deking 77, 10 puck control 83,
11 faceoffs 71, 12 aggressiveness 68, 13 body checking 71, 14 stick checking 72, 15 endurance 78,
17 discipline 78, 18 strength 72, 19 fighting skill 56, 20 durability 75, 22 hand-eye 82, 23 shot
blocking 62, 24 offensive awareness 83, 25 defensive awareness 71. Not on the screens: 16 = 0,
21 = 6, 26 = 66, 27 = 74, 28 = 68 (hidden; static: 26 goalie scale, 27 backhand / redirect, 28 fight).
Ties resolved by the static evidence: 8 / 22 (82) and 11 / 13 / 25 (71) and 10 / 24 (83).

**Checking feel (owner, round 9) + data:** many contacts never become a hit. In round 9 the reaction
choice ran 338 times for the pro on opponents = 60 contact episodes; only 11 became a hit or
knockdown. Peak power per episode: 22 below 0.2, 27 in 0.2-0.5, 11 above 0.5 (hit threshold ~0.5).
So most "slide past" contacts ARE evaluated, with low power. Round 10 tests a floor (user-guide 4i).
The speed loss while aiming a check is a separate system (candidates: gHitIntentAssist* read by
`sub_83585028`, gAutoHitTargetRadius / gAutoHitInterceptRadius by `sub_8351AE18`,
gInteractionCheck*SlowDown* with no static reader yet). All are global tunables.

## Round S1 + round 8 results (Kathleen, 2026-11-03; one full BAP game, owner notes 1-13)

Source: `rogue_probe_s.txt` (880 lines, 2,140 s) + `rogue_probe.txt` (round 8 marker scan), one
Be-A-Pro game played to the end, claim made at t=165 s. No exhibition this time. Times = seconds
after the probe started.

| Key | Result | Confidence | Evidence |
|---|---|---|---|
| ROGUE_VA_BAP_PLAYER_PTR 0x83C876D4 | the pro (idx 17, team 0) | OBSERVED, in addresses.h for the round 9 behaviour test | Set at PRE_GAME (t=0), before any claim; `cache==claim:yes` in every sample after the claim; the round 8 marker scan (independent) only ever marked this object (1,350 s "one = ME", "several" 0). After the match: match 0, BAP flag 0, the word keeps the old address and the object's class changes to 0x822708B0 (re-used), so the read MUST be gated |
| ROGUE_VA_GAME_STATE 0x83C1D220 + states | order as named | CONFIRMED (PRE_GAME, DONE) | -1 -> PRE_GAME once at load; STORYTELLER (10) at game intro and each intermission; per stoppage GAME_ON -> GOOF_OFF -> STORYTELLER -> PRE_FACEOFF -> SYNCHING -> FACEOFF -> GAME_ON (23x) or GAME_ON -> PRE_FACEOFF -> ... (21x, no replay); DONE once, 14 s after the third period end |
| ROGUE_VA_MATCH_PTR 0x83C1E89C, ROGUE_VA_BAP_MODE_FLAG 0x83AC5F63 | 1 / non-zero for the whole BAP game | CONFIRMED in BAP | both drop to 0 at the same sample ~154 s after DONE. Exhibition value still unchecked (round 9) |
| flow phase 0x83C1D254 | 1 during play, 0 from the last period end | OBSERVED | |
| period word 0x83C876B8 | 0-based: 0, 1, 2 = periods 1-3 | OBSERVED | 238 after the match |
| `sub_8331F408` period end | once per period (3x), r3 flag 0 | OBSERVED | intermission note 6 |
| `sub_83480248` goal | r3 = scorer. **r4 = the goalie scored on, NOT the assister** | OBSERVED | opponent goal (note 4): r4 = idx 18 (own goalie); teammate goal (note 11): r4 = idx 38 (their goalie). Simulated play does NOT call it (note 6: opponent goal during sim, no line). Only 2 goal lines in the game |
| `sub_83487DC0` penalty decide | rec+4 offender, +8 victim right; returns 1 = called | OBSERVED | 50 decisions, 10 called. Fires at the INFRACTION, ~5 s before the whistle (note 5). lr 0x8357F858 = body contact (types 0/2/3), 0x8358BE30 (type 7) and 0x8358B11C (type 6) = other infractions |
| `sub_83485BD0` penalty apply | r4 offender, r5 victim right; r7 = penalty type (different numbering from decide) | OBSERVED | notes 5, 7, 10 (pro offender: types 24, 3, 26) and 9 (pro victim, type 18) all match by time and role. Penalties on a goalie appear twice 0.4 s apart (type 13; decide type 3 then 2) |
| `sub_83328180` puck contact | fires (237 lines logged, capped 20/s) but meaning UNCLEAR | CANDIDATE | puck+180 "last touch" is -1 on every line (wrong field); codes 13-53 spread widely; bursts of 10-40 lines around whistles; notes 8 / 12 not identifiable |
| event post function | vtable slot 1 = 0x8299E478 (manager 0xBFB5DCA0, vtable 0x82057B74) | OBSERVED (one read) | next: slice `sub_8299E478` |
| GameEvent type slots | 169 of 175 filled at PRE_GAME; values are 32-bit hashes (e.g. Goal = 0xE47D2A18) | OBSERVED | list in rogue_probe_s.txt |
| ratings +1240 + 4 * id | 29 stable values, 56-83 except id 16 = 0 and id 21 = 6 | OBSERVED (layout); id meaning still CANDIDATE | identical all game; needs the pro's attribute screen to map ids |
| energy +360 | 100 -> drains on ice (lowest 4.3), refills on the bench, 100 at intermissions | CONFIRMED (behaviour matches shifts) | |
| +364 | 15 -> 9.5 during period 1, back to 15 at the first intermission, then 15 for the rest of the game | UNKNOWN | not "fatigue cap" as labelled |
| marker byte +0x22C | 0x00 on the pro while on the ice, 0xFF on the bench / at intermission; never on anyone else | OBSERVED | 76 changes, all on the pro |

Owner gameplay notes this round (for later work): "sim to next shift" can skip the pro's shifts
entirely when he never reaches the bench before a whistle (note 2); the pro dodges a teammate's
shot automatically with a small jump (note 12).

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
