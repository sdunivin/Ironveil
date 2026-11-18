#include "AbilitySystem/ArpgAbilitySystemComponent.h"

#include "AbilitySystem/ArpgGameplayAbility.h"

UArpgAbilitySystemComponent::UArpgAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UArpgAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// Collect handles first: activating an ability can change the ability list we're looping over.
	TArray<FGameplayAbilitySpecHandle> ToActivate;
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UArpgGameplayAbility* Ability = Cast<UArpgGameplayAbility>(Spec.Ability);
		if (Ability && Ability->InputTag.MatchesTagExact(InputTag))
		{
			AbilitySpecInputPressed(Spec);
			if (!Spec.IsActive())
			{
				ToActivate.Add(Spec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : ToActivate)
	{
		TryActivateAbility(Handle);
	}
}

void UArpgAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UArpgGameplayAbility* Ability = Cast<UArpgGameplayAbility>(Spec.Ability);
		if (Ability && Ability->InputTag.MatchesTagExact(InputTag))
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}

void UArpgAbilitySystemComponent::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities, UObject* SourceObject)
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Abilities)
	{
		if (AbilityClass)
		{
			GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, SourceObject));
		}
	}
}
