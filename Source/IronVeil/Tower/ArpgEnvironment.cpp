#include "Tower/ArpgEnvironment.h"

#include "Data/ArpgGameData.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"

AArpgEnvironment::AArpgEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bAtmosphereSunLight = true;
	Sun->SetIntensity(7.f);
	Sun->SetDynamicShadowDistanceMovableLight(12000.f);

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bRealTimeCapture = true;
	Sky->SetIntensity(1.f);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetMobility(EComponentMobility::Movable);
	Fog->SetFogDensity(0.02f);
	Fog->SetFogHeightFalloff(0.2f);

	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;
}

void AArpgEnvironment::ApplyBiome(EArpgBiome Biome)
{
	const FArpgBiomeDef& D = ArpgData::Biome(Biome);
	Sun->SetWorldRotation(FRotator(D.SunPitch, D.SunYaw, 0.f));
	Sun->SetLightColor(D.SunColor);
	Sun->SetIntensity(D.SunIntensity);
	Sky->SetIntensity(D.SkyIntensity);
	Fog->SetFogInscatteringColor(D.FogColor);
	Fog->SetFogDensity(D.FogDensity);
	Fog->SetStartDistance(800.f);
	Sky->RecaptureSky();

	BaseSaturation = D.Saturation;
	BaseTint = D.SceneTint;
	SetCombatGrade(0.f, 0.f);
}

void AArpgEnvironment::SetCombatGrade(float LowHealth, float TimeSlow)
{
	FPostProcessSettings& S = Post->Settings;
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = FMath::Lerp(BaseTint, FLinearColor(1.f, 0.85f, 0.75f), FMath::Clamp(TimeSlow, 0.f, 1.f) * 0.5f);
	S.bOverride_ColorSaturation = true;
	const float Sat = BaseSaturation * (1.f - 0.6f * FMath::Clamp(TimeSlow, 0.f, 1.f)) * (1.f - 0.3f * FMath::Clamp(LowHealth, 0.f, 1.f));
	S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.4f + 0.8f * FMath::Clamp(LowHealth, 0.f, 1.f);
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.9f;
}
