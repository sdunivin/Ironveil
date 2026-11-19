#include "Core/ArpgPlayerController.h"

#include "Characters/ArpgPlayerCharacter.h"
#include "Characters/ArpgEnemyCharacter.h"
#include "Core/ArpgGameMode.h"
#include "Loot/ArpgLootSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void AArpgPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AArpgPlayerController::SpireFloor(int32 Floor)
{
	if (AArpgGameMode* GM = GetWorld()->GetAuthGameMode<AArpgGameMode>()) { GM->CheatGoToFloor(Floor); }
}

void AArpgPlayerController::SpireLevel(int32 Level)
{
	if (AArpgPlayerCharacter* P = Cast<AArpgPlayerCharacter>(GetPawn())) { P->SetLevelDirect(Level); }
}

void AArpgPlayerController::SpireGear()
{
	AArpgPlayerCharacter* P = Cast<AArpgPlayerCharacter>(GetPawn());
	UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
	if (!P || !Loot) { return; }
	FRandomStream R(FMath::Rand());
	const int32 ILvl = FMath::Max(1, P->Level);
	const TArray<FName> Set = P->GetClass() == EArpgClass::Mage
		? TArray<FName>{ TEXT("OakStaff"), TEXT("IronHelm"), TEXT("LinenRobe"), TEXT("Gauntlets"), TEXT("RidingBoots"), TEXT("LeatherBelt"), TEXT("BoneAmulet"), TEXT("IronRing"), TEXT("IronRing") }
		: TArray<FName>{ P->GetClass() == EArpgClass::Monk ? FName(TEXT("WarWraps")) : (P->GetClass() == EArpgClass::Thief ? FName(TEXT("Stiletto")) : FName(TEXT("ArmingSword"))),
			TEXT("KiteShield"), TEXT("IronHelm"), TEXT("Brigandine"), TEXT("Gauntlets"), TEXT("RidingBoots"), TEXT("LeatherBelt"), TEXT("BoneAmulet"), TEXT("IronRing"), TEXT("IronRing") };
	for (FArpgItem& E : P->Equipped) { E = FArpgItem(); }
	for (const FName& Base : Set)
	{
		FArpgItem Item = Loot->RollBase(R, Base, ILvl, EArpgRarity::Rare);
		const EArpgGearSlot Slot = P->SlotFor(Item);
		if (P->GetClass() == EArpgClass::Thief && Slot == EArpgGearSlot::OffHand) { continue; }
		P->Equipped[(int32)Slot] = Item;
	}
	P->RecalculateStats();
	P->FullRestore();
	P->Message(TEXT("Cheat: equipped a set of rares."));
}

void AArpgPlayerController::SpireGod()
{
	if (AArpgPlayerCharacter* P = Cast<AArpgPlayerCharacter>(GetPawn()))
	{
		P->bGodMode = !P->bGodMode;
		P->Message(P->bGodMode ? TEXT("God mode ON") : TEXT("God mode OFF"));
	}
}

void AArpgPlayerController::SpireMighty()
{
	if (AArpgPlayerCharacter* P = Cast<AArpgPlayerCharacter>(GetPawn()))
	{
		P->CheatDamageMult = P->CheatDamageMult > 1.f ? 1.f : 25.f;
		P->Message(P->CheatDamageMult > 1.f ? TEXT("Mighty: x25 damage") : TEXT("Mighty off"));
	}
}

void AArpgPlayerController::SpireKillAll()
{
	for (TActorIterator<AArpgEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead()) { It->Kill(Cast<AArpgCharacterBase>(GetPawn())); }
	}
}

void AArpgPlayerController::SpireSeed(int32 Seed)
{
	if (AArpgGameMode* GM = GetWorld()->GetAuthGameMode<AArpgGameMode>()) { GM->CheatSetSeed(Seed); }
}
