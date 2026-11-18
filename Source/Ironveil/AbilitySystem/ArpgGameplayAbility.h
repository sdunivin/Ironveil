// Base class for every Ironveil ability (attacks, dodge, skills). Make Blueprint abilities
// from this class (GA_ prefix), not from Unreal's plain GameplayAbility.
//
// Adds:
//   InputTag      which button activates it (see ArpgGameplayTags.h)
//   StaminaCost   spent on activation; blocked while the character is exhausted
//   ManaCost      spent on activation; blocked if there isn't enough Mana
//
// Defaults suit co-op: Local Predicted (the player's machine starts it at once, the server
// confirms) and one instance per character.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ArpgGameplayAbility.generated.h"

class AArpgCharacterBase;

UCLASS(Abstract)
class IRONVEIL_API UArpgGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UArpgGameplayAbility();

	// The button that activates this ability. Leave empty for abilities triggered by code or events.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ironveil|Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ironveil|Costs", meta = (ClampMin = "0"))
	float StaminaCost = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ironveil|Costs", meta = (ClampMin = "0"))
	float ManaCost = 0.f;

	UFUNCTION(BlueprintPure, Category = "Ironveil|Ability")
	AArpgCharacterBase* GetArpgCharacter() const;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
