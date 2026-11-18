#include "Core/ArpgPlayerState.h"

#include "AbilitySystem/ArpgAbilitySystemComponent.h"
#include "AbilitySystem/ArpgAttributeSet.h"

AArpgPlayerState::AArpgPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UArpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed: the owning player gets full detail about their own effects; others get the minimum.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UArpgAttributeSet>(TEXT("AttributeSet"));

	// PlayerStates update slowly by default; abilities and attributes need it much faster.
	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* AArpgPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
