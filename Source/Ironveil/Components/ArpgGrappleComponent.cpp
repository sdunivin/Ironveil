#include "Components/ArpgGrappleComponent.h"

#include "CableComponent.h"
#include "Characters/ArpgCharacterBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Traversal/ArpgGrappleAnchor.h"

UArpgGrappleComponent::UArpgGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UArpgGrappleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArpgGrappleComponent, ActiveAnchor);
}

void UArpgGrappleComponent::BeginPlay()
{
	Super::BeginPlay();

	// Checks which anchor is aimed at, for the highlight. Started for everyone because the
	// controller may not be assigned yet at BeginPlay; UpdateAimedAnchor skips non-local players.
	GetWorld()->GetTimerManager().SetTimer(AimTimer, this, &ThisClass::UpdateAimedAnchor, AimCheckInterval, true);
}

void UArpgGrappleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AimTimer);
		World->GetTimerManager().ClearTimer(RopeTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool UArpgGrappleComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

UCableComponent* UArpgGrappleComponent::GetCable() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UCableComponent>() : nullptr;
}

// ---------------------------------------------------------------- Aiming

AArpgGrappleAnchor* UArpgGrappleComponent::FindBestAnchor() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();
	const FVector PawnLocation = Pawn->GetActorLocation();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(MaxAimAngle));

	// A floor has a few dozen anchors at most, so checking them all is cheap.
	AArpgGrappleAnchor* Best = nullptr;
	float BestDot = MinDot;
	for (TActorIterator<AArpgGrappleAnchor> It(GetWorld()); It; ++It)
	{
		AArpgGrappleAnchor* Anchor = *It;
		if (!Anchor->IsGrappleEnabled())
		{
			continue;
		}

		const FVector Hook = Anchor->GetHookLocation();
		const float Distance = FVector::Dist(Hook, PawnLocation);
		if (Distance > MaxRange || Distance < MinRange)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(ViewForward, (Hook - ViewLocation).GetSafeNormal());
		if (Dot <= BestDot)
		{
			continue;
		}

		// Rope needs a clear line from the character to the anchor.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArpgGrappleSight), false, GetOwner());
		Params.AddIgnoredActor(Anchor);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, PawnLocation, Hook, ECC_Visibility, Params))
		{
			continue;
		}

		BestDot = Dot;
		Best = Anchor;
	}
	return Best;
}

void UArpgGrappleComponent::UpdateAimedAnchor()
{
	if (!IsOwnerLocallyControlled())
	{
		return;
	}

	AArpgGrappleAnchor* NewAimed = FindBestAnchor();
	AArpgGrappleAnchor* OldAimed = AimedAnchor.Get();
	if (NewAimed == OldAimed)
	{
		return;
	}

	if (OldAimed)
	{
		OldAimed->OnHighlightChanged(false);
	}
	if (NewAimed)
	{
		NewAimed->OnHighlightChanged(true);
	}
	AimedAnchor = NewAimed;
}

// ---------------------------------------------------------------- Grappling

void UArpgGrappleComponent::TryGrapple()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastGrappleTime < Cooldown)
	{
		return;
	}

	AArpgGrappleAnchor* Anchor = FindBestAnchor();
	if (!Anchor)
	{
		return;
	}

	// Launch locally at once (feels instant), then ask the server to do the same.
	if (!LaunchToward(Anchor))
	{
		return;
	}
	LastGrappleTime = Now;
	ShowRope(Anchor);

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerGrapple(Anchor);
	}
	else
	{
		// Listen-server host: we are the server already.
		ActiveAnchor = Anchor;
		GetWorld()->GetTimerManager().SetTimer(RopeTimer, this, &ThisClass::ClearActiveAnchor, RopeVisibleTime, false);
	}
}

void UArpgGrappleComponent::ServerGrapple_Implementation(AArpgGrappleAnchor* Anchor)
{
	if (!Anchor || !Anchor->IsGrappleEnabled())
	{
		return;
	}

	// Allow some slack for lag, but reject requests that are clearly impossible.
	const float Distance = FVector::Dist(Anchor->GetHookLocation(), GetOwner()->GetActorLocation());
	if (Distance > MaxRange * 1.25f)
	{
		return;
	}

	if (!LaunchToward(Anchor))
	{
		return;
	}

	LastGrappleTime = GetWorld()->GetTimeSeconds();
	ActiveAnchor = Anchor;
	ShowRope(Anchor); // for the server's own screen when it's a listen server
	GetWorld()->GetTimerManager().SetTimer(RopeTimer, this, &ThisClass::ClearActiveAnchor, RopeVisibleTime, false);
}

bool UArpgGrappleComponent::LaunchToward(const AArpgGrappleAnchor* Anchor)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const AArpgCharacterBase* ArpgCharacter = Cast<AArpgCharacterBase>(GetOwner());
	if (!Character || !Anchor || (ArpgCharacter && !ArpgCharacter->IsAlive()))
	{
		return false;
	}

	// Work out the launch velocity for an arc that lands on the anchor's landing point.
	FVector LaunchVelocity;
	const bool bFound = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
		this, LaunchVelocity, Character->GetActorLocation(), Anchor->GetLandingLocation(),
		Character->GetCharacterMovement()->GetGravityZ(), ArcParam);
	if (!bFound)
	{
		return false;
	}

	// Override both horizontal and vertical velocity so the arc is exact.
	Character->LaunchCharacter(LaunchVelocity, true, true);
	return true;
}

// ---------------------------------------------------------------- Rope visuals

void UArpgGrappleComponent::ClearActiveAnchor()
{
	ActiveAnchor = nullptr;
	HideRope();
}

void UArpgGrappleComponent::OnRep_ActiveAnchor()
{
	// Runs on every client. The owner already showed its rope when it launched.
	if (ActiveAnchor)
	{
		ShowRope(ActiveAnchor);
	}
	else
	{
		HideRope();
	}
}

void UArpgGrappleComponent::ShowRope(AArpgGrappleAnchor* Anchor)
{
	UCableComponent* Cable = GetCable();
	if (!Cable || !Anchor)
	{
		return;
	}

	Cable->SetAttachEndToComponent(Anchor->GetRootComponent());
	Cable->EndLocation = FVector::ZeroVector;
	Cable->SetVisibility(true);

	// The owner hides its rope on a local timer too, in case replication is slow.
	if (IsOwnerLocallyControlled() && GetOwnerRole() < ROLE_Authority)
	{
		GetWorld()->GetTimerManager().SetTimer(RopeTimer, this, &ThisClass::HideRope, RopeVisibleTime, false);
	}
}

void UArpgGrappleComponent::HideRope()
{
	if (UCableComponent* Cable = GetCable())
	{
		Cable->SetVisibility(false);
	}
}
