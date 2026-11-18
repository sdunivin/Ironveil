#include "Core/ArpgPlayerController.h"

AArpgPlayerController::AArpgPlayerController()
{
}

void AArpgPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Gameplay: hide the mouse cursor and send all input to the game.
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}
