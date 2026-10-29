# Reverse-engineering ledger

One row per guest function / offset / global we rely on. Nothing goes into
`include/rogue/addresses.h` until its row says CONFIRMED.

Confidence: CANDIDATE (from static search) -> OBSERVED (probe fired, args look right) ->
CONFIRMED (behavior reproduced: e.g. forcing it changes the game as predicted, 2+ runs).

## Ledger

| Key (addresses.h) | Address / offset | Confidence | Evidence | Date / agent |
|---|---|---|---|---|
| ROGUE_ADDR_CHECK_RESOLVE | ? | - | - | - |
| ROGUE_VA_BAP_PLAYER_PTR / ROGUE_OFF_SKATER_CONTROLLER | ? | - | - | - |
| ROGUE_ADDR_FORCE_FALL / ROGUE_OFF_SKATER_BALANCE | ? | - | - | - |
| ROGUE_ADDR_PUCK_BODY_HIT | ? | - | - | - |
| ROGUE_ADDR_BAP_GAME_START / END | ? | - | - | - |
| (layout, no address) Tweak object value | name-pointer address N: tag N+8, N+20 (reset value?), min N+24, max N+28, LIVE VALUE N+32 (CONFIRMED TUN-5b), heap copy ptr N+36 (copy: tag +0, name +4, value +24) | CONFIRMED (value N+32) | TUN-4 layout; TUN-8 readers load N+32; TUN-5b: writing N+32 + H+36 changes slap shot power and LOD immediately | 2026-10-29 Victoria |

## Tweak object layout (TUN-4, 2026-10-29)

Source: `rogue_probe.txt` from the owner (3 snapshots: menu, in match, after setting
`gTweakByGameStyleGameSpeedLevel` to 20 in Engine Tunables). N = address of the word that
points at the `gXxx` name string. The parent store's "record_va" is N; its "value_va" is N+8.

Image object (in .data, fixed address, e.g. N = 0x83C8430C for `gSlapShotPowerMultiplier`):

| Offset from N | Meaning | Evidence |
|---|---|---|
| -8 | type-class pointer (one per type) | 0x8234B5F0 on every float, 0x8234B62C on game-style ints, 0x8234B5E4 int, 0x8234B5D8 bool |
| -4 | category / owner string pointer | |
| +0 | name string pointer | |
| +4 | label string pointer ("power multiplier", "Game Speed Level") | |
| **+8** | **type tag**: 4 float, 2 int, 1 bool, 13 game-style table, 12 indexed float slider | Matches the class pointer on all 7 objects; the only word changed by the overlay edit (13 -> 20) |
| +12 | link to the next object | |
| **+20** | **value at stock; TUN-8 shows readers do NOT load it (probably reset/default)** (float/int); bool: byte +20 = value, byte +21 = default; table types: entry count | `gSlapShotPowerMultiplier` 1.0, `gSlapShotPowerScale` 1.0, `gBeAPro_LineEnergyBoost` 0.2, `gLODShader` 1 |
| +24 | min (scalar) / pointer to static value table (table types) | 0.0 / 0 |
| +28 | max | 3.0, 2.0, 1.0, 2 |
| **+32** | **current value read by game code** (TUN-8: base+40 loads in hit, shot, LOD code) | 1.0, 1.0, 0.2, 1 |
| +36 | pointer H to the heap copy (H+0 tag, H+4 name, H+8 label, H+24 value, H+32 max, H+36 default) | Heap copy at 0xBFxxxxxx (mirrored at 0xDF/0xFF and image+0x10000000 views) holds identical values |

Game-style objects (tag 13): +20 = 3 (entries), +24 -> static table, heap copy +28 -> heap table.
Tables: GameSpeed 5,4,3; SlapShotPower 5,4,4; WristShotAccuracy 6,5,3. Which entry is the active
style and the legal range are unknown (TUN-7). Indexed slider (tag 12, `gSlapShotPowerSlider`):
+20 = 7 entries, +24 -> float table 0.85, 0.90, 0.95, 1.03, ...; +32 = 2 (index? unproven).

