# Static map (generated-code index, 2026-11-02)

Author: Amy (monday.com agent). Source: `index_generated.py` over the owner's `generated/default`
(181 files, **129,386 functions**; the NHL 12 reference docs say 103,714, that count is a different
build) + strings read from the owner's `NHL_DUMP_IMAGE` dump. **Everything here is STATIC =
CANDIDATE** in the re-findings.md scale. Nothing goes into `addresses.h` from this file without a probe.
Address reconstruction ignores control flow, so a listed reference is a lead, not proof.

Companion tables in this folder:
- `tunable_readers.tsv`: 12,211 tweak objects mapped name -> N (name-pointer address), category,
  type, and the functions that reference the live value N+32. 6,487 have at least one static reader.
  Mapping method: each tunable has a registration function that references its name string and its
  object (object base = N-8). Checked against all 12 addresses already in the repo (TUN-4/5, gGPFallMin): 12/12.
- `game_events.tsv`: the game's `GameEvent::*` types (198 strings, 175 with a type-id slot), the
  slot address and the functions that raise each event.

## 1. The GameEvent system (biggest lead for event perks)

The game names its events `GameEvent::<Name>`. Pattern (static, consistent over 175 types):
- A tiny static initializer per type (14 lines, no callers, e.g. `sub_829D6B10` for Goal) calls
  `sub_827572A8` (783 callers: HYPOTHESIS string hash / type-id registration) and stores the result
  in a per-type slot in .data (Goal = 0x83BD1780).
- Usually ONE gameplay function references the slot: that is where the event is raised.
- `sub_82FEC310` (25 lines, 196 callers, called by 172 of the 194 raisers) and `sub_82E28EC0`
  (135 lines, 5 indirect calls) are the most shared callees of the raisers. HYPOTHESIS: event
  post / dispatch. If one of them is the post function, ONE probe that logs the type id (r3/r4)
  would show every event with its arguments, and one hook could drive every event perk.

Raisers of the events the mod needs (one each unless listed):

| Event | Type slot | Raiser(s) | Mod use |
|---|---|---|---|
| GoalAwarded / Goal | 0x83B6FAA8 / 0x83BD1780 | `sub_834801A0`, `sub_83480248` (Goal also `sub_833DAD30`) | ROGUE_ADDR_GOAL_SCORED, Hype, kGoalHeal |
| StartPeriod | 0x83BE4F80 | `sub_8339B0C8`, `sub_8341CA80` | period start |
| EndPeriod | 0x83B6FAAC | `sub_8331F408` (only caller: `sub_83326280`) | ROGUE_ADDR_PERIOD_END, perk draft |
| EndIntermissionStartPeriod | 0x83BEF0AC | `sub_8331F150` | |
| GameOver | 0x83BE4F7C | `sub_83326280` (905 lines, 44 callees; also calls the EndPeriod raiser) | ROGUE_ADDR_BAP_GAME_END |
| PuckHitPlayer / BlockShot | 0x83C22D1C / 0x83BE87DC | `sub_83328180` (both) | ROGUE_ADDR_PUCK_BODY_HIT |
| Check | 0x83BEF09C | `sub_835A7078` (already known as the hit-credit caller), `sub_835A8268` | cross-check of RE-1 |
| BeAPro_ON_ICE + BeAPro_SHIFT_START | 0x83BD17A0 / A8 | `sub_8347D390` | BAP shift tracking; likely carries the BAP player (RE-2) |
| BeAPro_OFF_ICE + BeAPro_SHIFT_END | 0x83BD17A4 / AC | `sub_83483610` | |
| Penalty / PenaltyCall | 0x83BEF12C / 0x83BD1790 | `sub_83485BD0` (+`sub_83481520`) | penalty immunity perk |
| Whistle | 0x83BD1820 | `sub_83316038` | |
| Shot / Save | 0x83BE8834 / 0x83BE8830 | `sub_833ECF68`, `sub_83504FC0` / `sub_83322D70` | shot perks |
| Injury / ShotBlockInjury | 0x83BD180C / 0x83BD1814 | `sub_833C1720` / `sub_833C18E8` | |
| FightStart | 0x83BEF0D0 | `sub_833DF850` | |
| Intimidation | 0x83C84910 | `sub_833F48F8` | Notoriety |
| IntentToHitStart / BraceForHit | 0x83CB23C4 / - | `sub_83594BD0` / `sub_835D1388` | |
| Takeaway / Turnover | 0x83C98D64 / 0x83BEF168 | `sub_83472AE8` / `sub_8337DAA0`, `sub_833EC4A8` | |
| FaceoffResult | 0x83BEF0B4 | `sub_833BDD10` | |

## 2. Be-A-Pro scoring (ProfileTracker)

- `sub_8347B820` reads 43 `gProfile*Pts` tunables (assist, bad pass, block shot, ...); only caller
  `sub_83405F48`. HYPOTHESIS: the BAP per-event score. The `gEvtValPE_*` tunables (one per PE_ event)
  are the event values.
- The `AI/ProfileTracker/UserStatCount/PE_*` strings are referenced by blocks of 5 functions each in
  `sub_83766940`-`sub_83773EC8`, laid out like static registrations. HYPOTHESIS: if RE-1 round 2
  hooked functions like these, that explains why its PE_* handlers never fired (they run once at
  boot, or not at all). Check which addresses round 2 used.
- Save groups: `sub_8298EC10` names the BeAPro save chunks (BAP task, accomplishment,
  ProfileTracker, BAP logic). Do not write them (rule 2); reading them could locate BAP state.
- `gBeAPro_*` / `gBAPTasks_*` readers: see `tunable_readers.tsv`, category `Beapro` (218 with readers).

## 3. Hit / fall pipeline additions

| Function | Static evidence | Note |
|---|---|---|
| `sub_833CA500` | reads gGPStumbleMin, gGlobalBalanceScale, gBoardsCheckingEffectiveRating | CONFIRMED power source (RE-1); readers agree |
| `sub_833B9AE8` | reads gGPFallMin + gGPStumbleMin (83 lines) | HYPOTHESIS: fall/stumble classification |
| `sub_83588BF8` | reads 26 Checking tunables incl. gHitTune_GlobalScale, gHitTune_HitteeBalance* | HYPOTHESIS: hit tuning (TUN-5 saw no effect from gHitTune_GlobalScale) |
| `sub_833B90E0` | reads 36 Physics tunables (gDirectHit*, gGPPhysicalPreparedness*) | HYPOTHESIS: hit physics |
| `sub_8331B8F0` | only reader of gKnockDownValue (+21 PuckControl tunables) | puck control on contact |
| `sub_833C11D0` | only reader of gInjuryShotBlockPuckSpeedThreshold | shot-block injury |
| `sub_83584B50` | only reader of gShotBlockReactionPuckSpeedThreshold | shot-block reaction |
| `sub_83589B38` | reads gBigHitIntimidation(+Reduction) | already known (credit pair): hit -> intimidation |

