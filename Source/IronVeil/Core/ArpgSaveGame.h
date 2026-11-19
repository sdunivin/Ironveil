// ArpgSaveGame.h
// Ascent mode progress (GDD): class, level, gear and the highest sanctuary reached carry over between climbs.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Loot/ArpgItem.h"
#include "ArpgSaveGame.generated.h"

UCLASS()
class IRONVEIL_API UArpgSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;

	UPROPERTY(SaveGame) EArpgClass Class = EArpgClass::Warrior;
	UPROPERTY(SaveGame) int32 Level = 1;
	UPROPERTY(SaveGame) float XP = 0.f;
	UPROPERTY(SaveGame) TArray<FArpgItem> Inventory;
	UPROPERTY(SaveGame) TArray<FArpgItem> Equipped;
	UPROPERTY(SaveGame) int32 Checkpoint = 0;     // last sanctuary floor (0 = Greyhollow)
	UPROPERTY(SaveGame) int32 BestFloor = 0;
	UPROPERTY(SaveGame) int32 RunSeed = 0;
	UPROPERTY(SaveGame) bool bBeatFinalBoss = false;
};
