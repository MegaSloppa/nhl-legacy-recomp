# Current status (AUTHORITATIVE)

Last updated: 2026-11-03 (18:00, end of day) by Kathleen (monday.com agent). If this disagrees with another doc,
this wins; fix the other doc.

## Baseline

- **State at 2026-11-03 end of day:** the pro is found automatically (cached pointer, gated) and BAP
  games start / end by themselves (state machine poll). Live event perks: Freight Train, Wrecking Ball,
  Untouchable, Heavy Shoulders, Bulldozer. Shockwave parked. The bullets below are history.
- **The modded game builds and launches** on the owner's machine (branch `legacy-rogue` of
  `MegaSloppa/nhl-legacy-recomp`). Wiring is final: `rogue::fx::Draw()` and `rogue::Tick()` run
  every frame, `rogue::DrawPanel()` only while the menu is open.
- **BUILD-3 smoke test PASSED 2026-10-29** (owner, commit 3d595fe): career start, FX with the
  menu closed, perk layers write the store, revert at game end, save files.
- **Tunable perks write the live value since TUN-6** through `TweakIndex` (N+32 + heap copy
  H+36, CONFIRMED by TUN-5b). The parent ITunableStore is no longer used by the mod (it edits the
  TYPE TAG, N+8). **Perk readout verified 2026-10-30** (owner): wrist shot power showed
  stock -> perk value after "BAP game started" and returned to stock after "BAP game ended".
  All perk tunables are GLOBAL (CPU players get them too) until per-player hooks exist; only
  Cannon's tunable is confirmed to change gameplay.
- **Hit detection CONFIRMED (RE-1, rounds 3-4, 2026-10-30):** the hit contact `sub_833E0BF8` and
  its knockdown reaction value are in `addresses.h`; the hook lives in `rogue_hooks.cpp`.
- **The BAP player is claimed by hand (2026-10-30 17:45 package):** RE-2 round 5 rejected the
  +0x38 "human" byte. The panel's "That was me" buttons claim the latest contact's hitter / victim;
  with a claim the mod counts the pro's hits and knockdowns of skaters in a BAP game (works, round
  6: 27 hits / 13 knockdowns in 2 periods; the game's own hit stat is narrower, see below).
- **First event perks (2026-10-30 18:45 package, untested in a real run):** Freight Train (+15%
  hit power per stack) and Wrecking Ball (every normal hit becomes a knockdown) act on the claimed
  pro's hits only, through the reaction choice hook (CONFIRMED mechanism).
- Event perks are still inert. Shop, Notoriety, FX and saves are driven by the panel's Manual
  buttons.
- Owner's toolchain: VS Build Tools 2026 + standalone LLVM + Vulkan SDK + PowerShell 7.
  Every setup problem hit so far and its fix: `user-guide.md` section 7.
- GitHub: agents can READ the fork but writes fail from the agent's connection. Deliver changes
  as a zip of changed files only (00-START-HERE rule 12).

## END OF DAY 2026-11-03 (18:00): pinned, commit of rounds 9-19

Owner pinned everything for today and commits this package (rounds 9-19 + design direction).
**Not yet built / run since round 18:** the perks Heavy Shoulders and Bulldozer, the perk Untouchable
(only the probe test ran), `tools/refs_to.py`. First thing next session: build, quick BAP game with them
(user-guide 4j / 4r).
Owner 2026-11-03: changing the game's own career data is fine (00-START-HERE rule 2 updated).

Pinned work, in the order discussed (owner picks):
1. **RE-9, the league reacts** (owner priority, design.md section 0): run `tools/refs_to.py` on the
   2026-11-02 index with `docs/static/re9_anchors.txt`, then slices of the trade request / contract /
   news / award / trade interest code; make dominance visible there.
2. **Ratings past 99** (RE-11): override `sub_83501A68` for the pro (also feeds RE-9: trade interest
   and contract value probably read ratings).
3. **In-career unlocks** (DES-3): fusion (Heavy Shoulders x3 -> Bulldozer, Freight Train x3 -> Wrecking
   Ball, Cannon x3 -> Critical Slapshot) and milestones tied to league reactions; replaces the
   cross-career Legacy unlocks. Until then: `unlock=<id>` lines in rogue_meta.txt.
4. Tuning after play: Heavy Shoulders floors, Bulldozer feel, Hype / Notoriety inflation from knockdowns.
5. Parked: Shockwave (7 rounds, re-findings.md "Round 12" .. "Round 18"), puck contact codes (RE-4),
   final score (RE-5), event dispatch `sub_8299E478` (RE-6), the speed lost when aiming a check (DES-4).

## Design direction 2026-11-03 (17:50, owner): one career, the league reacts

