#include "Core/ArpgGameMode.h"

#include "IronVeil.h"
#include "Characters/ArpgPlayerCharacter.h"
#include "Characters/ArpgEnemyCharacter.h"
#include "Combat/ArpgAreaEffect.h"
#include "Combat/ArpgCombat.h"
#include "Combat/ArpgProjectile.h"
#include "Core/ArpgPlayerController.h"
#include "Core/ArpgSaveGame.h"
#include "Data/ArpgGameData.h"
#include "Loot/ArpgLootSubsystem.h"
#include "Tower/ArpgEnvironment.h"
#include "Tower/ArpgFloorGenerator.h"
#include "Tower/ArpgInteractables.h"
#include "UI/ArpgHUD.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AArpgGameMode::AArpgGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AArpgPlayerCharacter::StaticClass();
	PlayerControllerClass = AArpgPlayerController::StaticClass();
	HUDClass = AArpgHUD::StaticClass();
}

void AArpgGameMode::BeginPlay()
{
	Super::BeginPlay();
	LootRng.Initialize(FMath::Rand());
	RunSeed = FMath::Rand();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Environment = GetWorld()->SpawnActor<AArpgEnvironment>(AArpgEnvironment::StaticClass(), FTransform::Identity, SP);
	Floor = GetWorld()->SpawnActor<AArpgFloorGenerator>(AArpgFloorGenerator::StaticClass(), FTransform::Identity, SP);

	if (UArpgSaveGame* Save = Cast<UArpgSaveGame>(UGameplayStatics::LoadGameFromSlot(UArpgSaveGame::SlotName, 0)))
	{
		BestFloor = Save->BestFloor;
		bHasSave = true;
	}
	LoadHub();
}

void AArpgGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	if (Floor && Floor->Rooms.Num() > 0) { PlacePlayer(); }
}

AArpgPlayerCharacter* AArpgGameMode::GetPlayerCharacter() const
{
	return Cast<AArpgPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AArpgGameMode::Track(AActor* Actor)
{
	if (Actor) { FloorActors.Add(Actor); }
}

void AArpgGameMode::ClearFloorActors()
{
	for (const TWeakObjectPtr<AActor>& A : FloorActors)
	{
		if (A.IsValid()) { A->Destroy(); }
	}
	FloorActors.Reset();
	// Projectiles, ground effects and loot left behind.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AArpgLootDrop>() || It->IsA<AArpgAreaEffect>() || It->IsA<AArpgProjectile>())
		{
			It->Destroy();
		}
	}
	MeleeTokens.Reset();
	RangedTokens.Reset();
	Portal = nullptr;
	Relic = nullptr;
	Boss = nullptr;
	HuntTarget = nullptr;
}

void AArpgGameMode::PlacePlayer()
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!P || !Floor) { return; }
	P->SetActorLocationAndRotation(Floor->StartLocation, Floor->StartRotation, false, nullptr, ETeleportType::ResetPhysics);
	P->GetCharacterMovement()->StopMovementImmediately();
	if (AController* C = P->GetController()) { C->SetControlRotation(FRotator(-12.f, Floor->StartRotation.Yaw, 0.f)); }
}

void AArpgGameMode::Announce(const FString& Text, const FLinearColor& Color)
{
	AnnounceText = Text;
	AnnounceColor = Color;
	AnnounceTime = GetWorld()->GetTimeSeconds();
}

int32 AArpgGameMode::GetMonsterLevel() const
{
	return FMath::Max(1, CurrentFloor);
}

float AArpgGameMode::FloorRarityBonus() const
{
	// GDD: +2% increased item rarity per floor; guardian floors double it.
	return 2.f * CurrentFloor * (IsGuardianFloor(CurrentFloor) || CurrentFloor == TotalFloors ? 2.f : 1.f);
}

// ---------------------------------------------------------------- loading

