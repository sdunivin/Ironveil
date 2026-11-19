// ArpgEnemyCharacter.h
// Every monster in the Spire: the five GDD archetypes in each biome's outfit, magic and rare monsters with
// modifiers, the guardians on every 10th floor and the Hourless King on floor 100.
//
// The AI is a small state machine in Tick (idle, chase, telegraphed attack, recover) that steers with
// AddMovementInput and the floor's room graph, so it needs no NavMesh. Attacks are data (FArpgEnemyAttackDef).

#pragma once

#include "CoreMinimal.h"
#include "Characters/ArpgCharacterBase.h"
#include "Data/ArpgGameData.h"
#include "ArpgEnemyCharacter.generated.h"

class AArpgGameMode;

struct FArpgEnemySpawn
{
	EArpgArchetype Archetype = EArpgArchetype::Swordsman;
	EArpgBiome Biome = EArpgBiome::Forest;
	int32 Level = 1;
	EArpgMonsterRarity Rarity = EArpgMonsterRarity::Normal;
	TArray<EArpgMonsterMod> Mods;
	FString NameOverride;
	bool bHasGuardian = false;
	FArpgGuardianDef Guardian;
	bool bHuntTarget = false;
	bool bSummoned = false;
	bool bStartAggro = false;
};

UCLASS()
class IRONVEIL_API AArpgEnemyCharacter : public AArpgCharacterBase
{
	GENERATED_BODY()

public:
	AArpgEnemyCharacter();

	// Call between SpawnActorDeferred and FinishSpawning.
	void Setup(const FArpgEnemySpawn& Spawn);

	virtual void Tick(float DeltaSeconds) override;

	// Smoke Bomb: forget the player for a while; hits taken meanwhile count as backstabs.
	void LoseTrack(float Seconds);
	bool IsUnaware() const { return UnawareRemaining > 0.f || !bAggro; }
	void Alert();

	// Grabs and throws (GDD): staggered enemies, or light ones from behind.
	bool CanBeGrabbed(const AActor* By) const;
	void SetGrabbed(bool bGrabbed);
	bool IsGrabbed() const { return bIsGrabbed; }
	void Throw(const FVector& Velocity, AArpgCharacterBase* Thrower, const FArpgHit& ImpactHit, bool bExplodes);
	// Telekinesis: float helplessly for a moment.
	void Levitate(float Seconds);

	const FArpgEnemySpawn& GetSpawnInfo() const { return Info; }
	EArpgMonsterRarity GetRarity() const { return Info.Rarity; }
	bool IsHuntTarget() const { return Info.bHuntTarget; }
	int32 GetBossPhase() const { return Phase; }
	float GetXPReward() const;
	FString GetModText() const;
	FLinearColor GetNameColor() const;
	bool IsTelegraphing() const { return bBusy && TelegraphUnblockable; }

protected:
	virtual void BeginPlay() override;
	virtual bool PreReceiveHit(FArpgHit& Hit, AArpgCharacterBase* Source) override;
	virtual void OnHitLanded(AArpgCharacterBase* Target, const FArpgHit& Hit, float Dealt) override;
	virtual void OnDeath(AArpgCharacterBase* Killer) override;
	virtual void OnStaggered() override;
	virtual float GetBaseMoveSpeed() const override;

private:
	AArpgGameMode* GetGM() const;
	AArpgCharacterBase* GetTarget() const;
	void BuildBody();
	void ApplyStats();

	void ThinkMove(float Dt, AArpgCharacterBase* Target);
	void MoveToward(const FVector& Goal, float Scale = 1.f);
	void FaceTarget(AArpgCharacterBase* Target, float Dt, float Rate = 8.f);
	const FArpgEnemyAttackDef* ChooseAttack(float Dist);
	void StartAttack(const FArpgEnemyAttackDef& Atk);
	void EndAttack(float Recovery);
	void Telegraph(bool bUnblockable, float Seconds);

	FArpgHit MakeHit(float DamageMult, float PoiseDmg, bool bUnblockable = false) const;
	void MeleeStrike(float Range, float Arc, const FArpgHit& Hit);
	void FireShot(AArpgCharacterBase* Target, const FArpgHit& Hit, float Speed, float SpreadDeg = 0.f);
	void UpdatePhase();

	FArpgEnemySpawn Info;
	FArpgArchetypeDef Arch;
	const FArpgBiomeDef* BiomeDef = nullptr;
	EArpgDamageType Element = EArpgDamageType::Physical;
	float ElementShare = 0.f;
	float SpeedMult = 1.f;
	FLinearColor Primary = FLinearColor::Gray;
	FLinearColor Secondary = FLinearColor::Gray;
	FLinearColor Accent = FLinearColor::White;

	bool bAggro = false;
	bool bBusy = false;
	bool bHasToken = false;
	bool TelegraphUnblockable = false;
	float AttackCooldown = 0.6f;
	TMap<int32, float> AttackCooldowns;
	float UnawareRemaining = 0.f;
	float StrafeSign = 1.f;
	float StrafeTimer = 0.f;
	float ThinkTimer = 0.f;
	int32 HitsTaken = 0;
	int32 Phase = 1;
	bool bSummoned = false;
	float ChargeRemaining = 0.f;
	bool bChargeHit = false;
	FArpgHit ChargeHit;
	float LevitateRemaining = 0.f;

	bool bIsGrabbed = false;
	bool bThrown = false;
	float ThrownTime = 0.f;
	bool bThrowExplodes = false;
	FArpgHit ThrowHit;
	TWeakObjectPtr<AArpgCharacterBase> Thrower;
	TSet<const AArpgCharacterBase*> ThrowVictims; // identity only
	float SpawnZ = 0.f;
};
