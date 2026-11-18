# Ironveil: notes for Claude

Third-person action RPG in Unreal Engine 5.8 (C++ project) with Blender 5.2 art. Crimson Desert-style combat and traversal, a 100-floor procedurally generated tower, Path of Exile-style loot, skill gems and passive tree, solo or online co-op. The developer is a solo beginner: explain changes plainly, give exact editor menu paths, and prefer Blueprint for tuning and content.

## Layout

- `Source/Ironveil/` C++ module. Folders: `AbilitySystem/` (GAS: tags, attributes, ASC, abilities), `Animation/`, `Characters/`, `Components/`, `Core/` (GameMode, PlayerController, PlayerState), `Traversal/`.
- `Content/_Game/` all game content (plan 1.2 folder tree). Never put our assets elsewhere.
- `Data/` balance CSVs (source of truth for DataTables). `Data/Source/ArpgLootRows.h` is parked until Phase 3.
- `Tools/` scripts (loot simulator, Blender builders). `Docs/` setup guides.

## Rules

- **Multiplayer from day one.** The server decides gameplay; clients predict and display. Move characters only through the Character Movement Component (custom speeds via saved-move flags, see `ArpgCharacterMovementComponent`). Abilities are Local Predicted. Cosmetics (hitstop, shakes, VFX, sound) never change the server simulation. Test with 2 players, Listen Server, network emulation on.
- **GAS.** Player Ability System Component lives on `AArpgPlayerState` (Mixed replication); enemies own theirs (Minimal). New abilities derive from `UArpgGameplayAbility` and are activated by `InputTag.*` tags, not new input code. Add tags in `ArpgGameplayTags.h/.cpp`.
- **Naming.** C++ classes use the `Arpg` prefix (`AArpgPlayerCharacter`). Assets use the plan's prefixes: `BP_`, `ABP_`, `AM_`, `SK_`, `SM_`, `M_`, `MI_`, `T_`, `GA_`, `GE_`, `DA_`, `DT_`, `WBP_`, `NS_`, `IA_`, `IMC_`.
- **Don't rename** C++ classes or UPROPERTYs that Blueprints use without adding Core Redirects in `Config/DefaultEngine.ini`.
- **Comments** explain the why in plain language for a beginner.
- **Determinism.** On generated floors, every random choice uses the floor's `FRandomStream`; never `FMath::Rand`.
- Ask the developer to commit before large changes.

## Status

- Phase 0: GDD and pitch done (GDD is a Claude Doc linked from README.md).
- Phase 1: C++ foundation in place (movement, sprint, stamina, dodge, lock-on, grapple, GAS base). Editor setup steps in `Docs/PHASE1-EDITOR-SETUP.md`. Next: Game Animation Sample locomotion, mantling, ledge shimmy, camera shakes, HUD.
