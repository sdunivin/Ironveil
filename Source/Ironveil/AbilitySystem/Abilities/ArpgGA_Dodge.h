// Dodge: a short dash in the direction you're moving (a backstep if you're standing still),
// invulnerable for the first part. Numbers come from the GDD: 0.6 s total, i-frames for the
// first 0.25 s (Thief: 0.35 s), 20 stamina.
//
// Movement uses a Root Motion Source, which Unreal's Character Movement Component predicts and
// replicates, so the dash looks instant for the dodging player and stays in sync in co-op.
//
// In the editor: make a Blueprint child called GA_Dodge, optionally set DodgeMontage (an
// animation with root motion turned OFF; the dash supplies the movement), and add GA_Dodge to
// the character's Startup Abilities.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArpgGameplayAbility.h"
#include "ArpgGA_Dodge.generated.h"

class UAnimMontage;
class UCurveFloat;

UCLASS()
class IRONVEIL_API UArpgGA_Dodge : public UArpgGameplayAbility
{
	GENERATED_BODY()

public:
	UArpgGA_Dodge();

	// How far the dash travels, in centimetres.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge", meta = (ClampMin = "0", Units = "cm"))
	float DodgeDistance = 450.f;

	// How long the dash moves the character.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float DashDuration = 0.4f;

	// Total length of the dodge, including recovery. You can't dodge again until it ends.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float TotalDuration = 0.6f;

	// Invulnerable time from the start of the dodge.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge", meta = (ClampMin = "0", Units = "s"))
	float InvulnerableDuration = 0.25f;

	// Optional: shapes the dash speed over time (X 0..1 = time, Y = speed multiplier).
	// A curve that starts high and falls off feels snappier than constant speed.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge")
	TObjectPtr<UCurveFloat> SpeedOverTime;

	// Optional animation. Turn root motion OFF on it.
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;

	// Turn the character to face the dodge direction (ignored while locked on, where you sidestep).
	UPROPERTY(EditDefaultsOnly, Category = "Ironveil|Dodge")
	bool bFaceDodgeDirection = true;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInvulnerabilityEnded();

	UFUNCTION()
	void OnDodgeFinished();

	void RemoveInvulnerability();

	bool bInvulnerable = false;
};