void AArpgGameMode::LoadHub()
{
	ClearFloorActors();
	CurrentFloor = 0;
	CurrentBiome = EArpgBiome::Greyhollow;
	Objective = EArpgObjective::Hub;
	bObjectiveDone = true;
	bSanctuary = false;
	bInRun = false;
	Floor->Build(0, RunSeed, EArpgBiome::Greyhollow, AArpgFloorGenerator::EKind::Hub);
	Environment->ApplyBiome(EArpgBiome::Greyhollow);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Portal = GetWorld()->SpawnActor<AArpgExitPortal>(AArpgExitPortal::StaticClass(), Floor->PortalLocation, Floor->PortalRotation, SP);
	if (Portal) { Portal->Init(ArpgData::Biome(EArpgBiome::Greyhollow).Accent, true); Track(Portal); }
	if (AArpgShrine* Shrine = GetWorld()->SpawnActor<AArpgShrine>(AArpgShrine::StaticClass(), Floor->ShrineLocation, FRotator::ZeroRotator, SP))
	{
		Shrine->Init(FLinearColor(0.4f, 0.75f, 1.f));
		Track(Shrine);
	}
	PlacePlayer();
	BannerTitle = TEXT("GREYHOLLOW");
	BannerSub = TEXT("Climb  -  Survive  -  Ascend");
	BannerTime = GetWorld()->GetTimeSeconds();
	FadeTime = GetWorld()->GetTimeSeconds();
}

void AArpgGameMode::LoadFloor(int32 InFloor, bool bAsSanctuary)
{
	ClearFloorActors();
	CurrentFloor = FMath::Max(1, InFloor);
	bObjectiveDone = false;
	bSanctuary = false;
	KillsCounted = 0;
	KillsNeeded = 0;
	FloorTime = 0.f;
	bInRun = true;
	BestFloor = FMath::Max(BestFloor, CurrentFloor);

	const int32 Seed = FloorSeed(CurrentFloor);
	FRandomStream R(Seed);
	AArpgFloorGenerator::EKind Kind = AArpgFloorGenerator::EKind::Normal;
	if (CurrentFloor == TotalFloors) { Kind = AArpgFloorGenerator::EKind::Final; }
	else if (IsGuardianFloor(CurrentFloor)) { Kind = AArpgFloorGenerator::EKind::Guardian; }

	// Biome: weighted random from the seed, never the same twice in a row (GDD).
	if (Kind == AArpgFloorGenerator::EKind::Normal)
	{
		TArray<EArpgBiome> Pool = ArpgData::FloorBiomes();
		Pool.Remove(LastBiome);
		CurrentBiome = Pool[R.RandRange(0, Pool.Num() - 1)];
	}
	else
	{
		CurrentBiome = EArpgBiome::FracturedTime;
	}
	LastBiome = CurrentBiome;

	Floor->Build(CurrentFloor, Seed, CurrentBiome, Kind);
	Environment->ApplyBiome(CurrentBiome);
	PlacePlayer();

	const FArpgBiomeDef& BD = ArpgData::Biome(CurrentBiome);
	switch (Kind)
	{
	case AArpgFloorGenerator::EKind::Normal:   PopulateNormalFloor(R); break;
	case AArpgFloorGenerator::EKind::Guardian: PopulateGuardianFloor(R, bAsSanctuary); break;
	case AArpgFloorGenerator::EKind::Final:    PopulateFinalFloor(R); break;
	default: break;
	}

	BannerTitle = FString::Printf(TEXT("FLOOR %d  -  %s"), CurrentFloor, *BD.Name.ToUpper());
	BannerSub = BD.Tagline;
	if (Kind == AArpgFloorGenerator::EKind::Guardian && !bAsSanctuary)
	{
		const FArpgGuardianDef& G = ArpgData::GuardianForFloor(CurrentFloor, RunSeed);
		BannerSub = FString::Printf(TEXT("Guardian: %s  -  %s"), *G.Name, *G.Title);
	}
	if (Kind == AArpgFloorGenerator::EKind::Final) { BannerSub = TEXT("The Hourless King awaits at the top of the Spire."); }
	if (bAsSanctuary) { BannerSub = TEXT("Sanctuary  -  your checkpoint"); }
	BannerTime = GetWorld()->GetTimeSeconds();
	FadeTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogIronveil, Log, TEXT("Floor %d (%s), seed %d, objective %d"), CurrentFloor, *BD.Name, Seed, (int32)Objective);
}

