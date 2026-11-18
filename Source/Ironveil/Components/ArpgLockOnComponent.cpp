#include "Components/ArpgLockOnComponent.h"

#include "Characters/ArpgCharacterBase.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

UArpgLockOnComponent::UArpgLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UArpgLockOnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArpgLockOnComponent, Target);
}

// ---------------------------------------------------------------- Helpers

APlayerController* UArpgLockOnComponent::GetOwningPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool UArpgLockOnComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

bool UArpgLockOnComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		PC->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}
	return false;
}

FVector UArpgLockOnComponent::GetAimPoint(const AActor* Candidate) const
{
	return Candidate->GetActorLocation() + FVector(0.f, 0.f, TargetHeightOffset);
}

bool UArpgLockOnComponent::IsValidTarget(const AActor* Candidate) const
{
	const AArpgCharacterBase* Character = Cast<AArpgCharacterBase>(Candidate);
	return Character && Character != GetOwner() && Character->IsTargetable();
}

bool UArpgLockOnComponent::HasLineOfSight(const AActor* Candidate) const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArpgLockOnSight), false, GetOwner());
	Params.AddIgnoredActor(Candidate);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, GetAimPoint(Candidate), ECC_Visibility, Params);
}

void UArpgLockOnComponent::GatherCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArpgLockOnSearch), false, GetOwner());
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetOwner()->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(MaxLockDistance), Params);

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return;
	}
	const FVector ViewForward = ViewRotation.Vector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(MaxAngleFromCamera));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || OutCandidates.Contains(Candidate) || !IsValidTarget(Candidate))
		{
			continue;
		}

		const FVector ToCandidate = (GetAimPoint(Candidate) - ViewLocation).GetSafeNormal();
		if (FVector::DotProduct(ViewForward, ToCandidate) < MinDot)
		{
			continue;
		}

		if (HasLineOfSight(Candidate))
		{
			OutCandidates.Add(Candidate);
		}
	}
}

AActor* UArpgLockOnComponent::FindBestTarget() const
{
	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return nullptr;
	}
	const FVector ViewForward = ViewRotation.Vector();

	// Prefer what's near the centre of the screen, then what's close.
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* Candidate : Candidates)
	{
		const FVector ToCandidate = GetAimPoint(Candidate) - ViewLocation;
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ViewForward, ToCandidate.GetSafeNormal()), -1.f, 1.f)));
		const float Score = AngleDeg + ToCandidate.Size() * 0.01f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

// ---------------------------------------------------------------- Target changes

void UArpgLockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearTarget();
	}
	else if (AActor* NewTarget = FindBestTarget())
	{
		SetTargetLocal(NewTarget);
	}
}

void UArpgLockOnComponent::ClearTarget()
{
	SetTargetLocal(nullptr);
}

void UArpgLockOnComponent::SwitchTarget(float Direction)
{
	APlayerController* PC = GetOwningPlayerController();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!PC || !Target || FMath::IsNearlyZero(Direction) || Now - LastSwitchTime < SwitchCooldown)
	{
		return;
	}

	FVector2D CurrentScreen;
	if (!PC->ProjectWorldLocationToScreen(GetAimPoint(Target), CurrentScreen))
	{
		return;
	}

	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	// The nearest target on the requested side of the screen.
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (AActor* Candidate : Candidates)
	{
		FVector2D Screen;
		if (Candidate == Target || !PC->ProjectWorldLocationToScreen(GetAimPoint(Candidate), Screen))
		{
			continue;
		}

		const float DeltaX = Screen.X - CurrentScreen.X;
		if (DeltaX * Direction <= 0.f)
		{
			continue;
		}

		const float Distance = FVector2D::Distance(Screen, CurrentScreen);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}

	if (Best)
	{
		LastSwitchTime = Now;
		SetTargetLocal(Best);
	}
}

void UArpgLockOnComponent::SetTargetLocal(AActor* NewTarget)
{
	if (NewTarget == Target)
	{
		return;
	}

	AActor* OldTarget = Target;
	Target = NewTarget;
	TimeWithoutSight = 0.f;
	OnRep_Target(OldTarget);

	// Tell the server so it rotates our character the same way.
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerSetTarget(NewTarget);
	}
}

void UArpgLockOnComponent::ServerSetTarget_Implementation(AActor* NewTarget)
{
	// Don't trust the client blindly: the target must be a valid, reasonably close enemy.
	if (NewTarget && (!IsValidTarget(NewTarget)
		|| FVector::Dist(NewTarget->GetActorLocation(), GetOwner()->GetActorLocation()) > BreakDistance))
	{
		NewTarget = nullptr;
	}

	AActor* OldTarget = Target;
	Target = NewTarget;
	OnRep_Target(OldTarget);
}

void UArpgLockOnComponent::OnRep_Target(AActor* OldTarget)
{
	OnTargetChanged.Broadcast(Target);
}

// ---------------------------------------------------------------- Tick: keep the target in frame

void UArpgLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Target || !IsOwnerLocallyControlled())
	{
		return;
	}

	const AArpgCharacterBase* OwnerCharacter = Cast<AArpgCharacterBase>(GetOwner());
	if (!IsValidTarget(Target) || (OwnerCharacter && !OwnerCharacter->IsAlive())
		|| FVector::Dist(Target->GetActorLocation(), GetOwner()->GetActorLocation()) > BreakDistance)
	{
		ClearTarget();
		return;
	}

	TimeWithoutSight = HasLineOfSight(Target) ? 0.f : TimeWithoutSight + DeltaTime;
	if (TimeWithoutSight >= LoseSightTime)
	{
		ClearTarget();
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Aim the camera from the character's head height toward the target, tilted slightly down.
	const FVector From = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	FRotator Desired = (GetAimPoint(Target) - From).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch + CameraPitchOffset, -45.f, 25.f);
	Desired.Roll = 0.f;

	PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaTime, CameraInterpSpeed));
}