gTriggerFalls, gTriggerStumbleFall, gBalanceOffpuckFallThreshold and gBalanceCarrierFallThreshold have
no static N+32 reader. The skating snapshot `sub_828CC300` copies such values, so they are probably
read through the snapshot struct (`skating_params_struct.txt`).

## 4. Biggest tunable consumers (good "subsystem core" anchors)

`sub_828B5BF0` (44 Physics: collision depths), `sub_828B8520` (43 Defense/Misc: backskate),
`sub_828B9258` (40 Misc: speed boost), `sub_83503010` (39 Puck: puck control), `sub_8341FA70`
(37 Passing: bad-pass chance), `sub_8352DA88` (35 Analysis/Beapro: trait biases), `sub_83313118`
(34 Goalie), `sub_833E7398` (33 Shooting, CPU), `sub_8337E028` (33 Strat: forecheck),
`sub_82A3B770` (25 gDefSkater* defaults: player-rating defaults).

## 5. Module anchors (functions referencing a source-path string)

| Source file | Referencing functions | Address span |
|---|---|---|
| `FaceoffDrill.cpp` | `sub_83520258` | 83520258-83520258 |
| `GmModeData.cpp` | `sub_82B7A8D0` | 82B7A8D0-82B7A8D0 |
| `InjuryBrain.cpp` | `sub_82B8AE70`, `sub_82B8EA38` | 82B8AE70-82B8EA38 |
| `InjuryGen.cpp` | `sub_82B7BE58` | 82B7BE58-82B7BE58 |
| `NameGenerator.cpp` | `sub_82B8BEE0` | 82B8BEE0-82B8BEE0 |
| `PracticeMode.cpp` | `sub_83482830` | 83482830-83482830 |
| `ResourceKernel.cpp` | `sub_82A57670` | 82A57670-82A57670 |
| `ai_math.cpp` | `sub_833FCFB0`, `sub_83404058` | 833FCFB0-83404058 |
| `aistruct.cpp` | `sub_833FBB78`, `sub_833FBBF0` | 833FBB78-833FBBF0 |
| `animplayer.cpp` | `sub_8334DC50` | 8334DC50-8334DC50 |
| `bench.cpp` | `sub_83583110`, `sub_835831D0`, `sub_835833B8`, `sub_835834E0` +5 | 83583110-835A3020 |
| `ccheck.cpp` | `sub_83584B50` | 83584B50-83584B50 |
| `checkingStateMachine.cpp` | `sub_83596B38`, `sub_83597CA0`, `sub_835A3A80`, `sub_835A4258` | 83596B38-835A4258 |
| `clubsetup.cpp` | `sub_8320F560`, `sub_832116E0`, `sub_832118B8`, `sub_83211940` +3 | 8320F560-832170E0 |
| `dumpanalysis.cpp` | `sub_835CAE10` | 835CAE10-835CAE10 |
| `dynamic_items_interface.cpp` | `sub_835EA7E0`, `sub_835EDF30` | 835EA7E0-835EDF30 |
| `faceoff.cpp` | `sub_833B7798` | 833B7798-833B7798 |
| `fight.cpp` | `sub_833C7BA0`, `sub_833C8F10`, `sub_833D8868`, `sub_833DEF68` +1 | 833C7BA0-833DF850 |
| `goalie_analysis.cpp` | `sub_834AA6E8` | 834AA6E8-834AA6E8 |
| `goaliepose.cpp` | `sub_8348E188`, `sub_834944E0`, `sub_834A3D00`, `sub_834BE7C0` | 8348E188-834BE7C0 |
| `goaliesave.cpp` | `sub_834BC238` | 834BC238-834BC238 |
| `injury.cpp` | `sub_833B9F48`, `sub_833BA020`, `sub_833C09E8`, `sub_833C0BE0` +8 | 833B9F48-833D9AC0 |
| `livefaceoff.cpp` | `sub_833C4000` | 833C4000-833C4000 |
| `loosestickmanager.cpp` | `sub_83410268` | 83410268-83410268 |
| `mimic_utility.cpp` | `sub_834E96D8` | 834E96D8-834E96D8 |
| `oniceofficial.cpp` | `sub_83487DC0` | 83487DC0-83487DC0 |
| `oniceofficial.cpp` | `sub_83487DC0`, `sub_83488450` | 83487DC0-83488450 |
| `pcaiConfidencePolicies.cpp` | `sub_83417B20` | 83417B20-83417B20 |
| `postwhistlebrain.cpp` | `sub_83477438` | 83477438-83477438 |
| `puck.cpp` | `sub_833624F0` | 833624F0-833624F0 |
| `referee.cpp` | `sub_834ECA68`, `sub_83505AC8`, `sub_83511F20` | 834ECA68-83511F20 |
| `rulebook.cpp` | `sub_83486158` | 83486158-83486158 |
| `savespace.cpp` | `sub_8349C068` | 8349C068-8349C068 |
| `tSkaterExecutorPolicies.cpp` | `sub_833A7DD0` | 833A7DD0-833A7DD0 |
| `tface.cpp` | `sub_83385450`, `sub_83394568`, `sub_83394758`, `sub_833A1C58` | 83385450-833A1C58 |
| `toffplay.cpp` | `sub_833A6768`, `sub_833A6AE8`, `sub_833AE120`, `sub_833AE370` +1 | 833A6768-833AE640 |
| `twitch.cpp` | `sub_83396628` | 83396628-83396628 |
| `waterbottle.cpp` | `sub_83363770` | 83363770-83363770 |

## 6. Round S1 static read (slices_s1.txt, 2026-11-02, Amy)

All STATIC. Hex values decoded from the decimal lis/addi pairs.

### How an event is posted (pattern seen in every S1 call site)
```
mgr = sub_82FEC310();                    // getter, NOT the post: g = *(0x83BFE9A4); if g && *(g+4)
                                         //   tail-call sub_82E6F8E0 (7 lines); else return 0
if (mgr) {
  mgr = sub_82FEC310();
  a = sub_827572A8(&slotA, nameA);       // lazy type id: if *slot == 0, *slot = sub_82757200(name);
  b = sub_827572A8(&slotB, nameB);       //   RETURNS THE SLOT ADDRESS (r3 = slot), not the id
  (*(*mgr + 4))(mgr, r4 = b, r5 = a, r6 = payload (stack struct), r7 = payload size, r8 = 0);
}
```
- The post is the manager's virtual slot 1 (vtable +4). Its address is not visible statically.
  A probe that hooks `sub_82FEC310`, reads `*(*(r3) + 4)` once and logs it gives the address;
  one strong override of that function then sees EVERY event: type = r5 slot, payload r6, size r7.
