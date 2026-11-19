// ArpgLootSubsystem.h
// Loads Data/DT_Affixes.csv and Data/DT_ItemBases.csv and rolls items with the GDD's rules
// (the same rules as Tools/loot_sim.py, so the simulator stays a valid balance check).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loot/ArpgItem.h"
#include "ArpgLootSubsystem.generated.h"

UCLASS()
class IRONVEIL_API UArpgLootSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UArpgLootSubsystem* Get(const UObject* WorldContext);

	const FArpgItemBaseRow* FindBase(FName Name) const { return Bases.Find(Name); }
	bool IsLoaded() const { return Bases.Num() > 0; }

	// A normal (white) copy of a base, used for starting gear.
	FArpgItem MakeBaseItem(FName BaseName, int32 ItemLevel = 1) const;

	// Rolls a random droppable item. IncRarity is "% increased item rarity" from the floor and the player.
	FArpgItem RollItem(FRandomStream& Rng, int32 ItemLevel, float IncRarity, EArpgRarity MinRarity = EArpgRarity::Normal) const;

	// Rolls a specific base at a specific rarity (crafting, tests, cheats).
	FArpgItem RollBase(FRandomStream& Rng, FName BaseName, int32 ItemLevel, EArpgRarity Rarity) const;

	FArpgItem RollUnique(FRandomStream& Rng, int32 ItemLevel) const;

	// Tooltip lines for HUD display.
	TArray<FString> DescribeItem(const FArpgItem& Item) const;
	FString SlotName(const FArpgItem& Item) const;

private:
	void LoadBases(const FString& Path);
	void LoadAffixes(const FString& Path);
	void AddAffixes(FRandomStream& Rng, FArpgItem& Item, const FArpgItemBaseRow& Base, int32 Count, int32 MaxPrefix, int32 MaxSuffix) const;
	static FString FormatAffix(const FString& Template, float V, float V2);

	TMap<FName, FArpgItemBaseRow> Bases;
	TArray<FName> BaseOrder;
	TMap<FName, FArpgAffixRow> Affixes;
	TArray<FName> AffixOrder;
};
