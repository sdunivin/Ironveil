# Ironveil

A realistic third-person action RPG about climbing the Veil Spire, a 100-floor procedurally generated tower. Crimson Desert-style physical combat and traversal meet Path of Exile-style loot, skill gems and a passive tree. Solo or online co-op, with weekly timed leaderboards.

"Ironveil" is a working title.

## Tools

- Unreal Engine 5.8
- Blender 5.2
- Git + Git LFS (large binary files)

## Planning documents

- [Master plan](https://claude.ai/artifact/HUaZoXgk8oj5DTccQfyg5F)
- [Game Design Document](https://claude.ai/artifact/BsXvb394MYsDgr38r78TBB)

## Repository layout

```
Ironveil.uproject   Unreal project file (added in Phase 1)
Config/             Unreal settings
Content/_Game/      All of our game content
Source/Ironveil/    C++ code
Art/Source/         Blender and texture source files
Art/Export/         FBX exports for Unreal
Data/               Balance data (affixes, item bases, scaling) as CSV for Unreal DataTables
Tools/              Helper scripts, e.g. the loot simulator
```

## First-time setup

See [GIT-SETUP.md](GIT-SETUP.md).
