# Legacy Rogue - design

Owner: Cherlyn. Changes to sections 1-5 need owner approval; record them in handoff-log.md.

## 1. Pillars (from the owner, 2026-10-27)

1. **Power fantasy inside a realistic game.** You become unrealistically strong; the rest of
   the league stays sim-like. The contrast is the fun.
2. **The run is the whole BAP career.** Not 10 games. Perks accumulate over seasons.
3. **The game must react to dominance.** Unmodded BAP lets you top the league by a mile and
   nothing changes. Here, the league, media and your career respond.
4. **More meta progression**, realistic and unrealistic.
5. **Specialties feel special**: crits and other procs get visual and audio feedback.

## 2. Core loop

```
 play BAP game -> earn Hype (scaled by Notoriety tier) -> Notoriety updates
  -> Perk Shop between games (3 offers, buy / reroll / take a curse for Hype)
  -> perks apply next game -> ... -> retire -> Legacy points -> unlocks for next career
```

## 3. Economy (numbers live in src/run_state.cpp; DES-2 balances them)

- **Hype per game**: 10 base + 15/goal + 8/assist + 2/hit + 4/knockdown + 6/crit +
  3 per positive +/- + 20 for a win, times the Notoriety multiplier (1.00 to 1.75).
- **Prices**: Common 40, Rare 90, Epic 180, Legendary 400, times (1 + 0.08 x perks owned).
  Inflation keeps an 82-game season from maxing everything.
- **Curses** pay Hype (half their rarity price) and make the game harder.
- **Rerolls**: 20 Hype, +15 per reroll in the same shop.
- **Free picks**: one at career start, one every 20 games, one extra at season end.

## 4. Notoriety: the league reacts (src/reactions.cpp)

0-1000 heat. Gain = 12 per point + 5 per knockdown + 6 per crit + 25 for a 3+ point game.
Decays 4% per game, so you have to keep dominating to stay hot.

| Tier | Range | League reaction (Tier A, tunables) | Reward |
|---|---|---|---|
| Unknown | 0-99 | none | x1.00 Hype |
| Rising | 100-249 | CPU checks more often | x1.15 |
| Star | 250-449 | CPU aggression up, more fights | x1.30 |
| Phenom | 450-649 | CPU hit power and shot blocking up | x1.45 |
| Menace | 650-849 | sharper CPU goalies, enforcers | x1.60 |
| Legend | 850+ | whole league tuned to stop you | x1.75 |

Tier B reactions (need RE, tracked as RE-9 / RE-2):
- Shadow coverage: a defender assigned to you; targeted hits from the opponent's enforcer.
- Opposing coach line-matches its best defensive pair against your line.
- Media and career: big headlines, contract offers that scale with Notoriety, trade
  requests from contenders, faster line and captaincy promotion, BAP task grades boosted.
- Rivalries: a team you embarrassed gets a grudge flag; next meeting is chippier.

## 5. Meta progression

- **In-career, unrealistic**: perks (shooting, hitting, skating, line boosts, event perks).
- **In-career, realistic** (needs RE-8): perks or milestones that grant real BAP attribute
  points or XP, so the game's own ratings rise too.
- **Across careers**: Legacy points (career score / 20). Unlock rare+ perks into the shop pool,
  starting Hype, an extra starting pick. Unlock UI is TODO.

## 6. Win / loss condition: OPEN (DES-1)

Owner has not decided. Options, which can be combined:

- **A. Legacy score, no fail.** Career ends when you retire; score = points, knockdowns, crits,
  games, peak Notoriety. Compete with your own best. (Implemented as `LegacyScore()`.)
- **B. Season Trials.** Each season the league sets 3 public bets based on your Notoriety
  ("50 goals", "win the Cup", "Hart Trophy"). Meet one: big Hype + free Legendary pick. Miss all:
  a curse is forced on you. Soft fail, keeps a long career tense.
- **C. Hubris.** At Menace+ tiers, a losing streak or injury while hot makes you lose your
  highest-stack perk. Risk scales with how hard you push.
- **D. Bounty board.** Opponents put a bounty on you; every game you avoid being knocked down
  or injured pays it out; getting flattened gives Hype to nobody and drops Notoriety.

Recommendation: A + B now (B needs only season stats, RE-5/RE-6), C as an opt-in hard mode.

## 7. Feedback effects (fx.h)

| Event | Popup | Flash | Sound file |
|---|---|---|---|
| Crit slapshot | CRITICAL! (gold) | gold 0.15 s | crit.wav |
| Knockdown from perk | FLATTENED (red) | red | knockdown.wav |
| Shockwave | SHOCKWAVE (cyan) | cyan | shockwave.wav |
| Perk bought | PERK ACQUIRED | none | perk.wav |
| Notoriety tier up | tier name | magenta | tier.wav |

Later (FX-4/FX-5): game-native crowd roar, horn, camera shake, forced replay of crits,
0.2 s hit-stop.

## 8. Starter perks (src/perk_catalog.cpp)

Cannon, Sniper Scope, Critical Slapshot*, Freight Train, Wrecking Ball*, Shockwave*,
Jet Skates, Iron Lungs, Captain's Aura. Curses: Glass Bones, Goon Squad.
(* = event perk, inert until its hooks are confirmed.)
