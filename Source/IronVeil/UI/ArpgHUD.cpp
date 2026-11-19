#include "UI/ArpgHUD.h"

#include "Characters/ArpgPlayerCharacter.h"
#include "Characters/ArpgEnemyCharacter.h"
#include "Core/ArpgGameMode.h"
#include "Data/ArpgGameData.h"
#include "Loot/ArpgLootSubsystem.h"
#include "Tower/ArpgInteractables.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const FLinearColor Ink(0.92f, 0.89f, 0.82f);          // off-white UI text (GDD UI style)
	const FLinearColor Dim(0.6f, 0.58f, 0.54f);
	const FLinearColor Panel(0.035f, 0.03f, 0.028f, 0.82f); // worn leather / iron
	const FLinearColor Iron(0.38f, 0.33f, 0.27f, 1.f);

	FString Clock(float Seconds)
	{
		const int32 T = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d:%02d"), T / 3600, (T / 60) % 60, T % 60);
	}
}

// ---------------------------------------------------------------- helpers

void AArpgHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCenter, bool bShadow)
{
	const float Sc = Scale * S;
	if (bCenter)
	{
		X -= Measure(Str, Font, Scale).X * 0.5f;
	}
	if (bShadow) { DrawText(Str, FLinearColor(0, 0, 0, Color.A * 0.8f), X + 2.f * S, Y + 2.f * S, Font, Sc); }
	DrawText(Str, Color, X, Y, Font, Sc);
}

FVector2D AArpgHUD::Measure(const FString& Str, UFont* Font, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale * S);
	return FVector2D(W, H);
}

void AArpgHUD::Box(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor* Border)
{
	if (Border)
	{
		DrawRect(*Border, X - 2.f * S, Y - 2.f * S, W + 4.f * S, H + 4.f * S);
	}
	DrawRect(Fill, X, Y, W, H);
}

void AArpgHUD::Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Color, const FString& Label)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X - 2.f * S, Y - 2.f * S, W + 4.f * S, H + 4.f * S);
	DrawRect(Color * 0.25f, X, Y, W, H);
	DrawRect(Color, X, Y, W * FMath::Clamp(Frac, 0.f, 1.f), H);
	if (!Label.IsEmpty())
	{
		Text(Label, X + W * 0.5f, Y + (H - Measure(Label, Small, 0.9f).Y) * 0.5f, Ink, Small, 0.9f, true);
	}
}

bool AArpgHUD::ToScreen(const FVector& World, FVector2D& Out) const
{
	if (!Canvas || !PlayerOwner || !PlayerOwner->PlayerCameraManager) { return false; }
	const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector Fwd = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - Cam, Fwd) <= 0.f) { return false; }
	const FVector P = Canvas->Project(World);
	Out = FVector2D(P.X, P.Y);
	return Out.X > -50.f && Out.Y > -50.f && Out.X < Canvas->ClipX + 50.f && Out.Y < Canvas->ClipY + 50.f;
}

void AArpgHUD::AddFloatingText(const FVector& Location, const FString& Str, const FLinearColor& Color, float Scale)
{
	FFloating F;
	F.Location = Location;
	F.Text = Str;
	F.Color = Color;
	F.Scale = Scale;
	F.Born = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	F.Drift = FVector2D(FMath::FRandRange(-30.f, 30.f), 0.f);
	Floating.Add(F);
	if (Floating.Num() > 120) { Floating.RemoveAt(0); }
}

// ---------------------------------------------------------------- main

void AArpgHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }
	S = Canvas->ClipY / 1080.f;
	Now = GetWorld()->GetTimeSeconds();
	Big = GEngine->GetLargeFont();
	Med = GEngine->GetMediumFont();
	Small = GEngine->GetSmallFont();

	AArpgPlayerCharacter* P = Cast<AArpgPlayerCharacter>(GetOwningPawn());
	AArpgGameMode* GM = GetWorld()->GetAuthGameMode<AArpgGameMode>();
	if (!P || !GM) { return; }

	if (P->UIState == EArpgPlayerUI::ClassSelect)
	{
		DrawClassSelect(P, GM);
		DrawFade(GM);
		return;
	}

	DrawWorldLabels(P);
	DrawFloating();
	DrawTopLeft(P, GM);
	DrawBoss(GM);
	DrawVitals(P);
	DrawSkillBar(P);
	DrawPrompts(P, GM);
	if (P->UIState == EArpgPlayerUI::Inventory) { DrawInventory(P); }
	if (P->UIState == EArpgPlayerUI::Dead) { DrawEndScreen(P, GM, false); }
	if (P->UIState == EArpgPlayerUI::Victory) { DrawEndScreen(P, GM, true); }
	if (P->bHelpOpen) { DrawHelp(); }
	DrawFade(GM);
}

