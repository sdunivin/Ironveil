// A point the grappling hook can latch onto (plan 1.6). Drop these into a level on ledges,
// beams and towers. The player flies toward LandingPoint, so move that slightly above and past
// the ledge edge where you want them to arrive.
//
// Make a Blueprint child (BP_GrappleAnchor) to give it a mesh and a glow when targeted
// (implement the "On Highlight Changed" event).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArpgGrappleAnchor.generated.h"

class UBillboardComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class IRONVEIL_API AArpgGrappleAnchor : public AActor
{
	GENERATED_BODY()

public:
	AArpgGrappleAnchor();

	// Where the rope attaches.
	FVector GetHookLocation() const;

	// Where the player should end up.
	FVector GetLandingLocation() const;

	bool IsGrappleEnabled() const { return bGrappleEnabled; }

	// Called on the local player's machine when this becomes / stops being the aimed anchor.
	UFUNCTION(BlueprintImplementableEvent, Category = "Ironveil|Grapple")
	void OnHighlightChanged(bool bHighlighted);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Grapple")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Grapple")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil|Grapple")
	TObjectPtr<USceneComponent> LandingPoint;

	// Turn off to disable an anchor (e.g. until a lever is pulled).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironveil|Grapple")
	bool bGrappleEnabled = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif
};
