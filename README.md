# Ironveil

A realistic third-person action RPG about climbing the Veil Spire, a 100-floor procedurally generated tower. Crimson Desert-style physical combat and traversal meet Path of Exile-style loot, skill gems and a passive tree. Solo or online co-op, with weekly timed leaderboards.

"Ironveil" is a working title.

## Play the prototype

A playable gray-box build of the climb lives in `Source/IronVeil`: Greyhollow hub, four classes, 100 procedurally generated floors across five biomes, guardians every 10 floors and the Hourless King at the top. Setup, controls and status: [PROTOTYPE.md](PROTOTYPE.md).

## Tools

- Unreal Engine 5.3 (the current `IronVeil.uproject`; the plan targets 5.8, and the C++ sticks to APIs that exist in both)
- Blender 5.2
- Git + Git LFS (large binary files)

## Planning documents

- [Master plan](https://claude.ai/artifact/HUaZoXgk8oj5DTccQfyg5F)
- [Game Design Document](https://claude.ai/artifact/BsXvb394MYsDgr38r78TBB)

## Repository layout

```
IronVeil.uproject   Unreal project file (C++ module IronVeil)
Config/             Unreal settings
Content/_Game/      All of our game content
Source/IronVeil/    C++ code (the playable prototype; see PROTOTYPE.md)
Art/Source/         Blender and texture source files
Art/Export/         FBX exports for Unreal
Data/               Balance data (affixes, item bases, scaling) as CSV for Unreal DataTables
Tools/              Helper scripts: loot simulator, Blender blockout builders, Unreal content setup
```

## First-time setup

See [GIT-SETUP.md](GIT-SETUP.md).
