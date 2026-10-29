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

### 2026-10-29 (later) - Victoria - TUN-4 probe decoded; TUN-5 write test
**Objective:** Read the owner's probe run and find the real value location.
**Done:**
- Owner applied and committed the previous package (316c3a1); the probe compiled and ran:
  3 snapshots (menu, in match, after setting GameSpeedLevel to 20), ~0.6 s each.
- Decoded the tweak object layout (re-findings.md, TUN-4 entry). record+8 = type tag, value at
  record+20, min/max/default after it, heap copy via record+36. The only change after the edit
  was record+8 13 -> 20 in the image object. Values identical at menu and in match.
- The parent's sanity value (`gDekeRatEffect` 0.2) comes from the registration POOL
  (category, name, label, default), not from a live object.
- Added the TUN-5 write test to `src/rogue_probe.cpp` (+ `probe.h`, panel buttons): finds the
  float object by name, validates the layout on both copies, writes stock x factor to both,
  restores on demand, logs every write.
- Owner design direction: perks should be able to go far beyond vanilla (physics-defying checks,
  flying opponents, penalty immunity). Added RE-10 / RE-11.
**Files changed:** `src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`,
`docs/re-findings.md`, `docs/current-status.md`, `docs/handoff-log.md`, `docs/user-guide.md`,
`docs/00-START-HERE.md` (file map, tunable facts).
**Validation:** probe output analysed (sandbox script). Write test NOT compiled by the agent.
No new .cpp, so the incremental build is enough.
**Proven:** layout above for 7 objects (float, int, bool, game-style table, indexed slider).
**Hypotheses / unresolved:** which copy the game reads; whether min/max are enforced; whether
values are cached at match load; game-style table index (TUN-7).
**Next action:** TUN-5 (owner), then TUN-6 (agent).

### 2026-10-29 (evening) - Victoria - TUN-5 result: value writes have no visible effect
**Objective:** Evaluate the TUN-5 write test.
**Done:**
- First build of the TUN-5 package did nothing: the zip carried UTC timestamps, which unzip
  2 h old in Helsinki, so Ninja skipped the files. Re-issued with local timestamps. Agents: stamp
  zip entries with the owner's local time (Europe/Helsinki).
- Owner ran TUN-5 (log: 13 writes, all three objects found and validated, restores logged):
  live edits in a BAP match and x10 set at the menu before an exhibition. No visible effect on
  slap shots (x0.2 and x10) or hits.
- Wrote `tools/find_tweak_consumers.py` (TUN-8): static scan of generated code for references
  to each object span and the game-style tables, with two rendering tunables as positive
  controls. Self-tested on a synthetic sample only.
**Files changed:** `tools/find_tweak_consumers.py` (new), `docs/current-status.md`,
`docs/handoff-log.md`, `docs/re-findings.md`.
**Validation:** owner report + `rogue_probe.txt` write log.
**Proven:** writes to N+20 and the heap copy land and stay; no visible gameplay change.
**Hypotheses / unresolved:** see current-status "Recommended next action" A/B/C. The tool assumes
loads look like `ctx.rN.u32 + disp`; unverified against real generated code.
**Next action:** TUN-8 (owner runs the scan), then RE-0 / RE-1.

### 2026-10-29 (evening, later) - Victoria - TUN-8 scan: value field is N+32; TUN-5b
**Objective:** Read the consumer scan.
**Done:**
- `tweak_consumers.txt` (183 generated files). Object stride is 48 bytes (N-8 .. N+40), so
  "+40" hits are the NEXT object. Static-init code (files 165-180) constructs objects (-8).
  Real readers load base+40 = N+32: gHitTune_GlobalScale (sub_83588BF8, hit code area),
  gSlapShotPowerScale (sub_833E87E0), gBeAPro_LineEnergyBoost (sub_833CBC08), gLODShader
  (sub_8361FA70). gEnableLynxBinaryFileLoad reads byte N+21 and N+24/N+28/N+32.
  Game-style: sub_829F8A20 (file 49) loads *(N+36) = heap copy H, then *(H+32) = heap table.
  gSlapShotPowerMultiplier and gMomentumPhysicsImpulse: no static reader found.