Other facts from the same run:
- Nothing in these objects changed between the menu and in-match snapshots.
- The registration POOL (category, name, label, default) is a different structure in .data; the
  parent store's sanity value `gDekeRatEffect` = 0.2 is read from the pool, not a live object.
- Consequence: the parent Engine Tunables editor and Legacy Rogue's `TunablePatcher` (before
  TUN-6) write type tags. Writing tag 14 (424 other objects use it) did not crash in testing.

Unproven: which copy consumers read (TUN-5 writes both); whether consumers clamp to min/max;
whether values are cached when a match loads.

## Known facts inherited from the parent project (verified there, not re-verified here)

- Image base 0x82000000, size 0x1EA0000. Search the decompressed image
  (`default.decompressed.exe`, VA = 0x82000000 + file offset), NOT the LZX `default.xex`.
- Generated code loads addresses as DECIMAL `lis`/`addi`/`ori` pairs; hex grep finds nothing.
  `tools/franchise_re/find_franchise_code.py` reconstructs them; our `find_bap_code.py` reuses it.
- Heavy vtable dispatch: static call graphs hit `REX_CALL_INDIRECT_FUNC` walls. Runtime
  probes are the efficient way to confirm behavior.
- CORRECTED 2026-10-29: the parent docs say `record_va+8` is the live value. It is the type tag
  (see the layout above). A hardware write/read watch on N+20 (not N+8) is a way to find consumers.
- Strings of interest: `BAP_PLAYER_GROWTH` @ 0x82035818. BAP play-gen task
  `BAPGenerateOffenseNewTeamTask`. Difficulty ladder names Rookie_/Pro_/AllStar_/Superstar_.
- Be-A-Pro tunables (category `Beapro`, 279 entries) include `gBeAPro_LineSkillBoost`,
  `gBeAPro_LineEnergyBoost`, `gBeAPro_ScoreBonus_*`, `gBAPTasks_*`, sim-out gates.
- Hit-related tunables (category `Checking`, 527 entries): `gHitTune_*`, `gCheckMagnitude*Weight`,
  `gMomentumPhysics*`, `gHitPowerForPhysicsGlobalModifier`. Fall: `gBalanceOffpuckFallThreshold`,
  `gBalanceCarrierFallThreshold`, `gTriggerFalls`, `gTriggerStumbleFall`, `gKnockDownValue`.

## Probe recipe (copy for each candidate)

```cpp
// in a scratch TU (e.g. src/rogue_probes.cpp), env-gated, removed once CONFIRMED
REX_EXTERN(__imp__sub_82XXXXXX);
extern "C" REX_FUNC(sub_82XXXXXX) {
  static const bool on = std::getenv("NHL_ROGUE_PROBE") != nullptr;
  static std::atomic<int> n{0};
  if (on && n.fetch_add(1) < 200) {
    FILE* f = std::fopen("rogue_probe.txt", "a");
    if (f) { std::fprintf(f, "82XXXXXX t=%lu r3=%08X r4=%08X r5=%08X r6=%08X lr=%08llX\n",
        GetTickCount(), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
        (unsigned long long)ctx.lr); std::fclose(f); }
  }
  __imp__sub_82XXXXXX(ctx, base);
}
```
Ask the user to do a scripted action sequence (e.g. "3 big hits, then nothing for 10 s") and
note wall-clock times, so firing counts can be correlated with events.

## Results log

(append newest first: date, agent, what was probed, what fired, conclusion)

- 2026-10-29, Victoria: STATIC read of nhllegacy_recomp.39.cpp. sub_828CC300 = skating settings
  snapshot into a struct at r3 (map: docs/skating_params_struct.txt). Bools are read as the byte
  at object base+29 = N+21. Ragdoll blend-out code: sub_828C75A8 (gBlendOut*ForceScale),
  sub_828C7D50 (gBehaviorPitchUprightThreshold), sub_828CB380 (gHandInfluenceDistance*).