design.md section 0: one career is the whole game (no multi-career Legacy, no fail condition), balance
secondary, priority = the game's own career layer visibly reacting (offers, trade interest, contracts,
news, awards). Next RE: RE-9 static pass on those systems (string anchors in design.md section 0;
query the 2026-11-02 whole-program index with `tools/refs_to.py`).

## Checkpoint 2026-11-03 (17:05): shockwave parked; perks Heavy Shoulders + Bulldozer

Round 18: raising the player-player physics range created no contacts at range (0 in 30+ windows):
Shockwave parked after 7 rounds. But promoting a real light contact to a knockdown makes the target
fall, so two contact perks went in (perk_catalog.cpp, rogue_hooks.cpp ContactFloor): **Heavy
Shoulders** (Rare, 3 stacks, hit floor 0.20 / 0.10 / 0.05) and **Bulldozer** (Legendary, contacts >= 0.02
become knockdowns). Opposing skaters only. Not run in game yet. Existing meta files need
`unlock=heavy_shoulders` / `unlock=bulldozer` in rogue_meta.txt (user-guide 4r). Bulldozer will inflate
the knockdown count used for Hype / Notoriety (DES-2 balance).

## Session 2026-11-03 (16:40): round 17 result; contact floor works; round 18 physics window

Shockwave via our own dispatcher calls: 0 falls in 6 rounds, parked. Contact floor 0.10 works well
(owner wants 0.05). Round 18 (probe, no new .cpp): contact floor options 0.20 / 0.10 / 0.05 / 0.02;
shockwave "physics window" (default method): F8 / Arm / a knockdown raises gPlayerPlayerDistanceToEnable
to the radius for 0.4 s and promotes every real contact the pro makes with an opponent in that window
to a knockdown. user-guide 4q. Next perk candidate: a checking perk on the contact floor (DES-4).

## Session 2026-11-03 (15:15): round 16 result; round 17 = round 13 setup at close range

Balance is not the fall path (the game never called the balance penalty). Round 13's forced falls were
the 3 targets within 2 m of the pro; later rounds changed the setup. Round 17 (probe, no new .cpp):
stored-power forcing optional (default off, as in round 13), balance off by default, 2 m radius option.
user-guide 4p.

## Session 2026-11-03 (15:05): round 15 result - reaction path ruled out; round 16 tests balance

Round 15 forced the power everywhere (0.9 in the stored slot too): still no falls (0 of 14 fall-routine
starts). The reaction path does not drop players. Round 16 (probe, no new .cpp): the shockwave also
hits the target's balance - by calling the game's balance penalty `sub_834E9FA8(target, bumped,
amount)` (default, amount 5 / 50 / 500) or by writing 100 into the balance-loss float. Natural balance
penalties are logged for calibration. user-guide 4o.

## Session 2026-11-03 (14:50): round 14 result; round 15 forces the power at its source

Round 14 (kind 6): no target fell. The fall routine trace shows every natural skater knockdown starts a
fall, forced ones rarely: the stored power the fall path reads was never forced. Round 15 (probe, no
new .cpp): pass-through hook on the power function `sub_833CA500` (forced value during shockwave
calls only), mode default kind 0 + post step, on-ice filter |x| < 12.8 m. user-guide 4n.

## Session 2026-11-03 (14:05): round 13 result - first shockwave falls; round 14

Owner saw real falls from the shockwave, inconsistent. Cause found: the learned contact kind was
overwritten by a duplicate same-frame call with kind 0, so most forced calls still used kind 0. Velocity
found (body +0x50 / +0x60 / +0x70). Round 14 (probe, no new .cpp): learning fix + default kind 6 / 0.25,
on-ice filter, push test (velocity added away from the pro). user-guide 4m.

## Session 2026-11-03 (13:50): slices S10 read; shockwave round 13

The contact check (static-map.md 17) passes the dispatcher a contact KIND and a float and runs a
post-contact step `sub_833DBD18` after it; round 12 passed kind 0 and skipped the step. Live position =
*(player + 32) + 48. Round 13 (probe, no new .cpp): shockwave mode "learned" copies the kind / float of
the pro's last real knockdown and runs the post step; `contact-args` / `forced` log lines (kind, float,
player +40 before / after); velocity hunt on the physics body. user-guide 4l.

## Session 2026-11-03 (13:30): round 12 result - forced dispatch runs but nothing falls

The shockwave test fired and the game accepted 20 forced knockdowns (same log trail as a real one),
but nobody fell. The fall needs more than the reaction state (re-findings.md "Round 12 result").
+0xD0 is not the live position. Next: slices S10 (contact check), then a live-position finder.

## Session 2026-11-03 (13:00): shockwave test (probe round 12)

