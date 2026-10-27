# Current status (AUTHORITATIVE)

Last updated: 2026-10-27 by Brenda. If this disagrees with another doc, this wins; fix the other doc.

## Baseline

- Scaffold written, **never compiled**. Nobody has built it yet. First build will surface
  include-path / SDK-API mismatches; fixing those is task BUILD-1.
- No guest addresses are known. All event perks are inert; tunable perks + shop + Notoriety +
  FX + saves are usable through the panel's Manual buttons once built.
- Code lives outside the parent repo for now (GitHub fork/repo creation failed from the
  agent side on 2026-10-27). The owner applies it locally; see `user-guide.md`.

## Task board

Status: TODO / ACTIVE (owner in brackets) / BLOCKED (reason) / DONE (date).
Who can do it: A = any agent with no game access, U = needs the user to build/play/upload.

### Build and wiring
| ID | Task | Who | Status |
|---|---|---|---|
| BUILD-1 | Add `add_subdirectory(mods/legacy_rogue)` + link; fix compile errors (imgui target name, rex include dirs, `REX_PHYS_HOST_OFFSET` availability in rogue_hooks.cpp) | U (+A to fix errors from the log) | TODO |
| BUILD-2 | Add the 4 wiring lines (user-guide.md "Wiring") in `src/nhllegacy_app.h` and `renderer/core/nhl_overlay.cpp` | U/A | TODO |
| BUILD-3 | Smoke test: `NHL_ROGUE=1`, open overlay, start career, Test FX button, buy a perk, confirm an Engine Tunables value changed | U | TODO |

### Tunable perks (no RE)
| ID | Task | Who | Status |
|---|---|---|---|
| TUN-1 | Read stock values + ranges for every `gTweakByGameStyle*Level` used (docs/tunable_values_runtime.txt or live overlay); fix +1 steps in perk_catalog.cpp / reactions.cpp if out of range | A (from the repo dump) then U to confirm live | TODO |
| TUN-2 | Verify the human/CPU split in BAP: do AI TEAMMATES read *Level or *LevelCPU? Test: buy Freight Train, watch AI teammate hits | U | TODO |
| TUN-3 | Check which candidate tunables are rewritten every frame (edit does not stick) | U | TODO |

### Reverse engineering (see re-findings.md for the ledger)
| ID | Task | Who | Status |
|---|---|---|---|
| RE-0 | Run `tools/find_bap_code.py` on `default.decompressed.exe` with `--generated generated/default`, upload report | U | TODO |
| RE-1 | Check resolution function (hitter/hittee regs) | A writes probes, U runs | TODO |
| RE-2 | Identify the BAP-controlled skater (global ptr or controller field) | A+U | TODO |
| RE-3 | Knockdown: fall entry point, or skater balance field offset | A+U | TODO |
| RE-4 | Puck-vs-skater contact (struck skater, puck speed, last shooter, shot type) | A+U | TODO |
| RE-5 | BAP game start / end + final score + BAP player's stat line | A+U | TODO |
| RE-6 | BAP stat-event stream (goal/assist/hit events). Lead: `gBeAPro_*_StatEvent` tunables | A+U | TODO |
| RE-7 | BAP career save identity (slot / player id) to bind a rogue career automatically | A+U | TODO |
| RE-8 | BAP progression: where attribute points / XP live (lead: string `BAP_PLAYER_GROWTH` @ 0x82035818). Enables perks that grant real attribute growth | A+U | TODO |
| RE-9 | Meta reactions: contract offers, trade requests, line promotion, BAP task grading. Leads: `gBeAPro_ScoreBonus_*`, `gBAPTasks_*`, `BAPGenerateOffenseNewTeamTask` | A+U | TODO |

### Feedback effects
| ID | Task | Who | Status |
|---|---|---|---|
| FX-1 | Find a per-frame ImGui draw point that runs with the overlay CLOSED (perf HUD path in renderer/core/nhl_overlay.cpp?) and call `rogue::fx::Draw()` there | A (read source) + U | TODO |
| FX-2 | Create placeholder WAVs in `<exe>/rogue/sfx/` (crit, knockdown, shockwave, perk, tier, milestone) | U | TODO |
| FX-3 | Replace PlaySoundW (single voice) with XAudio2 or the SDK audio path | A | TODO |
| FX-4 | In-game effects via guest calls: crowd roar, horn, camera shake, replay flag | A+U, after RE-1..4 | TODO |
| FX-5 | Experiment: hit-stop via game-speed tunable (`gTweakByGameStyleGameSpeedLevel`) for 0.2 s on crits | U | TODO |

### Design
| ID | Task | Who | Status |
|---|---|---|---|
| DES-1 | Pick the win/loss structure (options in design.md section 6) | Owner | TODO |
| DES-2 | Balance pass: Hype income vs prices across an 82-game season | A after BUILD-3 | TODO |

## Recommended next action

Owner: BUILD-1..3, then RE-0. Agent: TUN-1 and FX-1 (both doable from repo source alone),
then write the RE-1 probe set once RE-0 report is uploaded.
