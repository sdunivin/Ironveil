// ArpgGameMode.h
// Runs the climb (GDD "The tower" and master plan 7.4): Greyhollow hub -> floors 1..100 -> the Hourless King.
//   Normal floors: random biome (never the same twice in a row), random objective, packs scaled to the floor.
//   Every 10th floor: a guardian; once it falls the floor becomes a sanctuary (shrine + checkpoint).
//   Floor 100: the final boss. Beyond it: endless Overtime floors.
// Also owns enemy attack tokens (max 2 melee + 1 ranged attackers at once), loot drops, XP and saving.
//
// Prototype note: single player. Co-op (listen server) is master plan Phase 7B; spawning and damage already
// only happen in the game mode / on the authority, which is the shape co-op needs.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/ArpgTypes.h"
#include "ArpgGameMode.generated.h"

class AArpgPlayerCharacter;
class AArpgEnemyCharacter;
class AArpgCharacterBase;
class AArpgFloorGenerator;
class AArpgEnvironment;
class AArpgExitPortal;
class AArpgVeilRelic;
struct FArpgEnemySpawn;

UCLASS()
class IRONVEIL_API AArpgGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArpgGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// ---------------------------------------------------------------- tower shape (DA_TowerDefinition later)
	UPROPERTY(EditAnywhere, Category = "Tower") int32 TotalFloors = 100;
	UPROPERTY(EditAnywhere, Category = "Tower") int32 GuardianInterval = 10;
	UPROPERTY(EditAnywhere, Category = "Tower") float HoldSeconds = 45.f;

	// ---------------------------------------------------------------- flow
	void StartNewGame(EArpgClass Class);
	void ContinueGame();
	bool HasSave() const { return bHasSave; }
	void EnterSpire();
	void AdvanceFloor();
	void OnPlayerDied();
	void RespawnPlayer();
	void ContinueAfterVictory();
	void RecoverPlayer();
	void SaveProgress();

	// ---------------------------------------------------------------- combat services
	AArpgPlayerCharacter* GetPlayerCharacter() const;
	AArpgFloorGenerator* GetFloor() const { return Floor; }
	bool RequestAttackToken(AArpgEnemyCharacter* Enemy, bool bRanged);
	void ReleaseAttackToken(AArpgEnemyCharacter* Enemy);
	void OnEnemyKilled(AArpgEnemyCharacter* Enemy, AArpgCharacterBase* Killer);
	void SpawnAdds(AArpgEnemyCharacter* Summoner, int32 Count);
	void SpawnRelicWave(const FVector& Center);
	void OnRelicHeld();
	void DropLoot(const FVector& At, int32 ItemLevel, int32 Count, EArpgRarity MinRarity, float ExtraRarity = 0.f, bool bGuaranteeUnique = false);
	void Announce(const FString& Text, const FLinearColor& Color);

	// ---------------------------------------------------------------- cheats
	void CheatGoToFloor(int32 InFloor);
	void CheatSetSeed(int32 Seed);

	// ---------------------------------------------------------------- read by the HUD
	int32 CurrentFloor = 0;
	EArpgBiome CurrentBiome = EArpgBiome::Greyhollow;
	EArpgObjective Objective = EArpgObjective::Hub;
	bool bObjectiveDone = false;
	bool bSanctuary = false;
	int32 Checkpoint = 0;
	int32 BestFloor = 0;
	int32 RunSeed = 0;
	int32 KillsCounted = 0;
	int32 KillsNeeded = 0;
	float RunTime = 0.f;
	float FloorTime = 0.f;
	float LastSplit = 0.f;
	FString BannerTitle;
	FString BannerSub;
	float BannerTime = -100.f;
	FString AnnounceText;
	FLinearColor AnnounceColor = FLinearColor::White;
	float AnnounceTime = -100.f;
	float FadeTime = -100.f;
	bool bBeatFinalBoss = false;
	TWeakObjectPtr<AArpgEnemyCharacter> Boss;
	TWeakObjectPtr<AArpgEnemyCharacter> HuntTarget;
	FString GetObjectiveText() const;
	float GetRelicRemaining() const;
	int32 GetMonsterLevel() const;

private:
	void LoadHub();
	void LoadFloor(int32 InFloor, bool bAsSanctuary);
	void ClearFloorActors();
	void PlacePlayer();
	void PopulateNormalFloor(FRandomStream& R);
	void PopulateGuardianFloor(FRandomStream& R, bool bAsSanctuary);
	void PopulateFinalFloor(FRandomStream& R);
	void CompleteObjective();
	void MakeSanctuary(const FVector& At);
	AArpgEnemyCharacter* SpawnEnemy(const FArpgEnemySpawn& Spawn, const FVector& Location);
	void Track(AActor* Actor);
	EArpgArchetype RollArchetype(FRandomStream& R, int32 InFloor, bool bAllowOgre) const;
	TArray<EArpgMonsterMod> RollMods(FRandomStream& R, int32 Count) const;
	int32 FloorSeed(int32 InFloor) const { return RunSeed * 7919 + InFloor * 104729 + Reshuffle * 15485863; }
	float FloorRarityBonus() const;
	bool IsGuardianFloor(int32 InFloor) const { return InFloor > 0 && InFloor % GuardianInterval == 0 && InFloor != TotalFloors; }

	UPROPERTY() TObjectPtr<AArpgFloorGenerator> Floor;
	UPROPERTY() TObjectPtr<AArpgEnvironment> Environment;
	UPROPERTY() TObjectPtr<AArpgExitPortal> Portal;
	UPROPERTY() TObjectPtr<AArpgVeilRelic> Relic;
	TArray<TWeakObjectPtr<AActor>> FloorActors;
	TArray<TWeakObjectPtr<AArpgEnemyCharacter>> MeleeTokens;
	TArray<TWeakObjectPtr<AArpgEnemyCharacter>> RangedTokens;
	EArpgBiome LastBiome = EArpgBiome::Greyhollow;
	int32 Reshuffle = 0;
	bool bInRun = false;
	bool bHasSave = false;
	float DeathTime = -1.f;
	FRandomStream LootRng;
};