- Usually slotA == slotB. The goal function posts `GameEvent::Goal` with r4 = the
  `GameEvent::StorableGameEvent` slot (0x83C22D0C), so r4 is probably a category / base type.
- A second family exists: `StatEvent::*` (e.g. `StatEvent::Shot` 0x8219F514, slot 0x83C22D48).
- The type id value (from `sub_82757200(name)`) is only known at runtime: dump the 175 slots once
  (addresses in `game_events.tsv`) to decode logs.
- `sub_82E28EC0` is NOT the post: it is an event object constructor (this = r3, u16 type at +12,
  vtable 0x82096160 at +0, +16/+20/+24/+26/+28 filled from virtual getters of the object at
  *(0x83BEF6CC), HYPOTHESIS game clock / period / tick).

### Goal: `sub_83480248` (only caller `sub_83484320`, which also posts PlayerStopPlay)
- r3 = scorer player object (byte +57 = player index, +48 = team side byte, +1648 pointer),
  r4 = assist player object or 0, r5-r10 saved (r9 = r27 compared with 2 and 0: HYPOTHESIS goal
  kind; r7 / r8 bytes are flags that skip the stat event).
- Posts, in order: `GameEvent::Goal` (176-byte payload built by `sub_8348F810(stack, r3..r10)`),
  `StatEvent::Shot`-family event (32 bytes: +0 scorer idx, +4 assist idx or -1, +8 u16 from
  0x83C87508+432, +10 / +11 flags, +12 r31), and `GameEvent::GoalAwarded` (4 bytes) when r9 == 0.
- Consequence: a strong override of `sub_83480248` is a direct ROGUE_ADDR_GOAL_SCORED candidate:
  r3 = scorer, r4 = assister. CANDIDATE until a probe fires on real goals.

### Be-A-Pro on ice: `sub_8347D390` (callers `sub_833B79D8`, `sub_83501718`)
- r3 = BAP controller object (r30; a per-shift byte flag at r3 + 0x10000 - 30896), r4 = flag byte.
- **`r31 = sub_834DC8A8()`**: returns a player object (byte +57 = index, vector at +360). If 0 the
  function exits. Then `GameEvent::BeAPro_ON_ICE` (8 bytes: player index, a float from +360) and
  `GameEvent::BeAPro_SHIFT_START` (4 bytes: player index) are posted for THAT player.
- **RE-2 lead (HYPOTHESIS, strongest so far): `sub_834DC8A8` = "get the Be-A-Pro player".**
  69 lines, 119 callers, references the game-state block 0x83C87508 (+0x1CC = 0x83C876D4) and
  0x83C1E89C. Test: hook it, log the returned pointer, compare with the claim and with +0x22C.
- `sub_834DC690` (42 lines, 3 callers, all BAP) gates the ON_ICE post. `sub_834E7CA0` (task
  assignment) also calls `sub_834DC8A8`.
- Game-state block 0x83C87508: +168, +432 (u16 in the goal stat payload), +452 / +456 written at
  shift start. 603 functions reference this 1 KB block (HYPOTHESIS: the match / game manager).

## 7. Round S2 static read (slices_s2.txt, 2026-11-02, Amy)

All STATIC (CANDIDATE). Globals are fixed .data addresses; objects behind them are heap.

### Global map (new)

| Address | What (static evidence) | Confidence |
|---|---|---|
| 0x83AC5F63 (byte) | **Be-A-Pro mode flag**: every BAP getter (`sub_834DC8A8`, `sub_834DC780`, `sub_834DC690`) returns 0 when it is 0. Same struct 0x83AC5F60: +22, +26 bytes gate game-flow branches | CANDIDATE |
| 0x83C1E89C | pointer to the **match object** (1,179 referencing functions). +192 = player pointer array indexed by player index (puck last-touch lookup `*(match + 4*(idx+48))`), +372, +392 objects | CANDIDATE |
| 0x83C1D220 | **game-flow struct** (r31 in `sub_83326280` / `sub_8331F150`): +12 last stoppage reason, +36 flag word, +40/+44/+48 pending player / team / flag, **+52 game phase** (0 -> GameOver branch, 2, 3 = reset), +60, +68 | CANDIDATE |
| 0x83C1D218 | pointer: +36 posted in EndPeriod (HYPOTHESIS period number), +56 phase (BAP ON_ICE allowed when 0 or 1) | CANDIDATE |
| 0x83C1D26C | team pointers [2] (both passed to the same per-team calls on game reset) | CANDIDATE |
| 0x83C1D278 | player pointers indexed 0..39 (fallback path of `sub_834DC780`) | HYPOTHESIS |
| 0x83C87508 | match-state block: +168 frame / tick counter (compared with player +512 / +4884 to run once per tick), +432 state (255 / 238 / 3 checked at stoppages), +440 byte, +460 = cached BAP player | CANDIDATE |
| **0x83C876D4** | **cached Be-A-Pro player object pointer** (= 0x83C87508 + 460). See below | CANDIDATE (strong) |
| 0x83C85EE0 | puck / play state block: +592 shot-in-flight byte, +660 shot kind (1 / 4 = shot-block reaction path), +756, +612 cleared after a puck contact | HYPOTHESIS |
| 0x83C86230 | 12 entries x 160 bytes: +0 index byte, +3 s8 (-1 = empty), +148 s32 (-1 = empty). Used to find the BAP player (HYPOTHESIS: user / controller slots) | HYPOTHESIS |
| 0x83BFE9A4 | current-context pointer for the event manager; `sub_82FEC708` loops a list, sets it to each node+8 and calls vtable +28 (per-context update). Manager = *(*(ctx+4)+20) | CANDIDATE |

### RE-2: the Be-A-Pro player is cached in a global (`sub_834DC8A8`)
```
match = *(0x83C1E89C); cached = *(0x83C876D4);
result = (match != 0) ? cached : 0;              // subfic/subfe mask idiom
if (result == 0 && BAP_FLAG /*byte 0x83AC5F63*/)
    *(0x83C876D4) = sub_834DC780();              // stores, but returns 0 on that first call
return result;
```
`sub_834DC780` walks the 12-entry table at 0x83C86230 for the first used entry: +148 >= 0 ->
`sub_8299F628(entry+0, entry+148, 0)`; else +3 != -1 -> `sub_8337BFA8(team[entry+0], 0)`; fallback
`players[*(*(*(0x83BFE998)+24)+44)]` (index 0..39). Returns a player object.

