// ArpgCombat.h
// Helpers shared by abilities, projectiles and enemy attacks.

#pragma once

#include "CoreMinimal.h"
#include "Data/ArpgTypes.h"

class AArpgCharacterBase;
class UWorld;

namespace ArpgCombat
{
	// Living characters hostile to AttackerTeam within Radius of Center. With Forward set, only those inside
	// an arc of ArcDegrees around it (measured on the ground plane).
	TArray<AArpgCharacterBase*> FindTargets(const UWorld* World, const FVector& Center, float Radius, EArpgTeam AttackerTeam,
		const FVector* Forward = nullptr, float ArcDegrees = 360.f);

	// Nearest hostile target, optionally preferring ones in front of Forward.
	AArpgCharacterBase* FindNearest(const UWorld* World, const FVector& Center, float Radius, EArpgTeam AttackerTeam,
		const FVector* Forward = nullptr, float ArcDegrees = 360.f);

	bool HasLineOfSight(const UWorld* World, const FVector& From, const FVector& To);

	// Floating combat text, drawn by AArpgHUD.
	void ShowNumber(const UWorld* World, const FVector& Location, const FString& Text, const FLinearColor& Color, float Scale = 1.f);

	// Mitigation (GDD). Exposed for the HUD's character sheet and tests.
	float ArmourReduction(float Armour, float Hit);
	float EvadeChance(float Evasion, int32 AttackerLevel);
}
