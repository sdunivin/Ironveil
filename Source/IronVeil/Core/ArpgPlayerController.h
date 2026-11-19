// ArpgPlayerController.h
// Owns the HUD and the developer console cheats for testing the climb. Open the console with the ` key:
//   SpireFloor 10      jump to floor 10 (guardian), SpireFloor 100 for the Hourless King
//   SpireLevel 30      set character level
//   SpireGear          equip a full set of rares for your level
//   SpireGod           toggle invulnerability
//   SpireMighty        toggle x25 damage
//   SpireKillAll       kill every monster on the floor
//   SpireSeed 1234     restart the climb with a fixed run seed (same seed = same floors)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArpgPlayerController.generated.h"

UCLASS()
class IRONVEIL_API AArpgPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UFUNCTION(Exec) void SpireFloor(int32 Floor);
	UFUNCTION(Exec) void SpireLevel(int32 Level);
	UFUNCTION(Exec) void SpireGear();
	UFUNCTION(Exec) void SpireGod();
	UFUNCTION(Exec) void SpireMighty();
	UFUNCTION(Exec) void SpireKillAll();
	UFUNCTION(Exec) void SpireSeed(int32 Seed);
};
