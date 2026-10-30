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

### 2026-10-29 (13:00) - Victoria - Static analysis of nhllegacy_recomp.39.cpp
**Objective:** Owner uploaded generated file 39 (skating); read it statically.
**Done:**
- sub_828CC300 (8,600 lines) is a SETTINGS SNAPSHOT: it copies ~890 tweak values (N+32 for
  float/int, N+21 byte for bools) into a struct at r3 (stores up to +3352). 822 fields named via
  the TUN-9 catalog -> `docs/skating_params_struct.txt`. No direct callers in file 39.
- Hypothesis: skating code reads the copy, so live tweak edits only apply when the snapshot
  re-runs; explains the no-effect fall-threshold test (TUN-5c). Sweep will show cadence (TUN-12).
- Bool objects are absent from the catalog (TUN-11).
- Rest of file 39: ragdoll blend-out / pitch-upright / hand-influence code (sub_828C75A8,
  sub_828C7D50, sub_828CB380); largest functions are component code with 60-117 indirect calls.
  Offset 640 is not read anywhere in file 39 (struct consumers live in other files).
- New tool `tools/slice_generated.py` (func / callers / callees / offset) for targeted slices.
**Files changed:** `docs/skating_params_struct.txt` (new), `tools/slice_generated.py` (new),
`docs/current-status.md`, `docs/handoff-log.md`, `docs/re-findings.md`, `docs/00-START-HERE.md`.
**Validation:** static only; slice tool self-tested on file 39.
**Next action:** user-guide 4b runs; `slice_generated.py offset 640 --float` over all files.

### 2026-10-29 (13:15) - Victoria - Fall-threshold consumer search; session end
**Objective:** Find the code that reads the snapshot's fall thresholds (+636 / +640).
**Done:**
- `slice_generated.py offset 640/636 --float` over all files: 10 / 9 functions. Best-looking hit
  sub_830E65F8 (file 107, reads +632..+648 together) is a FALSE POSITIVE: it reads a constant
  table at 0x83C0CBC0, runs only when its size argument == 320, and loops 40x doing complex
  multiply-adds (FFT / transform butterfly). Other hits: stack slots, idle PE handlers,
  sub_8334A360 (file 126, copies r29+640 / +648 to the stack; weak, unexplored).
- Lesson: an offset search only finds `reg + 640` accesses; the snapshot struct is probably
  embedded in a larger object, so consumers use (struct offset in parent) + 640. Check WHICH
  register / base an offset is read from before calling a hit a lead.
- Callers of sub_828CC300: sub_8332AF40 (125), sub_828F1550 (40), sub_82920828 (42),
  sub_82933A40, sub_8293B9A8 (43), sub_8293BF80 (44). Added `callsite` to slice_generated.py.
**Files changed:** `tools/slice_generated.py`, `docs/current-status.md`, `docs/handoff-log.md`,
`docs/re-findings.md`.
**Validation:** static only.
**Next action:** see current-status "Recommended next action" (sweep run first).

### 2026-10-30 - Hannah - Perk readout verified; RE-1 round 3 decoded; round 4 hit watch
**Objective:** Pick up after the context wipe, analyse the owner's sweep run, prepare the
confirmation run.
**Done:**
- Verified GitHub HEAD = 6f4d665 (contains every 2026-10-29 package; current-status said
  3ce8b2a + uncommitted work, now fixed).
- Owner: perk readout PASS (wrist shot power stock -> perk value -> stock). TUN-6 path verified.
- Owner sweep run (training 1-on-1, 472 s, 5 marked checks, 1.58 MB log). Analysed with a
  sandbox script (per-second counts + call lines vs marks; marks lag checks by 4.1-4.7 s).
  Contact = sub_833E0BF8 (r3 victim, r4 hitter, r6 0x83ABF598 on all 3 knockdowns /
  0x83ABF57C on both small hits); credit = sub_8357F9A0 / sub_83589B38 (r4 hitter, r5 victim,
  r7 0..3; missed mark 1); knockdown-only = sub_829380A0; skating snapshot sub_828CC300 runs in
  29 bursts, not per frame. Details: current-status Key finding 2026-10-30.
