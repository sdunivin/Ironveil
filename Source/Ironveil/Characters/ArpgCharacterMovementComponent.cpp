#include "Characters/ArpgCharacterMovementComponent.h"

#include "Characters/ArpgCharacterBase.h"
#include "GameFramework/Character.h"

UArpgCharacterMovementComponent::UArpgCharacterMovementComponent()
{
	bWantsToSprint = false;

	// Starting feel; tune these on BP_PlayerCharacter's movement component.
	MaxWalkSpeed = 450.f;
	MaxAcceleration = 1800.f;
	BrakingDecelerationWalking = 2000.f;
	GroundFriction = 8.f;
	JumpZVelocity = 520.f;
	AirControl = 0.35f;
	RotationRate = FRotator(0.f, 540.f, 0.f);
	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
	NavAgentProps.bCanCrouch = true;
}

void UArpgCharacterMovementComponent::SetSprinting(bool bNewSprinting)
{
	bWantsToSprint = bNewSprinting;
}

bool UArpgCharacterMovementComponent::CanSprintNow() const
{
	if (!IsMovingOnGround() || IsCrouching())
	{
		return false;
	}

	const AArpgCharacterBase* Owner = Cast<AArpgCharacterBase>(CharacterOwner);
	return Owner && Owner->CanUseStamina();
}

bool UArpgCharacterMovementComponent::IsSprinting() const
{
	return bWantsToSprint && CanSprintNow();
}

float UArpgCharacterMovementComponent::GetMaxSpeed() const
{
	if (MovementMode == MOVE_Walking && IsSprinting())
	{
		return SprintSpeed;
	}
	return Super::GetMaxSpeed();
}

void UArpgCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Runs on the server when a client's move arrives.
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UArpgCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UArpgCharacterMovementComponent* MutableThis = const_cast<UArpgCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Arpg(*this);
	}
	return ClientPredictionData;
}

// ---------------------------------------------------------------- Saved move

void UArpgCharacterMovementComponent::FSavedMove_Arpg::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
}

uint8 UArpgCharacterMovementComponent::FSavedMove_Arpg::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

bool UArpgCharacterMovementComponent::FSavedMove_Arpg::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	// Two moves can only be merged into one network update if the sprint flag didn't change.
	const FSavedMove_Arpg* Other = static_cast<const FSavedMove_Arpg*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void UArpgCharacterMovementComponent::FSavedMove_Arpg::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UArpgCharacterMovementComponent* Move = Cast<UArpgCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
	}
}

void UArpgCharacterMovementComponent::FSavedMove_Arpg::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	// Used when the client replays moves after a correction from the server.
	if (UArpgCharacterMovementComponent* Move = Cast<UArpgCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
	}
}

// ---------------------------------------------------------------- Prediction data

UArpgCharacterMovementComponent::FNetworkPredictionData_Client_Arpg::FNetworkPredictionData_Client_Arpg(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr UArpgCharacterMovementComponent::FNetworkPredictionData_Client_Arpg::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Arpg());
}
