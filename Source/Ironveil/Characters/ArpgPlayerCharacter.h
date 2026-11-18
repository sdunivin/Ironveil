// The player character: over-the-shoulder camera, input, lock-on, grapple.
//
// Make a Blueprint child called BP_PlayerCharacter and set its mesh, Anim Blueprint, Input
// Actions and Startup Abilities there (see Docs/PHASE1-EDITOR-SETUP.md).
//
// Controls (GDD, keyboard / gamepad):
//   Move WASD / left stick       Look mouse / right stick
//   Dodge tap Space / B          Sprint hold Space / B
//   Jump Left Ctrl / A           Grapple Q / Y
//   Lock-on middle mouse / R3    Switch target: flick the mouse / right stick while locked on

#pragma once

#include "CoreMinimal.h"
#include "Characters/ArpgCharacterBase.h"
#include "GameplayTagContainer.h"
#include "ArpgPlayerCharacter.generated.h"

class UAnimMontage;
class UCameraComponent;
class UCameraShakeBase;
class UCableComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UArpgGrappleComponent;
class UArpgLockOnComponent;
struct FInputActionValue;

// Binds one Input Action to one ability Input Tag. Add a row per attack/skill button; the
// ability with the matching InputTag activates. No code needed for new skills.
USTRUCT(BlueprintType)
struct FArpgAbilityInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InputAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

UCLASS()
class IRONVEIL_API AArpgPlayerCharacter : public AArpgCharacterBase
{
	GENERATED_BODY()

public:
	AArpgPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UArpgLockOnComponent* GetLockOnComponent() const { return LockOn; }

protected:
	virtual void InitAbilityActorInfo() override;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Combat")
	TObjectPtr<UArpgLockOnComponent> LockOn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Traversal")
	TObjectPtr<UArpgGrappleComponent> Grapple;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Traversal")
	TObjectPtr<UCableComponent> GrappleCable;

	// ---- Input assets (create these in the editor and assign them on BP_PlayerCharacter) ----
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> JumpAction;

	// One button: tap to dodge, hold to sprint.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> DodgeSprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> LockOnAction;

	// Axis1D. Fires once per flick (Pressed trigger); negative value = left, positive = right.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> SwitchTargetAction;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TObjectPtr<UInputAction> GrappleAction;

	// Attack and skill buttons (Phase 2 onward).
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input")
	TArray<FArpgAbilityInputBinding> AbilityInputBindings;

	// How long Dodge/Sprint must be held before it counts as sprint instead of a dodge.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Input", meta = (Units = "s"))
	float SprintHoldThreshold = 0.2f;

	// ---- Camera feel ----
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Camera")
	float DefaultFieldOfView = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Camera")
	float SprintFieldOfView = 97.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Camera")
	float FieldOfViewInterpSpeed = 6.f;

	// ---- Landing ----
	// Falling faster than this (cm/s) when you land counts as a heavy landing.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Landing")
	float HeavyLandingSpeed = 1100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Landing")
	TObjectPtr<UAnimMontage> HeavyLandingMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Landing")
	TSubclassOf<UCameraShakeBase> HeavyLandingCameraShake;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnDodgeSprintStarted();
	void OnDodgeSprintHeld();
	void OnDodgeSprintReleased();
	void OnLockOnPressed();
	void OnSwitchTarget(const FInputActionValue& Value);
	void OnGrapplePressed();
	void AbilityInputPressed(FGameplayTag InputTag);
	void AbilityInputReleased(FGameplayTag InputTag);

	void UpdateRotationMode();
	void UpdateCameraFieldOfView(float DeltaSeconds);

	double DodgeSprintPressTime = 0.0;
	bool bDodgeSprintIsSprinting = false;
};
