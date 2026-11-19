# Game data

Balance data for Ironveil, kept as CSV files so it can be edited in any spreadsheet app, compared in Git, and imported straight into Unreal. These CSV files are the source of truth: edit them here, then re-import into Unreal.

| File | Unreal asset | What it holds |
| --- | --- | --- |
| `DT_Affixes.csv` | DataTable, row struct `FArpgAffixRow` | 44 affix groups (22 prefixes, 22 suffixes), 278 tier rows, item levels 1 to 84+ |
| `DT_ItemBases.csv` | DataTable, row struct `FArpgItemBaseRow` | The 18 item bases from the GDD, 3 starter bases and War Wraps (the Monk's fist weapon) |
| `CT_MonsterScaling.csv` | CurveTable (Linear) | Monster Health and damage multipliers and floor loot bonuses, levels 1 to 120 |
| `Source/IronVeil/Data/ArpgLootRows.h` | C++ header | The row structs above (moved into the game module) |

## How the affix table works

- Each row is one **tier** of one affix. `LocalPhysInc_T1` is the best tier of "increased Physical Damage"; the highest number is the weakest.
- `MinItemLevel`: the lowest item level that can roll the tier. Tiers unlock at item levels 1, 10, 22, 35, 50, 68 and 84.
- `Weight`: how common the tier is, relative to everything else the item can roll. Lower tiers are more common.
- `AllowedTags`: the affix can roll on any item base that has at least one of these tags (see `ItemTags` in `DT_ItemBases.csv`).
- `ModGroup`: an item can't roll two affixes from the same group.
- "Adds X to Y" affixes roll X from `ValueMin`..`ValueMax` and Y from `Value2Min`..`Value2Max`.
- Rules from the GDD: magic items get 1 to 2 affixes (max 1 prefix, 1 suffix); rares get 4 to 6 (max 3 prefixes, 3 suffixes).

## Checking balance

Run the simulator after any change (needs Python 3):

```bash
python Tools/loot_sim.py
python Tools/loot_sim.py --base Greataxe --ilvl 50 --rolls 20000
```

It checks that every base can roll a full rare, rolls thousands of items and reports affix counts, damage-prefix odds and tier quality. You can also paste its output to Claude and ask what to change.

## Importing into Unreal (Phase 3)

The prototype reads these CSV files directly at startup (`UArpgLootSubsystem`), so while you develop, editing a CSV and pressing Play is enough. When you want DataTable assets (for packaged builds):

1. The row structs are already compiled in `Source/IronVeil/Data/ArpgLootRows.h`.
2. In the Content Browser, open `Content/_Game/Data`, click **Import**, choose `DT_Affixes.csv`.
3. In the import dialog choose **DataTable** and row type **ArpgAffixRow**. Repeat for `DT_ItemBases.csv` with **ArpgItemBaseRow**.
4. Import `CT_MonsterScaling.csv` as a **CurveTable** with **Linear** interpolation.
5. After editing a CSV later, right-click the asset → **Reimport**.

## Known limits (tune later)

- Item bases cover floors 1 to about 20. Add higher-level versions of each base (stronger base stats, higher `RequiredLevel`) before balancing deep floors.
- Rare weapons are generous: about 70% of rare swords roll 2 or more damage prefixes. Lower the damage-prefix weights if good weapons feel too common.
- Belts have exactly 3 prefix groups, so every 3-prefix belt rolls the same three. Add a belt prefix if belts feel samey.
- Monster Health grows 7% per level (about 800 times by level 100). Player power has to keep pace through affix tiers, gems and the tree; revisit when floors 20 and up exist.