void AArpgHUD::DrawFade(AArpgGameMode* GM)
{
	const float A = 1.f - (Now - GM->FadeTime) / 1.2f;
	if (A > 0.f) { DrawRect(FLinearColor(0.f, 0.f, 0.f, FMath::Clamp(A, 0.f, 1.f)), 0, 0, Canvas->ClipX, Canvas->ClipY); }

	// Floor banner (the transition card from the GDD).
	const float T = Now - GM->BannerTime;
	if (T < 5.f)
	{
		const float Alpha = FMath::Clamp(FMath::Min(T / 0.6f, (5.f - T) / 1.f), 0.f, 1.f);
		const float Y = Canvas->ClipY * 0.2f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha), 0, Y - 20.f * S, Canvas->ClipX, 110.f * S);
		Text(GM->BannerTitle, Canvas->ClipX * 0.5f, Y, FLinearColor(0.95f, 0.85f, 0.6f, Alpha), Big, 1.6f, true);
		Text(GM->BannerSub, Canvas->ClipX * 0.5f, Y + 50.f * S, FLinearColor(Ink.R, Ink.G, Ink.B, Alpha), Med, 1.f, true);
	}
}

// ---------------------------------------------------------------- class select

void AArpgHUD::DrawClassSelect(AArpgPlayerCharacter* P, AArpgGameMode* GM)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0, 0, W, H);
	Text(TEXT("IRONVEIL"), W * 0.5f, 40.f * S, FLinearColor(0.95f, 0.85f, 0.6f), Big, 2.2f, true);
	Text(TEXT("THE VEIL SPIRE  -  CLIMB  -  SURVIVE  -  ASCEND"), W * 0.5f, 105.f * S, Ink, Med, 1.f, true);

	const float CardW = 400.f * S, CardH = 560.f * S, Gap = 24.f * S;
	const float Total = CardW * 4.f + Gap * 3.f;
	const float X0 = (W - Total) * 0.5f;
	const float Y0 = H - CardH - 110.f * S;
	for (int32 i = 0; i < 4; ++i)
	{
		const FArpgClassDef& C = ArpgData::Class((EArpgClass)i);
		const bool bSel = P->MenuIndex == i;
		const float X = X0 + i * (CardW + Gap);
		const FLinearColor Border = bSel ? C.Color : Iron;
		Box(X, Y0, CardW, CardH, FLinearColor(0.03f, 0.03f, 0.04f, bSel ? 0.92f : 0.75f), &Border);
		DrawRect(C.Color * 0.6f, X, Y0, CardW, 6.f * S);
		float Y = Y0 + 18.f * S;
		Text(FString::Printf(TEXT("%d  %s"), i + 1, *C.Name.ToUpper()), X + 18.f * S, Y, C.Color, Big, 1.25f); Y += 46.f * S;
		Text(C.Tagline, X + 18.f * S, Y, Dim, Small, 1.f); Y += 30.f * S;
		// Word-wrap the description roughly.
		TArray<FString> Words;
		C.Description.ParseIntoArray(Words, TEXT(" "));
		FString Line;
		for (const FString& Wd : Words)
		{
			const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
			if (Measure(Try, Small, 1.f).X > CardW - 36.f * S) { Text(Line, X + 18.f * S, Y, Ink, Small); Y += 22.f * S; Line = Wd; }
			else { Line = Try; }
		}
		if (!Line.IsEmpty()) { Text(Line, X + 18.f * S, Y, Ink, Small); Y += 30.f * S; }
		Text(FString::Printf(TEXT("Str %d   Dex %d   Int %d"), (int32)C.Strength, (int32)C.Dexterity, (int32)C.Intelligence), X + 18.f * S, Y, Ink, Small); Y += 34.f * S;
		Text(TEXT("KEY ABILITIES"), X + 18.f * S, Y, C.Color, Small); Y += 26.f * S;
		for (int32 k = 0; k < 4; ++k)
		{
			DrawRect(C.Skills[k].Color * 0.8f, X + 18.f * S, Y + 3.f * S, 14.f * S, 14.f * S);
			Text(C.Skills[k].Name, X + 42.f * S, Y, Ink, Small); Y += 22.f * S;
			// First line of the skill text only.
			FString Desc = C.Skills[k].Description;
			if (Measure(Desc, Small, 0.85f).X > CardW - 60.f * S) { Desc = Desc.Left(46) + TEXT("..."); }
			Text(Desc, X + 42.f * S, Y, Dim, Small, 0.85f); Y += 26.f * S;
		}
		TArray<FString> Gear;
		for (const FName& G : C.StartingItems) { Gear.Add(G.ToString()); }
		Text(TEXT("Starts with: ") + FString::Join(Gear, TEXT(", ")), X + 18.f * S, Y0 + CardH - 34.f * S, Dim, Small, 0.85f);
	}
	FString Hint = TEXT("1-4 or Left/Right + Enter: choose a class (gamepad: D-pad + A)");
	if (GM->HasSave()) { Hint += TEXT("     C: continue your saved climb"); }
	Text(Hint, W * 0.5f, H - 70.f * S, Ink, Med, 1.f, true);
	if (GM->BestFloor > 0) { Text(FString::Printf(TEXT("Highest floor reached: %d"), GM->BestFloor), W * 0.5f, H - 40.f * S, Dim, Small, 1.f, true); }
}