- Round 4 hit watch: the 141-function sweep is replaced by 7 pass-through hooks. Each contact
  pushes an on-screen HIT / KNOCKDOWN line (hitter -> victim, last 4 hex digits); the panel
  lists the last 12 contacts with credit / fall flags; first sight of each player object (512
  bytes) and reaction pointer (64 bytes) is dumped once; call lines capped at 40/s per function.
- Slice command for the owner (sub_833E0BF8 body, callers, call sites; sub_8357F9A0 call sites).
**Files changed:** `src/rogue_probe.cpp` (RE-1 section rewritten; rest unchanged except
`#include "rogue/fx.h"`), `include/rogue/probe.h` (HitEvent + accessors), `src/rogue_panel.cpp`
(Hit watch section), `docs/current-status.md`, `docs/handoff-log.md`.
**Validation:** C++ not compiled by the agent (no local build). No new .cpp: incremental build.
**Proven:** perk readout. Round 3 reading is OBSERVED in one session only (5 marked checks).
**Hypotheses / unresolved:** r6 0x83ABF598 = knockdown descriptor; sub_8357F9A0 = hit stat
credit; player objects are per-session heap addresses; snapshot rebuilds at drill resets.
re-findings.md round-3 entry and user-guide 4c NOT written yet (owed; evidence is in
current-status).
**Next action:** owner slice command + BUILD-8 + round 4 run (current-status Recommended next
action 1-2); agent: confirm RE-1, addresses.h, first hit / knockdown wiring.

### 2026-10-30 (13:45) - Hannah - Round 4 decoded; hit contact CONFIRMED; round 5 package
**Objective:** Analyse round 4 + the owner's static slice; wire the confirmed hook.
**Done:**
- Round 4 log (157 KB, ~16 min, exhibition + training): 59 contacts in 39 interactions. Owner:
  hitter -> victim correct, knockdowns consistently labelled, 2-3 lines per collision, goalie
  knockdown with no effect. Explained: one contact call per reacting player; goalies take the
  knockdown reaction but never run the fall code; goalie class pointer differs.
- My error: the round 4 "credit" column showed `sub_8357F9A0` r7, which is the hitter object
  (both sessions); round 3's strength came from `sub_83589B38`. Fixed (hooked) in round 5.
- The round 4 reaction-pointer dump never appeared (cause not found); the static read made it
  unnecessary, so round 5 drops the dumps.
- Player dumps: +0x38 == 0x02 on the owner's player only (2 of 34); owner says he switched players
  in the exhibition, so it probably means "controlled right now" (fine for BAP, which is locked).
- Static read of `hit_slices.txt`: see re-findings.md "Hit contact sub_833E0BF8".
- Package: hit contact moved to `rogue_hooks.cpp` (literal sub_833E0BF8 names, forwards to the
  probe, counts BAP hits / knockdowns once ROGUE_OFF_SKATER_CONTROLLER is defined); addresses.h
  (2 CONFIRMED defines, 3 commented candidates); rogue_mod.cpp (hook hits replace the manual entry
  when larger); round 5 hit watch (YOU / G tags, power, strength, credit, fall, YOU counters,
  `sub_834EA6D0` result log); docs.
**Files changed:** `include/rogue/addresses.h`, `src/rogue_hooks.cpp`, `src/rogue_mod.cpp`,
`src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`, `docs/re-findings.md`,
`docs/user-guide.md`, `docs/current-status.md`, `docs/handoff-log.md`.
**Validation:** not compiled by the agent. No new .cpp: incremental build. First build with a
real address defined: if `REX_FUNC` / `REX_EXTERN` complain, the hit contact block in
rogue_hooks.cpp is the place to look (it uses literal names on purpose).
**Proven:** hit contact + knockdown reaction (2 sessions, owner check).
**Hypotheses / unresolved:** +0x38 human flag; `sub_834EA6D0` = human query; *r5 = hit power;
`sub_83589B38` r7 = strength 0..3; skater class (one session).
**Next action:** BUILD-9 + user-guide 4c (owner); then RE-2 decision and first event perk.

