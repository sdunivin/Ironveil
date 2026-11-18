#include "Characters/ArpgEnemyCharacter.h"

#include "AbilitySystem/ArpgAbilitySystemComponent.h"
#include "AbilitySystem/ArpgAttributeSet.h"
#include "AIController.h"

AArpgEnemyCharacter::AArpgEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OwnedAbilitySystem = CreateDefaultSubobject<UArpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	OwnedAbilitySystem->SetIsReplicated(true);
	OwnedAbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Created as a subobject of the same actor as the component, so GAS finds it automatically.
	OwnedAttributeSet = CreateDefaultSubobject<UArpgAttributeSet>(TEXT("AttributeSet"));

	AbilitySystemComponent = OwnedAbilitySystem;
	AttributeSet = OwnedAttributeSet;

	// GDD: bandits, archers and duelists have 30 poise and refill after 3 seconds.
	InitialAttributes.MaxHealth = 60.f;
	InitialAttributes.MaxPoise = 30.f;
	InitialAttributes.MaxMana = 0.f;
	PoiseRecoveryDelay = 3.f;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AArpgEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitAbilityActorInfo();
	InitializeAttributes();
	GrantStartupAbilities();
}

void AArpgEnemyCharacter::InitAbilityActorInfo()
{
	AbilitySystemComponent = OwnedAbilitySystem;
	AttributeSet = OwnedAttributeSet;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
