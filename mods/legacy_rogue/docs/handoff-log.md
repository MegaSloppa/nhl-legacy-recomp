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

### 2026-10-31 - Shirley - RE-2 round 7 result + round 8 marker scan
**Objective:** Read the round 7 log (owner: exhibition only, no AUTO seen) and prepare round 8.
**Done:**
- Round 7: AUTO never showed because the probe tested +0x22C == 0x00. At the claim only the
  controlled player had a non-0xFF value (0x06); in contacts 0x06 moved between the owner's
  skaters as he switched; CPU team and goalies always 0xFF. Revised hypothesis: != 0xFF = the
  player a human controls (0x00 BAP, 0x06 exhibition). Power vs reaction table held again.
- Round 8: AUTO = != 0xFF; once-a-second marker scan over all known players (`marker` /
  `marker-sum` log lines, panel "Marker scan" counters); `ResetPlayers()` on BAP game started;
  player table 64 -> 128.
- BUILD-11 marked DONE (its content is commit 0290eb7).
- Tooling: the GitHub connector returns file reads without content (commit diffs do come
  through); this session edited the owner's local repo directly through a tunnel and built there.
**Files changed:** `src/rogue_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`,
`docs/current-status.md`, `docs/re-findings.md`, `docs/user-guide.md` (4f), `docs/handoff-log.md`.
**Validation:** `scripts\_build_vkffx_incr.bat` run by the agent on the owner's machine: PASS
(rogue_panel.cpp + rogue_probe.cpp rebuilt, nhllegacy.exe linked; only the existing fopen/getenv
deprecation warnings). Not run in game.
**Proven:** round 7 facts above (owner log; the file holds round 6 BAP + round 7 exhibition).
**Hypotheses / unresolved:** marker on the bench / at whistles / in intermissions. IsBapSkater's
ROGUE_OFF_SKATER_CONTROLLER path tests equality with ROGUE_HUMAN_CONTROLLER_VALUE; switch it to
"!= 0xFF" when RE-2 is confirmed. 00-START-HERE.md still stale. Parent scripts
`_build_vkffx_incr.bat` / `_c5renderdoc.ps1` show line-ending-only changes in the working tree
(not touched by the agent; restore with `git checkout -- scripts/<file>`).
**Next action:** user-guide 4f (owner); RE-2 decision (agent).

### 2026-11-02 - Amy - Static map of the generated code (index + 4 slice rounds)
**Objective:** Map useful functions statically while the owner does no game runs; reach the code
through uploads only (the GitHub connector returns no file content; no tunnel used).
**Done:**
- Wrote `tools/index_generated.py`; owner ran it on generated/default + an `NHL_DUMP_IMAGE` dump:
  129,386 functions, 49,163 referenced strings.
- Mapped 12,211 tweak objects to their addresses (12/12 known ones reproduced) and their static
  readers; mapped the GameEvent system (198 types, raisers, type slots).
- Slice rounds S1-S4: event post pattern, BAP player cache 0x83C876D4, game state machine + names
  (read from the image), goal / period end / stoppage / puck contact functions, player tables.
- Wrote `tools/read_image.py` and `docs/static/` (static-map.md, tunable_readers.tsv, game_events.tsv).
**Files changed:** `tools/index_generated.py`, `tools/read_image.py` (new),
`docs/static/static-map.md`, `docs/static/tunable_readers.tsv`, `docs/static/game_events.tsv` (new),
`docs/current-status.md`, `docs/handoff-log.md`, `docs/00-START-HERE.md` (file map lines).
**Validation:** tools tested on synthetic inputs in the agent sandbox; the owner ran both on the
real tree (index and table reads came back as expected). No C++ changed, no build needed.
**Proven:** tunable name -> N mapping matches all 12 addresses already in the repo; state names and
reaction names are literal image strings.
**Hypotheses / unresolved:** everything else in static-map.md is CANDIDATE (static only). The event
post function address is runtime-only. 0x83C1D278 vs 0x83C1D080 player tables: relation unknown.
**Next action:** RE-10 penalty slices (owner), then RE-11 player object / physics.

### 2026-11-02 (19:45) - Amy - Penalties, player object, ratings; probe round S1 written
**Objective:** Continue the static map (owner: no game runs this session) and leave the next probe
round ready.
**Done:**
- RE-10 static: decide `sub_83487DC0` (rec +4 offender, +8 victim, +12 type; random(1000) vs chance
  from `sub_834859E0` x `sub_83476288`), assess `sub_83487CC0`, apply `sub_83485BD0` (every penalty).