Consequence (HYPOTHESIS until one read in a BAP game): **`ROGUE_VA_BAP_PLAYER_PTR` = 0x83C876D4**,
valid while *(0x83C1E89C) != 0 and byte 0x83AC5F63 != 0. The mod could read it every frame with
no hook. Check: in a BAP game, read the word at 0x83C876D4 and compare with the claimed player
(and with the +0x22C marker). Unknown: when the cache is cleared (no static writer other than this
function; probably a block reset of 0x83C87508).

### Game over and period end (`sub_83326280`, the stoppage handler)
- r3 = stoppage reason (r25). Codes seen: 1, 2 (with 0x83C87508+432 == 3: special), 4, 5 (forced
  when `sub_83403848()` is true and reason != 2), 19 (skips everything), 20, 21. Stores reason at
  0x83C1D220+12. Six callers (`sub_833268A8`, `sub_83327170`, `sub_83327258`, `sub_833275C0`,
  `sub_8346DF40`, `sub_83470DF8`).
- Calls `sub_8331DFE8` (WarmupExitConditionsMet) and stores the result in +52; when it changes to 0
  posts an event at slot 0x83BD1808 (payload 4 bytes = 2).
- Unless 0x83C87508+432 is 255 or 238: `sub_8331F408(flag)` = **EndPeriod** (12-byte payload:
  `sub_833FB680()`, *(*(0x83C1D218)+36), flag byte), then `sub_833192F8(*(match+392))`.
- **GameOver**: when game phase (+52) == 0: if `sub_8330D530()` >= 6 sets 0x83C87508+202 and a flag,
  then posts `GameEvent::GameOver` (slot 0x83BE4F7C, 4-byte payload 0).
- Phase 3 path resets the whole flow struct and calls `sub_8331F408(0)` (new game / reset).
- `sub_8330C890(n)` is called with 1, 5, 6, 10 at transitions: HYPOTHESIS set game-state-machine
  state (10 = start of period, after intermission).
- Consequence: ROGUE_ADDR_PERIOD_END candidate = `sub_8331F408` (one caller, so a strong override
  sees every period end; r3 = flag). ROGUE_ADDR_BAP_GAME_END candidate = the GameOver branch of
  `sub_83326280` (hook it, test +52 == 0 after the call) or the GameOver post itself.

### Period start (`sub_8331F150`, only caller `sub_83328930`)
- Posts `EndIntermissionStartPeriod` (slot 0x83BEF0AC) or, on the shootout branch,
  `StartOfShootoutInRegularGame` (0x83BEF154); then (unless byte 0x83AC5F60+26) sets flow +4 = +8 = 2,
  +60 = 0, `sub_8330C890(10)`. If flow +40 != -1 and +48 != 0 it posts a 40-byte event with a player
  index from `sub_833848D0(team[+44])` (type 79).
- Consequence: a perk-draft / intermission hook point. CANDIDATE.

### Puck hits player (`sub_83328180`, only caller `sub_83402AA8`)
- r3 = puck object (+180 = last-touch player index, -1 none), r4 = player hit (r30), r5 = contact
  code (r22; 39 and 41 are a separate branch, HYPOTHESIS stick / skate), r6 = contact info (+52 byte).
- Runs once per tick per player (player +512 vs 0x83C87508+168).
- Shooter = match players[puck+180]. When a shot is in flight (0x83C85EE0+592, kind +660 in {1, 4})
  it calls `sub_83584B50(player, code)` (ccheck.cpp, gShotBlockReactionPuckSpeedThreshold) and posts
  **BlockShot** (44 bytes: +36 victim idx, +38 shooter idx, +40 victim+144 in {8, 14}, +42 victim+144 == 7).
- Always (for player +40 in {0, 1}) posts **PuckHitPlayer** (44 bytes: +36 player object pointer, +40 code).
- Consequence: ROGUE_ADDR_PUCK_BODY_HIT candidate = `sub_83328180` (r3 puck, r4 victim, r5 code;
  shooter from puck+180). Puck speed is not read here; it is inside `sub_83584B50` / `sub_833C11D0`.

### Event manager
The post function is still not visible statically: the manager comes from a runtime list
(`sub_82FEC708`). One read of `*(*(mgr)+4)` at runtime, or the constructor that installs the
manager vtable (not found yet), would give it.

## 8. Round S3 static read (slices_s3.txt, 2026-11-02, Amy)

### Game state machine (`sub_8330C890(n)`)
- Current state = word at **0x83C1D220** (+0 of the game-flow struct; -1 = none). Handler table at
  **0x83AA5B20**, 16 bytes per state: +0 enter, +4 called right after enter (+28 of the flow struct
  is cleared between), +8 exit. Setting a state: old.exit(), store, new.enter(), new.+4().
- States set in code: 0 (`sub_83326018`), 1 (several: after stoppages, `sub_8331F0A8`), 2, 3, 4 (often
  right after a whistle), 5, 6 (game over path of `sub_83326280`), 7, 8, 9 (`sub_83328930`), 10 (period
  start, after a goal in `sub_83326AC8`, after intermission), 12, 13. Names: read the table from the
  image (`tools/read_image.py ... table 0x83AA5B20 14 16`) and map the handler addresses.
- Consequence: one strong override of `sub_8330C890` (r3 = new state, old = word 0x83C1D220) sees every
  game phase change, including game start / end. CANDIDATE hook for BAP_GAME_START / END once the
  state numbers are named.

### Stoppage reasons (callers of `sub_83326280`)
r3 = 9 from most gameplay callers (whistle; `sub_83470DF8`, `sub_83470EA8`, `sub_83479F30` also bump a
counter at +44 of the object at 0x83C952C4 first), 0 (`sub_83482830` = PracticeMode.cpp anchor,
`sub_83483150`, `sub_8351A7D0`), 1 (`sub_8351A2D0`), 24 (`sub_83471130`), -1 (`sub_833268A8`),
pass-through (`sub_83481038`, `sub_83479CE0`, `sub_83512438`). Exact meanings: unknown.

### Period and score
- **0x83C876B8 (= 0x83C87508 + 432) = current period** (`sub_833FB680` returns it; posted in
  EndPeriod and GoalAwarded). 255 and 238 are special values (`sub_833FB840` / `sub_833FBB58` test
  them; 255 leads to the EndShootout raiser `sub_8331F4B8`). HYPOTHESIS: 255 = shootout, 238 = another
  non-period phase. `sub_83403848` = "special phase" (period 238 / 255, or byte 0x83C87508+440).
