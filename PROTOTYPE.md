# Ironveil playable prototype: the Veil Spire climb

A third-person action RPG tower climb, built in C++ so it runs in an empty Unreal project with no hand-made assets.

1. Pick a class in Greyhollow and walk into the Spire gate.
2. Fight up through procedurally generated floors of random enemies in five biomes.
3. Beat a guardian every 10 floors; each guardian floor then becomes your checkpoint.
4. Face the Hourless King on floor 100.

Blender blockouts replace the gray-box shapes automatically once they are imported.

> **Status, honestly:** this code was written in a cloud session that has no Unreal Engine.
> - Every C++ file was syntax-checked with clang against stand-in engine headers, and the engine calls were checked by hand against the UE 5.3 API.
> - It has **not** been compiled or played in the real engine yet.
> - Expect a short round of fixes on the first build: paste the Output Log or compiler errors to Claude Code and it can fix them.
>
> The Blender scripts *were* run (Blender 5.3, headless): all 37 meshes build, export to FBX and render previews.

## What's in it

| Area | What you get |
| --- | --- |
| Classes | The four from the class concept sheet. Each has a light-attack combo (the Mage fires staff bolts), a charged heavy attack and 4 skills. |
| Warrior | Power Strike, Battle Fury, Shield Barrier, Charged Slash |
| Mage | Arcane Blast, Mana Surge, Telekinesis, Spell Barrier |
| Thief | Shadow Step, Twin Strike, Smoke Bomb (backstabs), Lockpick (locked coffers / Expose) |
| Monk | Flow Strike, Chi Heal, Zen Resolve, Divine Palm |
| Combat (GDD numbers) | Stamina, poise and stagger. Dodge with i-frames. Block, perfect parry and guard break. Grab, then throw, slam or shove. Grappling hook. Lock-on. Hitstop and camera shake. Ignite, chill, freeze, shock, bleed and poison. The "increased vs more" damage formula, armour, resistances and evasion. |
| The tower | 100 floors, each built from a seed: run seed + floor, so the same seed always builds the same floor. Never the same biome twice in a row. Floor objectives: **Slaughter** (kill 80%), **Hunt** (kill the marked rare), **Hold** (awaken the Veil Relic and survive). Guardian every 10 floors becomes a sanctuary with a shrine, a checkpoint and a save. **The Hourless King** waits on floor 100, and endless Overtime floors open after him. |
| Biomes | The five eras from the concept sheet: Forest, Jungle, Post-Apocalyptic City, Flood, Ice Lands. Plus Fractured Time arenas for bosses, and the Greyhollow hub with the Veil Spire on the skyline. Each has its own walls, props, backdrop, lighting, fog, enemy names, outfits and damage element. |
| Enemies | The GDD's 5 archetypes (Swordsman, Archer, Duelist, Shield Brute, Ogre) with telegraphed attacks and attack tokens (2 melee + 1 ranged at once). Magic and rare monsters with modifiers (Hasted, Fire Enchanted, Armoured, Regenerating, Volatile, Vampiric). |
| Bosses | Five guardians: Warlord Varrick Dunmar on floor 10, then a seeded pick from the others on floors 20 to 90. Each has two phases, a slow-time dome, summons and a leap. The Hourless King has three phases, ring bursts, an hour-hand spiral and echoes. |
| Loot | Items roll straight from `Data/DT_Affixes.csv` and `Data/DT_ItemBases.csv`, using the same rules as `Tools/loot_sim.py`. Normal, magic, rare and the GDD's uniques, with build-changing effects. Includes a list inventory with tooltips, equipment and a character sheet. |
| Progress | XP and levels with the GDD's Health and Mana formulas. Saves your class, level, gear and highest sanctuary. Death sends you back to your last sanctuary and reshuffles the floors above it. |

## Setting it up (Windows, UE 5.3)

1. **Install the C++ tools once.** Visual Studio 2022 with the "Game development with C++" workload. Epic's "Setting up Visual Studio for Unreal Engine" page lists the exact components.
2. **Get the files into your project folder.**
   - Pull this branch.
   - If your Unreal project lives elsewhere, copy `Source/`, `Config/`, `Data/`, `Tools/` and `IronVeil.uproject` into it. Merge `Config/DefaultEngine.ini` if you already have one; the needed lines are commented.
   - `IronVeil.uproject` is your 5.3 project file with a `Modules` entry for the C++ added, plus the Python plugins the setup script uses.
3. **Build the C++.**
   1. Right-click `IronVeil.uproject` and choose **Generate Visual Studio project files**.
   2. Double-click `IronVeil.uproject` and click **Yes** when it asks to rebuild modules.
   3. If the rebuild fails, open `IronVeil.sln`, build **Development Editor / Win64**, and paste any errors to Claude.
4. **Press Play.** The project starts on the empty engine Entry map, and the game mode builds Greyhollow at runtime. Everything is gray-box until step 6.
5. **Make the Blender assets** (optional but recommended). From the repo folder, in a terminal:

   ```bash
   blender --background --python Tools/blender/build_ironveil_assets.py
   ```

   This writes FBX files, a `.blend` and preview sheets to `Art/Export/`. The Warrior has its own script, `Tools/blender/build_warrior_blockout.py`; put its FBX in `Art/Export/Characters/` as well.