- So TUN-5 wrote N+20 (equal to N+32 at stock), the wrong field. Built TUN-5b.
**Files changed:** `src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`, docs.
**Validation:** none yet (not compiled by the agent).
**Hypotheses / unresolved:** N+20 = reset/default, N+32 = current. Whether sub_829F8A20 runs per
frame or at match start. Readers of SlapShotPowerMultiplier / MomentumPhysicsImpulse may use a
lookup by name (not visible statically).
**Next action:** TUN-5b (owner).

### 2026-10-29 (morning) - Victoria - TUN-5b: N+32 confirmed as the live value; TUN-5c + TUN-9
**Objective:** Evaluate TUN-5b, prepare the full catalog and hit/penalty tests.
**Done:**
- Owner TUN-5b report + log: gLODShader 0 / 2 changed rendering immediately (0 = flickering dark
  splotches on players, 2 = constant slight stutter). gSlapShotPowerScale x0.2 = limp, x10 = very
  fast (but not superhuman). Game-style tables 0 / 20: no effect. gHitTune_GlobalScale x10: no
  visible effect, but big hits are hard to land.
- TUN-5c targets (scalar, float or int auto-detected from the tag): SlapShotPowerScale x30,
  hit-power modifiers, fall thresholds, check reach, gPenaltyChanceCharging.
- TUN-9: panel "Dump tweak catalog" scans the image for every tweak object (signature:
  name "g...", tag, class pointer, heap copy with the same name) and writes
  rogue_tweak_catalog.tsv. `find_tweak_consumers.py --catalog` counts static readers of N+32.
**Files changed:** `src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`,
`tools/find_tweak_consumers.py`, docs.
**Validation:** tool self-tested on a synthetic sample; C++ not compiled by the agent.
**Proven:** N+32 is read live by game code (2 objects, repeated toggles, one session). Game code
does not clamp to the object's min/max.
**Hypotheses / unresolved:** N+20 = reset/default. Readers that go through the heap copy are not
counted by the static tool. Game-style presets are applied somewhere else.
**Next action:** TUN-5c + TUN-9 (owner), then TUN-6 (agent).

### 2026-10-29 (late morning) - Victoria - Catalog analysed; TUN-6 done; RE-1 probe hooks; TUN-10
**Objective:** Use the TUN-9 catalog to fix the perk write path and find the hip-check code.
**Done:**
- TUN-5c owner notes: slap x3..x30 differ less than expected (probable puck speed cap), fall
  thresholds / check distances inconclusive, gPenaltyChanceCharging stock 0,
  gHitPowerForPhysicsRatingModifier is tag 12 (the write test only takes tags 2/4).
- Catalog: 10,583 objects; readers: 7,779 with a static reader of N+32.
- TUN-6: new `include/rogue/tweak_index.h` + `src/tweak_index.cpp` (image scan by name, same
  signature as the catalog; writes N+32 and H+36). `tunable_patch.h` rewritten on top of it;
  `rogue_mod.cpp` builds the index 20 s after boot or at BAP game start. Parent store unused.
- perk_catalog.cpp / reactions.cpp rebuilt on tunables with readers (stock values in comments).
  New Common perk Quick Release (gWristShotPowerMultiplier). Existing saves keep their unlock
  list, so Quick Release only appears for a fresh meta file.
- RE-1 probe hooks (pass-through, NHL_ROGUE_PROBE=1 logs) on six candidate functions; hip-check
  write test (8 tunables + hit power) and a "Hip check combo (guess)" button; Mark log button.