- `sub_8330D530` compares two team values from the game-setup object *(0x83BFE99C) (+144, +148,
  through `sub_82C360A0`) and returns a result from `sub_82C360E0(+64, side, 4)` or 0 on a tie.
  HYPOTHESIS: score comparison / winner (used on the GameOver path).
- `sub_833FB710` returns the word at 0x83C86A54 (0x83C85EE0 + 2932), compared with a player index
  in the puck-contact code. HYPOTHESIS: puck carrier / last possessor index.

### Player lookup tables (BAP player chain resolved)
- **0x83C1D080: player pointers [2 teams][20 slots]** (`sub_8299F628(team, slot, flag)` returns
  *(0x83C1D080 + 4 * (team * 20 + slot)); team must be 0 / 1 and slot >= 0). Index = team * 20 + slot,
  which matches the player index byte +57 (0-19 / 20-39).
- 0x83C1D278: player pointers by index 0..39 (`sub_833848D0`, `sub_834DC780` fallback). Relation to
  0x83C1D080 unknown (different order or on-ice only?).
- Team object: byte +246 = team id passed to `sub_833C3530` / `sub_833B8AE0`.
- BAP user table entry (0x83C86230, 12 x 160 bytes): +0 = team, +148 = roster slot (so the BAP
  player = players[team][slot]), +3 = alternative team ref.

## 9. Round S4: the game state table (image read + handler slices, 2026-11-02, Amy)

Table 0x83AA5B20, 14 states x 16 bytes: +0 enter, +4 update (also run once inside the state switch),
+8 exit, **+12 name string pointer** (not referenced by code, so not in the index; read with
`read_image.py ... str`). Entries 14-15 are other data (floats 9.0). `sub_833FBEA0` (22 lines) = no-op.
In the table below "-" = no-op. Roles are HYPOTHESES from what each handler does.

| State | Name (image, owner read 2026-11-02) | Enter | Update | Exit | What the handlers do (static) |
|---|---|---|---|---|---|
| 0 | PRE_GAME | 8331E700 | 8330D220 (-> 10) | 8330D420 | match ptr, period checks; goes to STORYTELLER |
| 1 | PRE_FACEOFF | 8331E750 | 8330D278 (-> 2) | 8330D460 | overlay update, BAP flag bytes 0x83AC5F65 / 76 |
| 2 | SYNCHING | 8330D000 | 8330D2C8 (-> 10) | 8330D470 | "SITCOM_Resynch" |
| 3 | FACEOFF | 8331E8A8 | 83315BD8 | 8331ED28 | enter posts **BeAPro_Task_Assigned** + ProgressMessage: BAP tasks are handed out at faceoffs |
| 4 | GAME_ON | 83323B10 | - | - | live play; enter references AnnounceGoal (HYPOTHESIS: announces the previous goal when play resumes) |
| 5 | GOOF_OFF | 8330D098 | 83326AC8 (-> 10) | 8331EDB8 | post-whistle: PostWhistleBehaviorOver, PuckFrozenGoalieTraffic, GoalAwarded raiser `sub_834801A0`; exit posts GoofOffIsOver |
| 6 | PREFIGHT | - | 8330D380 (-> 7) | - | set by `sub_83326280` when the stoppage reason is 19: **reason 19 = fight** |
| 7 | FIGHT | 8330D0C8 | - | - | fight minigame |
| 8 | POSTFIGHT | 833268B0 | 8330D388 (-> 10) | 8331F090 | fight cleanup |
| 9 | DONE | 8331EB98 | 8330D3A8 | - | **game finished**: entered from STORYTELLER (`sub_83328930`). Candidate BAP_GAME_END = DONE enter `sub_8331EB98` |
| 10 | STORYTELLER | 8331EBF8 | 83328930 (-> 9 when the game is over) | 83315EB8 | presentation between phases: update runs `sub_8331F150` (intermission -> period) and `sub_8339B0C8` (StartPeriod) |
| 11 | NISVIEWER | - | 8330D7E0 (-> 1) | 833163C8 | in-game cutscene viewer; leads to PRE_FACEOFF |
| 12 | PRACTICE_GOOFOFF | - | 83327170 (whistle -> 4 / 5) | - | practice mode |
| 13 | TUTORIAL_GOOFOFF | - | 83327258 (whistle -> 5 / 4 / 1) | - | tutorial mode |

Corrections to section 7 (my earlier hypotheses): state 6 is PREFIGHT, not game over; the
`sub_83326280` path that sets 6 is reason 19 (fight). The GameOver event post in `sub_83326280`
(phase +52 == 0) is unaffected. Game flow: PRE_GAME -> STORYTELLER -> PRE_FACEOFF -> SYNCHING ->
(STORYTELLER) -> FACEOFF -> GAME_ON -> whistle -> GOOF_OFF -> STORYTELLER / PRE_FACEOFF ... ->
DONE; fights: PREFIGHT -> FIGHT -> POSTFIGHT. Order is HYPOTHESIS until a probe logs it.

Hook candidates from the state machine (CANDIDATE, one override of `sub_8330C890`, r3 = new state,
old = word 0x83C1D220): BAP game start = first PRE_GAME / FACEOFF with the BAP flag set;
BAP game end = DONE; perk draft = STORYTELLER while the period word 0x83C876B8 advanced;
fight perks = PREFIGHT / FIGHT / POSTFIGHT.

One state switch hook (`sub_8330C890`) therefore covers goal celebration (4), stoppage (5),
game over (6 -> 7) and period start (10). Names first, then a probe only to check order.

