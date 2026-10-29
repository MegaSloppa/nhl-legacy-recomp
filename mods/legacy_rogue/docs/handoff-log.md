# Handoff log (append-only, newest at the bottom)

Read the last 2 entries at session start. Append one before you stop.

## Template

```markdown
### YYYY-MM-DD - Agent name - Short title
**Objective:**
**Done:**
**Files changed:** path - effect
**Validation:** exact command or user report - PASS/FAIL
**Proven:**
**Hypotheses / unresolved:**
**Next action:** (task ID from current-status.md)
```

---

### 2026-10-27 - Brenda - Initial scaffold + career-long redesign
**Objective:** Map tools/workflow for a Be-A-Pro roguelite mod and scaffold it.
**Done:**
- Read parent repo: DEV-README, current-status, engine-settings-editor-plan, sim-engine-re-plan,
  diag_hooks.cpp, tunable_runtime.h, nhl_tunable_store.h, find_franchise_code.py,
  present_grade_compat.cpp, agent-coordination-handoff.md, codex-onboarding.md.
- Mined `docs/tunables_schema.tsv` for hit, fall, shot, BAP and human/CPU tunables.
- Wrote the mod scaffold (tunable layering, perks, career run with Hype/Notoriety/Legacy,
  league reactions, FX queue, hook stubs, panel, tools) and this doc set.
- Owner design direction 2026-10-27: run = whole BAP career; drafts between games cost
  currency; FX for crits; league should react to dominance; more meta progression; win/loss
  condition undecided (DES-1).
**Files changed:** all files under mods/legacy_rogue (new).
**Validation:** none; not compiled, not run. GitHub fork and repo creation both failed from
the agent's connected account, so nothing was pushed; delivered as a zip.
**Proven:** tunable names used in code exist in tunables_schema.tsv. Color-grade cvars are a
no-op in the rebuilt runtime (present_grade_compat.cpp), so FX use ImGui instead.
**Hypotheses / unresolved:** human/CPU tunable split may also govern AI teammates (TUN-2).
ScopedTunable only works if the consumer runs synchronously inside the hooked call.
**Next action:** owner BUILD-1..3 and RE-0; agent TUN-1, FX-1.

### 2026-10-28 - Brenda - Build bring-up on the owner's machine; first modded build
**Objective:** Get the parent project and then the mod building from a clean machine.
**Done (owner ran every command; agent diagnosed from logs):**
- Toolchain: VS Build Tools 2026 (not 2022), standalone LLVM, CMake, Ninja, Vulkan SDK,
  PowerShell 7. Every setup problem and its fix is in user-guide.md section 7.
- Vanilla game built and launched, then the mod.
- Mod build fixes (committed by the owner in 0f1a312):
  - `mods/legacy_rogue/CMakeLists.txt`: sources go into the `nhllegacy` target via
    `target_sources`. The previous separate OBJECT library could not see imgui, the rex SDK
    headers or `generated/`, and lacked the game's compile definitions (REX_HAS_VULKAN etc.,
    an ABI hazard). Root CMakeLists no longer links `legacy_rogue`.
  - `src/fx.cpp`: `ImFont::FontSize` is gone in this ImGui (1.92+); use `ImGui::GetFontSize()`.
  - `renderer/core/nhl_overlay.cpp`: also include `rogue/rogue_mod.h`.
- Reviewed the owner's wiring on GitHub: `rogue::Init` after `tunable_store_` creation in
  nhllegacy_app.h; `rogue::fx::Draw()` before the overlay visibility early-return (FX-1);
  `rogue::Tick(); rogue::DrawPanel();` inside the visible block (Tick should move: BUILD-4).
- Found an accidental CRLF->LF conversion of two parent scripts in 0213121 (BUILD-5).
- GitHub writes (push_files, create_or_update_file) failed from the agent; read works.
  Delivered these doc updates as a zip of changed files; owner committed them in 87a6b59.