- 2026-10-29, Victoria: RE-1 round 2. Player-event handlers PE_CHECK/CHECKED/DIVING_CHECK/GOAL/
  ASSIST/SHOT_ATTEMPT/GOODSHOT/PLUS/MINUS/WIN_GAME/LOSE_GAME/OT_LOSE_GAME/TIE_GAME/PENALTY/
  FACEOFFWIN + PE_NONE: never fired in an exhibition or a BAP game (hook method verified via
  the function table). Round 3 sweep targets (function, tunables it reads):
  sub_828B9258  gSpeedBoostRatioForMinBalanceLoss
  sub_828CC300  gAccelToProhibitForwardTurns, gBalanceCarrierFallThreshold, gBalanceStableThresholdVariation
  sub_828FEBD8  gEffectiveMinEnergy, gMinEnergyForNoEffectPK
  sub_82929FB0  gAlwaysUpdateBalanceRatio, gBalanceAdjustmentEnd, gBalanceAdjustmentStart
  sub_8292AE50  gHitBasedLengthCoefficient
  sub_8292B550  gCollisionBodyTension
  sub_8292B808  gContactCOMDelta, gDefaultCom
  sub_82936F28  gHitBasedBlacoutCoefficient, gHitBasedLengthCoefficient
  sub_829380A0  gClampDeltaAngle, gDrivenFallRelaxedModifier, gDrivenFallTenseModifier
  sub_8293A148  gCollisionVelocityCompensationValue
  sub_8293A3E8  gBalanceMax, gBalanceReactionMax, gBalanceReactionScale
  sub_8293A998  gStumbleVelocityCompensationValue
  sub_8293AB78  gStumbleBodyTension
  sub_82A3B770  gDefSkaterBalance, gDefSkaterBodyChecking
  sub_82DBEFB0  gBoardHitIntensityScale
  sub_82DBF2A0  gPlayerHitIntensityScale
  sub_83310E68  gHandlePotentialKnockedOverPenaltiesTime, gHandlePotentialNotKnockedOverPenaltiesTime
  sub_8331B8F0  gKnockDownValue
  sub_83328180  gBoardPinchNoCollisionFrames
  sub_83339558  gMinPushAwayScale
  sub_8333F9C0  gBoardCollisionStressForMaxEffect, gBoardCollisionStressForMinEffect, gBoardCollisionStressMaxEffect
  sub_833442E8  gSlowFallenGuysAfterThis
  sub_8334F930  gAugmentHipCheckYVelocity
  sub_833A1F40  gLayDownCollisionRadiusPadInches
  sub_833B90E0  gDirectHitForMax, gDirectHitForMin, gDirectHitValueAtMax
  sub_833B9730  gGPPhysicalPreparednessForShooting
  sub_833B9AE8  gGPFallMax, gGPFallMin, gGPIncidentalMin
  sub_833BF998  gMaxLowHitRatioForEffect, gMinLowHitRatioForEffect, gMinLowHitSize
  sub_833C09E8  gHitInjuryChanceMultiplier
  sub_833CA500  gBoardsCheckingEffectiveRating, gGPStumbleMin, gGlobalBalanceScale
  sub_833E0BF8  gGPFallMin
  sub_833E3D58  gBaseHitComparisonPriority
  sub_833EBD80  gHitsForIntimidatingShift
  sub_833F27E8  gBigHitterMinHitPct
  sub_833F3330  gTeamHitsMinHits
  sub_833F8AC8  gDesperationNoCollisionFrames
  sub_8340C690  gPhysicsRatingsDiffMax, gPhysicsRatingsDiffMin, gPhysicsRatingsWeight
  sub_8340C7E0  gSpecialCaseModifierShooting
  sub_8340DB90  gBodyTensionBase
  sub_83411AD0  gWeightOfRelativeSpeedToTarget, gWeightOfSpeedToTarget
  sub_83412298  gCheckeeDefaultBalance, gCheckerDefaultBalance, gMaxBalance
  sub_83427068  gBodyTensionBase
  sub_83429800  gBothBehaviorBalanceTunerMax, gBothBehaviorBalanceTunerMid, gBothBehaviorBalanceTunerMin
  sub_834299F0  gBalanceInequality, gBodyTensionBase, gBodyTensionCoeff
  sub_83429EC8  gBalanceInequality
  sub_8342A2A8  gBothBehaviorBalanceTunerMax, gBothBehaviorBalanceTunerMid, gBothBehaviorBalanceTunerMin
  sub_8342A938  gDefaultBalanceThreshold, gDefaultContactCOMRatio
  sub_8342B290  gPlayerPlayerDistanceToEnable
  sub_8342B8C8  gPlayerPlayerDistanceToEnable
  sub_8342C628  gHipCheckContactCOMRatio, gHipCheckNoVelocityUpdateFrames, gHipCheckWeightMin
  sub_8342CDD0  gPlayerPlayerDistanceToEnable
  sub_83437DF8  gDefaultBalance, gHipCheckRestartWithLowControlPenalty, gHipCheckRestartWithLowControlTime
  sub_834551C8  gBodyTensionBase
  sub_834553D8  gBodyTensionBase
  sub_83459350  gDegreesForFullLowHipSwing, gHipCheckLowIntentMinimalDistance, gHipCheckLowIntentSpeedProportion
  sub_83465C48  gMaxFatigue, gRightSickFramesForHold
  sub_83467988  gBodyTensionBase
  sub_8346E460  gBodyTensionBase
  sub_83470DF8  gBodyTensionBase
  sub_83470E48  gBodyTensionBase
  sub_83476288  gAITuneAIHittingTunePenInjuryModifier, gAITuneAIHittingTunePenPlayoffsModifier, gAITuneAIHittingTunePenSecondPeriodModifier
  sub_834859E0  gPenaltyChanceCrossChecking, gPenaltyChanceHitIntoBoards, gPenaltyChanceStumbleInterference
  sub_834BF428  gBFFallOverMaxThreatDist, gChanceToFallOver, gSpecialSaveBFFallOverMinSpeed
  sub_834D8580  gPlayPhysical_Hits, gPlayPhysical_WasHit
  sub_834E9FA8  gBalancePenalty_GlobalScale, gBalancePenalty_HitBumped
  sub_834F0A48  gHitterMaxDistToDestEffort, gHitterMaxDistToTarget, gHitterMinDistToTarget
  sub_834F24E0  gManualHipCheckWinHeightFactor, gSpeedBasedHipCheckProportion, gSpeedBasedHipCheckWinBack
  sub_835038B8  gBoardPlayFootCollisionRadius
  sub_83505630  gBodyTensionBase
  sub_8350E600  gBoardCollisionAnticipationTime
  sub_8351AE18  gAutoHitAimingAtTargetDist, gAutoHitEarlyHitFrames, gAutoHitExtraFeetToEarlyHit
  sub_8351B5A0  gAutoHitDefaultCancelActionRefractoryPeriod
  sub_8351B6D0  gMinRelSpeedForAutoCheck, gMinSpeedToCareAboutRelSpeedForAutoCheck
  sub_83522198  gAggressiveCheckButtonTap
  sub_83524F60  gAutoCheckCancelDelay, gHipCheckRefractoryTime
  sub_835334A0  gPairUpAutoHitChance, gPairUpAutoHitChanceSuperstarVictim
  sub_83552AB8  gCheckingContinueAngle
  sub_8357F748  gCheckSpeedModerate, gCheckSpeedSlow
  sub_8357F9A0  gEndorphineHitInjuryTestFrequency
  sub_8357FAC0  gMaxDistForHit
  sub_83580BF8  gHowDoIStepUpGuarding
  sub_83580CE8  gStepUp_Shove_IncludeBackAnglePies
  sub_83580E90  gHowDoIStepUpShove
  sub_83581208  gDeflectionNoCollisionFrames
  sub_83582C90  gBenchSitHitReactXOffset
  sub_835831D0  gBenchHitBoardDist, gBenchHitReactionDist
  sub_835834E0  gBenchHitReactionDist
  sub_835844B0  gHookReactTimeScale, gTripFallFrequency
  sub_83585028  gHitAssistAccelScale, gHitAssistMaxSpeedScale, gHitAssistanceNumFramesPrediction
  sub_835853A8  gAssumedHitterSpeed
  sub_835888E8  gMaxFramesToAllowCheckingLayer
  sub_83588BF8  gBiasedStrengthSizeAvg, gHitTune_GlobalScale, gHitTune_HitteeBalanceForMax
  sub_83588ED8  gMaxCheckIntentForHipCheck
  sub_83589190  gEndorphineBoardHitSizeDiff
  sub_83589570  gBackPieHitSpeedThreshold, gCheckCollisionRadiusMoving, gCheckCollisionRadiusMovingPush
  sub_83589B38  gBigHitIntimidation, gBigHitIntimidationReduction, gDecEnergyForHitScale_1Player_Hittee
  sub_8358A348  gTimeAfterReleaseForHitToCount
  sub_8358D100  gTriggerCancelDist
  sub_8358D310  gMaxDistToDestForHitCancelInMeters
  sub_8358D478  gInterceptTimeForCheckIntentTrigger, gMaxDistToDestForHitInMeters
  sub_8358DBD0  gGuardingActionEffort
  sub_8358DFC0  gIZGuardingActionEffort
  sub_8358EB88  gFinishChecks_MaxTimeBuffer, gFinishChecks_MaxUndisciplinedLateHit, gFinishChecks_MinTimeBuffer
  sub_8358EE30  gExtraShoveDistanceScale, gStepUp_Shove_DistFromBoards
  sub_83593FC8  gElbowLikelihoodScale
  sub_83594968  gDistForOpponentToBeConsideredCheckable, gMinNumFramesBetweenHit
  sub_83594BD0  gDefaultBalance
  sub_83594CB8  gBenchHitMaxCheckeeSpeed, gEndorphineBoardHitMinCheckingRating, gEndorphineHitBoardDist
  sub_83595320  gBoardplayGameEventFrame, gBoardplaySizeDiffCheckExit, gCheckForBoardplayPenaltyFrequency
  sub_83596B38  gPlayerFaceDiffMaxDegrees, gPlayerSizeDiffMax
  sub_83597360  gMomentumMaintainedWin, gMomentumPhysicsCheckingAttributeWeightBoost, gMomentumPhysicsHitterWeightBoost
  sub_83597FF0  gMaxFramesSinceLastTouchForNoPenalty
  sub_83599B68  gCheckingActionAngleToFacing, gCheckingActionEffort, gCheckingActionMaxLookAheadFrames
  sub_8359A1D0  gNearBoardsHighSlotCheckAngle
  sub_8359B378  gHowDoIStepUpPoking
  sub_8359B600  gHowDoIStepUpBoardPin, gStepUp_BoardPin_DefMaxSpeed, gStepUp_BoardPin_MinPlayers
  sub_8359B7C8  gShoveFrequency, gShoveMinScore
  sub_8359BA08  gChecking_NoSwitchTargetDist, gStepUp_SortMethod
  sub_8359BF10  gCheckingHumanPrevCarrierTicks
  sub_8359C3F8  gStepUpPossibleHitMaxDistApart, gStepUpPossibleHitPlayerReach, gStepUpPossibleHitPredictEnd
  sub_8359C700  gChecking_SlowSpeedDecline, gChecking_SlowSpeedDeclineRadiusMax, gChecking_SlowSpeedDeclineRadiusMin
  sub_8359FAF0  gDefaultBalance
  sub_8359FE68  gBracingPenalty, gFacingAngleDifferenceWeight, gIntentWeight
  sub_835A06E8  gDigAnimPctPenaltyCheckEnd, gDigAnimPctPenaltyCheckStart, gDigNoCollisionFrames
  sub_835A1640  gSlashActionEffort
  sub_835A18C0  gCheckingFromBehind_DistReq, gCheckingFromBehind_SpeedDiff, gCheckingScoreTraitFactorNonPhysical
  sub_835A2338  gHowDoIStepUpMinValidScore
  sub_835A3618  gMissedBoardhitDelayFrames
  sub_835A4258  gAllowHitFromBehindBoardDist, gHitTune_ClosingSpeedForMax, gHitTune_ClosingSpeedForMin
  sub_835A47D0  gMinStressForCoincidental
  sub_835A4BE0  gMaxForceDistBoardsDistRatio, gMinHeightForBoarding
  sub_835A5F10  gSlashHighChance
  sub_835A62E8  gChecking_SlowSpeedDeclineProbMax
  sub_835A6CE0  gSmallHitIntimidation
  sub_835A7078  gFinishChecks_MaxTimeBuffer, gInstigationChanceIntimidation, gIntimidationOnInstigationChanceThreshold
  sub_835A8690  gBenchHitResultVel, gBoardplayAngleDiff, gBoardplayAngleOpponentApproachMe
  sub_835A98D0  gMaxFramesToFindCollidingPlayer
  sub_835BD388  gDeviation_FallingRecoveryTime
  sub_835CF688  gMinPushAwayScale
  sub_835E7990  gBoardCollisionLeadTime, gHitLossChancePerFrame, gSkateIntoBoardsCollisionRetentionMultiplier
  sub_835EB920  gNoPlayerCollisionFrames, gPlayerCollisionClosestPlayerDist, gPlayerCollisionImpactDist

