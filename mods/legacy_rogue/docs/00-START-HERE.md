# 00 - Start here (agent onboarding)

Last reviewed: 2026-10-27 by Brenda (monday.com agent). Treat file/line references as hints;
verify against current source.

## 1. What this is

NHL Legacy (Xbox 360) is statically recompiled to native Windows x64 by the parent project
(ReXGlue SDK, Xenia-derived). Guest PowerPC functions become C++ functions named
`sub_XXXXXXXX` (their guest address) in `generated/default/*.cpp` (not tracked in git).

Legacy Rogue lives at `mods/legacy_rogue/` and adds, to Be-A-Pro (BAP) only:
- **Perks** bought between games with **Hype** (run currency earned from performance).
- **Event perks** with on-screen and audio feedback (crit slapshot knockdowns, etc.).
- **Notoriety**: a heat meter that makes the league react to a superhuman player.
- **Legacy**: permanent meta-progression across careers.
One run == one entire BAP career. Full design: `design.md`.

## 2. The three mechanisms the mod uses

| Mechanism | How | Needs reverse engineering? |
|---|---|---|
| Engine tunables | 12,098 named live settings (`gXxx`) in the guest tweak registry. Parent project indexes them (`src/tunable_runtime.h`, `ITunableStore`). We layer mul/add/set mods via `TunablePatcher` (`include/rogue/tunable_patch.h`). Catalog: `../../docs/tunables_schema.tsv`. | No |
| Function hooks | Strong-override a generated function: `REX_EXTERN(__imp__sub_X); extern "C" REX_FUNC(sub_X) { pre; __imp__sub_X(ctx, base); post; }`. Pattern proven in `../../src/diag_hooks.cpp`. Registers: `ctx.r3..r10` (args), guest memory via `base + va + REX_PHYS_HOST_OFFSET(va)`, big-endian. | Yes - addresses |
| Host-side UI/FX/save | ImGui panel in the existing overlay, ImGui foreground draw list for FX, `PlaySoundW` for sfx, key=value text saves next to the exe. | No |

Key trick: most tunables are global (they buff everyone). Make them per-player by
(a) using the `gTweakByGameStyle*Level` (human) vs `*LevelCPU` split, or
(b) `ScopedTunable` inside a hook that only fires while the BAP player's action resolves.

## 3. Hard rules

1. **Off by default.** Everything is gated on `NHL_ROGUE=1`. Without it the game must behave
   exactly as the parent project ships. Never change parent default behavior.
2. **Never write the game's own save or `nhlng.db`** from the mod without an explicit task
   saying so. Rogue state lives in `rogue_career.txt` / `rogue_meta.txt` next to the exe.
3. **Revert on exit.** All tunable layers are removed at BAP game end (`RevertAll`). Perks
   must never leak into other modes.
4. **No guessed addresses in code.** An address goes into `include/rogue/addresses.h` only
   after it is CONFIRMED in `re-findings.md` (evidence + confidence). Hooks compile only
   when their address is defined.
5. **Tunable names must exist** in `../../docs/tunables_schema.tsv`. Use
   `tools/perk_candidates.py` to search.
6. **Separate proven from hypothesis** in every doc you write. Label hypotheses.
7. **You cannot run the game unless your environment says so.** Many agents on this project
   (e.g. Brenda) have no local build. In that case: write code + probe hooks, hand the user
   exact build/run/upload steps, and record results when they come back.
8. **Update `current-status.md` and `handoff-log.md` before you stop.**

## 4. File map

```
mods/legacy_rogue/
  AGENTS.md, CLAUDE.md          entry points for agents
  CMakeLists.txt                OBJECT lib linked into the nhllegacy target
  include/rogue/
    tunable_patch.h             TunablePatcher (layered mods) + ScopedTunable
    perks.h                     PerkDef, Scope, EventKind
    run_state.h                 career run, Hype, Notoriety, shop, Legacy
    reactions.h                 Notoriety tier -> league reaction tunable mods
    fx.h                        feedback effects queue
    addresses.h                 CONFIRMED guest addresses only (all TODO today)
    rogue_mod.h                 entry points the host app calls
  src/                          implementations; rogue_hooks.cpp = guest hooks
  tools/find_bap_code.py        string/tunable anchor -> function cross-ref
  tools/perk_candidates.py      tunable search by theme
  docs/                         this folder
```

## 5. Doc map

| Doc | Purpose | Update when |
|---|---|---|
| `current-status.md` | Task board (IDs like RE-1, FX-1). Authoritative. | Every session |
| `handoff-log.md` | Append-only session log | Every session |
| `design.md` | Game design + open design questions | Design changes (owner approves) |
| `re-findings.md` | Address ledger with evidence and confidence | Any RE result |
| `user-guide.md` | Human instructions: build, enable, play, run probes, upload | Workflow changes |

## 6. Glossary

BAP = Be-A-Pro. Hype = per-career currency. Notoriety = heat 0-1000, 6 tiers. Legacy =
cross-career meta currency. Perk scope: HumanOnly / Global / BapScoped. Event perk = needs a
hook. Probe = temporary logging hook used to identify a function.
