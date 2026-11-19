#include "Combat/ArpgAreaEffect.h"

#include "Characters/ArpgCharacterBase.h"
#include "Characters/ArpgEnemyCharacter.h"
#include "Combat/ArpgCombat.h"
#include "Combat/ArpgProjectile.h"
#include "Core/ArpgAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	constexpr int32 RingSegments = 40;
}

AArpgAreaEffect::AArpgAreaEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeMesh = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetVisibility(false);
		return C;
	};
	Disc = MakeMesh(TEXT("Disc"));
	Fill = MakeMesh(TEXT("Fill"));
	Blob = MakeMesh(TEXT("Blob"));

	Ring = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Root);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0, 0, 80));
	Light->SetIntensity(0.f);
	Light->SetCastShadows(false);
}

FVector AArpgAreaEffect::GroundPoint(const UWorld* World, const FVector& Location)
{
	if (!World) { return Location; }
	FHitResult Hit;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArpgGround), false);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 150), Location - FVector(0, 0, 2000), Obj, Q))
	{
		return Hit.ImpactPoint + FVector(0, 0, 3);
	}
	return Location;
}

AArpgAreaEffect* AArpgAreaEffect::Spawn(AArpgCharacterBase* Owner, const FVector& Location, const FArpgAreaParams& Params)
{
	if (!Owner || !Owner->GetWorld()) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector At = GroundPoint(Owner->GetWorld(), Location);
	AArpgAreaEffect* A = Owner->GetWorld()->SpawnActor<AArpgAreaEffect>(AArpgAreaEffect::StaticClass(), At, FRotator::ZeroRotator, SP);
	if (A) { A->Init(Owner, Params); }
	return A;
}

void AArpgAreaEffect::SpawnFlash(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Color)
{
	if (!World) { return; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArpgAreaEffect* A = World->SpawnActor<AArpgAreaEffect>(AArpgAreaEffect::StaticClass(), Location, FRotator::ZeroRotator, SP);
	if (!A) { return; }
	FArpgAreaParams FP;
	FP.Kind = EArpgAreaKind::Flash;
	FP.Radius = FMath::Max(80.f, Radius);
	FP.Duration = 0.25f;
	FP.Color = Color;
	A->Init(nullptr, FP);
}

void AArpgAreaEffect::Init(AArpgCharacterBase* Owner, const FArpgAreaParams& Params)
{
	P = Params;
	OwnerChar = Owner;
	Team = Owner ? Owner->Team : EArpgTeam::Player;
	if (Owner) { P.Hit.SourceLevel = Owner->Level; }

	const float R = P.Radius;
	Light->SetLightColor(P.Color);
	Light->SetAttenuationRadius(R * 1.6f);

	switch (P.Kind)
	{
	case EArpgAreaKind::Flash:
		Blob->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Sphere));
		Blob->SetMaterial(0, ArpgAssets::SharedColor(P.Color, 6.f));
		Blob->SetVisibility(true);
		Blob->SetRelativeScale3D(FVector(0.1f));
		Light->SetIntensity(20000.f);
		SetLifeSpan(P.Duration + 0.05f);
		break;

	case EArpgAreaKind::Burst:
		Disc->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Cylinder));
		Disc->SetMaterial(0, ArpgAssets::SharedColor(P.Color * 0.35f, 0.5f));
		Disc->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, 0.02f));
		Disc->SetVisibility(true);
		Fill->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Cylinder));
		Fill->SetMaterial(0, ArpgAssets::SharedColor(P.Color, 2.f));
		Fill->SetRelativeLocation(FVector(0, 0, 1.5f));
		Fill->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.02f));
		Fill->SetVisibility(true);
		BuildRing(RingSegments);
		UpdateRing(R, 14.f, 12.f);
		Light->SetIntensity(1500.f);
		break;

	case EArpgAreaKind::Shockwave:
		BuildRing(RingSegments);
		UpdateRing(10.f, 40.f, 60.f);
		Light->SetIntensity(4000.f);
		SetLifeSpan(P.Duration + 0.1f);
		break;

	case EArpgAreaKind::SlowField:
	case EArpgAreaKind::Smoke:
	{
		const bool bSmoke = P.Kind == EArpgAreaKind::Smoke;
		Disc->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Cylinder));
		Disc->SetMaterial(0, ArpgAssets::SharedColor(bSmoke ? FLinearColor(0.12f, 0.13f, 0.15f) : P.Color * 0.3f, bSmoke ? 0.f : 0.8f));
		Disc->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, 0.02f));
		Disc->SetVisibility(true);
		if (bSmoke)
		{
			// A cloud: a squashed grey sphere.
			Blob->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Sphere));
			Blob->SetMaterial(0, ArpgAssets::SharedColor(FLinearColor(0.32f, 0.34f, 0.38f)));
			Blob->SetRelativeScale3D(FVector(R / 50.f * 0.85f, R / 50.f * 0.85f, 2.2f));
			Blob->SetRelativeLocation(FVector(0, 0, 60));
			Blob->SetVisibility(true);
		}
		BuildRing(RingSegments);
		UpdateRing(R, 18.f, bSmoke ? 20.f : 220.f);
		Light->SetIntensity(bSmoke ? 0.f : 2500.f);
		SetLifeSpan(P.Duration);
		break;
	}
	}
}

