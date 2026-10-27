# User guide (for Cherlyn)

You do the parts that need your PC and the game. Agents do the code and the analysis.

## 1. Put the mod in your repo (once)

The agent could not fork or push from its connected GitHub account, so do this yourself:

```bat
:: on GitHub: open github.com/puckhead73/nhl-legacy-recomp and click Fork
git clone https://github.com/<you>/nhl-legacy-recomp.git
cd nhl-legacy-recomp
git checkout -b legacy-rogue
:: unzip legacy_rogue_scaffold.zip, then move the legacy_rogue folder to mods\legacy_rogue
git add mods/legacy_rogue
git commit -m "Add Legacy Rogue scaffold"
git push -u origin legacy-rogue
```
If you already have a local working tree, just copy the folder to `mods\legacy_rogue` there.

To let an agent push for you later: reconnect GitHub with write access to the fork (contents
read and write). Then an agent can commit doc and code updates to the `legacy-rogue` branch.

## 2. Wiring (4 lines, once)

1. Root `CMakeLists.txt`, after the `nhllegacy` target is defined:
   ```cmake
   add_subdirectory(mods/legacy_rogue)
   target_link_libraries(nhllegacy PRIVATE legacy_rogue)
   ```
2. `src/nhllegacy_app.h`: `#include "rogue/rogue_mod.h"`, then in `OnCreateDialogs` right
   after the tunable store is created: `rogue::Init(<the store pointer>);`
3. `renderer/core/nhl_overlay.cpp`, inside the overlay window draw:
   `rogue::Tick(); rogue::DrawPanel();`
4. Same file, somewhere that runs every frame even when the overlay is closed:
   `#include "rogue/fx.h"` and `rogue::fx::Draw();` (if unsure, put it next to line 3 for now).

Build as usual: `scripts\_game_ffx_build.bat`. If it fails, copy the error lines into a chat
with an agent and say "BUILD-1 errors".

## 3. Play

```bat
set NHL_ROGUE=1
set NHL_VK_BACKEND=1
out\build\win-amd64-vk-ffx\nhllegacy.exe --game_data_root "H:\...\NHL Legacy - Vanilla"
```
1. Open the overlay (Guide button or F1), open **Legacy Rogue (Be-A-Pro)**.
2. Type a career name, click **Start Legacy Rogue career**, take your free perk.
3. Before a BAP game, click **BAP game started**. (Hooks will do this automatically later.)
4. After the game, enter your G / A / Hits / +/- and win or loss, click **BAP game ended**.
   Hype and Notoriety update and the Perk Shop opens.
5. Optional sounds: put WAV files named crit, knockdown, shockwave, perk, tier, milestone
   (`.wav`) in `<exe folder>\rogue\sfx\`.

Save files: `rogue_career.txt` and `rogue_meta.txt` next to the exe. Your real BAP save is
never touched. Delete those two files to reset.

## 4. Help the reverse engineering (makes event perks and auto-tracking work)

RE-0, once:
```bat
python mods\legacy_rogue\tools\find_bap_code.py "H:\Emulators\games\XBOX\default.decompressed.exe" --generated generated\default --out mods\legacy_rogue\docs\bap_hits.txt
```
Upload `bap_hits.txt` to the agent chat.

Probe rounds (3 to 5 of them): the agent sends you a probe file plus a short script, e.g.
"start a BAP game, make 3 big hits 10 seconds apart, then quit". You build with
`NHL_ROGUE_PROBE=1`, do the script, and upload `rogue_probe.txt` from the exe folder.

## 5. Starting a fresh agent session

Paste this:

> You are continuing work on Legacy Rogue (mods/legacy_rogue in my nhl-legacy-recomp fork).
> Read mods/legacy_rogue/AGENTS.md and follow it. Then do task <ID> from
> docs/current-status.md. Attached: <files>. Update current-status.md and handoff-log.md
> before you finish, and give me the updated files.

If the agent cannot push to GitHub, it gives you a zip or the changed files; copy them in and
commit.

## 6. Decisions waiting on you

- DES-1: win/loss structure. Options in `design.md` section 6 (recommended: A + B).
