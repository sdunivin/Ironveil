#include "AbilitySystem/ArpgAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Characters/ArpgCharacterBase.h"

UArpgAttributeSet::UArpgAttributeSet()
{
	// Starting values from the GDD (a level 1 Warrior). Characters override the
	// important ones through their "Initial Attributes" setting.
	InitHealth(66.f);
	InitMaxHealth(66.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitMana(40.f);
	InitMaxMana(40.f);
	InitEnergyShield(0.f);
	InitMaxEnergyShield(0.f);
	InitArmour(0.f);
	InitEvasion(0.f);
	InitPoise(50.f);
	InitMaxPoise(50.f);
	InitFireResist(0.f);
	InitColdResist(0.f);
	InitLightningResist(0.f);
	InitChaosResist(0.f);
	InitPhysicalDamage(0.f);
	InitCritChance(5.f);
	InitCritMultiplier(150.f);
	InitAttackSpeed(1.f);
	InitCastSpeed(1.f);
	InitIncomingDamage(0.f);
	InitIncomingPoiseDamage(0.f);
}

void UArpgAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, EnergyShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, MaxEnergyShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, Armour, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, Evasion, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, FireResist, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, ColdResist, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, LightningResist, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, ChaosResist, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, PhysicalDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, CritMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArpgAttributeSet, CastSpeed, COND_None, REPNOTIFY_Always);
}

void UArpgAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetEnergyShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxEnergyShield());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// A character must always have at least 1 maximum Health.
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetMaxStaminaAttribute() || Attribute == GetMaxManaAttribute()
		|| Attribute == GetMaxEnergyShieldAttribute() || Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UArpgAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UArpgAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UArpgAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AArpgCharacterBase* OwnerCharacter = Cast<AArpgCharacterBase>(Data.Target.GetAvatarActor());

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (Damage > 0.f && GetHealth() > 0.f)
		{
			// Energy Shield soaks damage first, then Health takes the rest.
			float Remaining = Damage;
			const float Shield = GetEnergyShield();
			if (Shield > 0.f)
			{
				const float Absorbed = FMath::Min(Shield, Remaining);
				SetEnergyShield(Shield - Absorbed);
				Remaining -= Absorbed;
			}

			if (Remaining > 0.f)
			{
				SetHealth(FMath::Clamp(GetHealth() - Remaining, 0.f, GetMaxHealth()));
			}

			if (OwnerCharacter)
			{
				OwnerCharacter->NotifyDamageTaken(Damage);
				if (GetHealth() <= 0.f)
				{
					OwnerCharacter->HandleOutOfHealth();
				}
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingPoiseDamageAttribute())
	{
		const float PoiseDamage = GetIncomingPoiseDamage();
		SetIncomingPoiseDamage(0.f);

		if (PoiseDamage > 0.f && GetPoise() > 0.f)
		{
			SetPoise(FMath::Max(GetPoise() - PoiseDamage, 0.f));
			if (OwnerCharacter)
			{
				OwnerCharacter->NotifyPoiseDamageTaken();
				if (GetPoise() <= 0.f)
				{
					OwnerCharacter->HandlePoiseBroken();
				}
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Healing effects: keep Health inside 0..MaxHealth.
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}

// ---------------- Replication notifies (tell GAS a value arrived from the server) ----------------

void UArpgAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, Health, OldValue);
}

void UArpgAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, MaxHealth, OldValue);
}

void UArpgAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, Stamina, OldValue);
}

void UArpgAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, MaxStamina, OldValue);
}

void UArpgAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, Mana, OldValue);
}

void UArpgAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, MaxMana, OldValue);
}

void UArpgAttributeSet::OnRep_EnergyShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, EnergyShield, OldValue);
}

void UArpgAttributeSet::OnRep_MaxEnergyShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, MaxEnergyShield, OldValue);
}

void UArpgAttributeSet::OnRep_Armour(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, Armour, OldValue);
}

void UArpgAttributeSet::OnRep_Evasion(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, Evasion, OldValue);
}

void UArpgAttributeSet::OnRep_Poise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, Poise, OldValue);
}

void UArpgAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, MaxPoise, OldValue);
}

void UArpgAttributeSet::OnRep_FireResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, FireResist, OldValue);
}

void UArpgAttributeSet::OnRep_ColdResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, ColdResist, OldValue);
}

void UArpgAttributeSet::OnRep_LightningResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, LightningResist, OldValue);
}

void UArpgAttributeSet::OnRep_ChaosResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, ChaosResist, OldValue);
}

void UArpgAttributeSet::OnRep_PhysicalDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, PhysicalDamage, OldValue);
}

void UArpgAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, CritChance, OldValue);
}

void UArpgAttributeSet::OnRep_CritMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, CritMultiplier, OldValue);
}

void UArpgAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, AttackSpeed, OldValue);
}

void UArpgAttributeSet::OnRep_CastSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArpgAttributeSet, CastSpeed, OldValue);
}