void AArpgGameMode::PopulateNormalFloor(FRandomStream& R)
{
	const int32 Lvl = CurrentFloor;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Objective: Slaughter 50, Hunt 25, Hold 25.
	const float Roll = R.FRand();
	Objective = Roll < 0.5f ? EArpgObjective::Slaughter : (Roll < 0.75f ? EArpgObjective::Hunt : EArpgObjective::Hold);

	// Packs. 0-2 magic packs per floor; magic packs gain a modifier from floor 20.
	TArray<int32> MagicPacks;
	const int32 NumMagic = FMath::Min(Floor->PackSpots.Num(), R.RandRange(0, 2));
	while (MagicPacks.Num() < NumMagic) { MagicPacks.AddUnique(R.RandRange(0, Floor->PackSpots.Num() - 1)); }
	int32 Spawned = 0;
	for (int32 p = 0; p < Floor->PackSpots.Num(); ++p)
	{
		const FArpgSpawnSpot& Spot = Floor->PackSpots[p];
		const bool bMagic = MagicPacks.Contains(p);
		const int32 Size = FMath::Min(8, R.RandRange(3, 5) + CurrentFloor / 15);
		bool bHasOgre = false;
		for (int32 m = 0; m < Size; ++m)
		{
			FArpgEnemySpawn S;
			S.Archetype = RollArchetype(R, CurrentFloor, !bHasOgre);
			bHasOgre |= S.Archetype == EArpgArchetype::Ogre;
			S.Biome = CurrentBiome;
			S.Level = Lvl;
			if (bMagic)
			{
				S.Rarity = EArpgMonsterRarity::Magic;
				S.Mods = RollMods(R, R.RandRange(1, 2) + (CurrentFloor >= 20 ? 1 : 0));
			}
			const FVector At = Floor->RandomPointNear(Spot.Location, 350.f, R);
			if (SpawnEnemy(S, At)) { ++Spawned; }
		}
	}

	// A rare monster every 3rd floor, or the Hunt target.
	if ((CurrentFloor % 3 == 0 || Objective == EArpgObjective::Hunt) && Floor->HuntRoom != INDEX_NONE)
	{
		FArpgEnemySpawn S;
		S.Archetype = RollArchetype(R, CurrentFloor, true);
		S.Biome = CurrentBiome;
		S.Level = Lvl;
		S.Rarity = EArpgMonsterRarity::Rare;
		S.Mods = RollMods(R, R.RandRange(3, 4) + (CurrentFloor >= 50 ? 1 : 0));
		S.NameOverride = ArpgData::RareMonsterName(R);
		S.bHuntTarget = Objective == EArpgObjective::Hunt;
		const FVector At = Floor->RandomPointInRoom(Floor->HuntRoom, R, 500.f);
		if (AArpgEnemyCharacter* Rare = SpawnEnemy(S, At))
		{
			++Spawned;
			if (S.bHuntTarget) { HuntTarget = Rare; }
			for (int32 m = 0; m < 2; ++m)
			{
				FArpgEnemySpawn Minion;
				Minion.Archetype = EArpgArchetype::Swordsman;
				Minion.Biome = CurrentBiome;
				Minion.Level = Lvl;
				if (SpawnEnemy(Minion, Floor->RandomPointNear(At, 300.f, R))) { ++Spawned; }
			}
		}
	}
	KillsNeeded = FMath::CeilToInt(Spawned * 0.8f);

	// Chests in side rooms; some are locked coffers.
	for (const FArpgSpawnSpot& Spot : Floor->ChestSpots)
	{
		AArpgChest* Chest = GetWorld()->SpawnActor<AArpgChest>(AArpgChest::StaticClass(), Spot.Location, FRotator(0, R.FRandRange(0.f, 360.f), 0), SP);
		if (Chest) { Chest->Init(R.FRand() < 0.35f, Lvl, FloorRarityBonus()); Track(Chest); }
	}

	Portal = GetWorld()->SpawnActor<AArpgExitPortal>(AArpgExitPortal::StaticClass(), Floor->PortalLocation, Floor->PortalRotation, SP);
	if (Portal) { Portal->Init(ArpgData::Biome(CurrentBiome).Accent, false); Track(Portal); }

	if (Objective == EArpgObjective::Hold)
	{
		Relic = GetWorld()->SpawnActor<AArpgVeilRelic>(AArpgVeilRelic::StaticClass(), Floor->RelicLocation, FRotator::ZeroRotator, SP);
		if (Relic) { Relic->Init(ArpgData::Biome(CurrentBiome).Accent, HoldSeconds); Track(Relic); }
	}
}

