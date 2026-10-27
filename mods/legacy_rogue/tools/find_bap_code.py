#!/usr/bin/env python3
"""Locate Be-A-Pro, hitting, shooting and fall code in the recompiled image.
Reuses tools/franchise_re/find_franchise_code.py (string scan + decimal lis/addi
cross-ref) with Be-A-Pro / gameplay keywords instead of franchise ones.

  python mods/legacy_rogue/tools/find_bap_code.py <default.decompressed.exe> \
      --generated generated/default --out mods/legacy_rogue/docs/bap_hits.txt

Also cross-refs the NAME strings of key tunables: the functions that load a
tunable's name VA are registration sites; their neighbours (same .cpp file,
nearby address) are usually the consumers we want to hook.
"""
import argparse, re, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools" / "franchise_re"))
import find_franchise_code as ff  # noqa: E402

ff.KEYWORDS[:] = ["beapro", "bap", "legend", "bodycheck", "hipcheck", "checking",
                  "hittune", "momentum", "slapshot", "shotblock", "knockdown",
                  "stumble", "fall", "balance", "ragdoll", "interaction", ".cpp"]
ff.KW_RE = re.compile("|".join(re.escape(k) for k in ff.KEYWORDS), re.IGNORECASE)

TUNABLE_ANCHORS = [
    "gHitTune_GlobalScale", "gMomentumPhysicsImpulse", "gCheckMagnitudeStrengthWeight",
    "gHitPowerForPhysicsGlobalModifier", "gBalanceOffpuckFallThreshold",
    "gBalanceCarrierFallThreshold", "gTriggerFalls", "gTriggerStumbleFall",
    "gSlapShotPowerMultiplier", "gShotBlockReactionPuckSpeedThreshold",
    "gInjuryShotBlockPuckSpeedThreshold", "gBeAPro_LineSkillBoost",
    "gTweakByGameStyleHitPowerLevel", "gTweakByGameStyleSlapShotPowerLevel",
]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image"); ap.add_argument("--generated"); ap.add_argument("--out")
    a = ap.parse_args()
    data = Path(a.image).read_bytes()
    lines = ["# Be-A-Pro / gameplay string hits"]
    hits = sorted(set(ff.scan_strings(data)))
    lines += [f"0x{va:08X}  {t}" for va, t in hits]
    targets = {}
    for name in TUNABLE_ANCHORS:
        off = data.find(name.encode() + b"\0")
        if off >= 0:
            targets[ff.IMAGE_BASE + off] = "tunable:" + name
            lines.append(f"0x{ff.IMAGE_BASE + off:08X}  tunable name {name}")
    for va, t in hits:
        if t.rstrip().endswith(".cpp"):
            targets[va] = t.rsplit("\\", 1)[-1]
    if a.generated:
        lines.append("\n# === functions referencing anchors ===")
        ff.crossref(Path(a.generated), targets, lines)
    out = "\n".join(lines) + "\n"
    Path(a.out).write_text(out, "utf-8") if a.out else print(out)

if __name__ == "__main__":
    main()
