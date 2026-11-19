#include "Tower/ArpgInteractables.h"

#include "Characters/ArpgPlayerCharacter.h"
#include "Combat/ArpgAreaEffect.h"
#include "Combat/ArpgCombat.h"
#include "Core/ArpgAssets.h"
#include "Core/ArpgGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

AArpgInteractable::AArpgInteractable()
{
	// Ticking is registered for every interactable but only switched on by the ones that animate.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0, 0, 120));
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(500.f);
	Light->SetCastShadows(false);
}

AArpgGameMode* AArpgInteractable::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArpgGameMode>() : nullptr;
}

UStaticMeshComponent* AArpgInteractable::AddShape(EArpgShape Shape, const FVector& Loc, const FVector& Scale, const FLinearColor& Color,
	float Emissive, const FRotator& Rot, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ArpgAssets::Shape(Shape));
	C->SetupAttachment(Root);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollide) { C->SetCollisionProfileName(TEXT("BlockAll")); }
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	C->SetMaterial(0, ArpgAssets::SharedColor(Color, Emissive));
	return C;
}

// ---------------------------------------------------------------- loot

AArpgLootDrop* AArpgLootDrop::SpawnDrop(UWorld* World, const FVector& Location, const FArpgItem& Item)
{
	if (!World || !Item.IsValid()) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArpgLootDrop* D = World->SpawnActor<AArpgLootDrop>(AArpgLootDrop::StaticClass(), Location + FVector(0, 0, 60), FRotator::ZeroRotator, SP);
	if (!D) { return nullptr; }
	D->Item = Item;
	D->InteractRadius = 220.f;
	D->SetActorTickEnabled(true);
	D->Velocity = FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 550.f);
	D->Build();
	return D;
}

void AArpgLootDrop::Build()
{
	const FLinearColor C = ArpgColors::Rarity(Item.Rarity);
	AddShape(EArpgShape::Cube, FVector(0, 0, 8), FVector(0.25f, 0.18f, 0.12f), C * 0.8f, 1.5f, FRotator(0, FMath::FRandRange(0.f, 360.f), 0));
	const float BeamHeight = Item.Rarity == EArpgRarity::Normal ? 0.f : (Item.Rarity == EArpgRarity::Magic ? 2.f : (Item.Rarity == EArpgRarity::Rare ? 4.f : 7.f));
	if (BeamHeight > 0.f)
	{
		AddShape(EArpgShape::Cylinder, FVector(0, 0, BeamHeight * 50.f), FVector(0.04f, 0.04f, BeamHeight), C, 6.f);
	}
	Light->SetLightColor(C);
	Light->SetIntensity(Item.Rarity == EArpgRarity::Normal ? 300.f : 1500.f + 1500.f * (int32)Item.Rarity);
	Light->SetAttenuationRadius(300.f);
	Light->SetRelativeLocation(FVector(0, 0, 40));
}

void AArpgLootDrop::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	if (bLanded) { return; }
	// A short toss so drops scatter instead of stacking.
	Velocity.Z -= 2000.f * Dt;
	const FVector From = GetActorLocation();
	const FVector To = From + Velocity * Dt;
	FHitResult Hit;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArpgLoot), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To - FVector(0, 0, 5), Obj, Q))
	{
		if (Hit.ImpactNormal.Z > 0.5f)
		{
			SetActorLocation(Hit.ImpactPoint);
			bLanded = true;
			SetActorTickEnabled(false);
			return;
		}
		Velocity.X = Velocity.Y = 0.f;
	}
	SetActorLocation(To);
	if (Age > 3.f) { bLanded = true; SetActorTickEnabled(false); }
}

FString AArpgLootDrop::GetPrompt(const AArpgPlayerCharacter* Player) const
{
	return FString::Printf(TEXT("Pick up %s"), *Item.DisplayName);
}

void AArpgLootDrop::Interact(AArpgPlayerCharacter* Player)
{
	if (!Player) { return; }
	Player->PickUp(Item);
	Destroy();
}

// ---------------------------------------------------------------- chest