void AArpgGameMode::PopulateGuardianFloor(FRandomStream& R, bool bAsSanctuary)
{
	Objective = EArpgObjective::Guardian;
	if (bAsSanctuary)
	{
		MakeSanctuary(Floor->ArenaCenter);
		return;
	}
	const FArpgGuardianDef& G = ArpgData::GuardianForFloor(CurrentFloor, RunSeed);
	FArpgEnemySpawn S;
	S.Archetype = EArpgArchetype::Guardian;
	S.Biome = G.Biome;
	S.Level = CurrentFloor + 2;     // GDD: guardians +2
	S.Rarity = EArpgMonsterRarity::Boss;
	S.bHasGuardian = true;
	S.Guardian = G;
	if (AArpgEnemyCharacter* E = SpawnEnemy(S, Floor->BossLocation))
	{
		E->SetActorRotation(FRotator(0.f, 180.f, 0.f));
		Boss = E;
	}
}

void AArpgGameMode::PopulateFinalFloor(FRandomStream& R)
{
	Objective = EArpgObjective::FinalBoss;
	const FArpgGuardianDef& K = ArpgData::FinalBoss();
	FArpgEnemySpawn S;
	S.Archetype = EArpgArchetype::FinalBoss;
	S.Biome = EArpgBiome::FracturedTime;
	S.Level = CurrentFloor + 2;
	S.Rarity = EArpgMonsterRarity::Boss;
	S.bHasGuardian = true;
	S.Guardian = K;
	if (AArpgEnemyCharacter* E = SpawnEnemy(S, Floor->BossLocation))
	{
		E->SetActorRotation(FRotator(0.f, 180.f, 0.f));
		Boss = E;
	}
}

void AArpgGameMode::MakeSanctuary(const FVector& At)
{
	bSanctuary = true;
	bObjectiveDone = true;
	Objective = EArpgObjective::Sanctuary;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AArpgShrine* Shrine = GetWorld()->SpawnActor<AArpgShrine>(AArpgShrine::StaticClass(), At, FRotator::ZeroRotator, SP))
	{
		Shrine->Init(FLinearColor(0.4f, 0.75f, 1.f));
		Track(Shrine);
	}
	if (!Portal)
	{
		Portal = GetWorld()->SpawnActor<AArpgExitPortal>(AArpgExitPortal::StaticClass(), Floor->PortalLocation, Floor->PortalRotation, SP);
		if (Portal) { Portal->Init(ArpgData::Biome(CurrentBiome).Accent, false); Track(Portal); }
	}
	if (Portal) { Portal->SetOpen(true); }
	if (CurrentFloor > Checkpoint) { Checkpoint = CurrentFloor; }
	SaveProgress();
}

AArpgEnemyCharacter* AArpgGameMode::SpawnEnemy(const FArpgEnemySpawn& Spawn, const FVector& Location)
{
	const float Lift = 100.f * ArpgData::Archetype(Spawn.Archetype).Scale;
	const FTransform Xf(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location + FVector(0, 0, Lift));
	AArpgEnemyCharacter* E = GetWorld()->SpawnActorDeferred<AArpgEnemyCharacter>(AArpgEnemyCharacter::StaticClass(), Xf, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!E) { return nullptr; }
	E->Setup(Spawn);
	UGameplayStatics::FinishSpawningActor(E, Xf);
	Track(E);
	return E;
}

EArpgArchetype AArpgGameMode::RollArchetype(FRandomStream& R, int32 InFloor, bool bAllowOgre) const
{
	struct FW { EArpgArchetype A; float W; int32 MinFloor; };
	const FW Table[] = {
		{ EArpgArchetype::Swordsman, 4.f, 1 },
		{ EArpgArchetype::Archer, 2.5f, 1 },
		{ EArpgArchetype::Duelist, 1.5f, 2 },
		{ EArpgArchetype::ShieldBrute, 1.5f, 3 },
		{ EArpgArchetype::Ogre, 0.6f, 4 } };
	float Total = 0.f;
	for (const FW& E : Table) { if (InFloor >= E.MinFloor && (bAllowOgre || E.A != EArpgArchetype::Ogre)) { Total += E.W; } }
	float Pick = R.FRand() * Total;
	for (const FW& E : Table)
	{
		if (InFloor < E.MinFloor || (!bAllowOgre && E.A == EArpgArchetype::Ogre)) { continue; }
		Pick -= E.W;
		if (Pick <= 0.f) { return E.A; }
	}
	return EArpgArchetype::Swordsman;
}