### 2026-10-30 (17:45) - Hannah - Round 5: no human flag; claim your pro; round 6 power test
**Objective:** Read round 5 (exhibition + BAP) and the reaction slice; unblock BAP counting.
**Done:**
- The 13:45 package compiled and ran (first build with a defined address).
- Round 5: byte +0x38 never matched the owner's player; it is a reaction state on victims (read
  after the reaction). My round 4 conclusion was wrong. `sub_834EA6D0` = team query.
- Static read of `sub_83429560`: the reaction comes from `sub_833B9028(f1 = power)`, power from
  `sub_833CA500`. Power (58 contacts): knockdowns 0.763-0.962, normal 0.501-0.734 (clean split near 0.75).
- Interim RE-2: the owner claims his pro once per game from the latest contact (panel). With a
  claim the contact hook counts the pro's hits / knockdowns of skaters (goalie class now
  CONFIRMED and defined). Round 6: ME tags, all-player dumps at the claim (to find a real field),
  hooks on the dispatcher and the reaction choice, opt-in power test for ME's hits.
**Files changed:** `include/rogue/rogue_mod.h`, `src/rogue_mod.cpp`, `src/rogue_hooks.cpp`,
`include/rogue/addresses.h`, `src/rogue_panel.cpp`, `src/rogue_probe.cpp`, `include/rogue/probe.h`,
`docs/re-findings.md`, `docs/user-guide.md`, `docs/current-status.md`, `docs/handoff-log.md`.
**Validation:** not compiled by the agent. No new .cpp: incremental build.
**Proven:** goalie / skater class (3 sessions); +0x38 is not a human flag.
**Hypotheses / unresolved:** player objects stay the same through one game (the claim relies on
it); knockdown is chosen from power alone; the power test changes only the claimed player's hits
(thread-local guard inside sub_83429560); sub_833B9028 may have other callers (logged).
**Next action:** BUILD-10 + user-guide 4d (owner); RE-2 dump diff and RE-3 test read (agent).

### 2026-10-30 (18:45) - Hannah - Power decides the hit; first event perks; checkpoint
**Objective:** Read round 6 (BAP game with claim + power test) and the power slices; ship the
first real perks and a checkpoint commit.
**Done:**
- Owner: claim worked; mod 27 hits / 13 knockdowns, knocked down 2x, game hit stat 5; power test
  felt possibly stronger, nothing abnormal.
- Log: the reaction choice table (0.5 / 0.75 thresholds) explains all 1836 decisions; the power
  test changed the pro's outcomes exactly as predicted (table in re-findings.md results log).
- Claim dumps: +0x22C byte 0x00 on the pro vs 0xFF on 20 others (candidate auto marker).
- Package: dispatcher + choice hooks in rogue_hooks.cpp; Freight Train = kHitPowerBoost +15% per
  stack (BAP pro only, replaces its global tunables); Wrecking Ball = normal hit -> knockdown;
  round 7 AUTO marker test; addresses.h (3 CONFIRMED); docs; commit message for the checkpoint.
- Owner design thoughts recorded as DES-3.
**Files changed:** `include/rogue/addresses.h`, `include/rogue/perks.h`, `src/perk_catalog.cpp`,
`src/rogue_hooks.cpp`, `src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`,
`docs/re-findings.md`, `docs/user-guide.md`, `docs/current-status.md`, `docs/handoff-log.md`.
**Validation:** not compiled by the agent. No new .cpp: incremental build.
**Proven:** reaction dispatcher / choice / table; power raising works for one player.
**Hypotheses / unresolved:** +0x22C marker; Wrecking Ball is Legendary and not in the starting
unlock list (test it by adding `unlock=wrecking_ball` to rogue_meta.txt); the hit count includes
hits on goalies; 00-START-HERE.md is stale.
**Next action:** BUILD-11 + user-guide 4e (owner); RE-2 decision, DES-3 options (agent).