// ---------------------------------------------------------------- world-space labels

void AArpgHUD::DrawWorldLabels(AArpgPlayerCharacter* P)
{
	const FVector PL = P->GetActorLocation();
	AArpgEnemyCharacter* Locked = P->GetLockTarget();

	for (TActorIterator<AArpgEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AArpgEnemyCharacter* E = *It;
		if (E->IsDead() || E->bIsBoss) { continue; }
		const float D = FVector::Dist(E->GetActorLocation(), PL);
		if (D > 2600.f) { continue; }
		FVector2D Sp;
		if (!ToScreen(E->GetActorLocation() + FVector(0, 0, E->GetSimpleCollisionHalfHeight() + 40.f), Sp)) { continue; }
		const bool bNotable = E->GetRarity() != EArpgMonsterRarity::Normal || E == Locked;
		const float BW = (bNotable ? 120.f : 80.f) * S;
		if (E->GetHealthFraction() < 1.f || bNotable)
		{
			Bar(Sp.X - BW * 0.5f, Sp.Y, BW, 7.f * S, E->GetHealthFraction(), FLinearColor(0.75f, 0.1f, 0.08f));
			if (E->MaxPoise > 0.f && E->GetPoiseFraction() < 1.f)
			{
				DrawRect(FLinearColor(1.f, 0.8f, 0.3f), Sp.X - BW * 0.5f, Sp.Y + 9.f * S, BW * E->GetPoiseFraction(), 3.f * S);
			}
		}
		if (bNotable)
		{
			Text(E->DisplayName, Sp.X, Sp.Y - 24.f * S, E->GetNameColor(), Small, 1.f, true);
			const FString Mods = E->GetModText();
			if (!Mods.IsEmpty()) { Text(Mods, Sp.X, Sp.Y - 42.f * S, Dim, Small, 0.8f, true); }
		}
		if (E->IsStaggered()) { Text(TEXT("STAGGERED - grab (F)"), Sp.X, Sp.Y + 14.f * S, FLinearColor(1.f, 0.8f, 0.3f), Small, 0.8f, true); }
		if (E->IsTelegraphing()) { Text(TEXT("! UNBLOCKABLE - DODGE !"), Sp.X, Sp.Y - 60.f * S, FLinearColor(1.f, 0.15f, 0.1f), Small, 1.f, true); }
		if (E == Locked)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.9f), Sp.X - 5.f * S, Sp.Y + 40.f * S, 10.f * S, 10.f * S);
		}
	}

	for (TActorIterator<AArpgLootDrop> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), PL);
		if (D > 1800.f) { continue; }
		FVector2D Sp;
		if (!ToScreen(It->GetActorLocation() + FVector(0, 0, 40), Sp)) { continue; }
		const FString& Name = It->Item.DisplayName;
		const FVector2D Sz = Measure(Name, Small, 0.9f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), Sp.X - Sz.X * 0.5f - 6.f * S, Sp.Y - 2.f * S, Sz.X + 12.f * S, Sz.Y + 4.f * S);
		Text(Name, Sp.X, Sp.Y, ArpgColors::Rarity(It->Item.Rarity), Small, 0.9f, true, false);
	}
}