Reaction table check (0x83ABF528, initial image values): 5 entries x 28 bytes: +4 threshold is 1.0
in the image (round 6 measured ~0.5 / ~0.75 at runtime, so the game fills it at load), +8 1.0,
+12 1000 or 0.001, +20 id (15, 15, 9, 10, 11), +24 name string pointer. **Names (image):**
0 `incidental`, 1 `nudge`, 2 `stumble`, 3 `stumble fall` (0x83ABF57C, the mod's "normal hit"),
4 `fall` (0x83ABF598, the mod's "knockdown"). re-findings.md should adopt these names.

## 10. Round S5: penalty decisions (RE-10, slices_s5.txt, 2026-11-02, Amy)

All STATIC (CANDIDATE). Tunable names from tunable_readers.tsv (value read at object base + 40 = N+32).

### Call pattern (all 9 contact sites: checks, stick checks, slash/hook, boardplay)
```
official = sub_8346F7E8();
if (sub_83487DC0(official, r4 = &infraction, r5 = &contact))   // DECIDE: roll the dice
    sub_83487CC0(official, &infraction, &contact);           // ASSESS: severity, then call / delay
```
Infraction record (r4): **+4 = offender player**, **+8 = victim player**, +12 = penalty type (0..11).
Contact record (r5): +56 hit kind (3 hip check, 4 cross check), +64 flag word (bit 0x01000000 knockdown
interference, 0x00800000 stumble interference, 0x20000000 into the boards, 0x04000000 charging;
bit 31 picks the "Knockdown" variant of each chance).

### Decide: `sub_83487DC0(official, rec, contact)` -> 1 = call it
1. Skip unless the game is running (*(*(0x83C1D218)+4) != 0); practice / mode gates (`sub_8346F700`,
   `sub_8330C998`) unless the victim is down (victim +368 != 0 and +396 == 1).
2. Grace period after a faceoff: ticks since 0x83C87508+52 < `gFaceOffPenaltyGracePeriod` -> no call.
3. Shot in flight and the puck state owner == offender -> no call. Same team -> no call.
4. chance = `sub_834859E0` (base chance by type) x `sub_83476288` (modifiers: injury, playoffs,
   2nd period, shorthanded, existing delayed penalty, `gDisallowPenaltyForTicksAfterPenalty`).
   Victim is a goalie (+40 == 1) and type < 6 -> fixed chance (constant 0x82013F58).
5. roll = `sub_8330C2C0(1000)` x scale; **roll < chance -> return 1**.
6. Otherwise, if the offender is on team 1 and the roll was close, post `GameEvent::MissedPenaltyCall`.

Base chance by type (`sub_834859E0`): types 0-5 = body contact, summed from the flags:
`gPenaltyChance(Knockdown)Interference`, `...HipCheckStumbleInterference`, `...StumbleInterference`,
`...(Knockdown)HitIntoBoards`, `...(Knockdown)Charging`, `...(Knockdown)Elbowing` (type 5),
`...(Knockdown)CrossChecking` (hit kind 4). Type 6 Poke (`gPenaltyChanceDiveFromPoke` when offender
+144 == 7, else `gPenaltyChancePoke`), 7 Slash, 8 / 11 HighStick, 9 Hook, 10 Hold.
Plus `gPenaltyChancePenaltyShotBonus` when `sub_83481728` says it is a penalty-shot situation.

### Assess: `sub_83487CC0` -> `sub_83486158` (rulebook.cpp: severity flags; reads gMajorChance,
`gDoubleMinorChanceOnHighStick`) -> flag 0x80 = delayed: `sub_83481520(off, offender, victim, ...)`
(PenaltyCall / penalty shot path), else `sub_83485BD0(off, offender, victim, flags, type, contact, 1)`.

### Apply: `sub_83485BD0(official, r4 = offender, r5 = victim, r6 flags, r7 type, r8 contact, r9)`
- 8 callers (the contact path above plus `sub_83488000` goalie restricted zone and other
  non-contact infractions), so it is THE place a penalty is put on a player.
- Gates: game running; `sub_83310DE8(team)`; not a special phase (`sub_83403848`); **bytes
  0x83AC5F76 and 0x83AC5F65 must be 0** (HYPOTHESIS: "penalties off" game settings); the offender is
  not already the pending delayed-penalty player (0x83C85EE0+68 / +84); offender +1104 < 3.
- Writes the delayed-penalty block 0x83C85EE0 (+66 team + 3, +67 type, +108 offender +1648),
  offender +1497 += 1 on some flags, then posts `GameEvent::PenaltyCall` (12 bytes: offender index,
  flags, type).

### Perk hooks (CANDIDATE)
- **Penalty immunity for the BAP pro**: strong override of `sub_83485BD0`; if r4 == the BAP player
  (0x83C876D4 or the claim) return without calling the original. Covers every penalty type.
  Optional: also override `sub_83487DC0` and return 0 when *(r4+4) == BAP player, so the dice are
  never rolled (no MissedPenaltyCall, no delayed-penalty state). `sub_83481520` (delayed path) is
  reached only after the decision, so blocking the decision covers it.
- **Draw more penalties**: in `sub_83487DC0`, when *(r4+8) == BAP player, raise the result (call the
  original, return 1 with a perk chance). Tunable-only alternative is global: all
  `gPenaltyChance*` objects (they apply to everyone).

## 11. Round S6: player object, ratings, energy, balance (RE-11, slices_s6.txt, 2026-11-02, Amy)

All STATIC (CANDIDATE).

### Object and lifetime
- **Skater object = 1968 bytes**: allocated in `sub_834006B8` (roster / game setup, the only caller;
  also writes the player table 0x83C1D080), constructed by `sub_83511CC8(this, r4)`: base constructor
  `sub_835119C8` (shared player base, 2 callers: skater and goalie), then class pointer 0x82271C10
  at +0 and skater-only fields +1856..+1956 (+1872 float = constant 0x82014BEC then overwritten from
  `sub_828A4650`, +1876 = 6, +1880 = 7, +1884 = 6, +1896 / +1900 / +1904 = -1, +1932 float).
  Destructor `sub_83511E60` (deletes when r4 & 1).
- Goalie class 0x8223F648 is written by `sub_8349D728` (a destructor-like teardown, frees 6 members) and
  `sub_834AE678`; goalie fields reach at least +4688.

### Player fields seen so far (all rounds, offsets from the player object)

| Offset | Meaning | Source |
|---|---|---|
| +0 | class pointer (skater 0x82271C10 / goalie 0x8223F648) | CONFIRMED (re-findings) |
| +40 | role: 0 skater, 1 goalie (HYPOTHESIS from penalty / puck code) | S2, S5 |
| +48 | team side byte (0 / 1) | S1, S5 |
| +57 | player index 0..39 | CONFIRMED (re-findings) |
| +60 / +72 / +144 | action / reaction state words (144 == 7 / 8 / 14 / 25 tested in puck and penalty code) | S2, S5 |
| +80 | pointer to a component; its +64 holds the object that carries **balance at +368** (maybe the player itself) | S6 |
| **+360** | **energy (float, current)**: written by energy drop `sub_8350BD80` and recovery `sub_8350B960`; posted in BeAPro_ON_ICE | S6 |
| **+364** | fatigue / energy cap (float), written by recovery | S6 |
| **+368** | **injury pointer** (non-zero = injured): the rating read calls the injury modifier `sub_833C1C10(player, id)` when it is set (S7). +396 == 1 tested with it in the penalty decision | S5, S7 |
| +508, +512 | contact bookkeeping (+512 = last tick processed) | S2 |
| +564 | per-player hit memory sub-object (CONFIRMED use in re-findings) | - |
| +1104 | penalty count this game (< 3 to get another) | S5 |
| +1497 | counter bumped on some penalties | S5 |
| +1648 | pointer (team / line object) | S1, S5 |
| +4768 / +4854 / +4884 | flags / tick fields (puck contact) | S2 |

### Ratings: `sub_83501A68(player, attrId)` -> float (115 callers)
- Calls `sub_83501838(player, id)` (16 callers; reads the roster database through `sub_82C37xxx` /
  `sub_82C38xxx` accessors), then `sub_834F6208`. HYPOTHESIS: raw database rating -> effective rating
  (fatigue / difficulty applied).
- Attribute ids from the formulas that use them (HYPOTHESIS names): **15 = endurance** (both energy
  functions), **12 = aggressiveness**, **18 = strength**, **19 = fighting skill** (`sub_8350C998` = fight
  rating = weighted gFightRatingAggressWeight / StrengthWeight / SkillWeight), **16** = a fight-related
  rating also read raw (gFightRatingEnableFrequency).
- Consequence: **perks past 99 = one strong override of `sub_83501A68`**: when r3 is the BAP player,
  scale or add to f1 for the attribute in r4. Per-player by construction (no global tunables).

### Energy (stamina perks)
- Drop `sub_8350BD80(player)`: gBaseEnergyDrop(_BAP_Authentic), gEnergyDecRateFromAccel(BAP / CPU /
  Human variants), speed boost, turn, high speed, PP / PK multipliers, ratings min / max via attribute 15.
- Recovery `sub_8350B960(player, r4, r5)`: bench / ice idle rates, gBAP_FatigueRecoveryRate, PP / PK.
- Consequence: stamina perk = override the drop function for the BAP player (scale the change to
  +360), or write +360 directly every frame (simplest, no hook).

### Balance
- `sub_834E9FA8(player, kind, f1 amount)`: kind 0 = hit loose stick, 1 = bumped; penalty =
  gBalancePenalty_* x gBalancePenalty_GlobalScale x amount; stores max(current, penalty) into
  *(*(player+80)+64)+368. HYPOTHESIS: +368 OF THAT OBJECT is a balance loss (higher = closer to falling).
  Correction (S7): player +368 is the injury pointer, so the object at *(player+80)+64 is NOT the
  player itself.
- Force-fall lead (RE-3 open item): write a large value to that +368 for a target player, or call
  `sub_834E9FA8(target, 1, big)`. Needs the reader of +368 that triggers the fall.

## 12. Round S7: how ratings are stored and read (slices_s7.txt, 2026-11-02, Amy)

All STATIC (CANDIDATE).

### Raw rating: `sub_83501838(player, id)` -> int
- Normal path: **raw ratings are ints in the player object at +1240 + 4 * id** (load of
  `player + 4 * (id + 310)`). This is the RE-11 target: a perk can write them directly, but see the
  curve limit below.
- Alternate path for skaters when byte 0x83AC5F65 is set: the value comes from a per-team block
  (*(0x83C1D610) + team * 38520, player slot = index % 20) through `sub_82C38xxx` / `sub_82C37xxx`
  accessors, + 36. HYPOTHESIS: HUT / card-based rating source (the same byte gates penalties in
  `sub_83485BD0`, so "penalties off" in section 10 is probably wrong; it is a mode flag).
- Injury: when player +368 != 0 and byte 0x83AC5F6F is set, `sub_833C1C10(player, id)` modifies it.

### Effective rating: `sub_83501A68(player, id)` -> float
1. raw = `sub_83501838(player, id)`.
2. Ids returned linearly (raw x constant 0x8201DBDC, probably 0.01): skaters 16, 21; goalies also 1, 7.
3. All others go through a **curve table**: value = table0x83CBC008[(raw - 36) * 7 + column], column
   = difficulty: `gOverrideAmpLevel` / `gHUTOverrideAmpLevel` override, else *(*(0x83C1D218)+316); or,
   when a bool tweak at 0x83CBD9D6 is set, a per-attribute column from 0x83CBC708 (skater) /
   0x83CBBF90 (goalie). Rows exist for raw 36..99 only.
4. Skaters on the wrong side (`sub_834F6208`, compares +304 of the player and of his line slot):
   x `gWristShotPowWrongStick` (id 4), `gWristShotAccWrongStick` (5), `gSlapShotPowWrongStick` (6),
   `gSlapShotAccWrongStick` (7), `gPassingWrongStick` (8), `gDekeWrongStick` (9),
   `gPuckControlWrongStick` (10).

Attribute ids so far: 4 wrist shot power, 5 wrist shot accuracy, 6 slap shot power, 7 slap shot
accuracy, 8 passing, 9 deking, 10 puck control (all from the wrong-stick table, CANDIDATE),
12 aggressiveness, 15 endurance, 18 strength, 19 fighting skill (HYPOTHESIS, from formulas),
16 / 21 linear (fight frequency / ?), 2 and 25 seen in single callers.

**Perks past 99:** writing raw > 99 at +1240 + 4 * id would index past the 64-row curve table
(out of bounds read). Override the float result of `sub_83501A68` instead (or clamp the raw write
to 99 and scale the float).

### Fall trigger leads (offset 368 float scan)
38 hits, mostly unrelated structs. S8 REJECTED the two best ones: `sub_829523F8` / `sub_82952300` add /
read a value at +368 / +780 of 876-byte records indexed by league and position (ProfileTracker-style
score records), not balance. `sub_8292FD88` is a pose / skeleton routine. The fall trigger is still open.

## 13. Round S8: rating ids and injuries (slices_s8.txt, 2026-11-02, Amy)

All STATIC. The curve table 0x83CBC008 is all zero in the image: it is filled at runtime (so its
values can only be read live).

### Injury effect: `sub_833C1C10(player, id)`
Injury object = player +368 (+372, +384 remaining time > 0, +388 type). By injury class:
**upper body** reduces ids 4, 5, 6, 7, 10, 18 (`gUpperBodyInjuryDrop`), **lower body** reduces ids
0, 1, 2, 3 (`gLowerBodyInjuryDrop`); fades with `gTempInjuryEffectFadeTime`. So ids 0-3 are the
skating ratings.

### Attribute id map
From 158 call sites of `sub_83501A68` with a literal id (43 more pass a variable), the tunables their
callers read, the wrong-stick table and the injury classes. CONFIRMED only where two sources agree.

| Id | Rating | Evidence |
|---|---|---|
| 0, 1, 2 | skating (speed / acceleration / agility, order unknown) | lower-body injury class; hustle / skating callers |
| 3 | balance (skating group) | lower-body class; read by hit power `sub_833CA500` and hit tune `sub_83588BF8` |
| 4 | wrist shot power | wrong-stick table + shot code (two sources) |
| 5 | wrist shot accuracy | wrong-stick table + shot accuracy code |
| 6 | slap shot power | wrong-stick table + shot code |
| 7 | slap shot accuracy | wrong-stick table + one-timer / shot code |
| 8 | passing | wrong-stick table + bad-pass code `sub_8341FA70` |
| 9 | deking | wrong-stick table + deke / puck protection code |
| 10 | puck control | wrong-stick table + gMaximumControl / gMinimumControl readers |
| 11 | faceoffs | faceoff tunables (gFaceoff*) |
| 12 | aggressiveness | post-whistle anger, mimic hit aggro, fight rating weights |
| 13 | body checking | hit power `sub_833CA500` (with 3) |
| 15 | endurance | energy drop / recovery |
| 16 | fight-related, linear (durability or fight frequency?) | fight rating, raw int check |
| 17 | discipline | tripping / elbowing / dive-for-penalty callers |
| 18 | strength | fight rating weight, puck protection, upper-body class |
| 19 | fighting skill | fight rating weight |
| 21 | linear (unknown) | rating read special case |
| 22 | hand-eye (HYPOTHESIS) | puck pickup / deflection callers |
| 23 | shot blocking | shot-block rating / get-up time tunables |
| 24 | ? (skating callers, gBT* tunables) | - |
| 25 | ? (puck pickup / bobble, 11 callers) | - |
| 26 | goalie (gCpuGoalieGeneralScale) | - |
| 27 | ? (backhand / redirect shot accuracy) | - |
| 28 | fight-related (gFightRatingWeight) | - |

Perk use: an override of `sub_83501A68` keyed on (BAP player, id) gives per-rating perks, e.g.
"+30 % shot power" = ids 4 + 6, "iron man" = 15, "unstoppable" = 3 + 18.

## 14. Picking this up next session

### 14.1 Method (upload-only; no GitHub file reads, no tunnel)
1. Owner uploads the repo as `repo.b64.txt` (user-guide section 5). The agent decodes it in its
   sandbox each time (the sandbox does not keep files between calls; re-decode from the asset).
2. Whole-program index: `tools/index_generated.py` (owner runs it, uploads `index.partNN.b64.txt`).
   Rebuild it only after a codegen change; the 2026-11-02 index (129,386 functions) matches the
   current manifest. `tunable_readers.tsv` and `game_events.tsv` here are derived from it.
3. Function bodies: `tools/slice_generated.py func|callers|callees|callsite|offset`, batched in one
   PowerShell loop into `slices_sN.txt` (PowerShell `>>` writes UTF-16; the agent decodes that).
   Use `callsite --context 16` or more: 3 lines are too few to see argument set-up.
4. Static data: `tools/read_image.py <guest_image_dump.bin> words|floats|str|table ...`
   (image from `NHL_DUMP_IMAGE=1`). .data holds INITIAL values only (runtime tables are zero).
5. Decimal constants in generated code: `lis` values are printed as negative decimals
   (e.g. -2084044800 = 0x83C80000). Tweak object value = object base + 40 = N + 32.

### 14.2 Probe round S1 (written, not yet built)
`src/rogue_static_probe.cpp`, run per user-guide 4g, log `rogue_probe_s.txt`. Read it like this:

| Log line | Question | Expected if the static read is right |
|---|---|---|
| `sample ... bapcache=X claim=Y cache==claim:yes` | RE-2: 0x83C876D4 = BAP player? | yes in BAP after the claim; bapcache 0 / bapflag 0 in exhibition |
| `player ... ratings[0..28]=...` | raw ratings at +1240 + 4 * id | plausible 36..99 values; endurance id 15 etc. match the pro's card |
| `state A(..) -> B(..)` | state order | PRE_GAME -> ... FACEOFF -> GAME_ON -> GOOF_OFF ... -> DONE at game end |
| `goal scorer=.. who=..` | goal hook registers | scorer / assist match the owner's notes |
| `period-end` | period end hook | once per period, period number right |
| `puck-contact ... code=` | puck contact registers | victim = player hit; last_touch_idx = shooter |
| `pen-decide result=1` then `pen-apply` | penalty decide / apply | offender / victim match the notes; type matches the call |
| `event-manager ... post(slot1)=` | event post function | an image address 0x82..-0x83..: next round hooks `sub_<that>` |
| `slots begin .. end` | type ids | non-zero ids; decode future event logs |

If a row holds: promote the ledger row in re-findings.md to OBSERVED (CONFIRMED needs the
behaviour test, rule 4) and only then add the define to addresses.h.

### 14.3 Static queue (next agent, in order)
1. **Force fall (RE-3 open part).** Balance-loss value at *(*(player+80)+64)+368 (written by
   `sub_834E9FA8`). Find who READS it and compares with a fall threshold: slices of
   `sub_829380A0` (runs only on real falls), its callers `sub_8292AA40`, and the readers of
   gBalanceOffpuckFallThreshold / gBalanceCarrierFallThreshold through the skating snapshot
   (`skating_params_struct.txt`, `sub_828CC300`).
2. **Rating ids still open:** 0 / 1 / 2 order, 16, 21, 24-28. Slice the single callers listed in
   section 13 (e.g. `sub_83492BB8` for 26, `sub_833E87E0` for 27, `sub_833BE660` for 28).
3. **Game end result (RE-5):** DONE enter `sub_8331EB98` and `sub_8330D530` (winner) +
   `sub_82C360A0` (score getter?) for the final score and the BAP player's stat line.
4. **BAP stat events (RE-6):** ProfileTracker scorer `sub_8347B820` and its caller `sub_83405F48`.
5. **Event post function:** if the probe gives it, slice it to learn the listener dispatch, so one
   hook can feed every event perk.

### 14.4 Perk hook catalog (all CANDIDATE)

| Perk need | Hook / read | Section |
|---|---|---|
| BAP player | read 0x83C876D4 | 7 |
| BAP game start / end, intermission | `sub_8330C890` state switch (PRE_GAME / DONE / STORYTELLER) | 9 |
| Goal | `sub_83480248` (r3 scorer, r4 assister) | 6 |
| Period end | `sub_8331F408` | 7 |
| Puck hits player / shot block | `sub_83328180` | 7 |
| Penalty immunity / draw penalties | `sub_83485BD0` / `sub_83487DC0` | 10 |
| Ratings past 99 | override `sub_83501A68` result | 12, 13 |
| Stamina | write player +360 or scale `sub_8350BD80` | 11 |
| Hit power / knockdowns | `sub_833B9028` (CONFIRMED, already used) | re-findings |
| Force fall | open (14.3 item 1) | 11, 12 |
