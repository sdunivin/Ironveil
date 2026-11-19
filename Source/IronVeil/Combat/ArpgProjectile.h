// ArpgProjectile.h
// One projectile class for every arrow, bolt, orb and wave. Moves itself (so slow-time zones affect it),
// hits hostile characters with a sphere check, and can pierce, explode and home.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ArpgTypes.h"
#include "ArpgProjectile.generated.h"

class AArpgCharacterBase;
class UStaticMeshComponent;
class UPointLightComponent;
enum class EArpgShape : uint8;

struct FArpgProjectileParams
{
	FArpgHit Hit;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 25.f;              // hit radius
	FVector VisualScale = FVector(0.3f);
	EArpgShape Shape;
	FLinearColor Color = FLinearColor::White;
	float Glow = 3.f;
	int32 Pierce = 0;                 // extra enemies it passes through; < 0 = pierces everything
	float ExplodeRadius = 0.f;
	float Lifetime = 2.5f;
	float HomingStrength = 0.f;       // turn rate toward Target, in 1/s
	TWeakObjectPtr<AArpgCharacterBase> Target;
	bool bIgnoreWalls = false;
	float HealOwnerFraction = 0.f;    // Palm of the Still Lake
	FArpgProjectileParams();
};

UCLASS()
class IRONVEIL_API AArpgProjectile : public AActor
{
	GENERATED_BODY()

public:
	AArpgProjectile();

	static AArpgProjectile* Launch(AArpgCharacterBase* Owner, const FVector& Start, const FArpgProjectileParams& Params);

	virtual void Tick(float DeltaSeconds) override;

	void ApplyZoneSlow(float Factor) { ZoneSlow = FMath::Min(ZoneSlow, Factor); ZoneSlowTimer = 0.2f; }
	EArpgTeam GetTeam() const { return Team; }

private:
	void Explode(const FVector& At);
	void HitTarget(AArpgCharacterBase* Target);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;

	FArpgProjectileParams P;
	TWeakObjectPtr<AArpgCharacterBase> OwnerChar;
	EArpgTeam Team = EArpgTeam::Enemy;
	TSet<const AArpgCharacterBase*> AlreadyHit; // identity only, never dereferenced
	float Age = 0.f;
	float ZoneSlow = 1.f;
	float ZoneSlowTimer = 0.f;
	bool bDone = false;
};
