// Anim Notify State that gives the character a Gameplay Tag for the length of the notify.
// Drag it onto a montage track and pick a tag. Uses:
//   State.Invulnerable   i-frames inside an attack or a custom dodge animation
//   (later) combo windows, attack windows, parry windows
//
// Anim notifies run on every machine that plays the animation, so the tag appears on the
// server and on clients without extra networking.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ArpgAnimNotifyState_GrantTag.generated.h"

UCLASS(meta = (DisplayName = "Ironveil: Grant Gameplay Tag"))
class IRONVEIL_API UArpgAnimNotifyState_GrantTag : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ironveil")
	FGameplayTag Tag;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
