#include "Animation/ArpgAnimNotifyState_GrantTag.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UAbilitySystemComponent* FindASC(const USkeletalMeshComponent* MeshComp)
	{
		// In the animation editor preview there's no character, so this returns null.
		return MeshComp ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner()) : nullptr;
	}
}

void UArpgAnimNotifyState_GrantTag::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UAbilitySystemComponent* ASC = FindASC(MeshComp); ASC && Tag.IsValid())
	{
		ASC->AddLooseGameplayTag(Tag);
	}
}

void UArpgAnimNotifyState_GrantTag::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UAbilitySystemComponent* ASC = FindASC(MeshComp); ASC && Tag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UArpgAnimNotifyState_GrantTag::GetNotifyName_Implementation() const
{
	return Tag.IsValid() ? FString::Printf(TEXT("Tag: %s"), *Tag.ToString()) : TEXT("Grant Tag");
}