TArray<EArpgMonsterMod> AArpgGameMode::RollMods(FRandomStream& R, int32 Count) const
{
	TArray<EArpgMonsterMod> All;
	for (int32 i = 0; i < (int32)EArpgMonsterMod::Count; ++i) { All.Add((EArpgMonsterMod)i); }
	TArray<EArpgMonsterMod> Out;
	for (int32 i = 0; i < Count && All.Num() > 0; ++i)
	{
		const int32 Idx = R.RandRange(0, All.Num() - 1);
		Out.Add(All[Idx]);
		All.RemoveAt(Idx);
	}
	return Out;
}

// ---------------------------------------------------------------- flow

void AArpgGameMode::StartNewGame(EArpgClass Class)
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!P) { return; }
	P->ChooseClass(Class, true);
	Checkpoint = 0;
	RunSeed = FMath::Rand();
	Reshuffle = 0;
	RunTime = 0.f;
	bBeatFinalBoss = false;
	Announce(FString::Printf(TEXT("%s ready. Walk to the glowing gate and press E to enter the Veil Spire."), *ArpgData::Class(Class).Name),
		ArpgData::Class(Class).Color);
	SaveProgress();
}

void AArpgGameMode::ContinueGame()
{
	UArpgSaveGame* Save = Cast<UArpgSaveGame>(UGameplayStatics::LoadGameFromSlot(UArpgSaveGame::SlotName, 0));
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!Save || !P) { Announce(TEXT("No saved climb yet. Pick a class with 1-4."), FLinearColor::White); return; }
	P->ChooseClass(Save->Class, false);
	P->Level = Save->Level;
	P->Inventory = Save->Inventory;
	for (int32 i = 0; i < ArpgNumGearSlots; ++i) { P->Equipped[i] = Save->Equipped.IsValidIndex(i) ? Save->Equipped[i] : FArpgItem(); }
	P->RecalculateStats();
	P->FullRestore();
	P->AddXP(Save->XP);
	Checkpoint = Save->Checkpoint;
	BestFloor = Save->BestFloor;
	RunSeed = Save->RunSeed;
	bBeatFinalBoss = Save->bBeatFinalBoss;
	Announce(Checkpoint > 0 ? FString::Printf(TEXT("Welcome back. The gate will take you to your sanctuary on floor %d."), Checkpoint)
		: FString(TEXT("Welcome back. Enter the gate to climb.")), FLinearColor(0.7f, 0.85f, 1.f));
}

void AArpgGameMode::SaveProgress()
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!P || !P->HasChosenClass()) { return; }
	UArpgSaveGame* Save = Cast<UArpgSaveGame>(UGameplayStatics::CreateSaveGameObject(UArpgSaveGame::StaticClass()));
	Save->Class = P->GetClass();
	Save->Level = P->Level;
	Save->XP = P->GetXP();
	Save->Inventory = P->Inventory;
	Save->Equipped.Reset();
	for (const FArpgItem& I : P->Equipped) { Save->Equipped.Add(I); }
	Save->Checkpoint = Checkpoint;
	Save->BestFloor = BestFloor;
	Save->RunSeed = RunSeed;
	Save->bBeatFinalBoss = bBeatFinalBoss;
	bHasSave = UGameplayStatics::SaveGameToSlot(Save, UArpgSaveGame::SlotName, 0) || bHasSave;
}

void AArpgGameMode::EnterSpire()
{
	if (Checkpoint <= 0) { RunTime = 0.f; }
	if (Checkpoint > 0) { LoadFloor(Checkpoint, true); }
	else { LoadFloor(1, false); }
}

void AArpgGameMode::AdvanceFloor()
{
	if (CurrentFloor == 0) { EnterSpire(); return; }
	LastSplit = FloorTime;
	LoadFloor(CurrentFloor + 1, false);
	SaveProgress();
}

void AArpgGameMode::OnPlayerDied()
{
	DeathTime = GetWorld()->GetTimeSeconds();
	Announce(TEXT("You have fallen. Press Enter (A) to return to your last sanctuary."), FLinearColor(1.f, 0.3f, 0.25f));
}

