"""
Ironveil loot simulator.

Reads Data/DT_Affixes.csv and Data/DT_ItemBases.csv, rolls thousands of items
with the same rules the game will use, and prints what players would see.
Run it after you change the affix table to catch balance problems early.

Usage (from the repository root, with Python 3 installed):
    python Tools/loot_sim.py
    python Tools/loot_sim.py --base ArmingSword --ilvl 40 --rolls 10000
"""
import argparse
import csv
import random
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "Data"

# Rarity rules (match the Game Design Document)
MAX_PREFIXES = 3
MAX_SUFFIXES = 3
RARE_AFFIX_COUNT_WEIGHTS = {4: 45, 5: 35, 6: 20}
MAGIC_AFFIX_COUNT_WEIGHTS = {1: 50, 2: 50}
DAMAGE_PREFIX_GROUPS = {
    "LocalPhysInc", "LocalAddedPhys", "LocalAddedFire", "LocalAddedCold",
    "LocalAddedLightning", "SpellDamageInc", "SpellAddedFire", "SpellAddedCold",
    "SpellAddedLightning", "ElementalDamageInc",
}


def load_csv(name):
    with open(DATA / name, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


AFFIXES = load_csv("DT_Affixes.csv")
BASES = {row["Name"]: row for row in load_csv("DT_ItemBases.csv")}
for a in AFFIXES:
    a["Weight"] = int(a["Weight"])
    a["MinItemLevel"] = int(a["MinItemLevel"])
    a["Tier"] = int(a["Tier"])
    a["Tags"] = set(a["AllowedTags"].split("|"))


def eligible(base, ilvl, affix_type=None):
    item_tags = set(base["ItemTags"].split("|"))
    return [
        a for a in AFFIXES
        if a["MinItemLevel"] <= ilvl
        and a["Tags"] & item_tags
        and (affix_type is None or a["AffixType"] == affix_type)
    ]


def weighted_choice(rng, rows):
    total = sum(r["Weight"] for r in rows)
    pick = rng.uniform(0, total)
    for r in rows:
        pick -= r["Weight"]
        if pick <= 0:
            return r
    return rows[-1]


def roll_item(rng, base, ilvl, rarity):
    weights = RARE_AFFIX_COUNT_WEIGHTS if rarity == "Rare" else MAGIC_AFFIX_COUNT_WEIGHTS
    max_p = MAX_PREFIXES if rarity == "Rare" else 1
    max_s = MAX_SUFFIXES if rarity == "Rare" else 1
    count = rng.choices(list(weights), weights=list(weights.values()))[0]
    pool = eligible(base, ilvl)
    chosen = []
    for _ in range(count):
        n_p = sum(1 for a in chosen if a["AffixType"] == "Prefix")
        n_s = len(chosen) - n_p
        used = {a["ModGroup"] for a in chosen}
        options = [
            a for a in pool
            if a["ModGroup"] not in used
            and ((a["AffixType"] == "Prefix" and n_p < max_p) or (a["AffixType"] == "Suffix" and n_s < max_s))
        ]
        if not options:
            break
        chosen.append(weighted_choice(rng, options))
    return chosen


def check_coverage():
    print("Coverage: affix groups each base can roll (need 3+ prefixes and 3+ suffixes for a full rare)")
    print(f"{'Base':<16}{'ilvl 1 P/S':>12}{'ilvl 84 P/S':>13}")
    problems = 0
    for name, base in BASES.items():
        out = []
        for ilvl in (1, 84):
            p = len({a['ModGroup'] for a in eligible(base, ilvl, 'Prefix')})
            s = len({a['ModGroup'] for a in eligible(base, ilvl, 'Suffix')})
            out.append(f"{p}/{s}")
            if p < 3 or s < 3:
                problems += 1
        print(f"{name:<16}{out[0]:>12}{out[1]:>13}")
    print("All bases can roll full rares." if problems == 0 else f"WARNING: {problems} base/level combos can't roll 3+3.")
    print()


def simulate(base_name, ilvl, rolls, seed):
    rng = random.Random(seed)
    base = BASES[base_name]
    rule_breaks = 0
    counts = Counter()
    dmg_prefixes = Counter()
    tiers = []
    group_hits = Counter()
    for _ in range(rolls):
        item = roll_item(rng, base, ilvl, "Rare")
        n_p = sum(1 for a in item if a["AffixType"] == "Prefix")
        groups = [a["ModGroup"] for a in item]
        if n_p > MAX_PREFIXES or len(item) - n_p > MAX_SUFFIXES or len(groups) != len(set(groups)):
            rule_breaks += 1
        counts[len(item)] += 1
        dmg_prefixes[sum(1 for g in groups if g in DAMAGE_PREFIX_GROUPS)] += 1
        tiers += [a["Tier"] for a in item]
        group_hits.update(groups)
    print(f"{rolls:,} rare {base['DisplayName']}s at item level {ilvl}")
    print(f"  Rule violations (caps / duplicate groups): {rule_breaks}")
    print("  Affix count: " + ", ".join(f"{k}: {v / rolls:.0%}" for k, v in sorted(counts.items())))
    print("  Damage prefixes: " + ", ".join(f"{k}: {v / rolls:.0%}" for k, v in sorted(dmg_prefixes.items())))
    two_plus = sum(v for k, v in dmg_prefixes.items() if k >= 2) / rolls
    print(f"  Chance of 2+ damage prefixes: {two_plus:.1%}")
    print(f"  Average tier rolled: {sum(tiers) / len(tiers):.2f} (Tier 1 = best)")
    top = ", ".join(f"{g} {c / rolls:.0%}" for g, c in group_hits.most_common(5))
    print(f"  Most common affixes: {top}")
    print()


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", help="Item base Name from DT_ItemBases.csv (default: a standard test set)")
    ap.add_argument("--ilvl", type=int, help="Item level (default: 10, 40 and 84)")
    ap.add_argument("--rolls", type=int, default=10000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    check_coverage()
    bases = [args.base] if args.base else ["ArmingSword", "OakStaff", "MailHauberk", "IronRing"]
    levels = [args.ilvl] if args.ilvl else [10, 40, 84]
    for b in bases:
        for lvl in levels:
            simulate(b, lvl, args.rolls, args.seed)
