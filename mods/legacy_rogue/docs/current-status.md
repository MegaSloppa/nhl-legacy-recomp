# Current status (AUTHORITATIVE)

Last updated: 2026-10-29 (session checkpoint, 12:00) by Victoria (monday.com agent). If this
disagrees with another doc, this wins; fix the other doc.

## Baseline

- **The modded game builds and launches** on the owner's machine (branch `legacy-rogue` of
  `MegaSloppa/nhl-legacy-recomp`). Wiring is final: `rogue::fx::Draw()` and `rogue::Tick()` run
  every frame, `rogue::DrawPanel()` only while the menu is open.
- **BUILD-3 smoke test PASSED 2026-10-29** (owner, commit 3d595fe): career start, FX with the
  menu closed, perk layers write the store (13 -> 14), revert at game end (back to 13), save
  files. Hype 80 / Notoriety 61 for G2 A1 Hits3 +/-2 Won, exactly as computed from run_state.cpp.
- **Tunable perks write the live value since TUN-6 (2026-10-29)** through `TweakIndex`
  (N+32 + heap copy H+36, CONFIRMED by TUN-5b). The parent ITunableStore is no longer used by the
  mod (it edits the TYPE TAG, N+8). All perk tunables are GLOBAL (CPU players get them too)
  until per-player hooks exist; only Cannon's tunable is confirmed in play.
- No guest addresses are known. All event perks are inert. Shop, Notoriety, FX and saves are
  driven by the panel's Manual buttons.
- Owner's toolchain: VS Build Tools 2026 + standalone LLVM + Vulkan SDK + PowerShell 7.
  Every setup problem hit so far and its fix: `user-guide.md` section 7.
- GitHub: agents can READ the fork but writes fail from the agent's connection (as of
  2026-10-28). Deliver changes as a zip of changed files only.

## Checkpoint 2026-10-29 12:00 (owner on a break; read this first)

Committed on GitHub: up to 3ce8b2a (N+32 confirmed, catalog, write tests). The owner's working
tree additionally has three applied, BUILT but UNTESTED packages (to be committed together):
- TUN-6 perk write path: `tweak_index.h/.cpp` (new), `tunable_patch.h`, `rogue_mod.cpp`,
  rebuilt `perk_catalog.cpp` / `reactions.cpp`, catalog/readers files as `.txt`. Owner saw
  "Tunable index ready" in the panel (index builds); perks themselves not yet verified.
- RE-1 rounds 2-3 probes in `rogue_probe.cpp` (round 2 result already analysed: PE_* idle).
- "Active perk effects (stock -> live)" readout in `rogue_panel.cpp`.
First things to do next session: user-guide 4b step 1 (perk readout) and step 2 (hit sweep),
then analyse the sweep log (see Recommended next action).
Untested-in-play items: every perk tunable except gSlapShotPowerScale; Notoriety reactions.

## Key finding 2026-10-29 (TUN-4): record+8 is a type tag, not the value (PROVEN by probe)

Proven:
- In `docs/tunable_values_runtime.txt` the 11,158 resolved `field+8` values collapse to a few
  small integers: 4 (7,227), 2 (1,360), 1 (1,110), 12/14/11/13 (1,441 together).
- Joined with `docs/tunables_schema.tsv` types: float-typed names read 4 in 4,287 of 4,881
  cases (including multipliers such as `gSlapShotPowerMultiplier`); bool-typed names read 1 in
  1,084 of 1,148; all 77 `gTweakByGameStyle*` read 13, including non-sliders such as
  `gTweakByGameStylePoweredPassingOption` and `...StrategyAdjustmentsBAP`.
- Only about 9 entries hold a plausible float (e.g. `gDekeRatEffect` = 0.2, the parent's sanity
  value), so a sanity check on that one value does not validate the store.
- Owner test 2026-10-29, in a match: `gTweakByGameStyleGameSpeedLevel` 13 -> 0 and 13 -> 20 had
  no visible effect; the game's own speed setting did not change the 13; a whistle did not
  reset the edit (so the value is neither read live nor rewritten).
Probe result (owner run, 3 snapshots): record+8 matches the object's type-class pointer for all 7
objects decoded; the value lives at record+20 (min +24, max +28, default +32) plus a heap copy.
Full layout and evidence: re-findings.md. Still unproven: which copy the game reads, and whether
writing it changes gameplay (TUN-5). This would
affect the parent project's Engine Tunables editor for nearly every entry (its live-effect test
is still listed as pending in `docs/engine-settings-editor-plan.md`). Parent files are NOT
changed by this mod (rule 12); report to the owner once proven.
Risk note: writing a type tag could misbehave. Owner should not use tunable perks in real play
until TUN-4 is resolved.

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
| PARENT-1 | Tell the parent project: its Engine Tunables editor / `ITunableStore` writes record+8 (the type tag) instead of the live value N+32 (+ heap copy H+36); its `gDekeRatEffect` sanity value comes from the registration pool. Evidence: re-findings.md. Owner decides how to report (issue / PR) | Owner (agent drafts) | TODO |
| BUILD-7 | Build + verify the 2026-10-29 package (probe + BUG-1/2) | U | DONE 2026-10-29 (commit 316c3a1; probe ran) |

