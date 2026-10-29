# Current status (AUTHORITATIVE)

Last updated: 2026-10-29 by Victoria (monday.com agent). If this disagrees with another doc,
this wins; fix the other doc.

## Baseline

- **The modded game builds and launches** on the owner's machine (branch `legacy-rogue` of
  `MegaSloppa/nhl-legacy-recomp`). Wiring is final: `rogue::fx::Draw()` and `rogue::Tick()` run
  every frame, `rogue::DrawPanel()` only while the menu is open.
- **BUILD-3 smoke test PASSED 2026-10-29** (owner, commit 3d595fe): career start, FX with the
  menu closed, perk layers write the store (13 -> 14), revert at game end (back to 13), save
  files. Hype 80 / Notoriety 61 for G2 A1 Hits3 +/-2 Won, exactly as computed from run_state.cpp.
- **Tunable perks currently have NO gameplay effect (proven for one setting, hypothesis for
  the rest).** See TUN-4. The mod's layering works; what it writes is most likely not the value
  the game reads. Do not tune perk numbers until TUN-4 is resolved.
- No guest addresses are known. All event perks are inert. Shop, Notoriety, FX and saves are
  driven by the panel's Manual buttons.
- Owner's toolchain: VS Build Tools 2026 + standalone LLVM + Vulkan SDK + PowerShell 7.
  Every setup problem hit so far and its fix: `user-guide.md` section 7.
- GitHub: agents can READ the fork but writes fail from the agent's connection (as of
  2026-10-28). Deliver changes as a zip of changed files only.

## Key finding 2026-10-29 (TUN-4): record+8 is probably a type tag, not the value

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
Hypothesis (unproven): `record+8` is a type/kind tag of the tweak record; the live value is at
another offset or behind a pointer in the record (e.g. the `gXxx` global in .data). This would
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
| BUILD-7 | Build + verify the 2026-10-29 package (new file `src/rogue_probe.cpp` -> `scripts\_game_ffx_build.bat`) | U | NEXT |

### Bugs
| ID | Bug | Status |
|---|---|---|
| BUG-1 | Perk Shop: every non-curse offer stayed FREE after the free pick was used, so all could be taken and `free` went negative (owner save showed `free=-1`). | FIXED in package 2026-10-29 (`Buy` refuses when no pick left, panel refreshes FREE flags after a buy, `Load` clamps `free` to >= 0). Verify in BUILD-7 |
| BUG-2 | Legacy Rogue section (and Start career, which writes save files) showed without `NHL_ROGUE=1`. The overlay itself is parent-owned and must stay. | FIXED in package 2026-10-29 (`DrawPanel` returns when `rogue::Enabled()` is false). Verify in BUILD-7 |

### Tunable perks (no RE)
| ID | Task | Who | Status |
|---|---|---|---|
| TUN-4 | Find where a tweak record's live value really is (see Key finding). Probe `src/rogue_probe.cpp` delivered; owner runs 3 snapshots, uploads `rogue_probe.txt` (user-guide section 4a). Agent then pins the offset/pointer, re-tests with one obvious setting, and moves `TunablePatcher` to it | U then A | ACTIVE (owner run) |
| TUN-1 | Stock values + ranges for every tunable the perks use; fix step sizes in perk_catalog.cpp / reactions.cpp. Note: the schema's 0..20 slider ranges are inferred, not proven | A then U | BLOCKED (TUN-4) |
| TUN-2 | Do AI TEAMMATES read *Level or *LevelCPU in BAP? | U | BLOCKED (TUN-4) |
| TUN-3 | Which tunables are rewritten every frame | U | PARTIAL: `gTweakByGameStyleGameSpeedLevel` record+8 is NOT rewritten (survives whistles). Redo per setting after TUN-4 |

### Reverse engineering (ledger: re-findings.md)
| ID | Task | Who | Status |
|---|---|---|---|
| RE-0 | Run `tools/find_bap_code.py` on the decompressed image with `--generated generated/default`, upload report. Owner may need to produce the image first (user-guide section 4) | U | TODO |
| RE-1 | Check resolution function (hitter/hittee regs) | A+U | TODO |
| RE-2 | Identify the BAP-controlled skater | A+U | TODO |
| RE-3 | Knockdown: fall entry point or skater balance offset | A+U | TODO |
| RE-4 | Puck-vs-skater contact | A+U | TODO |
| RE-5 | BAP game start / end + final score + BAP player's stat line | A+U | TODO |
| RE-6 | BAP stat-event stream. Lead: `gBeAPro_*_StatEvent` tunables | A+U | TODO |
| RE-7 | BAP career save identity | A+U | TODO |
| RE-8 | BAP progression storage (lead: `BAP_PLAYER_GROWTH` @ 0x82035818) | A+U | TODO |
| RE-9 | Meta reactions: contracts, trades, line promotion, task grading | A+U | TODO |

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

1. Owner: BUILD-7 (apply zip, `scripts\_game_ffx_build.bat`, verify BUG-1 and BUG-2 fixes),
   then TUN-4 probe run (user-guide section 4a), upload `rogue_probe.txt`.
2. Agent: read `rogue_probe.txt`. Look for a record word that differs from 4/13 and matches the
   expected type (0.2 float for `gDekeRatEffect`, ~1.0 for multipliers), or a pointer whose
   target does. Compare the menu / in-match / after-edit snapshots. Write the result to
   `re-findings.md` with evidence + confidence, then a one-setting live test for the owner.
3. Meanwhile any agent: RE-0 prep (decompressed image steps, user-guide section 4).

## How to verify the branch state without the owner

Agents can READ GitHub: list commits on `MegaSloppa/nhl-legacy-recomp` branch `legacy-rogue`,
read files from it, and compare blob SHAs against `puckhead73/nhl-legacy-recomp` master to spot
accidental edits to parent files. Expected parent-file differences from upstream: root
`CMakeLists.txt` (add_subdirectory), `src/nhllegacy_app.h`, `renderer/core/nhl_overlay.cpp`,
`.gitignore` (run_dev.bat), `scripts/_ffx_dll_build.bat` (VS 2026). Anything else is suspect.