void AArpgHUD::DrawFloating()
{
	const float RealNow = GetWorld()->GetRealTimeSeconds();
	Floating.RemoveAll([RealNow](const FFloating& F) { return RealNow - F.Born > 1.1f; });
	for (const FFloating& F : Floating)
	{
		const float Age = RealNow - F.Born;
		FVector2D Sp;
		if (!ToScreen(F.Location, Sp)) { continue; }
		const float Alpha = FMath::Clamp(1.4f - Age * 1.3f, 0.f, 1.f);
		const float Pop = 1.f + FMath::Max(0.f, 0.25f - Age) * 2.f;
		FLinearColor C = F.Color;
		C.A = Alpha;
		Text(F.Text, Sp.X + F.Drift.X * Age * S, Sp.Y - Age * 70.f * S, C, Med, F.Scale * Pop, true);
	}
}

// ---------------------------------------------------------------- vitals and skills

void AArpgHUD::DrawVitals(AArpgPlayerCharacter* P)
{
	const float X = 40.f * S;
	float Y = Canvas->ClipY - 170.f * S;
	const float W = 380.f * S;
	const float HP = P->Stats.MaxHealth;
	FString HLabel = FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(P->Health), FMath::RoundToInt(HP));
	if (P->Barrier > 0.f) { HLabel += FString::Printf(TEXT("  +%d barrier"), FMath::RoundToInt(P->Barrier)); }
	Bar(X, Y, W, 24.f * S, P->Health / FMath::Max(1.f, HP), FLinearColor(0.7f, 0.08f, 0.06f), HLabel);
	if (P->Stats.MaxEnergyShield > 0.f)
	{
		DrawRect(FLinearColor(0.5f, 0.75f, 1.f, 0.85f), X, Y - 6.f * S, W * (P->EnergyShield / P->Stats.MaxEnergyShield), 5.f * S);
	}
	if (P->Barrier > 0.f)
	{
		DrawRect(FLinearColor(0.6f, 0.8f, 1.f, 0.9f), X, Y + 20.f * S, W * FMath::Clamp(P->Barrier / HP, 0.f, 1.f), 4.f * S);
	}
	Y += 32.f * S;
	Bar(X, Y, W, 16.f * S, P->Mana / FMath::Max(1.f, P->Stats.MaxMana), FLinearColor(0.12f, 0.25f, 0.8f),
		FString::Printf(TEXT("%d / %d"), FMath::FloorToInt(P->Mana), FMath::RoundToInt(P->Stats.MaxMana)));
	Y += 24.f * S;
	Bar(X, Y, W, 10.f * S, P->Stamina / FMath::Max(1.f, P->Stats.MaxStamina), FLinearColor(0.85f, 0.75f, 0.2f));
	Y += 20.f * S;
	// Flasks
	for (int32 i = 0; i < P->MaxFlasks; ++i)
	{
		const FLinearColor C = i < P->Flasks ? FLinearColor(0.85f, 0.15f, 0.12f) : FLinearColor(0.15f, 0.1f, 0.1f);
		Box(X + i * 34.f * S, Y, 26.f * S, 34.f * S, C, &Iron);
	}
	Text(TEXT("R"), X + P->MaxFlasks * 34.f * S + 6.f * S, Y + 6.f * S, Dim, Small);
	// Buffs
	FString Buffs;
	if (P->FuryRemaining > 0.f) { Buffs += FString::Printf(TEXT("Fury %.0fs  "), P->FuryRemaining); }
	if (P->BarrierRemaining > 0.f) { Buffs += FString::Printf(TEXT("Barrier %.0fs  "), P->BarrierRemaining); }
	if (P->ZenRemaining > 0.f) { Buffs += FString::Printf(TEXT("Zen %.0fs  "), P->ZenRemaining); }
	if (P->GuaranteedCritRemaining > 0.f) { Buffs += TEXT("Next hit crits  "); }
	if (P->IsShocked()) { Buffs += TEXT("Shocked  "); }
	if (P->GetChill() > 0.f) { Buffs += TEXT("Chilled  "); }
	if (P->HasAilment(true, true, true)) { Buffs += TEXT("Burning/Bleeding/Poisoned  "); }
	if (!Buffs.IsEmpty()) { Text(Buffs, X, Canvas->ClipY - 205.f * S, FLinearColor(1.f, 0.85f, 0.5f), Small); }

	// XP bar across the bottom
	const float Need = ArpgData::XPToNext(P->Level);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0, Canvas->ClipY - 8.f * S, Canvas->ClipX, 8.f * S);
	DrawRect(FLinearColor(0.75f, 0.6f, 0.2f), 0, Canvas->ClipY - 7.f * S, Canvas->ClipX * FMath::Clamp(P->GetXP() / Need, 0.f, 1.f), 6.f * S);
	Text(FString::Printf(TEXT("Level %d %s"), P->Level, *ArpgData::Class(P->GetClass()).Name), X, Canvas->ClipY - 36.f * S, Ink, Small);
}

