#include "Characters/ArpgCharacterBase.h"

#include "AbilitySystem/ArpgAbilitySystemComponent.h"
#include "AbilitySystem/ArpgAttributeSet.h"
#include "AbilitySystem/ArpgGameplayTags.h"
#include "Characters/ArpgCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

AArpgCharacterBase::AArpgCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArpgCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// The controller turns the camera, not the body; movement turns the body.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Animations must keep running on the server even when nobody is looking, because hit
	// detection, anim notifies and i-frames come from them.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AArpgCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArpgCharacterBase, bStaminaExhausted);
	DOREPLIFETIME(AArpgCharacterBase, bIsDead);
}

UAbilitySystemComponent* AArpgCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UArpgCharacterMovementComponent* AArpgCharacterBase::GetArpgMovement() const
{
	return Cast<UArpgCharacterMovementComponent>(GetCharacterMovement());
}

// ---------------------------------------------------------------- Setup

void AArpgCharacterBase::InitializeAttributes()
{
	if (!HasAuthority() || !AbilitySystemComponent || AbilitySystemComponent->bInitialAttributesApplied)
	{
		return;
	}
	AbilitySystemComponent->bInitialAttributesApplied = true;

	// Maximums first, so the current values aren't clamped to the old maximums.
	UArpgAbilitySystemComponent* ASC = AbilitySystemComponent;
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetMaxHealthAttribute(), InitialAttributes.MaxHealth);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetMaxStaminaAttribute(), InitialAttributes.MaxStamina);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetMaxManaAttribute(), InitialAttributes.MaxMana);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetMaxEnergyShieldAttribute(), InitialAttributes.MaxEnergyShield);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetMaxPoiseAttribute(), InitialAttributes.MaxPoise);

	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetHealthAttribute(), InitialAttributes.MaxHealth);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetStaminaAttribute(), InitialAttributes.MaxStamina);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetManaAttribute(), InitialAttributes.MaxMana);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetEnergyShieldAttribute(), InitialAttributes.MaxEnergyShield);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetPoiseAttribute(), InitialAttributes.MaxPoise);

	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetArmourAttribute(), InitialAttributes.Armour);
	ASC->SetNumericAttributeBase(UArpgAttributeSet::GetEvasionAttribute(), InitialAttributes.Evasion);
}

void AArpgCharacterBase::GrantStartupAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent || AbilitySystemComponent->bStartupAbilitiesGiven)
	{
		return;
	}
	AbilitySystemComponent->bStartupAbilitiesGiven = true;
	AbilitySystemComponent->GrantAbilities(StartupAbilities, this);
}

// ---------------------------------------------------------------- Tick (server-side upkeep)

void AArpgCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && AttributeSet && AbilitySystemComponent && IsAlive())
	{
		UpdateStamina(DeltaSeconds);
		UpdateEnergyShield(DeltaSeconds);
		UpdatePoise(DeltaSeconds);
	}
}

float AArpgCharacterBase::GetStamina() const
{
	return AttributeSet ? AttributeSet->GetStamina() : 0.f;
}

float AArpgCharacterBase::GetMaxStamina() const
{
	return AttributeSet ? AttributeSet->GetMaxStamina() : 0.f;
}

bool AArpgCharacterBase::CanUseStamina() const
{
	return IsAlive() && !bStaminaExhausted && GetStamina() > 0.f;
}

void AArpgCharacterBase::SpendStamina(float Amount)
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet || Amount <= 0.f)
	{
		return;
	}

	const float NewStamina = FMath::Max(AttributeSet->GetStamina() - Amount, 0.f);
	AbilitySystemComponent->SetNumericAttributeBase(UArpgAttributeSet::GetStaminaAttribute(), NewStamina);
	LastStaminaSpendTime = GetWorld()->GetTimeSeconds();

	if (NewStamina <= 0.f)
	{
		bStaminaExhausted = true;
	}
}

void AArpgCharacterBase::UpdateStamina(float DeltaSeconds)
{
	const UArpgCharacterMovementComponent* Move = GetArpgMovement();
	const bool bSprintingAndMoving = Move && Move->IsSprinting() && GetVelocity().SizeSquared2D() > FMath::Square(50.f);

	if (bSprintingAndMoving)
	{
		SpendStamina(SprintStaminaPerSecond * DeltaSeconds);
	}
	else if (GetWorld()->GetTimeSeconds() - LastStaminaSpendTime >= StaminaRegenDelay)
	{
		const float Current = AttributeSet->GetStamina();
		const float Max = AttributeSet->GetMaxStamina();
		if (Current < Max)
		{
			const float NewStamina = FMath::Min(Current + StaminaRegenPerSecond * DeltaSeconds, Max);
			AbilitySystemComponent->SetNumericAttributeBase(UArpgAttributeSet::GetStaminaAttribute(), NewStamina);
		}
	}

	if (bStaminaExhausted && AttributeSet->GetStamina() >= FMath::Min(ExhaustionRecoveryThreshold, AttributeSet->GetMaxStamina()))
	{
		bStaminaExhausted = false;
	}
}

void AArpgCharacterBase::UpdateEnergyShield(float DeltaSeconds)
{
	const float Max = AttributeSet->GetMaxEnergyShield();
	const float Current = AttributeSet->GetEnergyShield();
	if (Max <= 0.f || Current >= Max)
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastDamageTime >= EnergyShieldRechargeDelay)
	{
		const float NewShield = FMath::Min(Current + Max * EnergyShieldRechargePercentPerSecond * 0.01f * DeltaSeconds, Max);
		AbilitySystemComponent->SetNumericAttributeBase(UArpgAttributeSet::GetEnergyShieldAttribute(), NewShield);
	}
}

void AArpgCharacterBase::UpdatePoise(float DeltaSeconds)
{
	const float Max = AttributeSet->GetMaxPoise();
	const float Current = AttributeSet->GetPoise();
	if (Max <= 0.f || Current >= Max)
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastPoiseDamageTime >= PoiseRecoveryDelay)
	{
		const float NewPoise = PoiseRecoveryPercentPerSecond <= 0.f
			? Max
			: FMath::Min(Current + Max * PoiseRecoveryPercentPerSecond * 0.01f * DeltaSeconds, Max);
		AbilitySystemComponent->SetNumericAttributeBase(UArpgAttributeSet::GetPoiseAttribute(), NewPoise);
	}
}

// ---------------------------------------------------------------- Damage and death (server)

void AArpgCharacterBase::NotifyDamageTaken(float Damage)
{
	LastDamageTime = GetWorld()->GetTimeSeconds();
}

void AArpgCharacterBase::NotifyPoiseDamageTaken()
{
	LastPoiseDamageTime = GetWorld()->GetTimeSeconds();
}

void AArpgCharacterBase::HandlePoiseBroken()
{
	// Phase 2.6: apply State.Staggered for 2 seconds here (a Gameplay Effect), play a stagger
	// montage, and allow grabs. For now Blueprints can react through OnPoiseBroken.
	OnPoiseBroken();
}

void AArpgCharacterBase::HandleOutOfHealth()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	ApplyDeathState(); // OnRep doesn't run on the server, so call it directly here.
}

void AArpgCharacterBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		ApplyDeathState();
	}
}

void AArpgCharacterBase::ApplyDeathState()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(ArpgTags::State_Dead);
	}

	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();

	// Placeholder ragdoll. Phase 2.5 adds an impulse in the direction of the killing blow.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	OnDeath();
}
