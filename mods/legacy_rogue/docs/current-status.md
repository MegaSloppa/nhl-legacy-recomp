# Current status (AUTHORITATIVE)

Last updated: 2026-10-28 by Brenda. If this disagrees with another doc, this wins; fix the other doc.

## Baseline

- **The modded game builds and launches** on the owner's machine (2026-10-28, branch
  `legacy-rogue` of `MegaSloppa/nhl-legacy-recomp`, commit 0f1a312). The Legacy Rogue section
  appears in the overlay (Guide/F1).
- Not yet verified in play: starting a career, buying a perk, a tunable actually changing,
  FX popups, save files. That is BUILD-3.
- No guest addresses are known. All event perks are inert. Tunable perks, shop, Notoriety,
  FX and saves are driven by the panel's Manual buttons.
- Owner's toolchain: VS Build Tools 2026 + standalone LLVM + Vulkan SDK + PowerShell 7.
  Every setup problem hit so far and its fix: `user-guide.md` section 7.
- GitHub: agents can READ the fork but writes (push, file update, fork, repo create) fail from
  the agent's connection as of 2026-10-28. Deliver changes as a zip of changed files only.

## Task board

Status: TODO / ACTIVE (owner in brackets) / BLOCKED (reason) / DONE (date).
Who: A = any agent with no game access, U = needs the user to build/play/upload.

### Build and wiring
| ID | Task | Who | Status |
|---|---|---|---|
| BUILD-1 | Integrate into CMake + fix compile errors | U+A | DONE 2026-10-28. Sources go into the `nhllegacy` target via `target_sources` (00-START-HERE rule 9) |
| BUILD-2 | Wiring lines in `src/nhllegacy_app.h` and `renderer/core/nhl_overlay.cpp` | U | DONE 2026-10-28 (commits 0213121, 0f1a312) |
| BUILD-3 | Smoke test with `NHL_ROGUE=1`: start career, Test FX, buy a perk, press "BAP game started" and confirm an Engine Tunables value changed, confirm `rogue_career.txt` written next to the exe | U | ACTIVE (panel visible; rest untested) |
| BUILD-4 | In `nhl_overlay.cpp`, move `rogue::Tick();` from the visible-overlay block to just after `rogue::fx::Draw();` (before `if (!visible_) return;`) so the tunable index builds and perks apply with the overlay closed | U | TODO |
| BUILD-5 | Commit 0213121 converted `scripts/_build_vkffx_incr.bat` and `scripts/_c5renderdoc.ps1` from CRLF to LF by accident. Restore: `git checkout master -- scripts/_build_vkffx_incr.bat scripts/_c5renderdoc.ps1`, commit | U | TODO |
| BUILD-6 | `scripts/_ffx_dll_build.bat` generator changed to `Visual Studio 18 2026` (commit 34a34a3). Machine-specific; do not upstream. Portable fix = auto-detect the VS generator | A | TODO (low) |

### Tunable perks (no RE)
| ID | Task | Who | Status |
|---|---|---|---|
| TUN-1 | Stock values + ranges for every `gTweakByGameStyle*Level` used (docs/tunable_values_runtime.txt or live overlay); fix +1 steps in perk_catalog.cpp / reactions.cpp if out of range | A then U | TODO |
| TUN-2 | Do AI TEAMMATES read *Level or *LevelCPU in BAP? | U | TODO |
| TUN-3 | Which candidate tunables are rewritten every frame (edit does not stick) | U | TODO |

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
| FX-1 | Per-frame FX draw with the overlay closed | U | LIKELY DONE: `rogue::fx::Draw()` runs in `NhlEnhancementsDialog::OnDraw` before the visibility early-return. Confirm: press Test FX, close the overlay within 1 s, popup should still be visible |
| FX-2 | Placeholder WAVs in `<exe>/rogue/sfx/` | U | TODO |
| FX-3 | Replace PlaySoundW with XAudio2 or SDK audio | A | TODO |
| FX-4 | In-game effects via guest calls (crowd, horn, camera shake, replay) | A+U after RE-1..4 | TODO |
| FX-5 | Hit-stop experiment via game-speed tunable | U | TODO |

### Design
| ID | Task | Who | Status |
|---|---|---|---|
| DES-1 | Pick the win/loss structure (design.md section 6) | Owner | TODO |
| DES-2 | Balance pass: Hype income vs prices over an 82-game season | A after BUILD-3 | TODO |

## Recommended next action

Owner: BUILD-3 smoke test, BUILD-4 one-line move, BUILD-5 restore. Then RE-0.
Agent: TUN-1 (repo data only), then the RE-1 probe set once RE-0 is uploaded.