Owner asked for an effect that knocks nearby players away. Slices S9 (static-map.md 16): the reaction
dispatcher can be called by us the way the game calls it for a hitter-less reaction; player
positions at +0xD0 (round 8 dumps). Round 12 (probe-only, no new .cpp): **Shockwave test** - when the
pro lands a knockdown (or at his next contact after Arm / F8), opposing skaters within 3 / 5 / 8 m are
knocked down through the dispatcher with forced power; plus a live velocity search for a later
"fling" impulse. user-guide 4k. Not compiled against the SDK; a wrong guess here can crash the game
(guest call from a hook), so it only runs with NHL_ROGUE_PROBE=1 and the radius set.

## Checkpoint 2026-11-03 (12:15): perk Untouchable; commit of rounds 9-11

Owner skipped the contact floor test for now (4i stays available) and asked for penalty immunity as
a Legendary perk. Added **Untouchable** (`untouchable`, Legendary, 1 stack, event kPenaltyImmunity):
the penalty decision hook now lives in `rogue_hooks.cpp` (moved out of the S1 probe, which logs
through `probe::PenaltyDecided`); a cancelled call shows "NO CALL". Not run in game yet. Legendary
perks are not unlockable in the UI yet: add `unlock=untouchable` to `rogue_meta.txt` to see it in the
shop. Owner commits this package (rounds 9-11, commit message in handoff-log).

## Session 2026-11-03 (11:45): round 9 passed; round 10 package (contact floor test)

Owner ran round 9 (exhibition period + one BAP game with no claim). Results: re-findings.md "Round 9
results". Automatic pro CONFIRMED (2 games, BAP flag 0 in exhibition), penalty immunity CONFIRMED
(6 infractions blocked, control called normally), full rating id map CONFIRMED from the pro's
attribute screens. Auto start came ~9 s early (state left over from the exhibition): round 10 also
waits for the pro pointer. Owner feedback: checks rarely connect; data: 60 contact episodes with
opponents, 11 hits. Round 10 (no new .cpp): start gate fix + contact floor test (user-guide 4i).

## Session 2026-11-03: probe S1 + round 8 analysed; round 9 package (auto pro, auto game start / end)

Owner ran BUILD-13 (S1 probe compiled and ran) and round 8 in one BAP game played to the end
(notes 1-13, handoff-log). Results: re-findings.md "Round S1 + round 8 results". Key points:
- **RE-2: the game caches the pro at 0x83C876D4** from game load on; it matched the claim all game and
  the +0x22C marker scan agreed (only the pro, only while on the ice). Gate: match 0x83C1E89C != 0 and
  BAP flag 0x83AC5F63 != 0 (after the match the word points at a re-used object).
- **Game state machine confirmed**: PRE_GAME at load, DONE once at game end, STORYTELLER intermissions.
- Goal hook r4 is the goalie scored on (not the assister); simulated goals skip it.
- Penalty decide / apply registers right; decide fires at the infraction (~5 s before the whistle).
- Puck contact hook unclear; event post function = 0x8299E478.
Round 9 package (no new .cpp, incremental build): automatic pro + automatic BAP game start / game
over (perks revert at DONE), penalty immunity behaviour test (probe), docs. user-guide 4h.
The owner's game was recorded as a WIN by mistake (Lost not ticked): +20 Hype x multiplier; fix by
editing `hype=` in `rogue_career.txt` if wanted.

## Session end 2026-11-02 (19:45): static map extended + probe round S1 written

After the 19:00 checkpoint (below): penalty path (RE-10), player object / ratings / energy /
injury (RE-11), rating id map, and `src/rogue_static_probe.cpp` (PROBE-S1, read-only, NEW .cpp ->
`_game_ffx_build.bat`). How to pick up: `docs/static/static-map.md` section 14 (method, how to read
the probe log, static queue, perk hook catalog). Not compiled by the agent (stub syntax check only).

## Checkpoint 2026-11-02 (19:00): static map of the generated code (no game runs)

Owner uploaded a whole-program index (`tools/index_generated.py`, 129,386 functions + referenced
strings from an `NHL_DUMP_IMAGE` dump) and four rounds of slices. Everything below is STATIC =
CANDIDATE; nothing went into `addresses.h`. Full write-up: `docs/static/static-map.md`.
- Tunables: 12,211 tweak objects mapped name -> N (12/12 known addresses reproduced), 6,487 with a
  static N+32 reader: `docs/static/tunable_readers.tsv`.
- The game's event system: 198 `GameEvent::*` types, raiser per type (`docs/static/game_events.tsv`).
  Post = virtual slot 1 of the event manager (`sub_82FEC310` returns it); address runtime-only.
- **RE-2 lead: cached BAP player pointer at 0x83C876D4** (filled by `sub_834DC8A8`; valid while the
  match pointer 0x83C1E89C != 0 and the BAP flag byte 0x83AC5F63 != 0). A read, no hook.
