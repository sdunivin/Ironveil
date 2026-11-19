// ArpgAreaEffect.h
// Ground effects: telegraphed bursts (every boss slam shows its circle first), expanding shockwaves
// (jump or dodge through the ring), slow-time fields, smoke clouds and explosion flashes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ArpgTypes.h"
#include "ArpgAreaEffect.generated.h"

class AArpgCharacterBase;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;

enum class EArpgAreaKind : uint8
{
	Flash,       // visual only
	Burst,       // telegraph circle for Delay seconds, then one hit to everything inside
	Shockwave,   // ring grows to Radius over Duration; hits grounded targets as it passes
	SlowField,   // hostile characters and projectiles inside move at SlowFactor speed
	Smoke        // enemies inside lose track of the player
};

struct FArpgAreaParams
{
	EArpgAreaKind Kind = EArpgAreaKind::Burst;
	float Radius = 300.f;
	float Delay = 0.8f;
	float Duration = 1.f;
	float SlowFactor = 0.4f;
	FArpgHit Hit;
	FLinearColor Color = FLinearColor(1.f, 0.3f, 0.1f);
	bool bFollowOwner = false;
};

UCLASS()
class IRONVEIL_API AArpgAreaEffect : public AActor
{
	GENERATED_BODY()

public:
	AArpgAreaEffect();

	static AArpgAreaEffect* Spawn(AArpgCharacterBase* Owner, const FVector& Location, const FArpgAreaParams& Params);
	static void SpawnFlash(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Color);
	// Snap a point to the floor below it.
	static FVector GroundPoint(const UWorld* World, const FVector& Location);

	virtual void Tick(float DeltaSeconds) override;

	EArpgTeam GetTeam() const { return Team; }

private:
	void Init(AArpgCharacterBase* Owner, const FArpgAreaParams& Params);
	void BuildRing(int32 Segments);
	void UpdateRing(float R, float Thickness, float Height);
	void Detonate();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Fill;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Blob;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ring;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;

	FArpgAreaParams P;
	TWeakObjectPtr<AArpgCharacterBase> OwnerChar;
	EArpgTeam Team = EArpgTeam::Enemy;
	TSet<const AArpgCharacterBase*> AlreadyHit; // identity only
	float Age = 0.f;
	bool bDetonated = false;
};
