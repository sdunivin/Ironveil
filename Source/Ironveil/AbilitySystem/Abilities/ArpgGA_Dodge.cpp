#include "AbilitySystem/Abilities/ArpgGA_Dodge.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystem/ArpgGameplayTags.h"
#include "Components/ArpgLockOnComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UArpgGA_Dodge::UArpgGA_Dodge()
{
	InputTag = ArpgTags::InputTag_Dodge;
	StaminaCost = 20.f;

	// While dodging the character has State.Dodging; a second dodge can't start until this one ends.
	ActivationOwnedTags.AddTag(ArpgTags::State_Dodging);
	ActivationBlockedTags.AddTag(ArpgTags::State_Dodging);
}

void UArpgGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Character || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Direction: where the player is pushing the stick. Acceleration exists on both the owning
	// client and the server (it travels in the movement updates), so both pick the same direction.
	FVector Direction = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector().GetSafeNormal2D(); // backstep
	}

	const UArpgLockOnComponent* LockOn = Character->FindComponentByClass<UArpgLockOnComponent>();
	const bool bLockedOn = LockOn && LockOn->IsLockedOn();
	if (bFaceDodgeDirection && !bLockedOn)
	{
		Character->SetActorRotation(Direction.Rotation());
	}

	// Invulnerability frames.
	if (InvulnerableDuration > 0.f)
	{
		ASC->AddLooseGameplayTag(ArpgTags::State_Invulnerable);
		bInvulnerable = true;

		UAbilityTask_WaitDelay* IFrameTimer = UAbilityTask_WaitDelay::WaitDelay(this, InvulnerableDuration);
		IFrameTimer->OnFinish.AddDynamic(this, &ThisClass::OnInvulnerabilityEnded);
		IFrameTimer->ReadyForActivation();
	}

	// The dash itself.
	const float Speed = DodgeDistance / FMath::Max(DashDuration, 0.05f);
	UAbilityTask_ApplyRootMotionConstantForce* Dash = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		NAME_None,
		Direction,
		Speed,
		DashDuration,
		/*bIsAdditive*/ false,
		SpeedOverTime,
		ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector,
		/*ClampVelocityOnFinish*/ 250.f,
		/*bEnableGravity*/ true);
	Dash->ReadyForActivation();

	// Optional animation.
	if (DodgeMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DodgeMontage, 1.f);
		MontageTask->ReadyForActivation();
	}

	// End after the full dodge length (dash + recovery).
	UAbilityTask_WaitDelay* EndTimer = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(TotalDuration, DashDuration));
	EndTimer->OnFinish.AddDynamic(this, &ThisClass::OnDodgeFinished);
	EndTimer->ReadyForActivation();
}

void UArpgGA_Dodge::OnInvulnerabilityEnded()
{
	RemoveInvulnerability();
}

void UArpgGA_Dodge::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UArpgGA_Dodge::RemoveInvulnerability()
{
	if (bInvulnerable)
	{
		bInvulnerable = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ArpgTags::State_Invulnerable);
		}
	}
}

void UArpgGA_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Never leave a character permanently invulnerable if the dodge is cancelled early.
	RemoveInvulnerability();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