void AArpgHUD::DrawSkillBar(AArpgPlayerCharacter* P)
{
	const FArpgClassDef& C = ArpgData::Class(P->GetClass());
	const float Slot = 72.f * S, Gap = 10.f * S;
	const float X0 = Canvas->ClipX * 0.5f - (Slot * 4.f + Gap * 3.f) * 0.5f;
	const float Y = Canvas->ClipY - Slot - 40.f * S;
	for (int32 i = 0; i < 4; ++i)
	{
		const FArpgSkillDef& Sk = C.Skills[i];
		const float X = X0 + i * (Slot + Gap);
		const bool bCanPay = P->Mana >= Sk.ManaCost;
		Box(X, Y, Slot, Slot, Sk.Color * (bCanPay ? 0.55f : 0.2f), &Iron);
		DrawRect(Sk.Color, X, Y, Slot, 4.f * S);
		const float Cd = P->SkillCooldowns[i];
		const float CdMax = P->GetCooldownMax(i);
		if (Cd > 0.f && CdMax > 0.f)
		{
			const float F = Cd / CdMax;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), X, Y + Slot * (1.f - F), Slot, Slot * F);
			Text(FString::Printf(TEXT("%.1f"), Cd), X + Slot * 0.5f, Y + Slot * 0.35f, Ink, Med, 1.f, true);
		}
		Text(FString::Printf(TEXT("%d"), i + 1), X + 5.f * S, Y + 6.f * S, Ink, Small, 0.9f);
		if (Sk.ManaCost > 0.f) { Text(FString::Printf(TEXT("%d"), (int32)Sk.ManaCost), X + Slot - 22.f * S, Y + Slot - 20.f * S, FLinearColor(0.5f, 0.65f, 1.f), Small, 0.8f); }
		Text(Sk.Name, X + Slot * 0.5f, Y + Slot + 4.f * S, Dim, Small, 0.75f, true);
	}
}

void AArpgHUD::DrawTopLeft(AArpgPlayerCharacter* P, AArpgGameMode* GM)
{
	const float X = 30.f * S;
	float Y = 24.f * S;
	const FArpgBiomeDef& B = ArpgData::Biome(GM->CurrentBiome);
	const FString Title = GM->CurrentFloor > 0 ? FString::Printf(TEXT("FLOOR %d / %d  -  %s"), GM->CurrentFloor, GM->TotalFloors, *B.Name) : FString(TEXT("GREYHOLLOW"));
	Box(X - 10.f * S, Y - 8.f * S, 470.f * S, 104.f * S, Panel);
	Text(Title, X, Y, FLinearColor(0.95f, 0.85f, 0.6f), Med, 1.f); Y += 30.f * S;
	Text(GM->GetObjectiveText(), X, Y, GM->bObjectiveDone ? FLinearColor(0.5f, 1.f, 0.6f) : Ink, Small, 1.f); Y += 24.f * S;
	if (GM->CurrentFloor > 0)
	{
		FString Timer = FString::Printf(TEXT("Run %s   Floor %s"), *Clock(GM->RunTime), *Clock(GM->FloorTime));
		if (GM->LastSplit > 0.f) { Timer += FString::Printf(TEXT("   Last split %s"), *Clock(GM->LastSplit)); }
		Text(Timer, X, Y, Dim, Small, 0.9f); Y += 20.f * S;
		Text(FString::Printf(TEXT("Monster level %d   Checkpoint: %s"), GM->GetMonsterLevel(), GM->Checkpoint > 0 ? *FString::FromInt(GM->Checkpoint) : TEXT("Greyhollow")),
			X, Y, Dim, Small, 0.9f);
	}
	else
	{
		Text(TEXT("F1 / H: controls     I: inventory"), X, Y, Dim, Small, 0.9f);
	}
}

void AArpgHUD::DrawBoss(AArpgGameMode* GM)
{
	AArpgEnemyCharacter* E = GM->Boss.Get();
	if (!E || E->IsDead()) { return; }
	const float W = Canvas->ClipX * 0.5f;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 40.f * S;
	Text(E->DisplayName, Canvas->ClipX * 0.5f, Y - 4.f * S, FLinearColor(1.f, 0.6f, 0.25f), Med, 1.1f, true);
	Bar(X, Y + 30.f * S, W, 18.f * S, E->GetHealthFraction(), FLinearColor(0.65f, 0.08f, 0.05f),
		FString::Printf(TEXT("%d%%  -  phase %d"), FMath::CeilToInt(E->GetHealthFraction() * 100.f), E->GetBossPhase()));
	DrawRect(FLinearColor(1.f, 0.8f, 0.3f), X, Y + 52.f * S, W * E->GetPoiseFraction(), 5.f * S);
}