- RE-11 static: skater object 1968 bytes (`sub_834006B8` / `sub_83511CC8`), energy +360, injury
  pointer +368, raw ratings +1240 + 4 * id, effective rating `sub_83501A68` (curve table 36..99,
  difficulty column, wrong-stick multipliers), rating id map (section 13), injury classes.
- Fall trigger: three leads rejected (score records / pose code); still open.
- `src/rogue_static_probe.cpp` (PROBE-S1): read-only pass-through hooks on the state switch, goal,
  period end, puck contact, penalty decide / apply and the event manager getter, plus a 1 s sampler
  of the BAP player cache, state, period, ratings, energy, and a one-time GameEvent slot dump.
  Log `rogue_probe_s.txt`. user-guide 4g. AGENTS.md / user-guide section 5: upload workflow.
- static-map.md section 14: method, how to read the probe log, static queue, perk hook catalog.
**Files changed:** `src/rogue_static_probe.cpp` (new), `CMakeLists.txt`, `docs/static/static-map.md`,
`docs/current-status.md`, `docs/handoff-log.md`, `docs/re-findings.md` (static candidates table),
`docs/user-guide.md` (4g, section 5), `docs/00-START-HERE.md`, `AGENTS.md`.
**Validation:** probe syntax-checked with g++ -std=c++20 -Wall -Wextra -Wformat=2 against stub
headers (PASS); NOT compiled against the SDK. No hooked function is overridden elsewhere (grep of
every .cpp/.h in the repo zip).
**Proven:** nothing new at runtime; image strings (state / reaction names) are literal.
**Hypotheses / unresolved:** all static-map.md entries are CANDIDATE; event post address is
runtime-only; force-fall reader unknown; rating ids 0-2 order, 16, 21, 24-28.
**Next action:** BUILD-13 + user-guide 4g / 4f (owner); then log analysis per static-map.md 14.2.

### 2026-11-03 - Kathleen - Probe S1 + round 8 analysed; round 9 package
**Objective:** Read the owner's first S1 / round 8 logs (one full BAP game) and act on them.
**Done:**
- Owner notes (BAP game): 14:09 P1 puck out, whistle, pro to bench; sims to next shift skipped his
  shifts until 8:46; 8:17 offside, faceoff; 4:09 opponent goal; 2:21 pro penalty (infraction 5-10 s
  earlier); opponent goal during sim, then intermission; P2 17:09 pro penalty; P2 11:34 wrist shot
  blocked, blocker had a hit reaction; P2 8:02 pro drew a penalty; P3 5:43 pro penalty; P3 3:30
  teammate goal; P3 ~1:14 pro hopped over a teammate's shot; game end. Lost, recorded as Won.
- Analysed both logs (sandbox scripts): results table in re-findings.md, summary in static-map.md 15.
- Round 9 code: gated auto pro (`ReadAutoBapPlayer`), lifecycle poll in `rogue::Tick` (start on a new
  BAP match, game over at DONE / match gone: revert perks, wait for the result), kill switch
  NHL_ROGUE_AUTO=0, panel "This game" section, penalty immunity test in the S1 decide hook, goal log
  label fixed (goalie).
**Files changed:** `include/rogue/addresses.h` (state, match, BAP flag, BAP player pointer),
`include/rogue/rogue_mod.h`, `src/rogue_mod.cpp`, `src/rogue_hooks.cpp`, `src/rogue_panel.cpp`,
`include/rogue/probe.h`, `src/rogue_static_probe.cpp`, docs (re-findings, static-map, current-status,
user-guide 4h, 00-START-HERE, this file).
**Validation:** g++ 11 -std=c++20 -fsyntax-only -Wall -Wextra on the 4 changed .cpp files against stub
headers: PASS. Lifecycle logic replayed in Python on the S1 state sequence (+ a synthetic 2nd game at the
same match address): one start at PRE_GAME, one game over at DONE, no restart. Not compiled against the SDK.
**Proven:** see re-findings.md results table (one game).
**Hypotheses / unresolved:** exhibition BAP flag value; whether forcing decide to 0 has side effects;
ratings id map; +364 meaning; puck contact codes. The pointer went into addresses.h at OBSERVED (one
game, two independent signals) so round 9 can be its behaviour test; NHL_ROGUE_AUTO=0 falls back.
**Next action:** BUILD-14 + user-guide 4h (owner).