### Bugs
| ID | Bug | Status |
|---|---|---|
| BUG-1 | Perk Shop: every non-curse offer stayed FREE after the free pick was used, so all could be taken and `free` went negative (owner save showed `free=-1`). | FIXED in package 2026-10-29 (`Buy` refuses when no pick left, panel refreshes FREE flags after a buy, `Load` clamps `free` to >= 0). Verify in BUILD-7 |
| BUG-2 | Legacy Rogue section (and Start career, which writes save files) showed without `NHL_ROGUE=1`. The overlay itself is parent-owned and must stay. | FIXED in package 2026-10-29 (`DrawPanel` returns when `rogue::Enabled()` is false). Verify in BUILD-7 |

### Tunable perks (no RE)
| ID | Task | Who | Status |
|---|---|---|---|
| TUN-4 | Find where a tweak record's live value really is | U then A | DONE 2026-10-29: layout decoded (re-findings.md, OBSERVED) |
| TUN-5 | Write test: `gSlapShotPowerMultiplier`, `gMomentumPhysicsImpulse`, `gHitTune_GlobalScale` at x0.2 / x3 / x10 of stock, both copies (user-guide 4b) | U | DONE 2026-10-29: writes landed (log), NO visible effect in a BAP match (live edit) or an exhibition (x10 set at the menu before the match). x0.2 slap power should have been obvious. Hits were hard to judge |
| TUN-8 | Static scan of generated code for references to each tweak object / table (`tools/find_tweak_consumers.py`) | U then A | DONE 2026-10-29: real code loads base+40 = N+32 (not N+20); game-style values are read through the heap copy's table (sub_829F8A20). `gSlapShotPowerMultiplier` and `gMomentumPhysicsImpulse` have no static reader. See re-findings.md |
| TUN-5b | Write test on the field real code reads (N+32 + heap copy H+36) | U | DONE 2026-10-29: **N+32 is the live value (CONFIRMED)**. `gSlapShotPowerScale` x0.2 = limp slap shots, x10 (its own max is 2.0) = very fast; `gLODShader` 0/2 changed rendering at once. Effects are immediate in a match. Game-style tables (all entries 0/20): no visible effect. `gHitTune_GlobalScale` x10: nothing seen (big hits hard to land) |
| TUN-5c | Write test round 3 (slap x30, fall thresholds, hit power, check reach, charging penalty) | U | DONE 2026-10-29: slap x3 -> x30 differs less than the numbers (puck speed probably capped, extra power goes into lift); fall thresholds (stock 25/23, x10) and check distances: nothing undeniable; `gPenaltyChanceCharging` stock is 0; `gHitPowerForPhysicsRatingModifier` is a tag-12 slider (not supported) |
| TUN-9 | Full catalog + static readers | U then A | DONE 2026-10-29: 10,583 tweak objects (tags: 8,009 float, 1,549 int, 484 slider, 396 tag-11, 145 game-style); 7,779 have a static reader of N+32. Hip-check code identified (re-findings.md) |
| TUN-10 | Hip-check write test + combo | U | DONE 2026-10-29: inconclusive (hip checks are too hard to land to judge); the combo maybe made knockdowns a bit easier. Parked until a hook can force the interaction |
| TUN-11 | Bool tweak objects (tag 1, ~1,100 e.g. gTriggerFalls) are missing from the TUN-9 catalog and TweakIndex: their layout differs (value byte at N+21; the heap-copy check fails). Extend the scan | A | TODO |
| TUN-12 | Skating settings snapshot: sub_828CC300 copies ~890 tweak values into a ~3.4 KB struct (r3); skating code reads the COPY. Map: `docs/skating_params_struct.txt` (822 named, e.g. +636 gBalanceCarrierFallThreshold, +640 gBalanceOffpuckFallThreshold, +596..+648 gBalance_*). Find how often it runs (in the RE-1 sweep list) and the struct address (sweep call lines log r3). If rare: apply skating perks before the match or patch the struct directly | A+U | NEXT (with the sweep) |
| TUN-6 | `TunablePatcher` writes the live value via the new `TweakIndex` (image scan; N+32 + H+36); perk_catalog / reactions rebuilt on tunables with static readers | A | DONE 2026-10-29 (needs `_game_ffx_build.bat`: new tweak_index.cpp). Owner to verify Cannon in play |
| TUN-7 | (low; TUN-5b showed no effect, probably presets applied elsewhere) Game-style tables (tag 13): which entry is the active style, and what range a level has (seen: GameSpeed 5/4/3, SlapShotPower 5/4/4, WristShotAccuracy 6/5/3) | A+U | TODO |
| TUN-1 | Stock values + ranges for perk tunables | A | DONE 2026-10-29 via TUN-9 (values in perk_catalog.cpp comments) |
| TUN-2 | Do AI TEAMMATES read *Level or *LevelCPU in BAP? | U | BLOCKED (TUN-6) |
| TUN-3 | Which tunables are rewritten every frame | U | PARTIAL: `gTweakByGameStyleGameSpeedLevel` record+8 is NOT rewritten (survives whistles). Redo per setting after TUN-4 |