void AArpgGameMode::RespawnPlayer()
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!P) { return; }
	DeathTime = -1.f;
	P->ResetForRespawn();
	// GDD: back to the last sanctuary; floors since then reshuffle with a new seed.
	++Reshuffle;
	if (Checkpoint > 0) { LoadFloor(Checkpoint, true); }
	else { LoadHub(); }
}

void AArpgGameMode::ContinueAfterVictory()
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!P) { return; }
	P->UIState = EArpgPlayerUI::Playing;
	// Overtime floors: endlessly scaling floors above the King (GDD "Later").
	if (Portal) { Portal->SetOpen(true); }
	Announce(TEXT("Overtime floors unlocked. The portal climbs higher than the Spire was built."), FLinearColor(0.8f, 0.7f, 1.f));
}

void AArpgGameMode::RecoverPlayer()
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (!P) { return; }
	PlacePlayer();
	FArpgHit Fall;
	Fall.Damage[0] = P->Stats.MaxHealth * 0.1f;
	P->ReceiveHit(Fall, nullptr);
}

// ---------------------------------------------------------------- combat services

bool AArpgGameMode::RequestAttackToken(AArpgEnemyCharacter* Enemy, bool bRanged)
{
	TArray<TWeakObjectPtr<AArpgEnemyCharacter>>& Tokens = bRanged ? RangedTokens : MeleeTokens;
	Tokens.RemoveAll([](const TWeakObjectPtr<AArpgEnemyCharacter>& W) { return !W.IsValid() || W->IsDead(); });
	if (Tokens.Contains(Enemy)) { return true; }
	if (Tokens.Num() >= (bRanged ? 1 : 2)) { return false; }
	Tokens.Add(Enemy);
	return true;
}

void AArpgGameMode::ReleaseAttackToken(AArpgEnemyCharacter* Enemy)
{
	MeleeTokens.Remove(Enemy);
	RangedTokens.Remove(Enemy);
}

void AArpgGameMode::DropLoot(const FVector& At, int32 ItemLevel, int32 Count, EArpgRarity MinRarity, float ExtraRarity, bool bGuaranteeUnique)
{
	UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
	if (!Loot || !Loot->IsLoaded()) { return; }
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	const float Rarity = FloorRarityBonus() + ExtraRarity + (P ? P->GetItemRarityBonus() : 0.f);
	for (int32 i = 0; i < Count; ++i)
	{
		FArpgItem Item = (bGuaranteeUnique && i == 0) ? Loot->RollUnique(LootRng, ItemLevel) : Loot->RollItem(LootRng, ItemLevel, Rarity, i == 0 ? MinRarity : EArpgRarity::Normal);
		if (AArpgLootDrop* D = AArpgLootDrop::SpawnDrop(GetWorld(), At, Item)) { Track(D); }
	}
}

