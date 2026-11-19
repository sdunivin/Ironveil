// ArpgGameData.h
// Design data for classes, skills, settings (biomes), enemy archetypes and guardians.
//
// It lives in code for the prototype so the game runs with no editor-authored assets.
// Every struct here maps one-to-one onto a future Data Asset (DA_Class, DA_Setting,
// DA_TowerDefinition in the master plan), so moving it into the editor later is mechanical.

#pragma once

#include "CoreMinimal.h"
#include "Data/ArpgTypes.h"

struct FArpgSkillDef
{
	FString Name;
	FString Description;
	float ManaCost = 0.f;
	float Cooldown = 0.f;
	FLinearColor Color = FLinearColor::White;
};

struct FArpgClassDef
{
	EArpgClass Class = EArpgClass::Warrior;
	FString Name;
	FString Tagline;
	FString Description;
	FLinearColor Color = FLinearColor::White;
	float Strength = 14.f;
	float Dexterity = 14.f;
	float Intelligence = 14.f;
	// Attribute points gained per level, split by the class's focus.
	float StrPerLevel = 1.f;
	float DexPerLevel = 1.f;
	float IntPerLevel = 1.f;
	float DodgeIFrames = 0.25f;
	float MoveSpeed = 500.f;
	bool bRangedBasicAttack = false;
	TArray<FName> StartingItems;    // Row names in DT_ItemBases.csv
	FArpgSkillDef Skills[4];
	// Mesh imported from Art/Export by Tools/unreal/setup_ironveil_content.py (optional).
	FString MeshPath;
};

// Per-setting look, mood, enemy names and damage flavour. Floors pick one at random from the seed.
struct FArpgBiomeDef
{
	EArpgBiome Biome = EArpgBiome::Forest;
	FString Name;
	FString Tagline;
	float Weight = 1.f;

	FLinearColor Floor;
	FLinearColor FloorAlt;
	FLinearColor Wall;
	FLinearColor WallAlt;
	FLinearColor Accent;           // crystals, neon, relic glow
	FLinearColor Foliage;
	FLinearColor Ground;           // terrain outside the rooms

	FLinearColor SunColor;
	float SunIntensity = 8.f;      // lux
	float SunPitch = -40.f;
	float SunYaw = 30.f;
	float SkyIntensity = 1.f;
	FLinearColor FogColor;
	float FogDensity = 0.02f;
	FLinearColor SceneTint = FLinearColor::White;
	float Saturation = 1.f;

	float WallHeightMin = 300.f;
	float WallHeightMax = 650.f;
	bool bWater = false;           // Flood: room floors are islands in shallow water

	// Enemy flavour: names per archetype (Swordsman, Archer, Duelist, ShieldBrute, Ogre) and colours.
	FString EnemyNames[5];
	FLinearColor EnemyPrimary;
	FLinearColor EnemySecondary;
	EArpgDamageType Element = EArpgDamageType::Physical;
	float ElementShare = 0.f;      // fraction of enemy damage converted to Element
};

enum class EArpgEnemyAttack : uint8
{
	Swipe,          // melee arc in front
	Combo3,         // three swipes
	Lunge,          // dash then swipe
	Shot,           // single projectile
	Volley,         // projectile fan
	Slam,           // telegraphed circle at the target
	Stomp,          // telegraphed circle around self
	Charge,         // unblockable dash (red flash)
	Shockwave,      // expanding ring from self (jump or dodge through it)
	Leap,           // jump onto the target's position
	SlowDome,       // time-slowing dome around self
	Summon,         // call adds (once per fight)
	RingBurst,      // three shockwaves in a row (final boss)
	HourHand        // spiral of projectiles (final boss)
};

struct FArpgEnemyAttackDef
{
	EArpgEnemyAttack Kind = EArpgEnemyAttack::Swipe;
	float MinRange = 0.f;
	float MaxRange = 250.f;
	float Windup = 0.5f;
	float Recovery = 0.6f;
	float DamageMult = 1.f;
	float Poise = 10.f;
	float Weight = 1.f;
	int32 MinPhase = 1;            // bosses unlock moves by phase
	float Cooldown = 0.f;
};

struct FArpgArchetypeDef
{
	EArpgArchetype Archetype = EArpgArchetype::Swordsman;
	// Stats at monster level 10 (GDD). Scaled by +7% Health / +5% damage per level, compounding.
	float Health10 = 90.f;
	float Hit10 = 12.f;
	float Armour = 40.f;
	float Evasion = 0.f;
	float Poise = 30.f;
	float PoiseRecoverDelay = 3.f;
	float MoveSpeed = 380.f;
	float Scale = 1.f;
	float PreferredRange = 0.f;    // > 0: ranged, keeps this distance
	float AggroRange = 1800.f;
	bool bFrontBlock = false;      // Shield Brute: 50% less damage from the front
	bool bParries = false;         // Duelist: parries every 3rd hit
	float XP = 10.f;
	TArray<FArpgEnemyAttackDef> Attacks;
	FString MeshPath;
};

struct FArpgGuardianDef
{
	FString Name;
	FString Title;
	EArpgBiome Biome = EArpgBiome::Forest;
	EArpgDamageType Element = EArpgDamageType::Physical;
	FLinearColor Primary;
	FLinearColor Secondary;
	FString MeshPath;
};

namespace ArpgData
{
	// Prototype tuning: monster Health relative to the GDD's numbers, so early floors play fast with starter gear.
	// Raise toward 1.0 as gear, gems and the passive tree come online.
	constexpr float PrototypeMonsterHealthScale = 0.6f;

	const FArpgClassDef& Class(EArpgClass C);
	const FArpgBiomeDef& Biome(EArpgBiome B);
	const FArpgArchetypeDef& Archetype(EArpgArchetype A);

	// The biomes normal floors can roll (the five eras on the concept sheet).
	const TArray<EArpgBiome>& FloorBiomes();

	// Guardian for a guardian floor (10, 20 ... 90). Floor 10 is always Warlord Varrick Dunmar.
	const FArpgGuardianDef& GuardianForFloor(int32 Floor, int32 RunSeed);
	const FArpgGuardianDef& FinalBoss();

	FString MonsterModName(EArpgMonsterMod Mod);

	// Monster scaling (matches Data/CT_MonsterScaling.csv): +7% Health, +5% damage per level, compounding.
	float HealthMultiplier(int32 Level);
	float DamageMultiplier(int32 Level);

	// Random two-part name for rare monsters and rare items.
	FString RareMonsterName(FRandomStream& Rng);
	FString RareItemName(FRandomStream& Rng);
	FString MagicItemPrefix(FRandomStream& Rng);

	// XP needed to go from Level to Level + 1.
	float XPToNext(int32 Level);
}