void AArpgAreaEffect::BuildRing(int32 Segments)
{
	Ring->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Cube));
	Ring->SetMaterial(0, ArpgAssets::SharedColor(P.Color, 3.f));
	Ring->ClearInstances();
	for (int32 i = 0; i < Segments; ++i)
	{
		Ring->AddInstance(FTransform::Identity);
	}
}

void AArpgAreaEffect::UpdateRing(float R, float Thickness, float Height)
{
	const int32 N = Ring->GetInstanceCount();
	if (N == 0) { return; }
	const float SegLen = 2.f * PI * R / N * 1.05f;
	for (int32 i = 0; i < N; ++i)
	{
		const float A = 2.f * PI * i / N;
		const FVector Pos(FMath::Cos(A) * R, FMath::Sin(A) * R, Height * 0.5f);
		const FRotator Rot(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f);
		Ring->UpdateInstanceTransform(i, FTransform(Rot, Pos, FVector(SegLen / 100.f, Thickness / 100.f, Height / 100.f)), false, i == N - 1, true);
	}
}

void AArpgAreaEffect::Detonate()
{
	if (bDetonated) { return; }
	bDetonated = true;
	AArpgCharacterBase* Owner = OwnerChar.Get();
	for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), P.Radius, Team))
	{
		FArpgHit Hit = P.Hit;
		if (!Hit.Knockback.IsNearlyZero())
		{
			Hit.Knockback = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * Hit.Knockback.Size() + FVector(0, 0, 250);
		}
		if (Owner) { Owner->DealHit(T, Hit); }
		else { T->ReceiveHit(Hit, nullptr); }
	}
	SpawnFlash(GetWorld(), GetActorLocation() + FVector(0, 0, 30), P.Radius * 0.6f, P.Color);
	Destroy();
}

void AArpgAreaEffect::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;

	if (P.bFollowOwner)
	{
		if (AArpgCharacterBase* Owner = OwnerChar.Get())
		{
			SetActorLocation(GroundPoint(GetWorld(), Owner->GetActorLocation()));
		}
	}

	switch (P.Kind)
	{
	case EArpgAreaKind::Flash:
	{
		const float A = FMath::Clamp(Age / P.Duration, 0.f, 1.f);
		Blob->SetRelativeScale3D(FVector(P.Radius / 50.f * FMath::Sin(A * PI * 0.5f) + 0.05f));
		Light->SetIntensity(20000.f * (1.f - A));
		break;
	}
	case EArpgAreaKind::Burst:
	{
		// Telegraph fills over Delay, then detonates.
		const float A = FMath::Clamp(Age / FMath::Max(0.01f, P.Delay), 0.f, 1.f);
		const float S = P.Radius / 50.f * A;
		Fill->SetRelativeScale3D(FVector(FMath::Max(0.01f, S), FMath::Max(0.01f, S), 0.025f));
		if (Age >= P.Delay) { Detonate(); }
		break;
	}
	case EArpgAreaKind::Shockwave:
	{
		const float A = FMath::Clamp(Age / FMath::Max(0.01f, P.Duration), 0.f, 1.f);
		const float R = FMath::Lerp(20.f, P.Radius, A);
		UpdateRing(R, 40.f, 60.f);
		AArpgCharacterBase* Owner = OwnerChar.Get();
		for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), R + 10.f, Team))
		{
			if (AlreadyHit.Contains(T)) { continue; }
			const float D = FVector::Dist2D(T->GetActorLocation(), GetActorLocation());
			if (D < R - 120.f) { continue; } // already inside the ring
			// Jump over it: a target well above the ground is missed.
			const float FeetZ = T->GetActorLocation().Z - T->GetSimpleCollisionHalfHeight();
			if (FeetZ - GetActorLocation().Z > 70.f) { continue; }
			AlreadyHit.Add(T);
			FArpgHit Hit = P.Hit;
			Hit.Knockback = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 500.f + FVector(0, 0, 200);
			if (Owner) { Owner->DealHit(T, Hit); }
			else { T->ReceiveHit(Hit, nullptr); }
		}
		break;
	}
	case EArpgAreaKind::SlowField:
	{
		for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), P.Radius, Team))
		{
			T->ApplyZoneSlow(P.SlowFactor);
		}
		for (TActorIterator<AArpgProjectile> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < P.Radius && It->GetTeam() != Team)
			{
				It->ApplyZoneSlow(P.SlowFactor);
			}
		}
		break;
	}
	case EArpgAreaKind::Smoke:
	{
		for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), P.Radius, Team))
		{
			if (AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(T))
			{
				E->LoseTrack(0.5f);
			}
		}
		break;
	}
	}
}