- Catalog / readers files are now .txt (tab-separated) so they can be uploaded.
**Files changed:** `include/rogue/tweak_index.h` (new), `src/tweak_index.cpp` (new),
`include/rogue/tunable_patch.h`, `src/rogue_mod.cpp`, `src/perk_catalog.cpp`, `src/reactions.cpp`,
`src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`, `CMakeLists.txt`,
`tools/find_tweak_consumers.py`, docs.
**Validation:** not compiled by the agent. New .cpp -> `_game_ffx_build.bat`.
**Proven:** catalog and reader counts above.
**Hypotheses / unresolved:** which hook fires per hip check; direction of each hip-check tunable;
all new perk tunables except gSlapShotPowerScale are unverified in play.
**Next action:** TUN-10 + RE-1 run (owner).

### 2026-10-29 (midday) - Victoria - RE-1 round 1 result; round 2 = player-event handlers
**Objective:** Read the first hook log.
**Done:**
- 30 min exhibition, 6 marks, one landed hip check. sub_83411E90, sub_83411C48, sub_83429698 and
  sub_83589230 never fired at all. sub_8342C628 ran ~385 calls/s from lr 0x8342D13C with ~14
  distinct r3 values (0xA9xxxxxx): a per-skater update, so r3 is a skater object. sub_83588BF8 ran
  in bursts (median 1/s, up to 77/s) from lr 0x835A44E4 / 0x8343832C with r3/r4 = two skater
  objects; no calls near the owner's marked hits.
- Conclusion (hypothesis): the Physics/Ragdoll/HipCheck tunable family belongs to a code path the
  normal user hip check does not use (or one that is dormant).
- Round 2 hooks: 15 player-event handlers (gEvtValPE_* each has one reader in file 44, spaced
  0x260 apart) + PE_NONE control + the two round-1 functions. Log also carries r7.
**Files changed:** `src/rogue_probe.cpp`, docs.
**Validation:** not compiled by the agent; no new files.
**Next action:** owner event script; then RE-2 / RE-5 / RE-6 wiring.

### 2026-10-29 (afternoon) - Victoria - Event handlers idle; hook method verified; RE-1 sweep
**Objective:** Find code that runs on a landed check.
**Done:**
- Exhibition (20 marks) and BAP game (13 marks): none of the 15 PE_* handlers or the PE_NONE
  control fired. Only sub_8342C628 / sub_83588BF8 fired, as before.
- Owner's findstr (hook_refs.txt): `nhllegacy_init.cpp` / `nhllegacy_register.cpp` register
  `sub_X` in the function table, so indirect calls reach a strong override; sub_83411E90 has 3
  direct callers in file 134 and still never ran. Conclusion: hooks work; those functions are idle.
- Round 3: hooks on 141 functions that read a hit / check / balance / fall / collision tunable
  (filtered TUN-9 readers; list in re-findings.md). Per-second counts + sparse call lines.
- Active perk effects readout in the panel (stock -> live for every perk / reaction tunable).
**Files changed:** `src/rogue_probe.cpp`, `src/rogue_panel.cpp`, docs.
**Validation:** not compiled by the agent; no new files.
**Next action:** owner sweep run, then RE-1 analysis.

### 2026-10-29 12:00 - Victoria - Session checkpoint (owner break)
**Objective:** Leave the branch ready for a commit, push and a fresh session.
**Done:** Verified GitHub HEAD = 3ce8b2a. Added the checkpoint section to current-status.md
(what is committed, what is applied but untested, first steps next session) and PARENT-1.
00-START-HERE: delivery conventions (local-time zip timestamps, commit messages only on request).
**Files changed:** `docs/current-status.md`, `docs/handoff-log.md`, `docs/00-START-HERE.md`.
**Validation:** doc review only.
**Proven / hypotheses:** unchanged from the previous entry.
**Next action:** owner commits; next session runs user-guide 4b (perk readout + hit sweep), then
RE-1 analysis of the sweep log.
