# Current status (AUTHORITATIVE)

Last updated: 2026-10-31 (18:20) by Shirley (monday.com agent). If this disagrees with another doc,
this wins; fix the other doc.

## Baseline

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
| BUILD-12 | Round 8 marker scan (edited in the local repo by the agent, 2026-10-31): incremental build, run user-guide 4f, commit + push | A+U | ACTIVE: edits applied, incremental build PASSED 2026-10-31 (agent, `_build_vkffx_incr.bat`; only the existing fopen/getenv warnings). Owner: 4f run, commit + push |
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
| RE-2 | Identify the BAP-controlled skater | A+U | ACTIVE. Interim claim works (round 6). Marker byte +0x22C: 0x00 on the pro in BAP (round 6), 0x06 on the controlled player in exhibition, 0xFF on everyone else (round 7), so the test is now != 0xFF. Round 8 (user-guide 4f): BAP game(s) with the marker scan. Two clean BAP games -> CONFIRMED -> automatic claim |
| RE-3 | Knockdown: fall entry point or skater balance offset | A+U | DONE 2026-10-30 for hit outcomes: power -> reaction table (CONFIRMED), Freight Train / Wrecking Ball built on it. A force-fall for non-contact effects (Shockwave on nearby players, crit slap) is still open: lead `sub_83429560` called for another victim |
| RE-4 | Puck-vs-skater contact | A+U | TODO |
| RE-5 | BAP game start / end + final score + BAP player's stat line | A+U | TODO: the PE_* handlers never run. Next lead: post-game screens / XP award code |
| RE-6 | BAP stat-event stream. Lead: `gBeAPro_*_StatEvent` tunables; hit credit pair `sub_83589B38` + `sub_8357F9A0` (from `sub_8358A1D0` / `sub_835A7078`) may be the hit stat | A+U | TODO |
| RE-7 | BAP career save identity | A+U | TODO |
| RE-8 | BAP progression storage (lead: `BAP_PLAYER_GROWTH` @ 0x82035818) | A+U | TODO |
| RE-9 | Meta reactions: contracts, trades, line promotion, task grading | A+U | TODO |
| RE-10 | Penalty call decision (for a "no checking penalties" perk). Lead: `sub_83310E68` (reads gHandlePotentialKnockedOverPenaltiesTime) ran twice in round 3 with r3=0x0D | A+U | TODO |
| RE-11 | BAP player's live ratings in memory (for perks past 99). Round 4 player-object dumps may help | A+U | TODO |

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
| DES-1 | Pick the win/loss structure (design.md section 6) | Owner | TODO |
| DES-2 | Balance pass: Hype income vs prices over an 82-game season | A | TODO |
| DES-3 | Perk system shape (owner 2026-10-30): maybe no stacking, perks much more expensive but much more powerful; risks: nothing left to buy too soon, a run that is overpowered and no longer fun later, and an early run that is just standard NHL. Owner wants something special from game 1. Agent to propose options (e.g. a strong starting "archetype" perk, perks that evolve or have drawbacks, Notoriety-scaled opposition, Hype sinks such as rerolls / temporary boosts / Legacy) in design.md | Owner + A | TODO |

## Recommended next action

1. Owner: commit + push the round 8 working-tree changes (BUILD-12; already built).
2. Owner: user-guide 4f: one or two BAP games with the marker scan (claim your pro, play through
   bench shifts, whistles and an intermission). Upload `rogue_probe.txt` + the "Marker scan"
   line at each period end.
3. Agent: RE-2 decision from the marker log. If clean: define ROGUE_OFF_SKATER_CONTROLLER 0x22C,
   change IsBapSkater (rogue_hooks.cpp) from "== ROGUE_HUMAN_CONTROLLER_VALUE" to "!= 0xFF",
   claim automatically, keep the manual claim as an override. Then DES-3 options in design.md,
   then refresh 00-START-HERE.md.
4. Next RE for more perks: puck-vs-body (RE-4, crit slap), a force-fall entry (Shockwave), BAP
   game start / end (RE-5) so the Manual buttons can go.

## How to verify the branch state without the owner

Agents can READ GitHub: list commits on `MegaSloppa/nhl-legacy-recomp` branch `legacy-rogue`,
read files from it, and compare blob SHAs against `puckhead73/nhl-legacy-recomp` master to spot
accidental edits to parent files. Expected parent-file differences from upstream: root
`CMakeLists.txt` (add_subdirectory), `src/nhllegacy_app.h`, `renderer/core/nhl_overlay.cpp`,
`.gitignore` (run_dev.bat), `scripts/_ffx_dll_build.bat` (VS 2026). Anything else is suspect.
Raw file text: `https://raw.githubusercontent.com/MegaSloppa/nhl-legacy-recomp/<commit>/<path>`
(the GitHub file tool may return only the SHA).