void AArpgHUD::DrawPrompts(AArpgPlayerCharacter* P, AArpgGameMode* GM)
{
	const float CX = Canvas->ClipX * 0.5f;
	const FString Prompt = P->GetInteractText();
	if (!Prompt.IsEmpty() && P->UIState == EArpgPlayerUI::Playing)
	{
		const FString Full = TEXT("[E]  ") + Prompt;
		const FVector2D Sz = Measure(Full, Med, 1.f);
		Box(CX - Sz.X * 0.5f - 12.f * S, Canvas->ClipY * 0.68f - 6.f * S, Sz.X + 24.f * S, Sz.Y + 12.f * S, Panel);
		Text(Full, CX, Canvas->ClipY * 0.68f, Ink, Med, 1.f, true);
	}
	// Player messages
	if (Now - P->LastMessageTime < 3.f)
	{
		Text(P->LastMessage, CX, Canvas->ClipY * 0.74f, FLinearColor(1.f, 0.95f, 0.8f, FMath::Clamp(3.f - (Now - P->LastMessageTime), 0.f, 1.f)), Small, 1.1f, true);
	}
	// Announcements
	if (Now - GM->AnnounceTime < 4.5f)
	{
		FLinearColor C = GM->AnnounceColor;
		C.A = FMath::Clamp(4.5f - (Now - GM->AnnounceTime), 0.f, 1.f);
		Text(GM->AnnounceText, CX, Canvas->ClipY * 0.32f, C, Med, 1.15f, true);
	}
	// Crosshair dot (helps aim the grapple and bolts)
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.5f), CX - 2.f * S, Canvas->ClipY * 0.5f - 2.f * S, 4.f * S, 4.f * S);
}

// ---------------------------------------------------------------- inventory

void AArpgHUD::DrawTooltip(const FArpgItem& Item, float X, float Y, float W, const FArpgItem* Compare, AArpgPlayerCharacter* P)
{
	UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
	if (!Loot) { return; }
	TArray<FString> Lines = Loot->DescribeItem(Item);
	const float LineH = 22.f * S;
	float H = (Lines.Num() + 2) * LineH + 20.f * S;
	const FLinearColor RC = ArpgColors::Rarity(Item.Rarity);
	Box(X, Y, W, H, FLinearColor(0.02f, 0.02f, 0.025f, 0.95f), &RC);
	float LY = Y + 10.f * S;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const bool bHeader = i == 0 || (i == 1 && (Item.Rarity == EArpgRarity::Rare || Item.Rarity == EArpgRarity::Unique));
		FLinearColor C = bHeader ? RC : Ink;
		if (Lines[i].StartsWith(TEXT("~"))) { C = Dim; }
		if (Lines[i].StartsWith(TEXT("*"))) { C = FLinearColor(1.f, 0.6f, 0.2f); }
		if (Lines[i].StartsWith(TEXT("Requires")) && P && !P->CanEquip(Item)) { C = FLinearColor(1.f, 0.35f, 0.3f); }
		Text(Lines[i], X + 12.f * S, LY, C, Small, bHeader ? 1.05f : 0.92f, false, false);
		LY += LineH;
	}
	if (Compare && Compare->IsValid())
	{
		Text(FString::Printf(TEXT("Replaces: %s"), *Compare->DisplayName), X + 12.f * S, LY + 6.f * S, Dim, Small, 0.85f, false, false);
	}
}