void AArpgGameMode::OnEnemyKilled(AArpgEnemyCharacter* Enemy, AArpgCharacterBase* Killer)
{
	if (!Enemy) { return; }
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	ReleaseAttackToken(Enemy);
	if (P) { P->AddXP(Enemy->GetXPReward()); }

	const FVector At = Enemy->GetActorLocation();
	const int32 ILvl = Enemy->Level;
	const float Quantity = 1.f + CurrentFloor / 100.f;    // GDD: +1% item quantity per floor
	const bool bSummoned = Enemy->GetSpawnInfo().bSummoned;
	switch (Enemy->GetRarity())
	{
	case EArpgMonsterRarity::Normal:
		if (LootRng.FRand() < 0.08f * Quantity * (bSummoned ? 0.3f : 1.f)) { DropLoot(At, ILvl, 1, EArpgRarity::Normal); }
		break;
	case EArpgMonsterRarity::Magic:
		if (LootRng.FRand() < 0.25f * Quantity) { DropLoot(At, ILvl, 1, EArpgRarity::Normal); }
		break;
	case EArpgMonsterRarity::Rare:
		DropLoot(At, ILvl, 2, EArpgRarity::Magic);
		if (P && P->Flasks < P->MaxFlasks) { ++P->Flasks; P->Message(TEXT("A flask charge drips from the rare monster.")); }
		break;
	case EArpgMonsterRarity::Boss:
		if (Enemy->GetSpawnInfo().Archetype == EArpgArchetype::FinalBoss) { DropLoot(At, ILvl, 6, EArpgRarity::Rare, 100.f, true); }
		else { DropLoot(At, ILvl, 4, EArpgRarity::Rare, 50.f, true); }
		break;
	}

	// Emberheart: ignited enemies spread Ignite to one nearby enemy on death.
	if (P && P->HasUnique(TEXT("Emberheart")) && Enemy->HasAilment(true, false, false))
	{
		if (AArpgCharacterBase* Next = ArpgCombat::FindNearest(GetWorld(), At, 600.f, EArpgTeam::Player))
		{
			FArpgHit Spread;
			Spread.Damage[(int32)EArpgDamageType::Fire] = 6.f * (1.f + 0.1f * ILvl);
			Spread.IgniteChance = 1.f;
			Spread.bSpell = true;
			P->DealHit(Next, Spread);
		}
	}

	// Objectives
	if (!bSummoned) { ++KillsCounted; }
	if (bObjectiveDone) { return; }
	switch (Objective)
	{
	case EArpgObjective::Slaughter:
		if (KillsCounted >= KillsNeeded) { CompleteObjective(); }
		break;
	case EArpgObjective::Hunt:
		if (Enemy->IsHuntTarget()) { CompleteObjective(); }
		break;
	case EArpgObjective::Guardian:
		if (Enemy->GetSpawnInfo().Archetype == EArpgArchetype::Guardian)
		{
			Announce(FString::Printf(TEXT("%s falls. Floor %d is now a sanctuary."), *Enemy->DisplayName, CurrentFloor), FLinearColor(1.f, 0.8f, 0.3f));
			MakeSanctuary(Floor->ArenaCenter);
			// Clear any adds left over.
			for (TActorIterator<AArpgEnemyCharacter> It(GetWorld()); It; ++It) { if (*It != Enemy && !It->IsDead()) { It->Kill(P); } }
		}
		break;
	case EArpgObjective::FinalBoss:
		if (Enemy->GetSpawnInfo().Archetype == EArpgArchetype::FinalBoss)
		{
			bObjectiveDone = true;
			bBeatFinalBoss = true;
			for (TActorIterator<AArpgEnemyCharacter> It(GetWorld()); It; ++It) { if (*It != Enemy && !It->IsDead()) { It->Kill(P); } }
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Portal = GetWorld()->SpawnActor<AArpgExitPortal>(AArpgExitPortal::StaticClass(), Floor->PortalLocation, Floor->PortalRotation, SP);
			if (Portal) { Portal->Init(FLinearColor(0.8f, 0.7f, 1.f), false); Track(Portal); }
			if (P) { P->UIState = EArpgPlayerUI::Victory; }
			SaveProgress();
		}
		break;
	default:
		break;
	}
}

void AArpgGameMode::CompleteObjective()
{
	if (bObjectiveDone) { return; }
	bObjectiveDone = true;
	if (Portal) { Portal->SetOpen(true); }
	Announce(TEXT("The way up is open. Find the glowing portal."), FLinearColor(0.5f, 0.85f, 1.f));
}

void AArpgGameMode::SpawnAdds(AArpgEnemyCharacter* Summoner, int32 Count)
{
	if (!Summoner) { return; }
	FRandomStream R(FMath::Rand());
	const bool bFinal = Summoner->GetSpawnInfo().Archetype == EArpgArchetype::FinalBoss;
	for (int32 i = 0; i < Count; ++i)
	{
		FArpgEnemySpawn S;
		S.Archetype = i % 2 == 0 ? EArpgArchetype::Swordsman : EArpgArchetype::Archer;
		S.Biome = bFinal ? ArpgData::FloorBiomes()[R.RandRange(0, ArpgData::FloorBiomes().Num() - 1)] : Summoner->GetSpawnInfo().Guardian.Biome;
		S.Level = Summoner->Level - 2;
		S.Rarity = bFinal ? EArpgMonsterRarity::Magic : EArpgMonsterRarity::Normal;
		if (bFinal) { S.Mods = RollMods(R, 1); S.NameOverride = TEXT("Echo of a Climber"); }
		S.bSummoned = true;
		S.bStartAggro = true;
		const FVector At = Floor->RandomPointNear(Summoner->GetActorLocation(), 600.f, R);
		if (SpawnEnemy(S, At)) { AArpgAreaEffect::SpawnFlash(GetWorld(), At, 150.f, FLinearColor(0.4f, 0.6f, 1.f)); }
	}
	Announce(bFinal ? TEXT("Echoes of fallen climbers answer the King.") : TEXT("The guardian calls for aid!"), FLinearColor(1.f, 0.6f, 0.3f));
}

