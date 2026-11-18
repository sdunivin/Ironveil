# Phase 1 editor setup

The repository now holds the Unreal project file, settings and the C++ foundation for Phase 1. This guide takes you from "the code is in the repo" to "I can run, dodge, sprint, lock on and grapple in a test level, in co-op".

Budget one or two evenings. Tick the boxes as you go.

> **Heads-up:** this C++ was written without an Unreal compiler available, so the first build may show a few errors. That's normal. Copy the **full** error text from Visual Studio's Output window (or Unreal's Output Log) into Claude Code and ask it to fix them. Commit after the first successful build.

## What the code gives you

| Feature | Where | Notes |
| --- | --- | --- |
| Player character with right-shoulder camera | `ArpgPlayerCharacter` | Spring arm 320 cm, offset 50 right / 60 up, camera lag |
| Sprint (hold Space / B) that works in co-op | `ArpgCharacterMovementComponent` | Sends a sprint flag with every movement update, so no rubber-banding |
| Stamina rules from the GDD | `ArpgCharacterBase` | 100 max, sprint 10/s, regen 30/s after 0.8 s, exhausted at 0 until 25 |
| Dodge (tap Space / B) with i-frames | `ArpgGA_Dodge` | 450 cm dash, 0.6 s total, invulnerable for the first 0.25 s, 20 stamina |
| Lock-on and target switching | `ArpgLockOnComponent` | Middle mouse / R3; flick to switch; strafes around the target |
| Grappling hook | `ArpgGrappleComponent`, `ArpgGrappleAnchor` | Q / Y flies you to the aimed anchor in an arc with a rope |
| Heavy landing | `ArpgPlayerCharacter` | Optional montage and camera shake when you fall fast |
| GAS foundation | `AbilitySystem/` | Attributes from plan 2.1, native Gameplay Tags, input-tag abilities, stamina/mana costs |
| Co-op ready | everywhere | Player abilities live on `ArpgPlayerState`; server decides, clients display |
| Enemy base and training dummy | `ArpgEnemyCharacter` | Own Ability System Component, lock-on target, ragdoll on death |
| I-frame / tag window notify | `ArpgAnimNotifyState_GrantTag` | Add a Gameplay Tag for part of an animation |

## 1. Build and open the project

- [ ] Get the code: in GitHub Desktop, **Fetch origin**, then merge the `phase1-foundation` pull request on GitHub (or switch to that branch to try it first).
- [ ] Make sure Visual Studio has the **Game development with C++** workload (the version listed in the UE 5.8 release notes).
- [ ] In File Explorer, right-click `Ironveil.uproject` → **Generate Visual Studio project files**. (On Windows 11, choose **Show more options** first.)
- [ ] Double-click `Ironveil.uproject`. When Unreal says modules are missing and asks to rebuild, click **Yes**. The first build takes several minutes.
- [ ] If the rebuild fails: open `Ironveil.sln`, pick **Development Editor** and **Win64** in the toolbar, then **Build → Build Solution**. Copy the errors from the Output window to Claude Code.
- [ ] If Unreal warns that a plugin couldn't be found, note its name and ask Claude. The project enables: Enhanced Input, Gameplay Abilities, Cable Component, Motion Warping, Animation Warping, Pose Search, Chooser, State Tree, Gameplay State Tree and Common UI.
- [ ] Enable the two Claude plugins from the plan yourself: **Edit → Plugins**, search **ModelContextProtocol** and **AllToolsets**, tick both, restart.

## 2. Add placeholder content

The Third Person content pack gives you a mannequin, animations and basic input assets to start with. You'll replace the locomotion with Epic's Game Animation Sample later (plan 1.4).

- [ ] Content Browser → **Add** (or right-click empty space) → **Add Feature or Content Pack** → **Blueprint** tab → **Third Person** → **Add to Project**.
- [ ] Find where it landed (usually `Content/Characters/Mannequins` and `Content/Input`). Names below may differ slightly between engine versions.

## 3. Create your folders

- [ ] Create `Content/_Game` with the folders from plan 1.2: `Core`, `Characters/Player`, `Characters/Enemies`, `Combat/Abilities`, `World/Maps`, `World/Props` and the rest. (Or ask Claude Code with prompt P4.)
- [ ] Also create `Content/_Game/Core/Input`.

## 4. Input actions and mapping

Right-click in `_Game/Core/Input` → **Input → Input Action** for each new action. Open each one to set its **Value Type**.

| Input Action | Value Type | Trigger (in the action) | Notes |
| --- | --- | --- | --- |
| `IA_DodgeSprint` | Digital (bool) | none | Tap = dodge, hold = sprint (handled in code) |
| `IA_LockOn` | Digital (bool) | none | |
| `IA_SwitchTarget` | Axis1D (float) | **Pressed**, Actuation Threshold **0.8** | Fires once per flick |
| `IA_Grapple` | Digital (bool) | none | |

- [ ] Duplicate the template's `IMC_Default` into `_Game/Core/Input` and rename it `IMC_Ironveil`. It already maps **IA_Move**, **IA_Look** and **IA_Jump**.
- [ ] Open `IMC_Ironveil` and set these mappings (click **+** next to Mappings to add an action, then **+** next to it to add keys):

| Action | Keyboard / mouse | Gamepad | Modifiers |
| --- | --- | --- | --- |
| IA_Jump | change Space Bar to **Left Ctrl** | Gamepad Face Button Bottom (A) | |
| IA_DodgeSprint | **Space Bar** | **Gamepad Face Button Right** (B) | |
| IA_LockOn | **Middle Mouse Button** | **Gamepad Right Thumbstick Button** (R3) | |
| IA_SwitchTarget | **Mouse X** | **Gamepad Right Thumbstick X-Axis** | Mouse X: **Scalar** with X = **0.05**. Stick: **Dead Zone** |
| IA_Grapple | **Q** | **Gamepad Face Button Top** (Y) | |

The Scalar on Mouse X means a fast flick (about 16 pixels in one frame) switches targets, while normal aiming doesn't. Raise or lower 0.05 to taste.

## 5. Blueprints

Create each one with right-click → **Blueprint Class** → **All Classes**, search for the C++ class, pick it, then name it.

### GA_Dodge (`_Game/Combat/Abilities`)
- [ ] Parent class: **ArpgGA_Dodge**. Defaults already match the GDD.
- [ ] Optional: set **Dodge Montage** to a dodge animation montage with root motion **off** (Mixamo has free dodge animations). The dash moves the character; the montage is only the look.

### BP_PlayerCharacter (`_Game/Characters/Player`)
- [ ] Parent class: **ArpgPlayerCharacter**.
- [ ] Select **Mesh**: Skeletal Mesh Asset = `SKM_Manny` (or `SKM_Quinn`), Location Z = **-96**, Rotation Z (yaw) = **-90**, Anim Class = the template's unarmed Anim Blueprint (`ABP_Unarmed` or similar).
- [ ] In **Class Defaults**, section **Ironveil | Input**: set Default Mapping Context = `IMC_Ironveil` and each action (Move, Look, Jump, Dodge Sprint, Lock On, Switch Target, Grapple).
- [ ] Section **Ironveil | Abilities**: add `GA_Dodge` to **Startup Abilities**.
- [ ] Compile and save.

### BP_ArpgGameMode (`_Game/Core`)
- [ ] Parent class: **ArpgGameMode**. Set **Default Pawn Class** = `BP_PlayerCharacter`.
- [ ] **Edit → Project Settings → Maps & Modes**: Default GameMode = `BP_ArpgGameMode`.

### BP_TrainingDummy (`_Game/Characters/Enemies`)
- [ ] Parent class: **ArpgEnemyCharacter**. Same mesh setup as the player (Z -96, yaw -90, unarmed Anim Blueprint).
- [ ] It stands still for now; AI comes in Phase 5.

### BP_GrappleAnchor (`_Game/World/Props`)
- [ ] Parent class: **ArpgGrappleAnchor**. Select **Mesh** and pick a small visible mesh (a sphere or a ring, scale about 0.3).
- [ ] Optional: in the Event Graph, add **Event On Highlight Changed** and switch the mesh's material to a glowing one when **Highlighted** is true.
- [ ] The **LandingPoint** component is where the player arrives. Leave it a little above and past the ledge.

## 6. A test level

- [ ] **File → New Level → Basic**, save as `_Game/World/Maps/L_Traversal_Test`.
- [ ] Add a few cubes (Place Actors → Shapes) as platforms at different heights, including one 6 to 10 m tall.
- [ ] Put a **BP_GrappleAnchor** on top of the tall platform and one on a far platform.
- [ ] Place 3 **BP_TrainingDummy** a few metres apart.
- [ ] **Project Settings → Maps & Modes**: set Editor Startup Map and Game Default Map to this level.

## 7. Test it

Press **Play** and check each item. Type `showdebug abilitysystem` in the console (backtick key) to see live Stamina, Health and tags.

- [ ] WASD / left stick moves relative to the camera; the mouse / right stick turns the camera.
- [ ] Hold Space: you sprint after 0.2 s, the view widens slightly, Stamina drains 10 per second. At 0 you stop sprinting until it refills to 25.
- [ ] Tap Space: a quick dash in your movement direction (a backstep when standing still). `State.Invulnerable` appears for 0.25 s. Stamina drops by 20.
- [ ] Left Ctrl jumps.
- [ ] Middle mouse locks on to the dummy you're facing. You strafe around it; the camera keeps it in frame. A quick mouse flick switches dummies. Middle mouse again releases.
- [ ] Aim at an anchor (it highlights if you set that up) and press Q: you fly to it with a rope from your hand.

### Co-op test (do this for every feature from now on)
- [ ] Play dropdown (⋮ next to Play) → **Number of Players: 2**, **Net Mode: Play As Listen Server**.
- [ ] Repeat the checks above in **both** windows. Watch for stuttering or snapping back while sprinting and dodging in the client window.
- [ ] Turn on lag: **Edit → Editor Preferences → Level Editor → Play → Multiplayer Options → Enable Network Emulation**, profile **Average**. Repeat. A tiny correction after a grapple is expected for now; anything worse, tell Claude.

## 8. Commit

- [ ] In GitHub Desktop, commit `Phase 1 editor setup: input, player, dummies, test level` and push.
- [ ] Check `git lfs ls-files` lists your new `.uasset` and `.umap` files (see GIT-SETUP.md).

## Next in Phase 1 (ask Claude Code)

1. Migrate the **Game Animation Sample** locomotion onto BP_PlayerCharacter (plan 1.4). This is the biggest visual jump. Prompt: *"Migrate the Game Animation Sample's motion-matched locomotion and traversal onto BP_PlayerCharacter, keeping my C++ movement component, sprint and dodge."*
2. Camera shake assets for light hits, heavy hits and landings (plan 1.5), and assign the landing one on BP_PlayerCharacter.
3. Mantle and vault with Motion Warping from the sample project (plan 1.6).
4. Ledge shimmy on surfaces tagged `Climbable` (plan 1.6; free climbing stays a stretch goal).
5. A simple HUD with Health and Stamina bars and a lock-on marker (bind to the lock-on component's **On Target Changed**).

Phase 1 is done when a friend can run, dodge, mantle and grapple around a gray-box level for 10 minutes and says it feels good.
