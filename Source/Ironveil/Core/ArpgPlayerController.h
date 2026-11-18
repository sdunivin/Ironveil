// The player's controller. Mostly empty for now; later it owns the HUD, menus and the
// switch between gameplay and menu input (plan 1.7, Phase 8).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArpgPlayerController.generated.h"

UCLASS()
class IRONVEIL_API AArpgPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArpgPlayerController();

protected:
	virtual void BeginPlay() override;
};
