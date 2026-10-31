# User guide (for Cherlyn)

You do the parts that need your PC and the game. Agents do the code and the analysis.
Last updated 2026-10-31 (Shirley).

## 1. Repo setup (done)

Fork: `MegaSloppa/nhl-legacy-recomp`, branch `legacy-rogue`, local path
`T:\git repos\nhl-legacy-recomp`. Agents can read the branch on GitHub but cannot push yet; they
deliver zips of changed files laid out from the repo root. Unzip into
`T:\git repos\nhl-legacy-recomp` and allow overwrite, then review and commit in SourceTree.

## 2. Wiring (done; for reference)

- Root `CMakeLists.txt`: `add_subdirectory(mods/legacy_rogue)` (no `target_link_libraries` line).
- `src/nhllegacy_app.h`: `#include "rogue/rogue_mod.h"` and `rogue::Init(tunable_store_.get());`
  right after `tunable_store_` is created.
- `renderer/core/nhl_overlay.cpp`: `#include "rogue/fx.h"` and `#include "rogue/rogue_mod.h"`.
  In `OnDraw`: `rogue::fx::Draw(); rogue::Tick();` before `if (!visible_) return;`, and
  `rogue::DrawPanel();` inside the menu.

## 3. Build and play

Always use a **fresh Command Prompt** per build (see 7.6). Builds are incremental: only changed
files and files that include a changed header are recompiled, then the exe is relinked.

- Everyday code edits (fastest, build only):
  ```bat
  cd /d "T:\git repos\nhl-legacy-recomp"
  scripts\_build_vkffx_incr.bat > build_log.txt 2>&1
  ```
- After any `CMakeLists.txt` change (e.g. an agent added a new .cpp file), or if unsure:
  ```bat
  scripts\_game_ffx_build.bat > build_log.txt 2>&1
  ```
- Close the game before building (a running exe cannot be replaced).
- If a change seems to have no effect, check the log for
  `Building CXX object ...<file>.cpp.obj`. If missing, re-save the file and rebuild.
Launcher `run_dev.bat` in the repo root (git-ignored):
```bat
@echo off
set NHL_VK_BACKEND=1
set NHL_ROGUE=1
"%~dp0out\build\win-amd64-vk-ffx\nhllegacy.exe" --game_data_root "T:\nhl-legacy-recomp-0.6.0\game\game"
```
Without `NHL_ROGUE=1` the Legacy Rogue section does not appear in the overlay (the overlay
itself always does).

In game:
1. Open the overlay (Guide or F1), open **Legacy Rogue (Be-A-Pro)**.
2. Type a career name, click **Start Legacy Rogue career**, take your free perk.
3. Before a BAP game click **BAP game started**. (Hooks will do this automatically later.)
4. After the game enter G / A / Hits / +/- and won/lost, click **BAP game ended**. Hype and
   Notoriety update and the Perk Shop opens.