- 2026-10-29, Victoria: RE-1 round 1 hooks, 30 min exhibition. NEVER fired: sub_83411E90,
  sub_83411C48, sub_83429698, sub_83589230 (hip-check family + hit power). sub_8342C628: ~385/s,
  lr 0x8342D13C, r3 = ~14 distinct 0xA9xxxxxx values (skater objects, OBSERVED). sub_83588BF8:
  bursts, lr 0x835A44E4 (95%) / 0x8343832C, r3/r4 = two skater objects. Round 2: PE_* handlers.

- 2026-10-29, Victoria: TUN-9 catalog + static readers. 10,583 tweak objects, 7,779 read by code.
  Hip-check code CANDIDATES (by which tunables each function reads):
  sub_83411E90 (file 133): RadiusMax/Min, AngleMin/Max/Tuner, SpeedMin/Max/Tuner, WeightMin/Max/
  Tuner, DistanceTuner, BalanceRatioTotalThreshold, BehaviorBalanceTunerMax -> hip-check scoring.
  sub_83411C48 (133): LookAheadTime, RadiusMin, RadiusMaxest, PositionOffset -> target acquisition.
  sub_83429698 (134): BalanceThreshold, BehaviorMin/Max, BehaviorBalanceTunerMin/Mid/Max,
  ContactCOMRatio -> knockdown decision. sub_8342C628 (134): ContactCOMRatio, WeightMin,
  NoVelocityUpdateFrames -> contact. Hit: sub_83588BF8 (gHitTune_GlobalScale), sub_83589230
  (gHitPowerForPhysicsGlobalModifier), sub_83588ED8 (gMaxCheckIntentForHipCheck). Skating
  sub_828CC300 reads the fall thresholds and many hip-check control values.
  Shot code: sub_833E87E0 (slap power/accuracy scales), sub_833ECF68 (wrist power).

