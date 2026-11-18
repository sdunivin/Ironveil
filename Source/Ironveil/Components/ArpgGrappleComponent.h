// Grappling hook (plan 1.6): aim at a grapple anchor and press Grapple to fly to it in an arc,
// with a rope drawn from your hand.
//
// Networking: the owning client launches itself right away so it feels instant, and asks the
// server to do the same. The server checks the request (range, anchor enabled) and launches
// the character there too; the rope shows on everyone's screen through a replicated value.
//
// Later (Phase 2.6): pull light enemies to you with the same component.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArpgGrappleComponent.generated.h"

class AArpgGrappleAnchor;
class UCableComponent;

UCLASS(ClassGroup = (Ironveil), meta = (BlueprintSpawnableComponent))
class IRONVEIL_API UArpgGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArpgGrappleComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Owning client: grapple to the anchor currently aimed at, if any.
	UFUNCTION(BlueprintCallable, Category = "Ironveil|Grapple")
	void TryGrapple();

	UFUNCTION(BlueprintPure, Category = "Ironveil|Grapple")
	AArpgGrappleAnchor* GetAimedAnchor() const { return AimedAnchor.Get(); }

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (Units = "cm"))
	float MaxRange = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (Units = "cm"))
	float MinRange = 250.f;

	// Anchors must be within this angle of the camera's centre.
	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (Units = "deg"))
	float MaxAimAngle = 25.f;

	// Shape of the flight: 0.5 is a medium arc, closer to 1 is flatter and more direct.
	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float ArcParam = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (Units = "s"))
	float Cooldown = 0.8f;

	// How long the rope stays visible after firing.
	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (Units = "s"))
	float RopeVisibleTime = 0.45f;

	// How often the aimed anchor is re-checked (for the highlight).
	UPROPERTY(EditAnywhere, Category = "Ironveil|Grapple", meta = (Units = "s"))
	float AimCheckInterval = 0.1f;

protected:
	UFUNCTION(Server, Reliable)
	void ServerGrapple(AArpgGrappleAnchor* Anchor);

	UFUNCTION()
	void OnRep_ActiveAnchor();

private:
	AArpgGrappleAnchor* FindBestAnchor() const;
	bool LaunchToward(const AArpgGrappleAnchor* Anchor);
	void UpdateAimedAnchor();
	void ShowRope(AArpgGrappleAnchor* Anchor);
	void HideRope();
	void ClearActiveAnchor();
	bool IsOwnerLocallyControlled() const;
	UCableComponent* GetCable() const;

	// The anchor we're currently swinging to; replicated so other players see the rope.
	UPROPERTY(ReplicatedUsing = OnRep_ActiveAnchor)
	TObjectPtr<AArpgGrappleAnchor> ActiveAnchor;

	TWeakObjectPtr<AArpgGrappleAnchor> AimedAnchor;
	double LastGrappleTime = -1000.0;
	FTimerHandle AimTimer;
	FTimerHandle RopeTimer;
};
