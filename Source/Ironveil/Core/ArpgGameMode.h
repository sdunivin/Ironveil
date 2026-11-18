// The rules of a play session; exists only on the server. Sets which classes are used for
// players. Make BP_ArpgGameMode from this and point Default Pawn Class at BP_PlayerCharacter.
// Later: tower runs, floor transitions, party wipes, timed climbs.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArpgGameMode.generated.h"

UCLASS()
class IRONVEIL_API AArpgGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArpgGameMode();
};
