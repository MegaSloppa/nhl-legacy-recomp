#!/usr/bin/env python3
"""Filter docs/tunables_schema.tsv into perk-candidate lists per theme, so new
perks are always built from tunables that really exist on this binary.
  python mods/legacy_rogue/tools/perk_candidates.py docs/tunables_schema.tsv
"""
import re, sys
THEMES = {
  "hitting":  r"(HitTune|HitPower|CheckMagnitude|MomentumPhysics|Momentum(Maintained)?)",
  "falls":    r"(Fall|Stumble|Balance|Knock|Getup)",
  "shooting": r"(ShotPower|SlapShot|WristShot|OneTimer|ShotRelease)",
  "skating":  r"(SpeedBoost|Accel|Burst|Fatigue)",
  "bap":      r"(BeAPro|BAP)",
  "human_vs_cpu": r"^gTweakByGameStyle",
}
rows = [l.rstrip("\n").split("\t") for l in open(sys.argv[1], encoding="utf-8")]
for theme, pat in THEMES.items():
    sel = [r for r in rows if re.search(pat, r[0])]
    print(f"\n## {theme} ({len(sel)})")
    for r in sel:
        print(f"  {r[0]:<55} {r[4] if len(r) > 4 else '?'}  {r[-1][:90]}")