void AArpgChest::Init(bool bInLocked, int32 InItemLevel, float InRarity)
{
	bLocked = bInLocked;
	ItemLevel = InItemLevel;
	Rarity = InRarity;
	InteractRadius = 260.f;
	const FLinearColor Wood(0.35f, 0.2f, 0.1f);
	const FLinearColor Trim = bLocked ? FLinearColor(0.75f, 0.6f, 0.25f) : FLinearColor(0.35f, 0.35f, 0.38f);
	AddShape(EArpgShape::Cube, FVector(0, 0, 35), FVector(1.1f, 0.7f, 0.7f), Wood, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Cube, FVector(0, 0, 35), FVector(1.14f, 0.15f, 0.74f), Trim, 0.3f);
	Lid = AddShape(EArpgShape::Cube, FVector(0, 0, 78), FVector(1.15f, 0.75f, 0.18f), Wood * 1.2f);
	if (bLocked)
	{
		AddShape(EArpgShape::Cube, FVector(37, 0, 55), FVector(0.06f, 0.18f, 0.22f), Trim, 1.f);
		Light->SetLightColor(FLinearColor(1.f, 0.8f, 0.3f));
		Light->SetIntensity(1200.f);
	}
}

FString AArpgChest::GetPrompt(const AArpgPlayerCharacter* Player) const
{
	if (!bLocked) { return TEXT("Open chest"); }
	return Player && Player->GetClass() == EArpgClass::Thief ? TEXT("Locked coffer: use Lockpick (4) for the best loot, or force it open")
		: TEXT("Force open the locked coffer (a Thief could pick it for better loot)");
}

void AArpgChest::Interact(AArpgPlayerCharacter* Player)
{
	Open(Player, false);
}

void AArpgChest::Open(AArpgPlayerCharacter* Player, bool bPicked)
{
	if (bOpened) { return; }
	bOpened = true;
	if (Lid) { Lid->SetRelativeLocationAndRotation(FVector(-30, 0, 100), FRotator(0, 0, 0) + FRotator(-70, 0, 0)); }
	Light->SetIntensity(0.f);
	if (AArpgGameMode* GM = GetGM())
	{
		// GDD: floor chest = 3 items. Locked coffers: 2 when forced, 4 (at least magic) when picked.
		int32 Count = 3;
		EArpgRarity Min = EArpgRarity::Normal;
		if (bLocked) { Count = bPicked ? 4 : 2; Min = bPicked ? EArpgRarity::Magic : EArpgRarity::Normal; }
		GM->DropLoot(GetActorLocation() + FVector(0, 0, 60), ItemLevel, Count, Min, Rarity + (bPicked ? 50.f : 0.f));
	}
	AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation() + FVector(0, 0, 60), 120.f, FLinearColor(1.f, 0.85f, 0.4f));
}

// ---------------------------------------------------------------- portal

void AArpgExitPortal::Init(const FLinearColor& InAccent, bool bInHub)
{
	Accent = InAccent;
	bHub = bInHub;
	InteractRadius = 320.f;
	SetActorTickEnabled(true);
	const FLinearColor Stone(0.3f, 0.3f, 0.33f);
	// Stair base and arch
	AddShape(EArpgShape::Cylinder, FVector(0, 0, 15), FVector(4.f, 4.f, 0.3f), Stone, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Cylinder, FVector(0, 0, 40), FVector(3.f, 3.f, 0.3f), Stone * 1.2f, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Cube, FVector(0, -170, 230), FVector(0.5f, 0.5f, 4.2f), Stone, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Cube, FVector(0, 170, 230), FVector(0.5f, 0.5f, 4.2f), Stone, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Cube, FVector(0, 0, 460), FVector(0.6f, 4.0f, 0.5f), Stone * 0.9f, 0.f, FRotator::ZeroRotator, true);
	Core = AddShape(EArpgShape::Cylinder, FVector(0, 0, 240), FVector(2.8f, 2.8f, 0.05f), FLinearColor(0.05f, 0.05f, 0.06f), 0.f, FRotator(0, 0, 90));
	for (int32 i = 0; i < 3; ++i)
	{
		Rings.Add(AddShape(EArpgShape::Cylinder, FVector(0, 0, 240), FVector(3.0f - i * 0.6f, 3.0f - i * 0.6f, 0.02f), Accent * 0.2f, 0.f, FRotator(0, 0, 90)));
	}
	Light->SetRelativeLocation(FVector(80, 0, 240));
	Light->SetLightColor(Accent);
	Light->SetAttenuationRadius(1200.f);
	SetOpen(bHub);
}

void AArpgExitPortal::SetOpen(bool bOpen)
{
	bIsOpen = bOpen;
	if (Core) { Core->SetMaterial(0, ArpgAssets::SharedColor(bOpen ? Accent : FLinearColor(0.05f, 0.05f, 0.06f), bOpen ? 6.f : 0.f)); }
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		Rings[i]->SetMaterial(0, ArpgAssets::SharedColor(bOpen ? FMath::Lerp(Accent, FLinearColor::White, i * 0.25f) : Accent * 0.2f, bOpen ? 3.f : 0.f));
	}
	Light->SetIntensity(bOpen ? 12000.f : 0.f);
}