void AArpgGameMode::SpawnRelicWave(const FVector& Center)
{
	FRandomStream R(FMath::Rand());
	const int32 Count = 3 + CurrentFloor / 10;
	for (int32 i = 0; i < Count; ++i)
	{
		FArpgEnemySpawn S;
		S.Archetype = RollArchetype(R, CurrentFloor, i == 0 && CurrentFloor >= 4);
		S.Biome = CurrentBiome;
		S.Level = CurrentFloor;
		S.bSummoned = true;
		S.bStartAggro = true;
		const float A = R.FRandRange(0.f, 2.f * PI);
		const FVector At = Floor->RandomPointNear(Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 800.f, 200.f, R);
		if (SpawnEnemy(S, At)) { AArpgAreaEffect::SpawnFlash(GetWorld(), At, 120.f, ArpgData::Biome(CurrentBiome).Accent); }
	}
}

void AArpgGameMode::OnRelicHeld()
{
	CompleteObjective();
	DropLoot(Floor->RelicLocation, CurrentFloor, 2, EArpgRarity::Magic);
}

float AArpgGameMode::GetRelicRemaining() const
{
	return Relic ? Relic->GetRemaining() : 0.f;
}

FString AArpgGameMode::GetObjectiveText() const
{
	if (bSanctuary) { return TEXT("Sanctuary: pray at the shrine, then take the portal up"); }
	if (bObjectiveDone && Objective != EArpgObjective::Hub) { return TEXT("Objective complete: reach the portal"); }
	switch (Objective)
	{
	case EArpgObjective::Hub:       return TEXT("Enter the Veil Spire through the gate");
	case EArpgObjective::Slaughter: return FString::Printf(TEXT("Slaughter: %d / %d monsters"), FMath::Min(KillsCounted, KillsNeeded), KillsNeeded);
	case EArpgObjective::Hunt:      return HuntTarget.IsValid() ? FString::Printf(TEXT("Hunt: slay %s"), *HuntTarget->DisplayName) : TEXT("Hunt: find the marked rare monster");
	case EArpgObjective::Hold:
		if (Relic && Relic->IsActive()) { return FString::Printf(TEXT("Hold: survive %.0f s"), Relic->GetRemaining()); }
		return TEXT("Hold: awaken the Veil Relic in the far room");
	case EArpgObjective::Guardian:  return TEXT("Defeat the guardian");
	case EArpgObjective::FinalBoss: return TEXT("Defeat the Hourless King");
	default: return TEXT("");
	}
}

// ---------------------------------------------------------------- tick

void AArpgGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (P && bInRun && P->UIState == EArpgPlayerUI::Playing)
	{
		RunTime += Dt;
		FloorTime += Dt;
	}
	if (P && Environment)
	{
		const float Low = P->IsDead() ? 1.f : FMath::Clamp((0.35f - P->GetHealthFraction()) / 0.35f, 0.f, 1.f);
		const float Slow = FMath::Clamp((1.f - P->GetZoneSlow()) / 0.6f, 0.f, 1.f);
		Environment->SetCombatGrade(Low, Slow);
	}
}

// ---------------------------------------------------------------- cheats

void AArpgGameMode::CheatGoToFloor(int32 InFloor)
{
	AArpgPlayerCharacter* P = GetPlayerCharacter();
	if (P && !P->HasChosenClass()) { StartNewGame(EArpgClass::Warrior); }
	if (InFloor <= 0) { LoadHub(); return; }
	LoadFloor(InFloor, false);
}

void AArpgGameMode::CheatSetSeed(int32 Seed)
{
	RunSeed = Seed;
	Reshuffle = 0;
	Announce(FString::Printf(TEXT("Run seed set to %d. Floors from here on use it."), Seed), FLinearColor::White);
}