**Files changed:** docs/current-status.md, docs/handoff-log.md, docs/00-START-HERE.md,
docs/user-guide.md
**Validation:** owner report: modded build PASS, game launches, Legacy Rogue section visible.
**Proven:** all 7 mod .cpp files compile against SDK bd9b519 + title patch with clang.
`rogue_hooks.cpp` compiles with no addresses defined (all hooks `#if`'d out).
**Hypotheses / unresolved:** runtime behavior of panel, tunable layering and saves unverified.
**Next action:** BUILD-3, BUILD-4, BUILD-5 (owner); TUN-1 (agent).

### 2026-10-28 (later) - Brenda - Verified fixes on GitHub; pre-break checkpoint
**Objective:** Confirm the owner's follow-up commit and leave the docs ready for a context wipe.
**Done:**
- Read commit 87a6b59 on `legacy-rogue`: docs updated; `rogue::Tick();` now directly after
  `rogue::fx::Draw();` before `if (!visible_) return;` in `NhlEnhancementsDialog::OnDraw`;
  `rogue::DrawPanel();` inside the menu. BUILD-4 DONE.
- Compared `scripts/` blob SHAs with upstream master: `_build_vkffx_incr.bat` and
  `_c5renderdoc.ps1` identical again. BUILD-5 DONE. Only `_ffx_dll_build.bat` differs (intended).
  Owner had no local `master` branch; restored from scaffold commit 9d2ed9b instead
  (`git checkout 9d2ed9b -- <files>`; `origin/master` also works after `git fetch`).
- Explained incremental builds; documented `_build_vkffx_incr.bat` (build only) vs
  `_game_ffx_build.bat` (configure + build, needed after CMakeLists changes) in user-guide.
**Files changed:** docs/current-status.md, docs/handoff-log.md, docs/user-guide.md,
docs/00-START-HERE.md (delivered as zip; GitHub writes still fail from the agent).
**Validation:** GitHub reads of commit 87a6b59, renderer/core/nhl_overlay.cpp and scripts/ SHAs.
**Proven:** wiring is final; parent scripts clean.
**Hypotheses / unresolved:** nothing about runtime behavior is verified yet.
**Next action:** BUILD-3 smoke test (owner), TUN-1 (agent).

### 2026-10-29 - Victoria - Smoke test passed; tunable perks found inert; probe + 2 bug fixes
**Objective:** Guide BUILD-3, then act on what it found.
**Done:**
- Guided the owner through BUILD-3 (all from the panel; no match needed while hooks are
  unmapped). All 5 checks PASS on commit 3d595fe.
- BUG-1 found from the owner's report (two perks owned after one free pick, save `free=-1`):
  `RollShop` marks every non-curse offer free while a pick exists and the panel never
  refreshed the flags after a buy. Fixed in `Buy`, the panel and `Load`.
- BUG-2 (found by reading `rogue_panel.cpp`): `DrawPanel` had no `NHL_ROGUE` check. Added
  `rogue::Enabled()`; only the Legacy Rogue section is hidden, the parent overlay is unchanged.
- Owner in-match test of `gTweakByGameStyleGameSpeedLevel` (0 and 20): no visible effect, not
  rewritten by whistles or the game's own speed setting.
- Analysed `docs/tunable_values_runtime.txt` joined with `docs/tunables_schema.tsv` (sandbox
  script): record+8 values cluster by TYPE (floats 4, bools 1, game-style 13), see the Key
  finding in current-status.md.
- Wrote the TUN-4 probe: `src/rogue_probe.cpp` + `include/rogue/probe.h`, panel Diagnostics
  section (only with `NHL_ROGUE=1` and `NHL_ROGUE_PROBE=1`). Read-only; dumps the registry
  records for 12 tunables plus whatever their pointer fields point at, to `rogue_probe.txt`.
  Uses `rex::runtime::current_kernel_state()->memory()->virtual_membase()` (same API as
  diag_hooks.cpp) and the same committed-memory walk as `TunableRuntimeStore::Build`.
**Files changed:**
- `src/rogue_probe.cpp` (new), `include/rogue/probe.h` (new) - TUN-4 probe.
- `CMakeLists.txt` - adds rogue_probe.cpp (owner must use `scripts\_game_ffx_build.bat`).
- `src/rogue_panel.cpp` - BUG-1 flag refresh, BUG-2 gate, Diagnostics buttons.
- `src/run_state.cpp` - BUG-1 guard in `Buy`, `free` clamp in `Load`.
- `include/rogue/rogue_mod.h`, `src/rogue_mod.cpp` - `rogue::Enabled()`.
- `docs/current-status.md`, `docs/handoff-log.md`, `docs/user-guide.md`.
**Validation:** BUILD-3 owner report PASS. New code NOT compiled by the agent (no local build).
**Proven:** smoke test results above; the record+8 value distribution; GameSpeedLevel edit has
no visible effect and is not rewritten.
**Hypotheses / unresolved:** record+8 is a type tag and the live value is elsewhere (TUN-4).
The probe's include set mirrors diag_hooks.cpp; if `virtual_membase()` or an include does not
resolve, fix the include, not the approach. The 0..20 ranges in tunables_schema.tsv are inferred.
00-START-HERE.md file map not yet updated for probe.h / rogue_probe.cpp.
**Next action:** BUILD-7 then TUN-4 probe run (owner); TUN-4 analysis (agent).
