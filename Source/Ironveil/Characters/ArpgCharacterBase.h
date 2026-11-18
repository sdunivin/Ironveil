// Shared base for the player and every enemy. Holds what both need:
//   - access to the Ability System Component and attributes (GAS)
//   - stamina rules from the GDD (spend, regenerate, exhaustion)
//   - Energy Shield recharge and Poise recovery
//   - death
//
// Server rule: Health, Stamina, Poise and death are decided on the server and replicate out.
// Clients only display them.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ArpgCharacterBase.generated.h"

class UArpgAbilitySystemComponent;
class UArpgAttributeSet;
class UArpgCharacterMovementComponent;
class UGameplayAbility;

// Starting values applied once when the character is created. Set them per Blueprint
// (e.g. a Bandit has 30 poise, an Ogre 150).
USTRUCT(BlueprintType)
struct FArpgInitialAttributes
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "1"))
	float MaxHealth = 66.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0"))
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0"))
	float MaxMana = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0"))
	float MaxEnergyShield = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0"))
	float MaxPoise = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0"))
	float Armour = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0"))
	float Evasion = 0.f;
};

UCLASS(Abstract)
class IRONVEIL_API AArpgCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArpgCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- GAS ----
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UArpgAbilitySystemComponent* GetArpgAbilitySystemComponent() const { return AbilitySystemComponent; }
	UArpgAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "Ironveil|Movement")
	UArpgCharacterMovementComponent* GetArpgMovement() const;

	// ---- Stamina ----
	UFUNCTION(BlueprintPure, Category = "Ironveil|Stamina")
	float GetStamina() const;

	UFUNCTION(BlueprintPure, Category = "Ironveil|Stamina")
	float GetMaxStamina() const;

	// True after stamina hits 0, until it refills to ExhaustionRecoveryThreshold.
	UFUNCTION(BlueprintPure, Category = "Ironveil|Stamina")
	bool IsStaminaExhausted() const { return bStaminaExhausted; }

	// Can this character start a stamina-costing action right now?
	UFUNCTION(BlueprintPure, Category = "Ironveil|Stamina")
	bool CanUseStamina() const;

	// Server only. Spends stamina and pauses regeneration.
	void SpendStamina(float Amount);

	// ---- Life ----
	UFUNCTION(BlueprintPure, Category = "Ironveil|Life")
	bool IsAlive() const { return !bIsDead; }

	// Can this character be locked on to? Enemies: yes while alive. Players: no (no PvP).
	virtual bool IsTargetable() const { return false; }

	// Called by the attribute set on the server.
	virtual void HandleOutOfHealth();
	virtual void HandlePoiseBroken();
	void NotifyDamageTaken(float Damage);
	void NotifyPoiseDamageTaken();

protected:
	// Finds the Ability System Component and tells it which actor is the avatar.
	virtual void InitAbilityActorInfo() {}

	// Server only. Apply InitialAttributes and StartupAbilities (once per Ability System Component).
	void InitializeAttributes();
	void GrantStartupAbilities();

	// Blueprint hooks for visuals and sound.
	UFUNCTION(BlueprintImplementableEvent, Category = "Ironveil|Life")
	void OnDeath();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ironveil|Life")
	void OnPoiseBroken();

	UFUNCTION()
	void OnRep_IsDead();

	// Abilities every character of this type starts with (e.g. GA_Dodge). Must be Ironveil abilities.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Attributes")
	FArpgInitialAttributes InitialAttributes;

	// ---- Stamina tuning (GDD values) ----
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Stamina", meta = (Units = "s"))
	float StaminaRegenDelay = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Stamina")
	float StaminaRegenPerSecond = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Stamina")
	float SprintStaminaPerSecond = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Stamina")
	float ExhaustionRecoveryThreshold = 25.f;

	// ---- Energy Shield (GDD: recharges 20% per second after 3 s without taking damage) ----
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Defense", meta = (Units = "s"))
	float EnergyShieldRechargeDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Defense", meta = (Units = "Percent"))
	float EnergyShieldRechargePercentPerSecond = 20.f;

	// ---- Poise (GDD: player refills fully after 2 s; Bandit 3 s; Warlord 10%/s after 4 s) ----
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Defense", meta = (Units = "s"))
	float PoiseRecoveryDelay = 2.f;

	// 0 = refill instantly when the delay ends. Above 0 = refill this percent of max per second.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Defense", meta = (Units = "Percent"))
	float PoiseRecoveryPercentPerSecond = 0.f;

	// Not owned here for players (it lives on the PlayerState); owned by enemies themselves.
	UPROPERTY(Transient)
	TObjectPtr<UArpgAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UArpgAttributeSet> AttributeSet;

private:
	void UpdateStamina(float DeltaSeconds);
	void UpdateEnergyShield(float DeltaSeconds);
	void UpdatePoise(float DeltaSeconds);
	void ApplyDeathState();

	UPROPERTY(Replicated)
	bool bStaminaExhausted = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	double LastStaminaSpendTime = -1000.0;
	double LastDamageTime = -1000.0;
	double LastPoiseDamageTime = -1000.0;
};