void AArpgHUD::DrawInventory(AArpgPlayerCharacter* P)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0, 0, W, H);
	const float PX = 60.f * S, PY = 80.f * S, PW = 470.f * S;
	Box(PX, PY, PW, H - 200.f * S, Panel, &Iron);
	Text(TEXT("EQUIPPED"), PX + 16.f * S, PY + 12.f * S, FLinearColor(0.95f, 0.85f, 0.6f), Med);
	static const TCHAR* SlotNames[] = { TEXT("Main hand"), TEXT("Off hand"), TEXT("Helmet"), TEXT("Body"), TEXT("Gloves"), TEXT("Boots"), TEXT("Belt"),
		TEXT("Amulet"), TEXT("Ring"), TEXT("Ring") };
	float Y = PY + 52.f * S;
	for (int32 i = 0; i < ArpgNumGearSlots; ++i)
	{
		const FArpgItem& E = P->Equipped[i];
		Text(SlotNames[i], PX + 16.f * S, Y, Dim, Small);
		Text(E.IsValid() ? E.DisplayName : FString(TEXT("-")), PX + 130.f * S, Y, E.IsValid() ? ArpgColors::Rarity(E.Rarity) : Dim, Small);
		Y += 24.f * S;
	}
	// Character sheet summary
	Y += 16.f * S;
	const FArpgStats& St = P->Stats;
	Text(TEXT("CHARACTER"), PX + 16.f * S, Y, FLinearColor(0.95f, 0.85f, 0.6f), Med); Y += 34.f * S;
	auto Line = [&](const FString& L) { Text(L, PX + 16.f * S, Y, Ink, Small, 0.92f); Y += 21.f * S; };
	Line(FString::Printf(TEXT("Str %d   Dex %d   Int %d"), (int32)St.Strength, (int32)St.Dexterity, (int32)St.Intelligence));
	Line(FString::Printf(TEXT("Health %d   Mana %d   Energy Shield %d"), (int32)St.MaxHealth, (int32)St.MaxMana, (int32)St.MaxEnergyShield));
	Line(FString::Printf(TEXT("Weapon %d-%d phys   %.2f attacks/s   %.1f%% crit"), (int32)St.WeaponMin[0], (int32)St.WeaponMax[0],
		St.WeaponAPS * (1.f + St.IncAttackSpeed / 100.f), St.WeaponCrit));
	Line(FString::Printf(TEXT("Armour %d   Evasion %d   Block %d%%"), (int32)St.Armour, (int32)St.Evasion, (int32)St.BlockChance));
	Line(FString::Printf(TEXT("Resist  Fire %d%%  Cold %d%%  Lightning %d%%  Chaos %d%%"), (int32)St.Resist[1], (int32)St.Resist[2], (int32)St.Resist[3], (int32)St.Resist[4]));
	Line(FString::Printf(TEXT("Increased: melee phys %d%%  spell %d%%  elemental %d%%"), (int32)(St.IncPhysical + St.IncMeleePhysical), (int32)St.IncSpell, (int32)St.IncElemental));
	Line(FString::Printf(TEXT("Item rarity +%d%%   Move speed +%d%%"), (int32)St.IncItemRarity, (int32)St.IncMoveSpeed));

	// Bag
	const float BX = PX + PW + 30.f * S, BW = 480.f * S;
	Box(BX, PY, BW, H - 200.f * S, Panel, &Iron);
	Text(FString::Printf(TEXT("INVENTORY  (%d / 40)"), P->Inventory.Num()), BX + 16.f * S, PY + 12.f * S, FLinearColor(0.95f, 0.85f, 0.6f), Med);
	Y = PY + 52.f * S;
	const int32 Visible = FMath::Max(1, FMath::FloorToInt((H - 300.f * S) / (24.f * S)));
	const int32 First = FMath::Clamp(P->InventoryCursor - Visible / 2, 0, FMath::Max(0, P->Inventory.Num() - Visible));
	for (int32 i = First; i < FMath::Min(P->Inventory.Num(), First + Visible); ++i)
	{
		const FArpgItem& It = P->Inventory[i];
		if (i == P->InventoryCursor) { DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), BX + 6.f * S, Y - 2.f * S, BW - 12.f * S, 24.f * S); }
		Text(It.DisplayName, BX + 16.f * S, Y, ArpgColors::Rarity(It.Rarity), Small);
		Text(FString::Printf(TEXT("ilvl %d"), It.ItemLevel), BX + BW - 70.f * S, Y, Dim, Small, 0.85f);
		Y += 24.f * S;
	}
	if (P->Inventory.Num() == 0) { Text(TEXT("Empty. Loot drops from monsters and chests."), BX + 16.f * S, Y, Dim, Small); }
	Text(TEXT("Up/Down: select   Enter: equip   X: salvage   I / Backspace: close"), BX + 16.f * S, PY + H - 240.f * S, Dim, Small, 0.85f);

	if (P->Inventory.IsValidIndex(P->InventoryCursor))
	{
		const FArpgItem& Sel = P->Inventory[P->InventoryCursor];
		const FArpgItem& Cur = P->Equipped[(int32)P->SlotFor(Sel)];
		DrawTooltip(Sel, BX + BW + 30.f * S, PY, 460.f * S, &Cur, P);
		if (Cur.IsValid()) { DrawTooltip(Cur, BX + BW + 30.f * S, PY + 380.f * S, 460.f * S, nullptr, P); }
	}
}

