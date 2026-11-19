// ArpgItem.h
// A rolled item instance. Saved in UArpgSaveGame, so every field is a UPROPERTY.

#pragma once

#include "CoreMinimal.h"
#include "Data/ArpgTypes.h"
#include "Data/ArpgLootRows.h"
#include "ArpgItem.generated.h"

USTRUCT(BlueprintType)
struct FArpgRolledAffix
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly) FName RowName;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName ModGroup;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName Stat;
	UPROPERTY(SaveGame, BlueprintReadOnly) EArpgAffixType AffixType = EArpgAffixType::Prefix;
	UPROPERTY(SaveGame, BlueprintReadOnly) EArpgModType ModType = EArpgModType::Flat;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 Tier = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Value = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Value2 = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) FString Text;
};

USTRUCT(BlueprintType)
struct FArpgItem
{
	GENERATED_BODY()

	// Row name in DT_ItemBases.csv. None = empty slot.
	UPROPERTY(SaveGame, BlueprintReadOnly) FName BaseName;
	UPROPERTY(SaveGame, BlueprintReadOnly) EArpgRarity Rarity = EArpgRarity::Normal;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 ItemLevel = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(SaveGame, BlueprintReadOnly) float ImplicitValue = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FArpgRolledAffix> Affixes;
	// Uniques: a build-changing rule the gameplay code checks by name (e.g. "VarricksGrip").
	UPROPERTY(SaveGame, BlueprintReadOnly) FName UniqueEffect;
	UPROPERTY(SaveGame, BlueprintReadOnly) FString UniqueText;

	bool IsValid() const { return !BaseName.IsNone(); }
};