### 2026-11-03 (11:45) - Kathleen - Round 9 passed; round 10 contact floor test
**Objective:** Read round 9 (exhibition + BAP game, no claim) and act on it.
**Done:**
- Owner: exhibition detected as not BAP; after quitting it the state stayed STORYTELLER until the next
  BAP PRE_GAME; BAP game worked without a claim; penalty prevention worked (no calls, only visible
  trips); after the game state GAME_ON / no match. Attribute screens for the pro (3 screenshots).
  Checking feel: contacts often produce no interaction; aiming a check loses speed.
- Logs: auto pro and BAP flag CONFIRMED, immunity CONFIRMED (6 blocked + a normal control call),
  rating id map CONFIRMED, auto start ~9 s early (fixed: waits for the pro pointer), contact stats.
- Round 10 code: start gate; contact floor test (rogue_hooks.cpp reaction choice, opponents only,
  team byte +48); panel radio; log lines `contact-floor`.
**Files changed:** `src/rogue_mod.cpp`, `src/rogue_hooks.cpp`, `src/rogue_panel.cpp`,
`src/rogue_static_probe.cpp`, `include/rogue/probe.h`, `include/rogue/addresses.h` (comments), docs
(re-findings, static-map, current-status, user-guide 4i, this file). Round 9 files are included again.
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra on the 4 .cpp files against stub headers: PASS.
**Proven:** re-findings.md "Round 9 results".
**Hypotheses / unresolved:** whether a floored contact looks natural; the aim slowdown tunables have no
static reader (read through the heap copy?); +364; puck contact codes.
**Next action:** BUILD-15 + user-guide 4i (owner); penalty immunity perk design (owner).

### 2026-11-03 (12:15) - Kathleen - Perk Untouchable; checkpoint commit (rounds 9-11)
**Objective:** Owner: penalty immunity as a Legendary perk, then commit.
**Done:** perk `untouchable` (kPenaltyImmunity, p0 1.0, 1 stack); the decide hook moved to
rogue_hooks.cpp (perk + probe test), probe logs via `probe::PenaltyDecided`; "NO CALL" FX (max 1/s);
design.md section 8; user-guide 4j (unlock line for testing). Contact floor test left for later.
**Files changed:** `include/rogue/perks.h`, `src/perk_catalog.cpp`, `src/rogue_hooks.cpp`,
`src/rogue_static_probe.cpp`, `include/rogue/probe.h`, docs (design, current-status, user-guide, this
file), plus every round 9-10 file (the zip is cumulative).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra against stub headers on rogue_mod, rogue_hooks,
rogue_static_probe, rogue_panel, perk_catalog, run_state: PASS. No function is overridden twice in
the repo (scan of every REX_FUNC). Not compiled against the SDK, not run.
**Proven:** nothing new at runtime.
**Hypotheses / unresolved:** Untouchable untested in game; Legendary unlock path missing (TODO UI).
**Next action:** owner build + commit; Untouchable check in the next BAP game (4j).

Commit message (owner, 2026-11-03):
```
Legacy Rogue: automatic pro + game lifecycle, perk Untouchable (rounds 9-11)

- Find the Be-A-Pro player automatically: game pointer 0x83C876D4, gated on match + BAP flag
  (CONFIRMED over 2 BAP games, 0 in exhibition). Claim buttons stay as an override.
- Start / end the BAP game from the game state machine (PRE_GAME / DONE), polled in rogue::Tick;
  perks revert at game over; result entry waits for "BAP game ended". NHL_ROGUE_AUTO=0 = manual.
- New Legendary perk Untouchable: penalty decision sub_83487DC0 cancelled for the pro (hook moved
  from the S1 probe to rogue_hooks.cpp; "NO CALL" feedback).
- Probe: penalty immunity test, contact floor test, lifecycle notes, goal log fix (r4 = goalie).
- Docs: S1 / round 8 / round 9 results, full rating id map, checking data, user-guide 4h-4j.
```

