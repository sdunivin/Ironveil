#include "AbilitySystem/ArpgGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/ArpgAttributeSet.h"
#include "AbilitySystem/ArpgGameplayTags.h"
#include "Characters/ArpgCharacterBase.h"

UArpgGameplayAbility::UArpgGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Nothing can be activated while dead, staggered, frozen or held by an enemy.
	ActivationBlockedTags.AddTag(ArpgTags::State_Dead);
	ActivationBlockedTags.AddTag(ArpgTags::State_Staggered);
	ActivationBlockedTags.AddTag(ArpgTags::State_Grabbed);
	ActivationBlockedTags.AddTag(ArpgTags::Status_Frozen);
}

AArpgCharacterBase* UArpgGameplayAbility::GetArpgCharacter() const
{
	return Cast<AArpgCharacterBase>(GetAvatarActorFromActorInfo());
}

bool UArpgGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (StaminaCost > 0.f)
	{
		// GDD: you can act with any stamina left, but at 0 you're exhausted until it refills to 25.
		const AArpgCharacterBase* Character = ActorInfo ? Cast<AArpgCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Character || !Character->CanUseStamina())
		{
			return false;
		}
	}

	if (ManaCost > 0.f)
	{
		const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		const UArpgAttributeSet* Attributes = ASC ? ASC->GetSet<UArpgAttributeSet>() : nullptr;
		if (!Attributes || Attributes->GetMana() < ManaCost)
		{
			return false;
		}
	}

	return true;
}

void UArpgGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	// Costs are only paid on the server; the new values replicate to everyone.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	if (StaminaCost > 0.f)
	{
		if (AArpgCharacterBase* Character = Cast<AArpgCharacterBase>(ActorInfo->AvatarActor.Get()))
		{
			Character->SpendStamina(StaminaCost);
		}
	}

	if (ManaCost > 0.f)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			if (const UArpgAttributeSet* Attributes = ASC->GetSet<UArpgAttributeSet>())
			{
				ASC->SetNumericAttributeBase(UArpgAttributeSet::GetManaAttribute(), Attributes->GetMana() - ManaCost);
			}
		}
	}
}
