// ArpgLootRows.h
// Row structures for the loot data in Data/*.csv.
//
// UArpgLootSubsystem reads the CSV files straight from the project's Data folder at
// startup and fills these structs, so balance edits need no reimport while you develop.
// You can still import the CSVs as DataTables with these row structs.
// Column names in the CSV files must match the UPROPERTY names below exactly.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ArpgLootRows.generated.h"

UENUM(BlueprintType)
enum class EArpgAffixType : uint8
{
	Prefix,
	Suffix
};

// How an affix's value is applied.
//   Flat           : +X to a stat (e.g. +30 to maximum Health)
//   Increased      : X% increased, summed with all other "increased" bonuses
//   Added          : adds X to Y damage (uses ValueMin..ValueMax for the low roll, Value2Min..Value2Max for the high roll)
//   LocalIncreased : X% increased to this item's own base stat (weapon damage, armour)
//   LocalAdded     : flat amount added to this item's own base stat
UENUM(BlueprintType)
enum class EArpgModType : uint8
{
	Flat,
	Increased,
	Added,
	LocalIncreased,
	LocalAdded
};

UENUM(BlueprintType)
enum class EArpgEquipSlot : uint8
{
	MainHand,
	OffHand,
	Helmet,
	Body,
	Gloves,
	Boots,
	Belt,
	Amulet,
	Ring
};

UENUM(BlueprintType)
enum class EArpgHandedness : uint8
{
	None,
	OneHand,
	TwoHand
};

// One row of DT_Affixes: a single tier of one affix. Row Name = e.g. "LocalPhysInc_T3".
USTRUCT(BlueprintType)
struct FArpgAffixRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	EArpgAffixType AffixType = EArpgAffixType::Prefix;

	// An item can never roll two affixes from the same group.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FName ModGroup;

	// Tier 1 is the best tier of its group.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	int32 MinItemLevel = 1;

	// Matches an attribute or stat name in UArpgAttributeSet (e.g. PhysicalDamage, MaxHealth).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FName Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	EArpgModType ModType = EArpgModType::Flat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	float ValueMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	float ValueMax = 0.f;

	// Only used by "Adds X to Y" affixes: the range for Y.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	float Value2Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	float Value2Max = 0.f;

	// Higher = more common.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	int32 Weight = 1000;

	// Gameplay tag names separated by "|". The affix can roll on an item that has ANY of these tags.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FString AllowedTags;

	// Tooltip text; {0} and {1} are replaced by the rolled values.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FText DisplayText;
};

// One row of DT_ItemBases. Row Name = e.g. "ArmingSword".
USTRUCT(BlueprintType)
struct FArpgItemBaseRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") EArpgEquipSlot Slot = EArpgEquipSlot::MainHand;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") EArpgHandedness Handedness = EArpgHandedness::None;

	// Gameplay tag names separated by "|", e.g. "Item.Weapon.Melee".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FString ItemTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements") int32 RequiredLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements") int32 ReqStrength = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements") int32 ReqDexterity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements") int32 ReqIntelligence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float PhysicalMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float PhysicalMax = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float AttacksPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float CritChance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence") float Armour = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence") float Evasion = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence") float EnergyShield = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence") float BlockChance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Implicit") FText ImplicitText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Implicit") FName ImplicitStat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Implicit") float ImplicitMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Implicit") float ImplicitMax = 0.f;

	// Size in a Path of Exile-style grid inventory (used later).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory") int32 InventoryWidth = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory") int32 InventoryHeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops") bool bCanDrop = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops") int32 DropWeight = 1000;
};