### 2026-11-03 (13:00) - Kathleen - Slices S9 read; shockwave test (probe round 12)
**Objective:** Owner: can we knock nearby players down / fling them? Read slices_s9.txt and build a test.
**Done:** static read (static-map.md 16, re-findings "Fall path"); positions from the round 8 dumps;
round 12 probe: `MaybeShockwave` / `ForceKnockdown` in rogue_hooks.cpp (dispatcher called from our
dispatcher hook after the original returns, scratch on the guest stack, power forced via the choice
hook, no chain reactions, 1 s cooldown, max 8 targets, opponents on the ice only), Arm button + F8,
velocity hunt thread in the static probe, panel section, fixed duplicate ImGui ids of "off" radios.
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_static_probe.cpp`, `src/rogue_panel.cpp`,
`src/rogue_mod.cpp`, `include/rogue/probe.h`, docs (static-map 16, re-findings, current-status,
user-guide 4k, this file). Zip is cumulative (rounds 9-12).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra on the 4 .cpp files against stub headers: PASS; no
function overridden twice. Not compiled against the SDK, not run.
**Hypotheses / unresolved:** host call of a guest function from a hook (first time in this mod);
whether the forced fall looks like a knockdown or a stumble; fall direction; the velocity field.
Forced knockdowns also count as the pro's knockdowns (live stats). Perk Shockwave stays inert until
the test passes.
**Next action:** BUILD-16 + user-guide 4k (owner).

### 2026-11-03 (13:30) - Kathleen - Round 12 result: forced knockdowns accepted, no visible fall
**Objective:** Read the shockwave test.
**Done:** log analysis (re-findings.md "Round 12 result"): 28 forced dispatches, 20 accepted with the
knockdown descriptor, no visible effect (owner); 8 refused on already-hit players; +0xD0 stale.
**Files changed:** docs only (re-findings, current-status, this file).
**Validation:** owner run (no crash), rogue_probe.txt / rogue_probe_s.txt.
**Hypotheses / unresolved:** the fall is driven by the contact / interaction step or its physics data,
not by the reaction state alone; where the live position is.
**Next action:** slices S10:
```powershell
$t='mods/legacy_rogue/tools/slice_generated.py'; $o='slices_s10.txt'; Remove-Item $o -ErrorAction Ignore
@('func sub_8342B8C8','callers sub_8342B8C8','func sub_8342B290','callers sub_8342B290',
  'callers sub_8342A2A8','func sub_8350FB88','func sub_8350C728','func sub_833DBC50',
  'offset 208 --float --max 60') | % {
  "==== $_" | Out-File -Append -Encoding utf8 $o
  python $t $_.Split(' ') | Out-File -Append -Encoding utf8 $o }
```

### 2026-11-03 (13:50) - Kathleen - Slices S10: contact check; shockwave round 13
**Objective:** find why forced knockdowns do nothing.
**Done:** read slices_s10.txt (static-map.md 17): live position *(player+32)+48; the contact check passes
the dispatcher a contact kind + float and calls `sub_833DBD18` after it. Round 13 probe: learned kind
mode (default) + post step (pass-through override of `sub_833DBD18` so it can be called), kind 0 mode for
comparison, contact-args / forced log lines, live position for the shockwave, velocity hunt on the body
(log every 100 samples).
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_static_probe.cpp`, `src/rogue_panel.cpp`,
`include/rogue/probe.h`, docs (static-map 17, re-findings, current-status, user-guide 4l, this file).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers): PASS; no function overridden twice.
**Hypotheses / unresolved:** whether the kind is what triggers the fall; whether `sub_833DBD18` needs the
real geometry; +40 meaning.
**Next action:** BUILD-17 + user-guide 4l (owner).

### 2026-11-03 (14:05) - Kathleen - Round 13 result; shockwave round 14
**Objective:** read round 13 (first visible shockwave falls, inconsistent).
**Done:** log analysis (re-findings.md "Round 13 result"): learning overwritten by a duplicate kind-0
call, contact kinds, live position OK, velocity at body +0x50/+0x60/+0x70, bench players counted.
Round 14: learn only kind != 0 + applied; default kind 6 / 0.25; on-ice filter (+0xD0 != 0); push test.
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_static_probe.cpp`, `src/rogue_panel.cpp`,
`include/rogue/probe.h`, docs (re-findings, current-status, user-guide 4m, this file).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers): PASS.
**Hypotheses / unresolved:** kind 6 = body check; which velocity copy the physics uses; whether a push
fights the game's own movement.
**Next action:** BUILD-18 + user-guide 4m (owner).

### 2026-11-03 (14:50) - Kathleen - Round 14 result; fall trace; round 15
**Objective:** owner: no shockwave falls with kind 6.
**Done:** matched the fall routine bursts (`call 829380A0`, rogue_probe.txt) with every knockdown in
rounds 13-14: 8/8 natural skater knockdowns fell, 3/14 forced. Cause: the stored power slot (r1+84) was
never forced. Round 15: pass-through hook `sub_833CA500` (forced power during shockwave calls), modes
0 / 1 (default, kind 0 + post) / 2 (learned + post), on-ice |x| < 12.8 m.
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_static_probe.cpp`, `src/rogue_panel.cpp`,
`include/rogue/probe.h`, docs (re-findings, current-status, user-guide 4n, this file).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers): PASS; no duplicate overrides.
**Hypotheses / unresolved:** whether the power slot is the whole story; Freight Train / Wrecking Ball also
only force the choice (fine for real contacts, which fall on their own).
**Next action:** BUILD-19 + user-guide 4n (owner).