// ---------------------------------------------------------------- overlays

void AArpgHUD::DrawHelp()
{
	const float W = 760.f * S, H = 690.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f, Y = (Canvas->ClipY - H) * 0.5f;
	Box(X, Y, W, H, FLinearColor(0.02f, 0.02f, 0.025f, 0.94f), &Iron);
	Text(TEXT("CONTROLS"), X + W * 0.5f, Y + 16.f * S, FLinearColor(0.95f, 0.85f, 0.6f), Big, 1.f, true);
	const TCHAR* Rows[][3] = {
		{ TEXT("Move / camera"), TEXT("WASD / mouse"), TEXT("Left / right stick") },
		{ TEXT("Light attack (combo)"), TEXT("Left mouse"), TEXT("RB") },
		{ TEXT("Heavy (hold 0.8s to charge)"), TEXT("Right mouse"), TEXT("RT") },
		{ TEXT("Dodge (hold: sprint)"), TEXT("Space"), TEXT("B") },
		{ TEXT("Jump"), TEXT("Left Ctrl"), TEXT("A") },
		{ TEXT("Block / perfect parry"), TEXT("Left Shift"), TEXT("LB") },
		{ TEXT("Grab (F again: throw, LMB: slam, RMB: shove)"), TEXT("F"), TEXT("X") },
		{ TEXT("Grappling hook"), TEXT("Q"), TEXT("Y") },
		{ TEXT("Lock-on / switch (flick)"), TEXT("Middle mouse / Tab"), TEXT("R3") },
		{ TEXT("Skills 1-4"), TEXT("1 2 3 4"), TEXT("Hold LT + A B X Y") },
		{ TEXT("Health flask"), TEXT("R"), TEXT("D-pad up") },
		{ TEXT("Interact / pick up"), TEXT("E"), TEXT("X (near item)") },
		{ TEXT("Inventory"), TEXT("I"), TEXT("View") },
		{ TEXT("This help"), TEXT("F1 / H"), TEXT("Menu") } };
	float RY = Y + 70.f * S;
	for (const auto& R : Rows)
	{
		Text(R[0], X + 30.f * S, RY, Ink, Small);
		Text(R[1], X + 420.f * S, RY, FLinearColor(0.95f, 0.85f, 0.6f), Small);
		Text(R[2], X + 590.f * S, RY, Dim, Small);
		RY += 30.f * S;
	}
	RY += 10.f * S;
	Text(TEXT("Stagger enemies by breaking their poise (yellow bar), then grab and throw them into others."), X + 30.f * S, RY, Dim, Small, 0.9f); RY += 22.f * S;
	Text(TEXT("Red flash = unblockable: dodge it. Jump over shockwave rings. Block just before a hit lands to parry."), X + 30.f * S, RY, Dim, Small, 0.9f); RY += 22.f * S;
	Text(TEXT("Console (`): SpireFloor N, SpireLevel N, SpireGear, SpireGod, SpireMighty, SpireKillAll"), X + 30.f * S, RY, Dim, Small, 0.9f);
}

void AArpgHUD::DrawEndScreen(AArpgPlayerCharacter* P, AArpgGameMode* GM, bool bVictory)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.4f;
	if (bVictory)
	{
		Text(TEXT("THE HOURLESS KING HAS FALLEN"), CX, CY, FLinearColor(1.f, 0.85f, 0.4f), Big, 1.8f, true);
		Text(FString::Printf(TEXT("You climbed the Veil Spire in %s"), *Clock(GM->RunTime)), CX, CY + 70.f * S, Ink, Med, 1.1f, true);
		Text(TEXT("Enter / A: continue into the Overtime floors"), CX, CY + 120.f * S, Dim, Med, 1.f, true);
	}
	else
	{
		Text(TEXT("YOU HAVE FALLEN"), CX, CY, FLinearColor(0.85f, 0.15f, 0.1f), Big, 1.8f, true);
		Text(FString::Printf(TEXT("Floor %d  -  level %d"), GM->CurrentFloor, P->Level), CX, CY + 70.f * S, Ink, Med, 1.f, true);
		Text(GM->Checkpoint > 0 ? FString::Printf(TEXT("Enter / A: return to your sanctuary on floor %d (the floors above will reshuffle)"), GM->Checkpoint)
			: FString(TEXT("Enter / A: return to Greyhollow")), CX, CY + 110.f * S, Dim, Med, 1.f, true);
	}
}
