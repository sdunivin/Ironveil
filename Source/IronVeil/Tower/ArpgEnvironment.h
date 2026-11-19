// ArpgEnvironment.h
// Sun, sky, atmosphere, fog and a colour-grading post process, re-lit for every floor's biome so a floor
// is recognisable in one second (GDD art direction). Everything is movable, so it works with Lumen in an empty map.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ArpgTypes.h"
#include "ArpgEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

UCLASS()
class IRONVEIL_API AArpgEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AArpgEnvironment();

	void ApplyBiome(EArpgBiome Biome);
	// Low-health vignette and slow-time desaturation (cosmetic, local only).
	void SetCombatGrade(float LowHealth, float TimeSlow);

private:
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<USkyLightComponent> Sky;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;

	float BaseSaturation = 1.f;
	FLinearColor BaseTint = FLinearColor::White;
};