### 2026-11-03 (15:05) - Kathleen - Round 15 result; balance test (round 16)
**Objective:** owner: still no shockwave falls (round 15).
**Done:** forced power reached the stored slot (0.900 in reaction + contact lines) but 0 / 14 fall starts
-> the reaction path is ruled out (re-findings "Round 15 result"). Round 16: pass-through override of
`sub_834E9FA8` (balance penalty; logs natural calls), shockwave balance modes (game call with amount 5 /
50 / 500, or write 100 to the balance-loss float), `forced-balance` log lines. Fixed hex slips in
static-map.md 16-17.
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_static_probe.cpp`, `src/rogue_panel.cpp`,
`include/rogue/probe.h`, docs (re-findings, static-map, current-status, user-guide 4o, this file).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers): PASS; no duplicate overrides.
**Hypotheses / unresolved:** whether +368 of *(*(player+80)+64) is the balance the fall code reads.
**Next action:** BUILD-20 + user-guide 4o (owner).

### 2026-11-03 (15:15) - Kathleen - Round 16 result; why round 13 worked; round 17
**Objective:** owner: no falls with balance; why did round 13 drop some players?
**Done:** balance lead rejected (no natural calls of `sub_834E9FA8`, balance 0 everywhere). Round 13's
forced falls = the targets within 2 m (1.2 / 1.6 / 2.0 m), with the stored power left natural; 7 targets
at 2.6 m+ stayed up; round 15's close targets with forced stored power did not fall. Round 17: stored
power forcing optional (default off), balance off by default, 2 m radius option.
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_static_probe.cpp`, `src/rogue_panel.cpp`,
`include/rogue/probe.h`, docs (re-findings, current-status, user-guide 4p, this file).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers): PASS.
**Hypotheses / unresolved:** falls need the game's own player-player physics to be active for the pair.
**Next action:** BUILD-21 + user-guide 4p (owner).

### 2026-11-03 (16:40) - Kathleen - Round 17 result; contact floor confirmed; round 18 physics window
**Objective:** owner: shockwave still nothing (round 17); contact floor 0.10 works, wants 0.05.
**Done:** re-findings "Round 17 result + contact floor". Round 18: contact floor 0.20 / 0.10 / 0.05 / 0.02;
shockwave method "physics window" (default): `ShockwaveWindowTick` (rogue_hooks.cpp, called from Tick)
raises gPlayerPlayerDistanceToEnable (feet) to the radius for 0.4 s through TweakIndex and restores it;
during the window the choice hook (and, ticked, the power source) gives knockdown power to every real
contact pro -> opposing skater. F8 opens the window directly (no contact needed).
**Files changed:** `src/rogue_hooks.cpp`, `src/rogue_mod.cpp`, `include/rogue/rogue_mod.h`,
`src/rogue_static_probe.cpp`, `include/rogue/probe.h`, `src/rogue_panel.cpp`, docs.
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers): PASS.
**Hypotheses / unresolved:** whether the contact check also gates on actual body overlap (then the
window does nothing at range); side effects of a global physics range for 0.4 s.
**Next action:** BUILD-22 + user-guide 4q (owner); contact-floor perk design (owner).

### 2026-11-03 (17:05) - Kathleen - Round 18 result; shockwave parked; contact perks
**Objective:** owner: physics window did nothing; contact floor 0.02 ~ 0.10; wants small contacts to
become full knockdowns.
**Done:** round 18 analysis (re-findings "Round 18 result"): no contacts at range; a promoted 0.23
contact fell. Shockwave parked. Perks Heavy Shoulders (kContactFloor) and Bulldozer
(kContactKnockdown) in perks.h / perk_catalog.cpp; `ContactFloor` in rogue_hooks.cpp applies the perk
floors (probe test still works on top). design.md section 8, user-guide 4r.
**Files changed:** `include/rogue/perks.h`, `src/perk_catalog.cpp`, `src/rogue_hooks.cpp`, docs. Zip is
cumulative (rounds 9-19).
**Validation:** g++ 11 -fsyntax-only -Wall -Wextra (stub headers) on rogue_hooks, perk_catalog,
rogue_panel, run_state: PASS. Not run in game.
**Hypotheses / unresolved:** how Bulldozer looks on 0.02 brushes; Hype inflation from knockdown counts.
**Next action:** BUILD-23 + user-guide 4r, commit (owner).

