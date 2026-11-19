#include "Combat/ArpgCombat.h"

#include "Characters/ArpgCharacterBase.h"
#include "UI/ArpgHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "CollisionQueryParams.h"

TArray<AArpgCharacterBase*> ArpgCombat::FindTargets(const UWorld* World, const FVector& Center, float Radius, EArpgTeam AttackerTeam,
	const FVector* Forward, float ArcDegrees)
{
	TArray<AArpgCharacterBase*> Out;
	if (!World) { return Out; }
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5f));
	FVector Fwd2D = Forward ? FVector(Forward->X, Forward->Y, 0.f).GetSafeNormal() : FVector::ZeroVector;
	for (TActorIterator<AArpgCharacterBase> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AArpgCharacterBase* C = *It;
		if (!C || C->IsDead() || C->Team == AttackerTeam) { continue; }
		const float CapsuleR = C->GetSimpleCollisionRadius();
		FVector Delta = C->GetActorLocation() - Center;
		if (FMath::Abs(Delta.Z) > Radius + 200.f) { continue; }
		Delta.Z = 0.f;
		const float Dist = Delta.Size();
		if (Dist > Radius + CapsuleR) { continue; }
		if (Forward && ArcDegrees < 359.f && Dist > CapsuleR)
		{
			if (FVector::DotProduct(Delta / Dist, Fwd2D) < CosHalf) { continue; }
		}
		Out.Add(C);
	}
	return Out;
}

AArpgCharacterBase* ArpgCombat::FindNearest(const UWorld* World, const FVector& Center, float Radius, EArpgTeam AttackerTeam,
	const FVector* Forward, float ArcDegrees)
{
	AArpgCharacterBase* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (AArpgCharacterBase* C : FindTargets(World, Center, Radius, AttackerTeam, Forward, ArcDegrees))
	{
		const float D = FVector::DistSquared(C->GetActorLocation(), Center);
		if (D < BestD) { BestD = D; Best = C; }
	}
	return Best;
}

bool ArpgCombat::HasLineOfSight(const UWorld* World, const FVector& From, const FVector& To)
{
	if (!World) { return false; }
	FHitResult Hit;
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArpgLOS), false);
	return !World->LineTraceSingleByObjectType(Hit, From, To, Obj, Params);
}

void ArpgCombat::ShowNumber(const UWorld* World, const FVector& Location, const FString& Text, const FLinearColor& Color, float Scale)
{
	if (!World) { return; }
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (AArpgHUD* HUD = Cast<AArpgHUD>(PC->GetHUD()))
		{
			HUD->AddFloatingText(Location, Text, Color, Scale);
		}
	}
}

float ArpgCombat::ArmourReduction(float Armour, float Hit)
{
	if (Armour <= 0.f || Hit <= 0.f) { return 0.f; }
	return FMath::Min(0.9f, Armour / (Armour + 5.f * Hit));
}

float ArpgCombat::EvadeChance(float Evasion, int32 AttackerLevel)
{
	if (Evasion <= 0.f) { return 0.f; }
	return FMath::Min(0.75f, Evasion / (Evasion + 100.f + 20.f * (float)AttackerLevel));
}
