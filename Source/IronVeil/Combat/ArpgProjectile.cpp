#include "Combat/ArpgProjectile.h"

#include "Characters/ArpgCharacterBase.h"
#include "Combat/ArpgCombat.h"
#include "Combat/ArpgAreaEffect.h"
#include "Core/ArpgAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

FArpgProjectileParams::FArpgProjectileParams()
	: Shape(EArpgShape::Sphere)
{
}

AArpgProjectile::AArpgProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetIntensity(3000.f);
	Light->SetAttenuationRadius(450.f);
	Light->SetCastShadows(false);
}

AArpgProjectile* AArpgProjectile::Launch(AArpgCharacterBase* Owner, const FVector& Start, const FArpgProjectileParams& Params)
{
	if (!Owner || !Owner->GetWorld()) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArpgProjectile* Proj = Owner->GetWorld()->SpawnActor<AArpgProjectile>(AArpgProjectile::StaticClass(), Start,
		Params.Velocity.Rotation(), SP);
	if (!Proj) { return nullptr; }
	Proj->P = Params;
	Proj->OwnerChar = Owner;
	Proj->Team = Owner->Team;
	Proj->P.Hit.SourceLevel = Owner->Level;
	Proj->Visual->SetStaticMesh(ArpgAssets::Shape(Params.Shape));
	Proj->Visual->SetRelativeScale3D(Params.VisualScale);
	Proj->Visual->SetMaterial(0, ArpgAssets::SharedColor(Params.Color, Params.Glow));
	Proj->Light->SetLightColor(Params.Color);
	return Proj;
}

void AArpgProjectile::Tick(float Dt)
{
	Super::Tick(Dt);
	ZoneSlowTimer -= GetWorld()->GetDeltaSeconds();
	if (ZoneSlowTimer <= 0.f) { ZoneSlow = 1.f; }
	CustomTimeDilation = ZoneSlow;
	if (bDone) { return; }

	Age += Dt;
	if (Age > P.Lifetime)
	{
		if (P.ExplodeRadius > 0.f) { Explode(GetActorLocation()); }
		Destroy();
		return;
	}

	if (P.HomingStrength > 0.f && P.Target.IsValid() && !P.Target->IsDead())
	{
		const FVector ToT = (P.Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		const float Speed = P.Velocity.Size();
		const FVector NewDir = FMath::Lerp(P.Velocity.GetSafeNormal(), ToT, FMath::Clamp(P.HomingStrength * Dt, 0.f, 1.f)).GetSafeNormal();
		P.Velocity = NewDir * Speed;
	}

	const FVector From = GetActorLocation();
	const FVector To = From + P.Velocity * Dt;

	if (!P.bIgnoreWalls)
	{
		FHitResult Wall;
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ArpgProjectile), false, this);
		if (GetWorld()->LineTraceSingleByObjectType(Wall, From, To, Obj, Q))
		{
			if (P.ExplodeRadius > 0.f) { Explode(Wall.ImpactPoint); }
			Destroy();
			return;
		}
	}

	SetActorLocationAndRotation(To, P.Velocity.Rotation());

	for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), To, P.Radius, Team))
	{
		if (AlreadyHit.Contains(T)) { continue; }
		// FindTargets is a flat check; keep projectiles from hitting through the floor or far above heads.
		const float ZDelta = FMath::Abs(T->GetActorLocation().Z - To.Z);
		if (ZDelta > T->GetSimpleCollisionHalfHeight() + P.Radius) { continue; }
		AlreadyHit.Add(T);
		HitTarget(T);
		if (P.ExplodeRadius > 0.f)
		{
			Explode(To);
			Destroy();
			return;
		}
		if (P.Pierce == 0)
		{
			Destroy();
			return;
		}
		if (P.Pierce > 0) { --P.Pierce; }
	}
}

void AArpgProjectile::HitTarget(AArpgCharacterBase* Target)
{
	FArpgHit Hit = P.Hit;
	if (Hit.Knockback.IsNearlyZero() == false)
	{
		Hit.Knockback = P.Velocity.GetSafeNormal2D() * Hit.Knockback.Size();
	}
	float Dealt = 0.f;
	if (AArpgCharacterBase* Owner = OwnerChar.Get())
	{
		Dealt = Owner->DealHit(Target, Hit);
		if (P.HealOwnerFraction > 0.f && Dealt > 0.f) { Owner->Heal(Dealt * P.HealOwnerFraction, false); }
	}
	else
	{
		Target->ReceiveHit(Hit, nullptr);
	}
}

void AArpgProjectile::Explode(const FVector& At)
{
	if (bDone) { return; }
	bDone = true;
	AArpgCharacterBase* Owner = OwnerChar.Get();
	for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), At, P.ExplodeRadius, Team))
	{
		if (AlreadyHit.Contains(T)) { continue; }
		AlreadyHit.Add(T);
		if (Owner) { Owner->DealHit(T, P.Hit); }
		else { T->ReceiveHit(P.Hit, nullptr); }
	}
	AArpgAreaEffect::SpawnFlash(GetWorld(), At, P.ExplodeRadius, P.Color);
}