5. Optional sounds: WAVs named crit, knockdown, shockwave, perk, tier, milestone (`.wav`) in
   `<exe folder>\rogue\sfx\`.

Saves: `rogue_career.txt`, `rogue_meta.txt` next to the exe. Your real BAP save is never
touched. Delete them to reset.

**Perks today (2026-10-30):** setting perks (Cannon, Sniper Scope, Freight Train, Jet Skates, Iron
Lungs, Captain's Aura, the curses) write the live value since TUN-6; the panel's **Active perk
effects** readout shows it (verified). Only Cannon is confirmed to change gameplay, and every
setting perk is GLOBAL: CPU players get it too. Event perks (crit knockdowns, shockwave) do
nothing yet; hit detection works (RE-1) but the mod cannot tell which player is you until the
round 6 claim (4d) is done in a BAP game; with a claim the mod counts your hits and knockdowns.

**Event perks (2026-10-30):** Freight Train (+15% hit power per stack) and Wrecking Ball (every
normal hit becomes a knockdown) act on YOUR hits only, in a BAP game, after you claim your pro
("That was me"). Wrecking Ball is Legendary and not unlocked at the start; to test it, add the
line `unlock=wrecking_ball` to `rogue_meta.txt` (game closed).

**Smoke test (BUILD-3)** - PASSED 2026-10-29. Kept for re-testing after big changes:
1. Career start: enter a name, click Start Legacy Rogue career. Perk Shop opens with a free pick.
   After you take it, the other offers show a Hype price (not FREE).
2. FX: press Test FX: crit, then close the menu at once. Gold CRITICAL! + flash stays ~1 s.
3. Perk applies: take Cannon (or any non-curse setting perk), press BAP game started, open
   **Active perk effects (stock -> live)**: the perk's settings show `stock -> new value`.
4. Revert: press BAP game ended. The readout is back to grey "(stock)".
5. Saves: `rogue_career.txt` and `rogue_meta.txt` exist in `out\build\win-amd64-vk-ffx\`.
On a crash upload the newest `out\build\win-amd64-vk-ffx\logs\nhllegacy_NNN.log`.

## 4. Help the reverse engineering

RE-0 needs the decompressed game image `default.decompressed.exe`. If you do not have one,
an agent will give you the steps to create it (runtime dump with `NHL_DUMP_IMAGE=1`). Then:
```bat
python mods\legacy_rogue\tools\find_bap_code.py "<path>\default.decompressed.exe" --generated generated\default --out mods\legacy_rogue\docs\bap_hits.txt
```
Upload `bap_hits.txt`. Probe rounds (3 to 5): the agent sends a probe file plus a short
script ("start a BAP game, make 3 big hits 10 s apart, quit"). Build, run with
`set NHL_ROGUE_PROBE=1`, do the script, upload `rogue_probe.txt` from the exe folder.

Static slices: agents may ask for a piece of the generated code. Run from the repo root and
upload the .txt, e.g.
```bat
python mods\legacy_rogue\tools\slice_generated.py func sub_833E0BF8 > slice.txt
```
(`func`, `callers`, `callees`, `callsite NAME --context 40`, `offset 640 --float`).

### 4a. TUN-4 tunable probe (2026-10-29)

Finds where the engine really keeps a setting's value. Read-only; it never changes the game.
1. Make a copy of `run_dev.bat` named `run_probe.bat` and add `set NHL_ROGUE_PROBE=1` under
   `set NHL_ROGUE=1`.
2. Delete any old `rogue_probe.txt` in `out\build\win-amd64-vk-ffx\`.
3. Launch `run_probe.bat`. At the main menu wait ~20 s, open the overlay, open Legacy Rogue,
   click **Snapshot: menu** in the Diagnostics section. Wait until it says Done (up to a minute;
   the game may stutter).
4. Start any match. During play open the overlay and click **Snapshot: in match**. Wait for Done.
5. Still in the match: in Engine Tunables set `gTweakByGameStyleGameSpeedLevel` to 20, then click
   **Snapshot: after edit**. Wait for Done, then reset the setting.
6. Quit and upload `out\build\win-amd64-vk-ffx\rogue_probe.txt`.

### 4b. Perk readout + hit sweep (RE-1 round 3, 2026-10-29) - DONE 2026-10-30

Incremental build is enough. Delete the old `rogue_probe.txt`.
1. **Perk readout** (`run_dev.bat` is fine): start a career and take any setting perk (Cannon,
   Freight Train, Jet Skates ...). In the panel open **Active perk effects (stock -> live)**: all
   grey "(stock)". Click BAP game started: the perk's settings show `stock -> new value`. Click BAP
   game ended: back to grey. Report what you saw.
2. **Sweep** (`run_probe.bat`, any mode): play normally for about a minute with no contact, click
   **Mark log**, then throw checks. Click **Mark log** right after each landed check and note what
   kind it was (light, big, boards, knockdown, hip check, you got hit). Ten or more landed checks
   is ideal; misses do not need a mark.
3. Upload `rogue_probe.txt` and your numbered marks.

### 4c. Hit watch: is YOU really you? (RE-1 / RE-2 round 5, 2026-10-30)

Incremental build is enough (no new files). Delete the old `rogue_probe.txt`, launch
`run_probe.bat`. Every body contact shows a line on screen: `HIT` or `KNOCKDOWN`, then
`hitter -> victim`. **YOU** = the player the game marks as human-controlled (this is what is being
tested), `G....` = a goalie, other players are a 4-digit tag. A knockdown on a goalie shows
"(goalie shrugs)". One collision can show 2-3 lines (one per player who reacts): that is normal.
1. **Exhibition:** play a few minutes and hit with different players. Check: does YOU always
   follow the player you control at that moment (also right after you switch)? Does a hit on you
   show `-> YOU`?
2. **Be-A-Pro game** (if you can): YOU should be your pro every time, and never anyone else.
3. Open the panel now and then: **Hit watch** shows "YOU so far: N hits, N knockdowns". At the
   end of a period or game, compare the hit count with the game's own stats for your player /
   team (box score). Write both numbers down. **Reset counts** starts again.
4. Upload `rogue_probe.txt` and your notes (any wrong YOU, the two hit counts).

### 4d. Claim your pro + power test (RE-2 / RE-3 round 6, 2026-10-30)

Round 5 showed the game has no simple "this is you" byte where we looked, so for now you tell the
mod which player is yours, once per game. Incremental build is enough. Delete the old
`rogue_probe.txt`, launch `run_probe.bat`, start a Be-A-Pro game, click **BAP game started**.
1. **Claim:** deliver a check with your pro, open the panel, click **That was me: I hit** (or,
   right after taking a hit, **That was me: I got hit**). The Manual section then shows "Your pro:
   ...." and the on-screen lines show **ME**. If ME is ever someone else, note it.
2. **Counting:** play on. The Manual line "this game: N hits, N knockdowns" is the mod's own count
   (goalies excluded from knockdowns). Before **BAP game ended**, write it down next to the
   game's own hit stat for your pro. The Hits box is filled from the mod's count if it is larger.
3. **Power test** (Hit watch section): pick **x1.3**, throw 5+ normal checks; then **x2**, 5+
   more; then **off**. Lines marked "(boosted)" had their power raised. Do more of your hits
   become KNOCKDOWNs? Anything odd (players flying, freezes)? The test only touches YOUR hits.
4. Upload `rogue_probe.txt` and your notes (where ME was wrong, the two hit counts, what the
   power test did).

### 4e. AUTO marker + first perks (RE-2 round 7, 2026-10-30) - exhibition part DONE 2026-10-31; BAP part moved to 4f

Incremental build is enough. Delete the old `rogue_probe.txt`, launch `run_probe.bat`.
1. **Exhibition** (no claim possible there): hit with different players and switch often. On
   screen, **AUTO** should always be the player you control at that moment, and nobody else.
2. **BAP game** with Freight Train owned: click **BAP game started**, claim your pro (4d). The tag
   should read **ME*** (claim and AUTO agree). The panel line "AUTO marker vs claim" should only
   grow in "agree". Play normally; your checks should floor people a bit more often (Freight
   Train). Power test stays **off**.
3. Optional: Wrecking Ball (see Event perks above): every hit you land should be a KNOCKDOWN.
4. Upload `rogue_probe.txt` and your notes (any wrong AUTO, the counters, how the perk felt).

### 4f. Marker in Be-A-Pro (RE-2 round 8, 2026-10-31)

Incremental build is enough (no new files). Delete the old `rogue_probe.txt`, launch
`run_probe.bat`. No exhibition this time: there you switch players, so the marker moves on purpose.
1. Start a BAP game. Once it has loaded, click **BAP game started** (it now also clears the
   probe's player list).
2. Claim your pro (4d). Your lines should read **ME*** (claim and marker agree). **AUTO** on
   anyone else: note roughly when (game clock) and what was happening.
3. Play at least one full period: bench shifts, whistles / faceoffs and an intermission.
   Freight Train owned is fine; power test **off**.
4. Hit watch panel, line **Marker scan**: ideally "several" stays 0 and "one (ME)" is close to
   "one". At each period end write the line down, and note whether "none" grows while you sit on
   the bench.
5. Optional but useful: a second BAP game (repeat 1-4). Two clean games and the claim becomes
   automatic.
6. Upload `rogue_probe.txt` and your notes.

## 5. Starting a fresh agent session

Paste:

> You are continuing work on Legacy Rogue: repo MegaSloppa/nhl-legacy-recomp, branch
> legacy-rogue, folder mods/legacy_rogue. Read mods/legacy_rogue/AGENTS.md and follow it. Then do
> task <ID> from docs/current-status.md. Attached: <files>. Before finishing, update
> current-status.md and handoff-log.md and give me a zip of only the changed files, laid out
> from the repo root.

## 6. Decisions waiting on you

- DES-1: win/loss structure. Options in `design.md` section 6 (recommended: A + B).
- PARENT-1: how to tell the parent project that its Engine Tunables editor writes type tags
  (issue or PR; an agent can draft it).

## 7. Troubleshooting: every setup problem hit so far

Prerequisites that worked: VS Build Tools 2026 (C++ workload), standalone LLVM
(`winget install LLVM.LLVM`, clang 20+), CMake + Ninja (`winget install Kitware.CMake
Ninja-build.Ninja`), LunarG Vulkan SDK, PowerShell 7 (`winget install Microsoft.PowerShell`),
Git, Python 3. Bootstrap order: `BOOTSTRAP.md` in the repo root.

1. **`setup_sdk.ps1`: "reference is not a tree: bd9b519..."** The pinned SDK commit is not on
   a branch, so clone skips it. Fix:
   `git -C third_party\rexglue-sdk fetch origin bd9b51918faa71f962fa38af6d5f744af9aba2ce`,
   then re-run the script.
2. **`setup_sdk.ps1` dies at `git apply --check` (NativeCommandError).** Windows PowerShell 5.1
   treats git stderr as fatal. Run with PowerShell 7: `pwsh -ExecutionPolicy Bypass -File
   scripts\setup_sdk.ps1`.
3. **"patch does not apply" / "patch failed: cmake/rexglue_fidelityfx.cmake:103".** Line endings.
   SDK clone: `git -C third_party\rexglue-sdk config core.autocrlf false`,
   `git -C third_party\rexglue-sdk rm -r -q --cached .`, `git -C third_party\rexglue-sdk reset
   --hard`. Patch files: convert `sdk\*.patch` to LF (PowerShell:
   `[IO.File]::WriteAllText($f, ([IO.File]::ReadAllText($f) -replace "`r`n","`n"))`). Optional
   global fix: `git config --global core.autocrlf false`.
4. **"vcvars64.bat not found" with VS 2026.** `_env.bat` only searches VS 2022 folders. Find the
   file: `"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest
   -products * -find VC\Auxiliary\Build\vcvars64.bat`, then `set "VCVARS64=<path>"` (quotes around
   the whole assignment) or `setx VCVARS64 "<path>"` (quotes around the path only).
5. **"The filename, directory name, or volume label syntax is incorrect."** `VCVARS64` contains
   quote marks. Check with `echo [%VCVARS64%]`; set it again as in 4.
6. **"The input line is too long."** Every script re-runs vcvars and grows PATH. Open a new
   Command Prompt (re-set `VCVARS64` if you did not use `setx`).
7. **`_ffx_dll_build.bat`: "Generator Visual Studio 17 2022 could not find any instance".** Change
   it to `-G "Visual Studio 18 2026"` (needs CMake 4.2+), delete `%USERPROFILE%\_ffxbuild\vk`,
   re-run. Alternative: install VS 2022 Build Tools alongside. (Local change; do not upstream.)
8. **Codegen:** edit `nhllegacy_manifest.toml` `game_root` / `file_path` to your vanilla xex
   (forward slashes). Do not commit it. Then
   `third_party\rexglue-sdk\out\install\win-amd64-ffx\bin\rexglue.exe codegen nhllegacy_manifest.toml`.
9. **Game: "amd_fidelityfx_vk.dll" missing.** `copy /y
   third_party\rexglue-sdk\out\install\win-amd64-ffx\bin\amd_fidelityfx_vk.dll
   out\build\win-amd64-vk-ffx\` (or from `third_party\rexglue-sdk\out\ffx-prebuilt\vk\`).
10. **Game shows black window ~5 s then closes; log says "--game_data_root was not provided".**
    Pass `--game_data_root` (the folder with `default.xex` and `_compiled`), or link it once:
    `mklink /J "out\build\win-amd64-vk-ffx\game" "T:\nhl-legacy-recomp-0.6.0\game\game"`.
    Logs: `out\build\win-amd64-vk-ffx\logs\nhllegacy_NNN.log` (newest = highest number).
11. **Mod build: `imgui.h` / `rex/filesystem.h` / `rex/ppc.h` not found.** The mod must compile
    into the `nhllegacy` target (`target_sources`), not as its own library. Fixed in 0f1a312.
12. **"no member named 'FontSize' in 'ImFont'".** Newer ImGui: use `ImGui::GetFontSize()`.
13. **"no member named 'Tick' in namespace 'rogue'".** Include `rogue/rogue_mod.h` where it is used.
14. **SourceTree: "LF will be replaced by CRLF" warnings.** Harmless for mod files. But never let
    `.bat` / `.ps1` files under `scripts\` change line endings. To restore one:
    `git checkout 9d2ed9b -- scripts/<file>` (or `git fetch origin` then
    `git checkout origin/master -- scripts/<file>`; there is no local `master` branch).
