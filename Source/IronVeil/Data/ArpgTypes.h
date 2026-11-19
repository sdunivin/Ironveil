// ArpgTypes.h
// Shared enums and plain structs used by every Ironveil system.

#pragma once

#include "CoreMinimal.h"
#include "ArpgTypes.generated.h"

class AArpgCharacterBase;

UENUM(BlueprintType)
enum class EArpgClass : uint8
{
	Warrior,
	Mage,
	Thief,
	Monk
};

UENUM(BlueprintType)
enum class EArpgTeam : uint8
{
	Player,
	Enemy
};

// Index order matters: FArpgStats and FArpgHit store per-type values in arrays indexed by this enum.
UENUM(BlueprintType)
enum class EArpgDamageType : uint8
{
	Physical,
	Fire,
	Cold,
	Lightning,
	Chaos
};
static constexpr int32 ArpgNumDamageTypes = 5;

UENUM(BlueprintType)
enum class EArpgRarity : uint8
{
	Normal,
	Magic,
	Rare,
	Unique
};

// The settings ("eras") a floor can be built in. FracturedTime is used for guardian arenas.
UENUM(BlueprintType)
enum class EArpgBiome : uint8
{
	Forest,
	Jungle,
	Wasteland,
	Flood,
	IceLands,
	FracturedTime,
	Greyhollow
};

UENUM(BlueprintType)
enum class EArpgArchetype : uint8
{
	Swordsman,
	Archer,
	Duelist,
	ShieldBrute,
	Ogre,
	Guardian,
	FinalBoss
};

UENUM(BlueprintType)
enum class EArpgMonsterRarity : uint8
{
	Normal,
	Magic,
	Rare,
	Boss
};

UENUM(BlueprintType)
enum class EArpgMonsterMod : uint8
{
	Hasted,
	FireEnchanted,
	Armoured,
	Regenerating,
	Volatile,
	Vampiric,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EArpgObjective : uint8
{
	Slaughter,
	Hunt,
	Hold,
	Guardian,
	FinalBoss,
	Sanctuary,
	Hub
};

// Equipment slots on the character. Rings use two slots.
UENUM(BlueprintType)
enum class EArpgGearSlot : uint8
{
	MainHand,
	OffHand,
	Helmet,
	Body,
	Gloves,
	Boots,
	Belt,
	Amulet,
	Ring1,
	Ring2,
	Count UMETA(Hidden)
};
static constexpr int32 ArpgNumGearSlots = (int32)EArpgGearSlot::Count;

// Final character stats after level, attributes and gear. Rebuilt by AArpgPlayerCharacter::RecalculateStats;
// enemies fill it from their archetype. "Inc" fields are percentages that add together (the Path of Exile-style
// "increased" rule); "more" multipliers are applied separately where the hit is built.
struct FArpgStats
{
	float Strength = 0.f;
	float Dexterity = 0.f;
	float Intelligence = 0.f;

	float MaxHealth = 50.f;
	float MaxMana = 40.f;
	float MaxStamina = 100.f;
	float MaxEnergyShield = 0.f;

	float Armour = 0.f;
	float Evasion = 0.f;
	float BlockChance = 0.f;           // % chance to block a hit while not actively blocking (shields)
	float Resist[ArpgNumDamageTypes] = { 0.f, 0.f, 0.f, 0.f, 0.f }; // Physical slot unused

	// Main-hand weapon, after the weapon's own local modifiers.
	float WeaponMin[ArpgNumDamageTypes] = { 2.f, 0.f, 0.f, 0.f, 0.f };
	float WeaponMax[ArpgNumDamageTypes] = { 5.f, 0.f, 0.f, 0.f, 0.f };
	float WeaponAPS = 1.3f;
	float WeaponCrit = 5.f;            // % base critical strike chance

	float AttackAddedPhysMin = 0.f;
	float AttackAddedPhysMax = 0.f;
	float SpellAddedMin[ArpgNumDamageTypes] = { 0.f, 0.f, 0.f, 0.f, 0.f };
	float SpellAddedMax[ArpgNumDamageTypes] = { 0.f, 0.f, 0.f, 0.f, 0.f };

	float IncPhysical = 0.f;
	float IncMeleePhysical = 0.f;
	float IncElemental = 0.f;
	float IncSpell = 0.f;
	float IncAttackSpeed = 0.f;
	float IncCastSpeed = 0.f;
	float IncMoveSpeed = 0.f;
	float IncPoise = 0.f;
	float IncSkillDuration = 0.f;
	float IncCooldownRecovery = 0.f;
	float IncThrowSlam = 0.f;
	float IncItemRarity = 0.f;
	float IncManaRegen = 0.f;
	float IncStaminaRegen = 0.f;

	float CritMultiplier = 150.f;      // %
	float HealthRegen = 0.f;           // per second
	float HealthOnHit = 0.f;
};

// One hit, fully rolled by the attacker (base, added, increased, more, crit). The target applies mitigation.
struct FArpgHit
{
	float Damage[ArpgNumDamageTypes] = { 0.f, 0.f, 0.f, 0.f, 0.f };
	float PoiseDamage = 0.f;
	bool bCrit = false;
	bool bSpell = false;
	bool bUnblockable = false;
	bool bIsThrow = false;
	float IgniteChance = 0.f;          // 0..1
	float BleedChance = 0.f;
	float ShockChance = 0.f;
	int32 PoisonStacks = 0;
	float HitstopSeconds = 0.06f;
	FVector Knockback = FVector::ZeroVector;
	int32 SourceLevel = 1;

	float Total() const
	{
		float T = 0.f;
		for (float D : Damage) { T += D; }
		return T;
	}
	void Scale(float M)
	{
		for (float& D : Damage) { D *= M; }
	}
	EArpgDamageType Dominant() const
	{
		int32 Best = 0;
		for (int32 i = 1; i < ArpgNumDamageTypes; ++i) { if (Damage[i] > Damage[Best]) { Best = i; } }
		return (EArpgDamageType)Best;
	}
};

namespace ArpgColors
{
	inline FLinearColor Rarity(EArpgRarity R)
	{
		switch (R)
		{
		case EArpgRarity::Magic:  return FLinearColor(0.35f, 0.45f, 1.0f);
		case EArpgRarity::Rare:   return FLinearColor(1.0f, 0.92f, 0.35f);
		case EArpgRarity::Unique: return FLinearColor(1.0f, 0.45f, 0.08f);
		default:                  return FLinearColor(0.85f, 0.85f, 0.85f);
		}
	}
	inline FLinearColor Damage(EArpgDamageType T)
	{
		switch (T)
		{
		case EArpgDamageType::Fire:      return FLinearColor(1.0f, 0.45f, 0.1f);
		case EArpgDamageType::Cold:      return FLinearColor(0.55f, 0.85f, 1.0f);
		case EArpgDamageType::Lightning: return FLinearColor(0.85f, 0.7f, 1.0f);
		case EArpgDamageType::Chaos:     return FLinearColor(0.55f, 0.95f, 0.3f);
		default:                         return FLinearColor(0.95f, 0.95f, 0.95f);
		}
	}
	inline FLinearColor Hex(uint32 RGB)
	{
		return FLinearColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF));
	}
}
