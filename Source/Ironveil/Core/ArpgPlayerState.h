// Per-player data that exists on every machine in a co-op game and survives the character dying.
// Players' Ability System Component and attributes live here (plan 1.3 / 2.1).
// Later: level, XP, class, party info.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "ArpgPlayerState.generated.h"

class UArpgAbilitySystemComponent;
class UArpgAttributeSet;

UCLASS()
class IRONVEIL_API AArpgPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArpgPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UArpgAbilitySystemComponent* GetArpgAbilitySystemComponent() const { return AbilitySystemComponent; }
	UArpgAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Ironveil|Abilities")
	TObjectPtr<UArpgAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Ironveil|Abilities")
	TObjectPtr<UArpgAttributeSet> AttributeSet;
};