void AArpgExitPortal::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bIsOpen) { return; }
	Spin += Dt;
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		const float S = (3.0f - i * 0.6f) * (1.f + 0.05f * FMath::Sin(Spin * 3.f + i));
		Rings[i]->SetRelativeScale3D(FVector(S, S, 0.02f));
	}
}

FString AArpgExitPortal::GetPrompt(const AArpgPlayerCharacter* Player) const
{
	return bHub ? TEXT("Enter the Veil Spire") : TEXT("Climb to the next floor");
}

void AArpgExitPortal::Interact(AArpgPlayerCharacter* Player)
{
	if (!bIsOpen) { return; }
	if (AArpgGameMode* GM = GetGM())
	{
		if (bHub) { GM->EnterSpire(); }
		else { GM->AdvanceFloor(); }
	}
}

// ---------------------------------------------------------------- shrine

void AArpgShrine::Init(const FLinearColor& Accent)
{
	InteractRadius = 300.f;
	const FLinearColor Stone(0.55f, 0.55f, 0.52f);
	AddShape(EArpgShape::Cylinder, FVector(0, 0, 20), FVector(2.2f, 2.2f, 0.4f), Stone, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Cylinder, FVector(0, 0, 110), FVector(0.5f, 0.5f, 1.6f), Stone * 1.1f, 0.f, FRotator::ZeroRotator, true);
	AddShape(EArpgShape::Sphere, FVector(0, 0, 220), FVector(0.6f), Accent, 6.f);
	Light->SetRelativeLocation(FVector(0, 0, 230));
	Light->SetLightColor(Accent);
	Light->SetIntensity(8000.f);
	Light->SetAttenuationRadius(900.f);
}

void AArpgShrine::Interact(AArpgPlayerCharacter* Player)
{
	if (!Player) { return; }
	Player->FullRestore();
	Player->ClearAilments();
	Player->Message(TEXT("The shrine restores you. Progress saved."));
	AArpgAreaEffect::SpawnFlash(GetWorld(), Player->GetActorLocation(), 220.f, FLinearColor(0.6f, 0.9f, 1.f));
	if (AArpgGameMode* GM = GetGM()) { GM->SaveProgress(); }
}

// ---------------------------------------------------------------- relic

void AArpgVeilRelic::Init(const FLinearColor& Accent, float InDuration)
{
	Duration = Remaining = InDuration;
	InteractRadius = 300.f;
	SetActorTickEnabled(true);
	const FLinearColor Brass(0.6f, 0.45f, 0.2f);
	AddShape(EArpgShape::Cylinder, FVector(0, 0, 20), FVector(2.5f, 2.5f, 0.4f), Brass * 0.6f, 0.f, FRotator::ZeroRotator, true);
	Crystal = AddShape(EArpgShape::Cone, FVector(0, 0, 160), FVector(0.7f, 0.7f, 2.2f), Accent, 2.f);
	AddShape(EArpgShape::Cone, FVector(0, 0, 160), FVector(0.7f, 0.7f, 2.2f), Accent * 0.7f, 1.f, FRotator(180, 0, 0));
	Light->SetRelativeLocation(FVector(0, 0, 180));
	Light->SetLightColor(Accent);
	Light->SetIntensity(3000.f);
	Light->SetAttenuationRadius(800.f);
}

void AArpgVeilRelic::Interact(AArpgPlayerCharacter* Player)
{
	if (bActive || bDone) { return; }
	bActive = true;
	WaveTimer = 0.5f;
	Light->SetIntensity(15000.f);
	if (AArpgGameMode* GM = GetGM()) { GM->Announce(TEXT("The relic wakes. Hold your ground!"), FLinearColor(0.5f, 0.8f, 1.f)); }
}

void AArpgVeilRelic::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Crystal) { Crystal->AddRelativeRotation(FRotator(0.f, (bActive ? 120.f : 20.f) * Dt, 0.f)); }
	if (!bActive) { return; }
	Remaining -= Dt;
	WaveTimer -= Dt;
	AArpgGameMode* GM = GetGM();
	if (WaveTimer <= 0.f)
	{
		WaveTimer = 9.f;
		if (GM) { GM->SpawnRelicWave(GetActorLocation()); }
	}
	if (Remaining <= 0.f)
	{
		bActive = false;
		bDone = true;
		Light->SetIntensity(3000.f);
		if (GM) { GM->OnRelicHeld(); }
	}
}
