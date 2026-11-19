// ArpgHUD.h
// A Canvas-drawn HUD so the prototype needs no widget assets: class select, vitals, skill bar, boss bar,
// enemy nameplates, loot labels, floating damage numbers, the inventory with tooltips, help and end screens.
// Master plan 8.1 replaces this with UMG + Common UI; the data it reads stays the same.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArpgHUD.generated.h"

class AArpgPlayerCharacter;
class AArpgGameMode;
class UFont;
struct FArpgItem;

UCLASS()
class IRONVEIL_API AArpgHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void AddFloatingText(const FVector& Location, const FString& Text, const FLinearColor& Color, float Scale);

private:
	struct FFloating
	{
		FVector Location;
		FString Text;
		FLinearColor Color;
		float Scale = 1.f;
		float Born = 0.f;
		FVector2D Drift;
	};
	TArray<FFloating> Floating;

	float S = 1.f;     // UI scale (1 at 1080p)
	float Now = 0.f;
	UFont* Big = nullptr;
	UFont* Med = nullptr;
	UFont* Small = nullptr;

	// Helpers
	void Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f, bool bCenter = false, bool bShadow = true);
	FVector2D Measure(const FString& Str, UFont* Font, float Scale = 1.f);
	void Box(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor* Border = nullptr);
	void Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Color, const FString& Label = FString());
	bool ToScreen(const FVector& World, FVector2D& Out) const;

	void DrawClassSelect(AArpgPlayerCharacter* P, AArpgGameMode* GM);
	void DrawWorldLabels(AArpgPlayerCharacter* P);
	void DrawFloating();
	void DrawVitals(AArpgPlayerCharacter* P);
	void DrawSkillBar(AArpgPlayerCharacter* P);
	void DrawTopLeft(AArpgPlayerCharacter* P, AArpgGameMode* GM);
	void DrawBoss(AArpgGameMode* GM);
	void DrawPrompts(AArpgPlayerCharacter* P, AArpgGameMode* GM);
	void DrawInventory(AArpgPlayerCharacter* P);
	void DrawTooltip(const FArpgItem& Item, float X, float Y, float W, const FArpgItem* Compare, AArpgPlayerCharacter* P);
	void DrawHelp();
	void DrawEndScreen(AArpgPlayerCharacter* P, AArpgGameMode* GM, bool bVictory);
	void DrawFade(AArpgGameMode* GM);
};