Commit message (owner, 2026-11-03 17:05):
```
Legacy Rogue: contact perks Heavy Shoulders + Bulldozer; shockwave probe (parked) (rounds 12-19)

- New perks on the reaction choice hook, opposing skaters only: Heavy Shoulders (Rare, 3 stacks, light
  contacts become hits, floor 0.20 / 0.10 / 0.05) and Bulldozer (Legendary, contacts >= 0.02 become
  knockdowns). Built on the contact floor test (owner: works; promoted contacts fall).
- Shockwave probe (Diagnostics, NHL_ROGUE_PROBE=1): own dispatcher calls, power at the source, balance,
  physics-range window. No falls at range after 7 rounds: parked, perk inert.
- Findings: live position *(player+32)+48, velocity body +0x50/+0x60/+0x70, contact check S9/S10,
  contact kinds, gPlayerPlayerDistanceToEnable stock 9 ft. Docs: re-findings, static-map 16-17,
  user-guide 4k-4r.
```

### 2026-11-03 (17:50) - Kathleen - Design direction: one career, league reacts; RE-9 anchors
**Objective:** owner design talk on unlocks and meta.
**Done:** design.md section 0 (owner direction: one career, no Legacy across careers, balance secondary,
the game's own career layer must react: offers, trade interest, contracts, news, awards). Found string
anchors for those systems in docs/franchise_re_strings.txt (docs/static/re9_anchors.txt). New tool
`tools/refs_to.py` (functions referencing addresses, from the 2026-11-02 index).
**Files changed:** `docs/design.md`, `docs/current-status.md`, `docs/static/re9_anchors.txt` (new),
`tools/refs_to.py` (new), this file. No C++ change.
**Validation:** refs_to.py tested in the agent sandbox on a synthetic index only.
**Next action:** owner: `python mods/legacy_rogue/tools/refs_to.py <index_out>/func_index.tsv.gz --file
mods/legacy_rogue/docs/static/re9_anchors.txt > refs_re9.txt`, upload; agent: pick functions, slices.

### 2026-11-03 (18:00) - Kathleen - End of day: pinned, final commit of rounds 9-19
**Objective:** owner: pin everything, prepare the final commit of the day.
**Done:** cumulative package (every file changed since the uploaded repo), 00-START-HERE rule 2 updated
(owner: changing the game's career data is fine), current-status end-of-day section with the pinned
list, baseline note, commit message below.
**Files changed:** see the zip (16 files + tools/refs_to.py + docs/static/re9_anchors.txt).
**Validation:** every changed .cpp syntax-checked with g++ 11 against stub headers during the day (PASS).
Owner builds ran through round 18; the round 19 perks and refs_to.py are not built / run yet.
**Next action:** owner build + commit; next session see current-status "END OF DAY 2026-11-03".

Commit message (owner, 2026-11-03 18:00):
```
Legacy Rogue: auto pro + lifecycle, perks Untouchable / Heavy Shoulders / Bulldozer, shockwave probe (parked), design direction

Rounds 9-19 (2026-11-03):
- Pro found automatically (game pointer 0x83C876D4, gated on match + BAP flag; CONFIRMED 2 games,
  0 in exhibition). BAP games start / end from the game state machine (PRE_GAME / DONE); perks revert
  at game over. NHL_ROGUE_AUTO=0 restores the manual flow.
- Perks: Untouchable (Legendary, penalty decision cancelled for the pro), Heavy Shoulders (Rare,
  3 stacks, light contacts become hits), Bulldozer (Legendary, contacts >= 0.02 become knockdowns).
- Probe: penalty immunity test, contact floor test, shockwave tests (own dispatcher calls, power at the
  source, balance, physics-range window; no falls at range: parked), velocity search, contact args.
- Findings: rating id map, live position *(player+32)+48, velocity body +0x50/+0x60/+0x70, contact
  check S9/S10, contact kinds, game state machine, penalty decide / apply, goal hook r4 = goalie.
- Design: one career is the whole game, the league must react (offers, trades, news, awards);
  changing the game's career data allowed (00-START-HERE rule 2). tools/refs_to.py + RE-9 anchors.
```