6. **Import them into Unreal.** **Tools > Execute Python Script...** and pick `Tools/unreal/setup_ironveil_content.py`. It:
   - creates the glowing base material;
   - imports every mesh into `/Game/_Game/Art/`;
   - creates the `L_Spire` map with the right game mode and sets it as the start map.

   Press Play again.

## Controls

| Action | Keyboard and mouse | Gamepad |
| --- | --- | --- |
| Move / camera | WASD / mouse | Sticks |
| Light attack (3-hit combo) | Left mouse | RB |
| Heavy (hold 0.8 s to charge) | Right mouse | RT |
| Dodge (hold to sprint) | Space | B |
| Jump | Left Ctrl | A |
| Block / perfect parry | Left Shift | LB |
| Grab (F again: throw, LMB: slam, RMB: shove) | F | X |
| Grappling hook (aim with the centre dot) | Q | Y |
| Lock-on / switch target | Middle mouse or Tab / flick | R3 |
| Skills 1-4 | 1 2 3 4 | Hold LT + A B X Y |
| Health flask | R | D-pad up |
| Interact / pick up | E | X near an item |
| Inventory (Enter equip, X salvage) | I | View |
| Help | F1 or H | Menu |

Class select: press **1 to 4**, or Left/Right then Enter. Press **C** to continue a saved climb.

### Test cheats (press ` to open the console)

| Command | Effect |
| --- | --- |
| `SpireFloor 10` | Jump to a floor; 10 is the Warlord and 100 is the Hourless King |
| `SpireLevel 40` | Set your character level |
| `SpireGear` | Equip a set of rares for your level |
| `SpireGod` | Toggle invulnerability |
| `SpireMighty` | Toggle 25x damage |
| `SpireKillAll` | Kill every monster on the floor |
| `SpireSeed 1234` | Fixed run seed: the same floors every time |

## How it maps to the plan, and what is placeholder

- **Classes follow the new class concept sheet** (Warrior, Mage, Thief, Monk). The GDD still lists the Time Mage. Its Time Stop, Slow Field and Rewind ideas live on in the bosses' slow-time domes and the Stolen Hour unique. Decide which roster you want and Claude can update the GDD or the code.
- **Five biomes**, where the GDD slice has two. Jungle, Flood and Ice Lands come from the setting concept sheet.
- **Combat runs on a small custom stat system, not GAS yet.**
  - The formulas are the GDD's, so they carry straight into a GAS Execution Calculation in master plan Phase 2.
  - `AArpgCharacterBase` is the one place to swap.
- **Single player.** Co-op is Phase 7B. Spawning, loot and damage already sit in the game mode or on the hit pipeline, which is where a listen server needs them.
- **Data in code.** Classes, biomes, archetypes and guardians are in `Source/IronVeil/Data/ArpgGameData.cpp` as plain structs that map one-to-one onto the plan's Data Assets (DA_Class, DA_Setting, DA_TowerDefinition).
- **Static-mesh blockouts with procedural limb swings**, not skeletal meshes with animation. MetaHumans, motion matching and montages come in Phases 1, 2 and 6.
- **Canvas HUD** instead of UMG and Common UI (Phase 8).
- **Navigation** uses the floor's room graph instead of a NavMesh. Room kits as Level Instances (plan 7.2) can replace the gray-box rooms later; the generator already has the slots (spawn spots, doors, portal, relic).
- **Tuning:**
  - Monster Health is at 60% of the GDD's numbers (`PrototypeMonsterHealthScale`) so floor 1 plays fast with starter gear.
  - Monster armour and evasion scale with level.
  - The tower's height, guardian interval and Hold timer are editable on the game mode.

## Code map

```
Source/IronVeil/
  Core/        ArpgGameMode (the climb, objectives, loot, XP, saving), ArpgPlayerController (cheats),
               ArpgSaveGame, ArpgAssets (basic shapes, materials, optional imported meshes)
  Characters/  ArpgCharacterBase (hits, mitigation, poise, ailments, hitstop, gray-box rig),
               ArpgPlayerCharacter (input, combat, 16 skills, gear), ArpgEnemyCharacter (AI, attacks, bosses)
  Combat/      ArpgCombat (targeting, formulas), ArpgProjectile, ArpgAreaEffect (telegraphs, shockwaves, slow fields, smoke)
  Tower/       ArpgFloorGenerator (seeded layouts, biome kits, hub, arenas, room-graph navigation),
               ArpgEnvironment (sun, sky, fog, grading), ArpgInteractables (loot, chests, portal, shrine, relic)
  Loot/        ArpgItem, ArpgLootSubsystem (reads Data/*.csv, rolls items)
  Data/        ArpgTypes, ArpgGameData (classes, biomes, enemies, guardians), ArpgLootRows (CSV row structs)
  UI/          ArpgHUD
Tools/blender/ build_ironveil_assets.py + ironveil_kit.py (heroes, enemies, bosses, props, the Spire)
Tools/unreal/  setup_ironveil_content.py (material, mesh import, L_Spire map)
```
