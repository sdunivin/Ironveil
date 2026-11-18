// Every number a character has: Health, Stamina, Armour, damage bonuses and so on.
// Players keep theirs on AArpgPlayerState (so it survives death and replicates in co-op);
// enemies keep theirs on the enemy character.
//
// Gameplay Effects change these values. Never set them directly from gameplay code,
// except for the stamina helpers in AArpgCharacterBase.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ArpgAttributeSet.generated.h"

// Creates GetHealth(), SetHealth(), InitHealth() and GetHealthAttribute() for an attribute.
#define ARPG_ATTRIBUTE_ACCESSORS(PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UArpgAttributeSet, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class IRONVEIL_API UArpgAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UArpgAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Clamps current values (e.g. Health can't go above MaxHealth).
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// Clamps base values, including ones set directly with SetNumericAttributeBase.
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	// Runs on the server after an instant Gameplay Effect changes an attribute. Turns
	// IncomingDamage into Energy Shield and Health loss, and IncomingPoiseDamage into Poise loss.
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// ---------------- Vitals ----------------
	// Current Health. At 0 the character dies.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ARPG_ATTRIBUTE_ACCESSORS(Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ARPG_ATTRIBUTE_ACCESSORS(MaxHealth)

	// Spent by dodges, sprinting, attacks and blocks. Regenerates after a short pause.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ARPG_ATTRIBUTE_ACCESSORS(Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ARPG_ATTRIBUTE_ACCESSORS(MaxStamina)

	// Spent by skills.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ARPG_ATTRIBUTE_ACCESSORS(Mana)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ARPG_ATTRIBUTE_ACCESSORS(MaxMana)

	// Absorbs damage before Health and recharges after 3 s without taking damage.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_EnergyShield)
	FGameplayAttributeData EnergyShield;
	ARPG_ATTRIBUTE_ACCESSORS(EnergyShield)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals", ReplicatedUsing = OnRep_MaxEnergyShield)
	FGameplayAttributeData MaxEnergyShield;
	ARPG_ATTRIBUTE_ACCESSORS(MaxEnergyShield)

	// ---------------- Defense ----------------
	// Reduces physical hits: reduction = Armour / (Armour + 5 x hit).
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_Armour)
	FGameplayAttributeData Armour;
	ARPG_ATTRIBUTE_ACCESSORS(Armour)

	// Chance to avoid attacks entirely.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_Evasion)
	FGameplayAttributeData Evasion;
	ARPG_ATTRIBUTE_ACCESSORS(Evasion)

	// Hits deal poise damage; at 0 the character is Staggered.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_Poise)
	FGameplayAttributeData Poise;
	ARPG_ATTRIBUTE_ACCESSORS(Poise)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_MaxPoise)
	FGameplayAttributeData MaxPoise;
	ARPG_ATTRIBUTE_ACCESSORS(MaxPoise)

	// Percent. Capped at 75 when damage is calculated (overcap is allowed).
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_FireResist)
	FGameplayAttributeData FireResist;
	ARPG_ATTRIBUTE_ACCESSORS(FireResist)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_ColdResist)
	FGameplayAttributeData ColdResist;
	ARPG_ATTRIBUTE_ACCESSORS(ColdResist)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_LightningResist)
	FGameplayAttributeData LightningResist;
	ARPG_ATTRIBUTE_ACCESSORS(LightningResist)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", ReplicatedUsing = OnRep_ChaosResist)
	FGameplayAttributeData ChaosResist;
	ARPG_ATTRIBUTE_ACCESSORS(ChaosResist)

	// ---------------- Offense ----------------
	// Percent increased physical damage (all 'increased' bonuses add together).
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense", ReplicatedUsing = OnRep_PhysicalDamage)
	FGameplayAttributeData PhysicalDamage;
	ARPG_ATTRIBUTE_ACCESSORS(PhysicalDamage)

	// Percent chance to deal a critical strike.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense", ReplicatedUsing = OnRep_CritChance)
	FGameplayAttributeData CritChance;
	ARPG_ATTRIBUTE_ACCESSORS(CritChance)

	// Percent damage on a critical strike (150 = 1.5x).
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense", ReplicatedUsing = OnRep_CritMultiplier)
	FGameplayAttributeData CritMultiplier;
	ARPG_ATTRIBUTE_ACCESSORS(CritMultiplier)

	// Attack speed multiplier (1 = normal speed).
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;
	ARPG_ATTRIBUTE_ACCESSORS(AttackSpeed)

	// Cast speed multiplier (1 = normal speed).
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense", ReplicatedUsing = OnRep_CastSpeed)
	FGameplayAttributeData CastSpeed;
	ARPG_ATTRIBUTE_ACCESSORS(CastSpeed)

	// ---------------- Meta (server only, not replicated) ----------------
	// Damage about to be taken. The damage calculation writes here; PostGameplayEffectExecute moves it into Energy Shield and Health.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	ARPG_ATTRIBUTE_ACCESSORS(IncomingDamage)

	// Poise damage about to be taken.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingPoiseDamage;
	ARPG_ATTRIBUTE_ACCESSORS(IncomingPoiseDamage)

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_EnergyShield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxEnergyShield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armour(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Evasion(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Poise(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FireResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ColdResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_LightningResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ChaosResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_PhysicalDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CastSpeed(const FGameplayAttributeData& OldValue);

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
