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

## 10. Proposed probe round (when the owner tests again)

Most of the earlier probe plan is now a read, not a hook:
1. Read 0x83C876D4 once a second in BAP (+ byte 0x83AC5F63, match pointer 0x83C1E89C); compare
   with the claimed pro and +0x22C. If it matches, RE-2 is done without hooks.
2. Hooks on `sub_8331F408` (period end), `sub_83326280` (log r3 reason + flow +52 after), `sub_83480248`
   (goal r3/r4), `sub_83328180` (r3/r4/r5): owner notes events, agent matches times.
3. Optional: log `*(*(sub_82FEC310())+4)` once to get the event post function.
