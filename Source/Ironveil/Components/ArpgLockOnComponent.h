// Lock-on (plan 1.5): press to target the best enemy in view, flick the mouse or right stick
// to switch (the IA_SwitchTarget input action), press again to release. While locked on, the
// camera keeps the target in frame and the character strafes facing it.
//
// The owning client picks targets and drives the camera; the server is told the target so it
// rotates the character the same way (needed for co-op).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArpgLockOnComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArpgLockOnTargetChanged, AActor*, NewTarget);

UCLASS(ClassGroup = (Ironveil), meta = (BlueprintSpawnableComponent))
class IRONVEIL_API UArpgLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArpgLockOnComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Ironveil|LockOn")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "Ironveil|LockOn")
	void ClearTarget();

	// Direction: below 0 = switch to the nearest target on the left of the screen, above 0 = right.
	UFUNCTION(BlueprintCallable, Category = "Ironveil|LockOn")
	void SwitchTarget(float Direction);

	UFUNCTION(BlueprintPure, Category = "Ironveil|LockOn")
	bool IsLockedOn() const { return Target != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Ironveil|LockOn")
	AActor* GetTarget() const { return Target; }

	// Bind in Blueprint to show / move / hide the lock-on marker widget.
	UPROPERTY(BlueprintAssignable, Category = "Ironveil|LockOn")
	FArpgLockOnTargetChanged OnTargetChanged;

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "cm"))
	float MaxLockDistance = 2000.f;

	// Lock breaks if the target gets further than this.
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "cm"))
	float BreakDistance = 2600.f;

	// Lock breaks if the target stays hidden behind walls for this long.
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "s"))
	float LoseSightTime = 1.5f;

	// Only targets within this angle of the camera's centre can be picked.
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "deg"))
	float MaxAngleFromCamera = 45.f;

	// Aim at the chest, not the feet.
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "cm"))
	float TargetHeightOffset = 40.f;

	// Negative tilts the camera to look down at the target a little.
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "deg"))
	float CameraPitchOffset = -12.f;

	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn")
	float CameraInterpSpeed = 9.f;

	// Minimum time between two target switches.
	UPROPERTY(EditAnywhere, Category = "Ironveil|LockOn", meta = (Units = "s"))
	float SwitchCooldown = 0.3f;

protected:
	UFUNCTION()
	void OnRep_Target(AActor* OldTarget);

	UFUNCTION(Server, Reliable)
	void ServerSetTarget(AActor* NewTarget);

private:
	void SetTargetLocal(AActor* NewTarget);
	AActor* FindBestTarget() const;
	void GatherCandidates(TArray<AActor*>& OutCandidates) const;
	bool HasLineOfSight(const AActor* Candidate) const;
	bool IsValidTarget(const AActor* Candidate) const;
	FVector GetAimPoint(const AActor* Candidate) const;
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	class APlayerController* GetOwningPlayerController() const;
	bool IsOwnerLocallyControlled() const;

	UPROPERTY(ReplicatedUsing = OnRep_Target)
	TObjectPtr<AActor> Target;

	double LastSwitchTime = -1000.0;
	float TimeWithoutSight = 0.f;
};