- 2026-10-29, Victoria: TUN-5b write test on N+32 + H+36. gSlapShotPowerScale x0.2 / x10 and
  gLODShader 0 / 2 changed gameplay / rendering immediately; x10 exceeds the object's max (2.0),
  so game code does not clamp. Game-style heap + static tables set to 0 / 20: no visible effect.
  Conclusion: value field N+32 CONFIRMED.

- 2026-10-29, Victoria: TUN-8 static consumer scan. Readers load N+32 (base+40), not N+20, for
  4 objects incl. the gLODShader control; game-style tables are read via heap copy H=*(N+36),
  table=*(H+32) in sub_829F8A20. Revised hypothesis: N+32 = current value, N+20 = reset/default.
  Next: TUN-5b write test on N+32 / tables.

- 2026-10-29, Victoria: TUN-5 write test. Wrote stock x0.2 / x3 / x10 to N+20 and the heap copy
  of gSlapShotPowerMultiplier (image 0x83C84320, copy 0xBFC39108), gMomentumPhysicsImpulse
  (0x83CD9F8C / 0xBFC7D630, stock 0.8) and gHitTune_GlobalScale (0x83CDEE8C / 0xBFC7DBDC),
  in a BAP match and before an exhibition. No visible effect. Conclusion: the object value is not
  what gameplay reads during play (or is read only at boot). Layout stays OBSERVED, not CONFIRMED.

- 2026-10-29, Victoria: TUN-4 registry probe (`src/rogue_probe.cpp`, snapshots menu / in match /
  after edit). 12 names, all found once in the image; image object + heap copy each. Conclusion:
  layout table above (OBSERVED). Next: TUN-5 write test for gameplay effect.
