// Ironveil's Ability System Component. Adds two things to Unreal's version:
//   1. Abilities are activated by Input Tags (InputTag.Dodge, InputTag.Skill.1 ...), so new
//      abilities and skill gems never need new input code.
//   2. Flags so startup abilities and starting attributes are only given once, even when a
//      player's character respawns (the component lives on the PlayerState, which survives death).

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ArpgAbilitySystemComponent.generated.h"

class UGameplayAbility;

UCLASS(ClassGroup = (Ironveil), meta = (BlueprintSpawnableComponent))
class IRONVEIL_API UArpgAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UArpgAbilitySystemComponent();

	// Called by the player character when a button bound to InputTag is pressed / released.
	// Activates every granted ability whose InputTag matches.
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	// Server only. Gives each ability at level 1, with SourceObject as the thing that granted it.
	void GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities, UObject* SourceObject);

	bool bStartupAbilitiesGiven = false;
	bool bInitialAttributesApplied = false;
};
