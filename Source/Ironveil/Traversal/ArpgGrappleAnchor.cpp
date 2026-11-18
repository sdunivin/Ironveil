#include "Traversal/ArpgGrappleAnchor.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AArpgGrappleAnchor::AArpgGrappleAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Anchors are placed in the level, so every machine already has them; no need to replicate.
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LandingPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LandingPoint"));
	LandingPoint->SetupAttachment(Root);
	LandingPoint->SetRelativeLocation(FVector(80.f, 0.f, 110.f));

#if WITH_EDITORONLY_DATA
	EditorIcon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorIcon"));
	if (EditorIcon)
	{
		EditorIcon->SetupAttachment(Root);
		EditorIcon->bIsScreenSizeScaled = true;
	}
#endif
}

FVector AArpgGrappleAnchor::GetHookLocation() const
{
	return Root->GetComponentLocation();
}

FVector AArpgGrappleAnchor::GetLandingLocation() const
{
	return LandingPoint->GetComponentLocation();
}
