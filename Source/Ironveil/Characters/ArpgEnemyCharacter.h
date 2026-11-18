// Base for every enemy. Enemies own their Ability System Component (Minimal replication:
// cheap, since only the server needs full detail about enemy effects).
//
// Phase 1 use: make BP_TrainingDummy from this to test lock-on (give it a mesh).
// Phase 5 adds AI (State Tree), loot drops and monster rarity.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ArpgCharacterBase.h"
#include "ArpgEnemyCharacter.generated.h"

UCLASS()
class IRONVEIL_API AArpgEnemyCharacter : public AArpgCharacterBase
{
	GENERATED_BODY()

public:
	AArpgEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual bool IsTargetable() const override { return IsAlive(); }

	// Monster level: sets item level of drops and scales Health/damage (CT_MonsterScaling) later.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ironveil|Enemy", meta = (ClampMin = "1"))
	int32 MonsterLevel = 1;

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;

	UPROPERTY(VisibleAnywhere, Category = "Ironveil|Abilities")
	TObjectPtr<UArpgAbilitySystemComponent> OwnedAbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Ironveil|Abilities")
	TObjectPtr<UArpgAttributeSet> OwnedAttributeSet;
};
