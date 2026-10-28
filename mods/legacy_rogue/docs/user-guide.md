# User guide (for Cherlyn)

You do the parts that need your PC and the game. Agents do the code and the analysis.
Last updated 2026-10-28.

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

**Smoke test (BUILD-3)** - report pass/fail for each:
1. Career start: enter a name, click Start Legacy Rogue career. Perk Shop opens with a free pick.
2. FX: press Test FX: crit, then close the menu at once. Gold CRITICAL! + flash stays ~1 s.
3. Perk applies: take Cannon (or any non-curse perk), press BAP game started. In Engine Tunables
   search the perk's setting (Cannon: `gTweakByGameStyleSlapShotPowerLevel`): orange
   (overridden) and 1 above stock. The index needs ~12 s after boot; wait and search again.
4. Revert: press BAP game ended. The setting returns to stock.
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

## 5. Starting a fresh agent session

Paste:

> You are continuing work on Legacy Rogue: repo MegaSloppa/nhl-legacy-recomp, branch
> legacy-rogue, folder mods/legacy_rogue. Read mods/legacy_rogue/AGENTS.md and follow it. Then do
> task <ID> from docs/current-status.md. Attached: <files>. Before finishing, update
> current-status.md and handoff-log.md and give me a zip of only the changed files, laid out
> from the repo root.

## 6. Decisions waiting on you

- DES-1: win/loss structure. Options in `design.md` section 6 (recommended: A + B).

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