- **Game state machine**: current state 0x83C1D220, switch `sub_8330C890`, handler table 0x83AA5B20
  with names (PRE_GAME, PRE_FACEOFF, SYNCHING, FACEOFF, GAME_ON, GOOF_OFF, PREFIGHT, FIGHT, POSTFIGHT,
  DONE, STORYTELLER, NISVIEWER, PRACTICE_GOOFOFF, TUTORIAL_GOOFOFF). RE-5 candidates: DONE = game end.
- Goal `sub_83480248` (r3 scorer, r4 assister), period end `sub_8331F408`, period word 0x83C876B8,
  puck hits player / block shot `sub_83328180` (r3 puck, r4 victim, r5 code), stoppage handler
  `sub_83326280` (r3 reason; 19 = fight), player table 0x83C1D080 [team*20 + slot].
- Reaction table entry names (image): incidental, nudge, stumble, stumble fall (the mod's "normal
  hit", 0x83ABF57C), fall (the mod's "knockdown", 0x83ABF598).
- New tools: `tools/index_generated.py` (index for upload), `tools/read_image.py` (read words /
  floats / strings / tables from the image dump).
- Round 8 (BUILD-12) is still waiting for its in-game run; the static RE-2 lead can be checked in
  the same session (read 0x83C876D4 next to the claim).

## Checkpoint 2026-10-31 (18:20)

Committed on GitHub: up to 0290eb7 (the 2026-10-30 18:45 checkpoint, BUILD-11). The owner ran the
exhibition part of round 7. This session the agent edited the owner's local repo directly (tunnel)
and ran the incremental build there; not committed yet. Round 8, no new .cpp:
- `src/rogue_probe.cpp`, `include/rogue/probe.h`: AUTO = byte +0x22C != 0xFF (was == 0x00);
  once-a-second marker scan over every known player (`marker` log line on every change,
  `marker-sum` every 60 s, panel counters); `ResetPlayers()`; player table 64 -> 128.
- `src/rogue_panel.cpp`: "Marker scan" line in the hit watch; BAP game started also clears the
  probe's player list.
- Docs: re-findings.md, user-guide.md (4f), this file, handoff-log.md.
Still stale: `00-START-HERE.md` file map / mechanism table.

## Key finding 2026-10-31 (round 7, exhibition): the marker is "not 0xFF", not "0x00"

- No AUTO tag appeared because the probe required +0x22C == 0x00 (the BAP value from round 6).
- At the claim (40 players dumped) only the claimed, controlled player had a value other than
  0xFF: 0x06. All 39 others, both teams' goalies included, were 0xFF.
- In the contact lines 0x06 appeared only on skaters of the owner's team (team 1) and moved
  between them as he switched players; the CPU team was always 0xFF.
- HYPOTHESIS: +0x22C != 0xFF = the player a human controls right now (value depends on the mode:
  0x00 BAP, 0x06 exhibition). In BAP you cannot switch, so it should stay on the pro. Unknown:
  bench, whistles, intermissions. Round 8 (user-guide 4f) answers them.
- Power vs reaction held again: 11 knockdowns 0.785-1.077, normal hits 0.501-0.750.

## Checkpoint 2026-10-30 (18:45)

Committed on GitHub: up to 6f4d665. The 17:45 package was built and run (round 6). Delivered
18:45 as a zip + commit message for a checkpoint commit (owner pushes):
- `include/rogue/addresses.h`: ROGUE_ADDR_REACTION_DISPATCH 83429560, ROGUE_ADDR_REACTION_CHOICE
  833B9028, ROGUE_VA_REACTION_TABLE (CONFIRMED); +0x22C as a commented candidate.
- `src/rogue_hooks.cpp`: reaction dispatcher + choice hooks (moved from the probe), BapHitPower
  (Freight Train / Wrecking Ball); the unused CHECK_RESOLVE stub is gone.
- `include/rogue/perks.h`, `src/perk_catalog.cpp`: kHitPowerBoost; Freight Train is now an event
  perk (+15% per stack, was global tunables); Wrecking Ball live.
- `src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`: round 7 hit watch (AUTO
  marker from +0x22C vs the claim, agreement counters).
- Docs: re-findings.md, user-guide.md (4e), this file, handoff-log.md. No new .cpp.
Still stale: `00-START-HERE.md` file map / mechanism table (says no guest addresses are known).

## Key finding 2026-10-30 (round 6): hit outcome is decided by power, and we can change it

- `sub_833B9028` picks the reaction from power with a table at 0x83ABF528: <= ~0.5 no hit
  reaction, <= ~0.75 hit, above = knockdown. All 1836 logged decisions match.
- Raising the power for the claimed pro's hits (test x1.15 / x1.3 / x2) changed outcomes exactly
  as predicted: x1.3 turned 5 of 6 normal hits into knockdowns; nothing abnormal happened.
- The game's own hit stat (5 in 2 periods) counts far fewer hits than any contact (27) or the
  credit pair (19); the owner thinks it only counts hits on the puck carrier. The mod keeps its
  own definition (any hit reaction you cause) for Hype.
- Candidate automatic BAP marker: player byte +0x22C = 0x00 on the pro, 0xFF on 20 others.

## Key finding 2026-10-30 (round 5 + reaction slice)

- Byte +0x38 is a reaction state, not a human flag (REJECTED). `sub_834EA6D0(player, 0)` is a
  team query (0 / 1 by team, all 40 players). No automatic way to find the BAP player yet.
- The reaction (knockdown or not) is chosen by `sub_833B9028` from the power computed by
  `sub_833CA500`, inside the dispatcher `sub_83429560` (static). Round 5 power (58 contacts): every
  knockdown 0.763-0.962, every normal hit 0.501-0.734, so the threshold sits near 0.75. Round 6 tests raising the power for the claimed player's hits.

## Key finding 2026-10-30 (RE-1 round 4 + static read)

- Round 4 (owner, exhibition + training, 59 contacts / 39 interactions): hitter -> victim always
  right on screen; knockdown labels consistent. One contact call per player who REACTS, so one
  collision gives 1-3 lines. Goalies take the knockdown reaction but do not fall (no fall code);
  goalie objects have a different class pointer (0x8223F648 vs skater 0x82271C10).
- `sub_8357F9A0` r7 is the hitter object, not a strength (round 3's 0..3 came from
  `sub_83589B38` r7). Credit ran for 27 of 39 interactions.
- Player object byte +0x38 = 0x02 on exactly 2 of 34 objects, both the owner's player at the time
  (in the exhibition he switched players, so it probably means "controlled right now"). Byte
  +0x39 = player index, +0x30 = team side. Details: re-findings.md "Player object".
- Static (hit_slices.txt): `sub_833E0BF8` is bookkeeping after the hit is decided (updates
  per-player sub-objects at +564: who hit whom, durations 30 / 300); r6 is never read by it (the
  caller `sub_83429560` passes its r30); *r5 is compared against gGPFallMin (hit power?);
  `sub_834EA6D0(hitter, 0)` picks a human / CPU table entry (candidate human query). The
  knockdown decision is upstream in `sub_83429560` (file 134).

## Key finding 2026-10-30 (RE-1 round 3): the game's hit pipeline (OBSERVED, one session)

Source: owner run, training mode 1-on-1, open ice, 472 s, 5 marked checks (1 big hit delivered +
knockdown, 2 small hits delivered, 3 small hit delivered, 4 big hit delivered + knockdown, 5 big
hit taken + knockdown). Each MARK was clicked 4.1-4.7 s after its check (time to open the menu).
Owner noted a few small contacts were not marked.
- **`sub_833E0BF8` (called from lr 0x83429644) = contact start.** Fired at all 5 marks and at 7
  unmarked contacts, never without contact. r3 = player HIT, r4 = player HITTING: for marks 2, 3,
  4 r4 was the same object (A9BFB8A0) and for mark 5 that object was r3, matching the notes.
  r6 = 0x83ABF598 on all 3 marked knockdowns, 0x83ABF57C on both marked small hits (a constant
  in the image: probably a reaction descriptor).
- **`sub_8357F9A0` + `sub_83589B38` (one caller, lr 0x835A74D0 / 0x835A74AC), ~0.35 s after
  contact:** r4 = hitter, r5 = victim, r7 = 0..3 (marks 2/3: 0 and 2; marks 4/5: 3). Did NOT
  fire for mark 1 (a knockdown). HYPOTHESIS: hit stat credit / hit strength.
- **`sub_829380A0` (lr 0x8292A904):** ran only on knockdowns (marks 1, 4, 5 + one unmarked at
  255.7 s between the opponent and a third object AA315CE0). Fall path: best RE-3 lead.
- `sub_8293A998` (lr 0x828B5520) runs with each contact; r5 = 0x0E with knockdowns, 0x11 with
  normal hits. `sub_835888E8` (r4 = a player) ran at 3 of 4 knockdowns.
- **Player objects (RE-2 lead):** user's skater = A9BFB8A0, opponent = A9577560 in this session.
  Heap addresses; expect them to change per session. The per-skater update `sub_8342C628` saw
  A9BFB8A0 and AA315CE0.
- **Skating snapshot `sub_828CC300` (TUN-12): NOT per frame.** 29 bursts in 472 s, each ~236 calls
  inside one second, each with a fresh struct (new r3), at the same seconds as `sub_8334F930` and
  `sub_83582C90` (probably drill resets in training). Callers seen: lr 828F158C (sub_828F1550),
  82921208 (sub_82920828), 8332AF60 (sub_8332AF40). HYPOTHESIS: live edits to snapshotted
  skating/balance tunables apply only at the next rebuild; perks applied before a match should
  work. Not tested.
- Log size: 1.58 MB for 8 min (the sweep). Round 4 hooks 7 functions only.

## Key finding 2026-10-29 (TUN-4/TUN-5b): record+8 is a type tag; N+32 is the live value

The parent Engine Tunables editor / ITunableStore edits record+8, which is the tweak object's
type tag (4 float, 2 int, 1 bool, 13 game-style table, 12 indexed slider), not its value. Game
code reads N+32 (plus a heap copy at H+36); writing it changed slap shots and LOD rendering at
once, and values above the object's own max work. Full layout and evidence: re-findings.md.
Report to the parent project: PARENT-1.

## Task board

Status: TODO / ACTIVE (owner in brackets) / BLOCKED (reason) / DONE (date).
Who: A = any agent with no game access, U = needs the user to build/play/upload.

### Build and wiring
| ID | Task | Who | Status |
|---|---|---|---|
| BUILD-1 | Integrate into CMake + fix compile errors | U+A | DONE 2026-10-28. Sources go into the `nhllegacy` target via `target_sources` (00-START-HERE rule 9) |
| BUILD-2 | Wiring lines in `src/nhllegacy_app.h` and `renderer/core/nhl_overlay.cpp` | U | DONE 2026-10-28 (commits 0213121, 0f1a312) |
| BUILD-3 | Smoke test with `NHL_ROGUE=1` (5 checks, user-guide section 3) | U | DONE 2026-10-29, all 5 PASS. Found BUG-1 |
| BUILD-4 | Move `rogue::Tick();` before the overlay visibility early-return | U | DONE 2026-10-28 (87a6b59) |
| BUILD-5 | Restore CRLF on two parent scripts | U | DONE 2026-10-28 (87a6b59) |
| BUILD-6 | `scripts/_ffx_dll_build.bat` generator `Visual Studio 18 2026` (34a34a3). Machine-specific; do not upstream. Portable fix = auto-detect the VS generator | A | TODO (low) |
| BUILD-7 | Build + verify the 2026-10-29 package (probe + BUG-1/2) | U | DONE 2026-10-29 (commit 316c3a1; probe ran) |
| BUILD-8 | Build the 2026-10-30 hit watch package (incremental) | U | DONE 2026-10-30 (round 4 ran) |
| BUILD-9 | Build the 2026-10-30 13:45 package (hit contact hook + round 5 hit watch, incremental) | U | DONE 2026-10-30 (round 5 ran) |
| BUILD-10 | Build the 2026-10-30 17:45 package (claim + round 6, incremental) | U | DONE 2026-10-30 (round 6 ran) |
| BUILD-11 | Build the 2026-10-30 18:45 checkpoint (first event perks + round 7), commit + push | U | DONE 2026-10-30 (0290eb7); round 7 exhibition part ran 2026-10-31 |
| BUILD-12 | Round 8 marker scan | A+U | DONE 2026-11-03 (owner ran 4f in one BAP game; results in re-findings.md) |
| BUILD-13 | Build PROBE-S1 + user-guide 4g | U | DONE 2026-11-03 (`_game_ffx_build.bat`, compiled first time; one BAP game logged) |
| BUILD-14 | Round 9 package (auto pro, auto start / game over, penalty immunity test) | U | DONE 2026-11-03 (built incrementally, 4h run: all checks passed) |
| BUILD-23 | Rounds 9-19 + design package: incremental build, commit + push | U | ACTIVE 2026-11-03 18:00: owner commits (end of day). In-game check of Heavy Shoulders / Bulldozer / Untouchable next session |
| BUILD-22 | Round 18 probe (physics window, contact floor 0.05 / 0.02) | U | DONE 2026-11-03 (no contacts at range; promoted real contact fell) |
| BUILD-21 | Round 17 probe (round 13 setup, 2 m) | U | DONE 2026-11-03 (no falls; contact floor 0.10 tested: works) |
| BUILD-20 | Round 16 probe (balance test) | U | DONE 2026-11-03 (no falls; balance lead rejected) |
| BUILD-19 | Round 15 probe (power at the source) | U | DONE 2026-11-03 (power 0.9 everywhere, still no falls) |
| BUILD-18 | Round 14 probe (learning fix, on-ice filter, push test) | U | DONE 2026-11-03 (no falls with kind 6; push not tried) |
| BUILD-17 | Round 13 probe (learned contact kind + post step, live position) | U | DONE 2026-11-03 (some real falls; learning bug found) |
| BUILD-16 | Round 12 probe (shockwave test, velocity hunt) | U | DONE 2026-11-03 (ran: fires, no visible fall, no crash) |
| BUILD-15 | Rounds 10-11 package (start gate fix, contact floor test, perk Untouchable): `scripts\_build_vkffx_incr.bat`; commit + push | U | ACTIVE: owner commits 2026-11-03 12:15 (stub syntax check PASS, not compiled against the SDK yet). 4i (contact floor) optional, later |
| PARENT-1 | Tell the parent project: its Engine Tunables editor / `ITunableStore` writes record+8 (the type tag) instead of the live value N+32 (+ heap copy H+36); its `gDekeRatEffect` sanity value comes from the registration pool. Evidence: re-findings.md. Owner decides how to report (issue / PR) | Owner (agent drafts) | TODO |

### Bugs
| ID | Bug | Status |
|---|---|---|
| BUG-1 | Perk Shop: every non-curse offer stayed FREE after the free pick was used (`free` went negative) | FIXED 2026-10-29 (316c3a1) |
| BUG-2 | Legacy Rogue section showed without `NHL_ROGUE=1` | FIXED 2026-10-29 (316c3a1) |

### Tunable perks (no RE)
| ID | Task | Who | Status |
|---|---|---|---|
| TUN-1 | Stock values + ranges for perk tunables | A | DONE 2026-10-29 via TUN-9 (values in perk_catalog.cpp comments) |
| TUN-2 | Do AI TEAMMATES read *Level or *LevelCPU in BAP? | U | TODO (low; game-style tables showed no effect) |
| TUN-3 | Which tunables are rewritten every frame | U | PARTIAL: `gTweakByGameStyleGameSpeedLevel` record+8 is NOT rewritten. Superseded by TUN-12 for skating values |
| TUN-4 | Find where a tweak record's live value really is | U then A | DONE 2026-10-29: layout decoded (re-findings.md) |
| TUN-5 / 5b / 5c | Write tests | U | DONE 2026-10-29: N+32 is the live value (CONFIRMED). Slap x3 -> x30 differ less than the numbers (puck speed probably capped). Fall thresholds, check distances, hit power: nothing undeniable. `gPenaltyChanceCharging` stock 0. `gHitPowerForPhysicsRatingModifier` is a tag-12 slider (unsupported) |
| TUN-6 | `TunablePatcher` writes the live value via `TweakIndex`; perk_catalog / reactions rebuilt on tunables with static readers | A | DONE 2026-10-29. Readout VERIFIED 2026-10-30 (wrist shot power stock -> perk -> stock) |
| TUN-7 | (low) Game-style tables (tag 13): active style entry and level range | A+U | TODO |
| TUN-8 | Static scan for readers of each tweak object | U then A | DONE 2026-10-29 |
| TUN-9 | Full catalog + static readers | U then A | DONE 2026-10-29: 10,583 objects; 7,779 with a static reader of N+32 |
| TUN-10 | Hip-check write test + combo | U | DONE 2026-10-29: inconclusive. Parked: the hip-check family never runs (RE-1) |
| TUN-11 | Bool tweak objects (tag 1, ~1,100, e.g. gTriggerFalls) are missing from the TUN-9 catalog and TweakIndex (value byte at N+21; heap-copy check fails). Extend the scan | A | TODO |
| TUN-12 | Skating settings snapshot `sub_828CC300` (~890 values into a ~3.4 KB struct; map `docs/skating_params_struct.txt`) | A+U | ACTIVE. 2026-10-30: NOT per frame (29 bursts in 8 min, fresh struct each time, probably at drill resets / player setup). Next: test a fall-threshold perk applied BEFORE the match starts (or before a drill reset); the round 4 log shows burst times. Callers: sub_8332AF40 (file 125), sub_828F1550 (40), sub_82920828 (42), sub_82933A40 + sub_8293B9A8 (43), sub_8293BF80 (44) |

### Reverse engineering (ledger: re-findings.md)
| ID | Task | Who | Status |
|---|---|---|---|
| RE-0 | Run `tools/find_bap_code.py` on the decompressed image, upload report | U | TODO (low; the sweep approach works better) |
| RE-1 | Check resolution (hitter/hittee) | A+U | DONE 2026-10-30 for detection: hit contact `sub_833E0BF8` (r3 victim, r4 hitter) + knockdown reaction 0x83ABF598 CONFIRMED (rounds 3-4), in addresses.h. Changing hit outcomes needs the decision point: RE-3 / CHECK_RESOLVE (`sub_83429560`) |
| RE-2 | Identify the BAP-controlled skater | A+U | DONE 2026-11-03: 0x83C876D4 (gated on match + BAP flag) CONFIRMED over 2 BAP games, 0 in exhibition; used by every hook since round 9. Claim buttons stay as an override |
| RE-3 | Knockdown: fall entry point | A+U | PARTIAL 2026-11-03: real contacts promoted to a knockdown fall (CONFIRMED, contact perks). Knocking down players the pro does not touch: not found after 7 probe rounds (own dispatcher calls, power at the source, balance, physics range); PARKED |
| RE-4 | Puck-vs-skater contact | A+U | TODO. S1: `sub_83328180` fires but the shooter field (puck+180) is always -1 and the codes are unclear; re-slice (static-map.md 15, item 4) |
| RE-5 | BAP game start / end + final score + BAP player's stat line | A+U | PARTIAL 2026-11-03: start / end from the state machine (PRE_GAME / DONE, CONFIRMED, round 9 uses it). Final score still open: goal calls miss simulated goals, so the score must be read (static-map.md 15, item 2) |
| RE-6 | BAP stat-event stream | A+U | TODO. Event post function = 0x8299E478 (S1); slice it next (static-map.md 15, item 1) |
| RE-7 | BAP career save identity | A+U | TODO |
| RE-8 | BAP progression storage (lead: `BAP_PLAYER_GROWTH` @ 0x82035818) | A+U | TODO |
| RE-9 | Meta reactions: offers, trade interest / requests, contracts, news feed, awards, BAP rank (owner priority 2026-11-03, design.md section 0) | A+U | ACTIVE: string anchors found; next = functions that reference them (`tools/refs_to.py` on the 2026-11-02 index), then slices |
| RE-10 | Penalty call decision ("no penalties" perk) | A+U | DONE 2026-11-03: forcing `sub_83487DC0` to 0 for the pro blocks the call (6/6). Perk Untouchable built on it (rogue_hooks.cpp), first in-game check pending |
| RE-11 | BAP player's live ratings in memory | A+U | ACTIVE: raw ratings +1240 + 4 * id, full id map CONFIRMED 2026-11-03 (re-findings.md Round 9). Next: behaviour test of an override of `sub_83501A68` for the pro (ratings past 99) |

### Feedback effects
| ID | Task | Who | Status |
|---|---|---|---|
| FX-1 | Per-frame FX draw with the overlay closed | U | DONE 2026-10-29 |
| FX-2 | Placeholder WAVs in `<exe>/rogue/sfx/` | U | TODO |
| FX-3 | Replace PlaySoundW with XAudio2 or SDK audio | A | TODO |
| FX-4 | In-game effects via guest calls (crowd, horn, camera shake, replay) | A+U after RE-1..4 | TODO |
| FX-5 | Hit-stop experiment via game-speed tunable | U | TODO (unblocked by TUN-5b; game-style tables showed no effect, needs another speed tunable) |

### Design
| ID | Task | Who | Status |
|---|---|---|---|
| DES-1 | Win / loss structure | Owner | DONE 2026-11-03: no loss condition; one career is the whole game (design.md section 0) |
| DES-2 | Balance pass: Hype income vs prices over an 82-game season | A | TODO |
| DES-4 | Checking feel (owner 2026-11-03) | Owner + A | ACTIVE: perks Heavy Shoulders + Bulldozer built on the contact floor (2026-11-03 17:05). Later: the speed lost while aiming a check |
| DES-3 | Perk system shape (owner 2026-10-30): maybe no stacking, perks much more expensive but much more powerful; risks: nothing left to buy too soon, a run that is overpowered and no longer fun later, and an early run that is just standard NHL. Owner wants something special from game 1. Agent to propose options (e.g. a strong starting "archetype" perk, perks that evolve or have drawbacks, Notoriety-scaled opposition, Hype sinks such as rerolls / temporary boosts / Legacy) in design.md | Owner + A | TODO |

## Recommended next action

1. Owner: build (`scripts\_build_vkffx_incr.bat`), commit + push (message in the 2026-11-03 18:00
   handoff entry). Next session: quick BAP game with Untouchable, Heavy Shoulders, Bulldozer (4j / 4r).
2. Then the pinned list above (owner picks; RE-9 is the stated priority).

## How to verify the branch state without the owner

Agents can READ GitHub: list commits on `MegaSloppa/nhl-legacy-recomp` branch `legacy-rogue`,
read files from it, and compare blob SHAs against `puckhead73/nhl-legacy-recomp` master to spot
accidental edits to parent files. Expected parent-file differences from upstream: root
`CMakeLists.txt` (add_subdirectory), `src/nhllegacy_app.h`, `renderer/core/nhl_overlay.cpp`,
`.gitignore` (run_dev.bat), `scripts/_ffx_dll_build.bat` (VS 2026). Anything else is suspect.
Raw file text: `https://raw.githubusercontent.com/MegaSloppa/nhl-legacy-recomp/<commit>/<path>`
(the GitHub file tool may return only the SHA).