### Reverse engineering (ledger: re-findings.md)
| ID | Task | Who | Status |
|---|---|---|---|
| RE-0 | Run `tools/find_bap_code.py` on the decompressed image with `--generated generated/default`, upload report. Owner may need to produce the image first (user-guide section 4) | U | TODO |
| RE-1 | Check resolution (hitter/hittee). Rounds 1-2 (2026-10-29): hip-check family and all 15 player-event handlers NEVER run (exhibition and BAP); hook method verified (function table registers `sub_X`). Round 3: sweep of 141 functions that read hit/check/balance/fall/collision tunables | A+U | ACTIVE (owner run) |
| RE-2 | Identify the BAP-controlled skater | A+U | TODO |
| RE-3 | Knockdown: fall entry point or skater balance offset | A+U | TODO |
| RE-4 | Puck-vs-skater contact | A+U | TODO |
| RE-5 | BAP game start / end + final score + BAP player's stat line | A+U | TODO: the PE_* handlers never run, so not usable. Next lead: post-game screens / XP award code |
| RE-6 | BAP stat-event stream. Lead: `gBeAPro_*_StatEvent` tunables | A+U | TODO |
| RE-7 | BAP career save identity | A+U | TODO |
| RE-8 | BAP progression storage (lead: `BAP_PLAYER_GROWTH` @ 0x82035818) | A+U | TODO |
| RE-9 | Meta reactions: contracts, trades, line promotion, task grading | A+U | TODO |
| RE-10 | Penalty call decision (for a "no checking penalties" perk): find where an infraction becomes a called penalty and its offender register | A+U | TODO |
| RE-11 | BAP player's live ratings in memory (for perks that push attributes past 99) | A+U | TODO |

### Feedback effects
| ID | Task | Who | Status |
|---|---|---|---|
| FX-1 | Per-frame FX draw with the overlay closed | U | DONE 2026-10-29 (BUILD-3 check 2: flash + text stay with the menu closed) |
| FX-2 | Placeholder WAVs in `<exe>/rogue/sfx/` | U | TODO |
| FX-3 | Replace PlaySoundW with XAudio2 or SDK audio | A | TODO |
| FX-4 | In-game effects via guest calls (crowd, horn, camera shake, replay) | A+U after RE-1..4 | TODO |
| FX-5 | Hit-stop experiment via game-speed tunable | U | BLOCKED (TUN-4) |

### Design
| ID | Task | Who | Status |
|---|---|---|---|
| DES-1 | Pick the win/loss structure (design.md section 6) | Owner | TODO |
| DES-2 | Balance pass: Hype income vs prices over an 82-game season. Note: `gBeAPro_LineEnergyBoost` / `gBeAPro_LineSkillBoost` read 4 in the store and Iron Lungs adds 10 (3.5x), but both readings are suspect until TUN-4 | A after TUN-4 | TODO |

## Recommended next action

1. Owner: apply the zip (incremental build). Check the new "Active perk effects" readout (user-guide
   4b step 1), then run the round-3 sweep script, upload `rogue_probe.txt` + numbered marks.
2. Agent: also read the sweep counts for sub_828CC300 (TUN-12: snapshot cadence, r3 = struct).
   Then line up per-function call spikes with the marked hits; the function that jumps once per
   landed check (and not otherwise) is the hit-resolution candidate. Then read its args to find
   hitter / hittee (compare with the skater objects seen in sub_8342C628 r3).

## How to verify the branch state without the owner

Agents can READ GitHub: list commits on `MegaSloppa/nhl-legacy-recomp` branch `legacy-rogue`,
read files from it, and compare blob SHAs against `puckhead73/nhl-legacy-recomp` master to spot
accidental edits to parent files. Expected parent-file differences from upstream: root
`CMakeLists.txt` (add_subdirectory), `src/nhllegacy_app.h`, `renderer/core/nhl_overlay.cpp`,
`.gitignore` (run_dev.bat), `scripts/_ffx_dll_build.bat` (VS 2026). Anything else is suspect.
