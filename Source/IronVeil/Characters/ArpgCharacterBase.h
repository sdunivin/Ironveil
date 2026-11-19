// ArpgCharacterBase.h
// Shared by the player and enemies: vitals, stats, the hit/mitigation pipeline, poise and stagger,
// ailments, hitstop, time dilation (slow fields), a gray-box body built from basic shapes, and death.
//
// Prototype note: the master plan moves this onto the Gameplay Ability System in Phase 2.
// The formulas here (increased vs more, armour, resistances, evasion, poise) are the GDD's,
// so they carry over unchanged into a GAS Execution Calculation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/ArpgTypes.h"
#include "ArpgCharacterBase.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
enum class EArpgShape : uint8;

struct FArpgDot
{
	EArpgDamageType Type = EArpgDamageType::Physical;
	float DPS = 0.f;
	float Remaining = 0.f;
	bool bBleed = false;
	bool bPoison = false;
};

struct FArpgBodyPart
{
	TWeakObjectPtr<UStaticMeshComponent> Comp;
	TWeakObjectPtr<UMaterialInstanceDynamic> MID;
	FLinearColor BaseColor = FLinearColor::White;
	float BaseEmissive = 0.f;
};

UCLASS(Abstract)
class IRONVEIL_API AArpgCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AArpgCharacterBase();

	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------------------------------------------- vitals
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float Health = 50.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float Mana = 40.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float Stamina = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float EnergyShield = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float Poise = 30.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float MaxPoise = 30.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") float Barrier = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") int32 Level = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironveil") EArpgTeam Team = EArpgTeam::Enemy;

	FArpgStats Stats;

	bool IsDead() const { return bDead; }
	bool IsStaggered() const { return StaggerRemaining > 0.f; }
	bool IsFrozen() const { return FreezeRemaining > 0.f; }
	bool IsIncapacitated() const { return bDead || StaggerRemaining > 0.f || FreezeRemaining > 0.f || StunRemaining > 0.f; }
	float GetHealthFraction() const { return Stats.MaxHealth > 0.f ? Health / Stats.MaxHealth : 0.f; }
	float GetPoiseFraction() const { return MaxPoise > 0.f ? Poise / MaxPoise : 0.f; }
	float GetChill() const { return ChillAmount; }
	bool HasAilment(bool bIgnite, bool bBleed, bool bPoison) const;
	bool IsShocked() const { return ShockRemaining > 0.f; }
	bool IsExposed() const { return ExposedRemaining > 0.f; }

	// ---------------------------------------------------------------- combat
	// Applies attacker-side effects, then the target's mitigation. Returns damage dealt to Health + shields.
	float DealHit(AArpgCharacterBase* Target, FArpgHit Hit);

	// Target side: evasion, block/parry, mitigation, shields, poise, ailments, numbers, death.
	virtual float ReceiveHit(const FArpgHit& Hit, AArpgCharacterBase* Source);

	void Heal(float Amount, bool bShowNumber = true);
	void ApplyStagger(float Seconds);
	void ApplyStun(float Seconds);
	void ApplyExpose(float Seconds) { ExposedRemaining = FMath::Max(ExposedRemaining, Seconds); }
	void ClearAilments();
	void Kill(AArpgCharacterBase* Killer);

	// Called every tick by slow-time zones while the character stands inside them.
	void ApplyZoneSlow(float Factor) { ZoneSlow = FMath::Min(ZoneSlow, Factor); ZoneSlowTimer = 0.2f; }
	float GetZoneSlow() const { return ZoneSlow; }
	// Brief freeze on impact (cosmetic; real-time timer so it ends even at low dilation).
	void ApplyHitstop(float Seconds);

	// Damage-taken multipliers that buffs change (Zen Resolve etc.).
	float DamageTakenMultiplier = 1.f;
	bool bUnstaggerable = false;
	bool bInvulnerable = false;
	bool bIsBoss = false;
	// How long a poise break staggers this character (GDD: 2 s for enemies; shorter for the player).
	float StaggerDuration = 2.f;

	// ---------------------------------------------------------------- body (gray-box)
	UPROPERTY(VisibleAnywhere, Category = "Ironveil") TObjectPtr<USceneComponent> VisualRoot;

	UStaticMeshComponent* AddPart(EArpgShape Shape, const FVector& Loc, const FVector& Scale, const FLinearColor& Color,
		const FRotator& Rot = FRotator::ZeroRotator, USceneComponent* Parent = nullptr, float Emissive = 0.f);
	UStaticMeshComponent* AddMeshPart(UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FRotator& Rot,
		USceneComponent* Parent = nullptr);
	USceneComponent* AddPivot(const FVector& Loc, USceneComponent* Parent = nullptr);
	void FlashParts(const FLinearColor& Color, float Seconds, float Emissive = 2.f);
	void TintParts(const FLinearColor& Color, float Alpha, float Emissive = 0.f);
	void ResetTint();
	// Size the capsule and keep the body's feet on the ground.
	void SetBodySize(float Radius, float HalfHeight);

	// A simple jointed humanoid (feet at 0, about 180 units tall before VisualRoot scale) with procedural walk and swing poses.
	struct FArpgRig
	{
		USceneComponent* HipL = nullptr;
		USceneComponent* HipR = nullptr;
		USceneComponent* ShoulderL = nullptr;
		USceneComponent* ShoulderR = nullptr;
		USceneComponent* HandL = nullptr;
		USceneComponent* HandR = nullptr;
		USceneComponent* Chest = nullptr;
		UStaticMeshComponent* Head = nullptr;
	};
	FArpgRig Rig;
	void BuildHumanoid(const FLinearColor& Skin, const FLinearColor& Torso, const FLinearColor& Legs, const FLinearColor& Arms,
		float Bulk = 1.f);
	// Swing an arm: pitch 0 = hanging, 90 = pointing forward, 160 = raised overhead.
	void PlaySwing(float FromPitch, float ToPitch, float Duration, bool bLeftArm = false, float Hold = 0.12f);
	// Lean / twist the whole body briefly (e.g. a spinning slash).
	void PlayBodyTwist(float Yaw, float Duration);

	// ---------------------------------------------------------------- delayed actions (respect time dilation)
	void After(float Seconds, TFunction<void()> Fn, bool bInterruptible = true);
	void CancelActions();

	// Text shown above the head (enemies) or nothing (player).
	FString DisplayName;

