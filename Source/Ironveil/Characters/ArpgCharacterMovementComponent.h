// Character movement with a co-op-safe sprint.
//
// Why this exists: if a client just raises MaxWalkSpeed to sprint, the server still thinks it
// walks at normal speed and keeps snapping the player back ("rubber-banding"). Here the client
// sends a "wants to sprint" flag with every movement update, so client and server run the same
// movement and agree.
//
// This is the standard Unreal pattern (custom saved move + compressed flag). Use the same
// approach for any future movement mode that changes speed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ArpgCharacterMovementComponent.generated.h"

UCLASS()
class IRONVEIL_API UArpgCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UArpgCharacterMovementComponent();

	// Ground speed while sprinting, in cm/s. MaxWalkSpeed is the normal jog speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironveil|Sprint", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 700.f;

	// Called on the owning client when the sprint button is held / released.
	UFUNCTION(BlueprintCallable, Category = "Ironveil|Sprint")
	void SetSprinting(bool bNewSprinting);

	// True when the player wants to sprint AND is allowed to (on the ground, has stamina).
	UFUNCTION(BlueprintPure, Category = "Ironveil|Sprint")
	bool IsSprinting() const;

	bool WantsToSprint() const { return bWantsToSprint; }

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
	bool CanSprintNow() const;

	uint8 bWantsToSprint : 1;

	// One recorded movement step, with our extra sprint flag.
	class FSavedMove_Arpg : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;

		bool bSavedWantsToSprint = false;
	};

	// Tells the movement system to record FSavedMove_Arpg instead of the default move.
	class FNetworkPredictionData_Client_Arpg : public FNetworkPredictionData_Client_Character
	{
	public:
		typedef FNetworkPredictionData_Client_Character Super;

		explicit FNetworkPredictionData_Client_Arpg(const UCharacterMovementComponent& ClientMovement);
		virtual FSavedMovePtr AllocateNewMove() override;
	};
};
