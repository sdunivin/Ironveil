#include "Core/ArpgGameMode.h"

#include "Characters/ArpgPlayerCharacter.h"
#include "Core/ArpgPlayerController.h"
#include "Core/ArpgPlayerState.h"

AArpgGameMode::AArpgGameMode()
{
	DefaultPawnClass = AArpgPlayerCharacter::StaticClass();
	PlayerControllerClass = AArpgPlayerController::StaticClass();
	PlayerStateClass = AArpgPlayerState::StaticClass();
}