protected:
	virtual void BeginPlay() override;
	virtual void OnDeath(AArpgCharacterBase* Killer);
	virtual void OnStaggered() {}
	// Lets subclasses react to (or cancel) a hit before mitigation. Return false to ignore the hit.
	virtual bool PreReceiveHit(FArpgHit& Hit, AArpgCharacterBase* Source) { return true; }
	virtual void OnHitLanded(AArpgCharacterBase* Target, const FArpgHit& Hit, float Dealt) {}
	virtual float GetBaseMoveSpeed() const { return 400.f; }

	void UpdateMoveSpeed();
	void UpdateDilation();

	bool bDead = false;
	float StaggerRemaining = 0.f;
	float StunRemaining = 0.f;
	float FreezeRemaining = 0.f;
	float ChillRemaining = 0.f;
	float ChillAmount = 0.f;
	float ShockRemaining = 0.f;
	float ShockAmount = 0.f;
	float ExposedRemaining = 0.f;
	float TimeSinceHit = 100.f;
	float TimeSinceDamaged = 100.f;
	float PoiseRecoverDelay = 3.f;
	bool bPoiseRecoversGradually = false;
	TArray<FArpgDot> Dots;
	float DotNumberAccum = 0.f;

	float ZoneSlow = 1.f;
	float ZoneSlowTimer = 0.f;
	double HitstopUntil = 0.0;

	TArray<FArpgBodyPart> Parts;
	float FlashRemaining = 0.f;
	bool bScatterOnDeath = true;
	TWeakObjectPtr<AArpgCharacterBase> LastAttacker;

	void AnimateRig(float DeltaSeconds);
	float WalkPhase = 0.f;
	struct FSwing
	{
		float From = 0.f, To = 0.f, Duration = 0.f, Hold = 0.f, Time = -1.f;
	};
	FSwing SwingR, SwingL;
	float TwistYaw = 0.f, TwistDuration = 0.f, TwistTime = -1.f;
	float RestArmPitch = 12.f;

	struct FDelayed
	{
		float Time = 0.f;
		TFunction<void()> Fn;
		bool bInterruptible = true;
	};
	TArray<FDelayed> Delayed;
};
