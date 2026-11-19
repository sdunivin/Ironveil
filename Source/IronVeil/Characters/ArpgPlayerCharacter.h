// ArpgPlayerCharacter.h
// The player: over-the-shoulder camera with lock-on, combos, charged heavies, dodge with i-frames,
// block and perfect parry, grabs and throws, a grappling hook, the four class kits (16 skills),
// flasks, gear, inventory and levels.
//
// Input is built in code with Enhanced Input (keyboard/mouse and gamepad, GDD layout), so the
// project needs no Input Action assets. Hold LT on a gamepad to turn A/B/X/Y into skills 1-4.

#pragma once

#include "CoreMinimal.h"
#include "Characters/ArpgCharacterBase.h"
#include "Loot/ArpgItem.h"
#include "InputActionValue.h"
#include "ArpgPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class AArpgEnemyCharacter;
class AArpgGameMode;
struct FKey;

UENUM()
enum class EArpgPlayerUI : uint8
{
	ClassSelect,
	Playing,
	Inventory,
	Dead,
	Victory
};

UCLASS()
class IRONVEIL_API AArpgPlayerCharacter : public AArpgCharacterBase
{
	GENERATED_BODY()

public:
	AArpgPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	// ---------------------------------------------------------------- class and progression
	void ChooseClass(EArpgClass NewClass, bool bGiveStartingGear);
	bool HasChosenClass() const { return bClassChosen; }
	bool IsInGame() const { return bClassChosen && UIState != EArpgPlayerUI::ClassSelect; }
	EArpgClass GetClass() const { return PlayerClass; }
	void AddXP(float Amount);
	void SetLevelDirect(int32 NewLevel);
	float GetXP() const { return XP; }
	void RecalculateStats();
	void FullRestore();
	void ResetForRespawn();

	// ---------------------------------------------------------------- items
	TArray<FArpgItem> Inventory;
	FArpgItem Equipped[ArpgNumGearSlots];
	bool CanEquip(const FArpgItem& Item, FString* OutReason = nullptr) const;
	bool EquipFromInventory(int32 Index);
	void PickUp(const FArpgItem& Item);
	EArpgGearSlot SlotFor(const FArpgItem& Item) const;
	bool HasUnique(FName Effect) const;
	float GetItemRarityBonus() const { return Stats.IncItemRarity; }

	// ---------------------------------------------------------------- read by the HUD
	EArpgPlayerUI UIState = EArpgPlayerUI::ClassSelect;
	int32 MenuIndex = 0;
	int32 InventoryCursor = 0;
	bool bHelpOpen = false;
	int32 Flasks = 3;
	int32 MaxFlasks = 3;
	float SkillCooldowns[4] = { 0.f, 0.f, 0.f, 0.f };
	float GetCooldownMax(int32 Skill) const;
	float GetCooldownMultiplier() const;
	AArpgEnemyCharacter* GetLockTarget() const;
	AActor* GetInteractTarget() const;
	FString GetInteractText() const;
	bool IsBlocking() const { return bBlocking; }
	float FuryRemaining = 0.f;
	float BarrierRemaining = 0.f;
	float ZenRemaining = 0.f;
	float GuaranteedCritRemaining = 0.f;
	FString LastMessage;
	float LastMessageTime = -100.f;
	void Message(const FString& Text);

	// Cheats (see AArpgPlayerController)
	bool bGodMode = false;
	float CheatDamageMult = 1.f;

protected:
	virtual void BeginPlay() override;
	virtual bool PreReceiveHit(FArpgHit& Hit, AArpgCharacterBase* Source) override;
	virtual void OnHitLanded(AArpgCharacterBase* Target, const FArpgHit& Hit, float Dealt) override;
	virtual void OnDeath(AArpgCharacterBase* Killer) override;
	virtual float GetBaseMoveSpeed() const override;

private:
	AArpgGameMode* GetGM() const;
	void BuildBody();
	void ClearBody();
	void CreateInput();

	// Input handlers
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnLightPressed();
	void OnHeavyPressed();
	void OnHeavyReleased();
	void OnDodgePressed();
	void OnDodgeReleased();
	void OnJumpPressed();
	void OnBlockPressed();
	void OnBlockReleased();
	void OnGrabPressed();
	void OnGrapplePressed();
	void OnLockOnPressed();
	void OnSkill(int32 Index);
	void OnSkill1() { OnSkill(0); }
	void OnSkill2() { OnSkill(1); }
	void OnSkill3() { OnSkill(2); }
	void OnSkill4() { OnSkill(3); }
	void OnFlask();
	void OnInteract();
	void OnInventory();
	void OnHelp();
	void OnMenuUp();
	void OnMenuDown();
	void OnMenuLeft();
	void OnMenuRight();
	void OnConfirm();
	void OnBack();
	void OnContinue();
	void OnPadA();
	void OnPadB();
	void OnPadX();
	void OnPadY();
	void OnSkillModPressed() { bSkillModifier = true; }
	void OnSkillModReleased() { bSkillModifier = false; }
	void OnSalvage();
	void OnLookPad(const FInputActionValue& V);

	// Combat
	bool CanAct() const;
	bool SpendStamina(float Amount);
	bool SpendMana(float Amount);
	void QueueOrRun(int32 Action);
	void DoLightAttack();
	void DoHeavyAttack(bool bCharged);
	void DoDodge();
	void UseSkill(int32 Index);
	void Lock(float Seconds) { ActionLock = FMath::Max(ActionLock, Seconds); }
	FVector AimDirection() const;
	AArpgCharacterBase* SoftTarget(float Range, float Arc = 120.f) const;
	void FaceDirection(const FVector& Dir);
	float AttackSpeedMult() const;
	FArpgHit MakeAttackHit(float WeaponPct, float MoreMult, float Poise, bool bMelee = true) const;
	FArpgHit MakeSpellHit(EArpgDamageType Type, float Min, float Max, float MoreMult, float Poise) const;
	bool IsBackstab(AArpgCharacterBase* Target) const;
	void StrikeArc(float Range, float Arc, const FArpgHit& Hit, float Knockback = 150.f);
	void FireBolt(bool bHeavy);
	void ReleaseGrab(bool bThrow);
	void UpdateCamera(float Dt);

	// Class kits
	void WarriorSkill(int32 Index);
	void MageSkill(int32 Index);
	void ThiefSkill(int32 Index);
	void MonkSkill(int32 Index);
	void FlowStrikeChain(int32 Remaining, TSet<AArpgCharacterBase*> Hit);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
	UInputAction* NewAction(const TCHAR* Name, bool bAxis2D = false);
	void Map(UInputAction* Action, const FKey& Key, bool bNegate = false, bool bSwizzle = false);

	EArpgClass PlayerClass = EArpgClass::Warrior;
	bool bClassChosen = false;
	float XP = 0.f;

	// Action state
	float ActionLock = 0.f;
	int32 BufferedAction = -1;   // 0 light, 1 heavy, 2 dodge, 10+ skills
	float BufferTime = 0.f;
	int32 ComboIndex = 0;
	float ComboTimer = 0.f;
	bool bChargingHeavy = false;
	float HeavyChargeStart = 0.f;
	bool bDodgeHeld = false;
	float DodgeHeldTime = 0.f;
	bool bSprinting = false;
	float InvulnRemaining = 0.f;
	float StaminaDelay = 0.f;
	bool bExhausted = false;
	bool bBlocking = false;
	float BlockStartTime = -10.f;
	bool bSkillModifier = false;
	float FlaskHealRemaining = 0.f;
	float FlaskHealRate = 0.f;
	float ChiHealRemaining = 0.f;
	float ChiHealRate = 0.f;
	float DodgeCooldownBoost = 0.f;   // Stolen Hour
	TWeakObjectPtr<AArpgEnemyCharacter> LockTarget;
	TWeakObjectPtr<AArpgEnemyCharacter> Grabbed;
	FVector2D MoveInput = FVector2D::ZeroVector;
	float ShakeTime = 0.f;
	float ShakeStrength = 0.f;
	float BaseFOV = 90.f;
	void Shake(float Strength) { ShakeStrength = FMath::Max(ShakeStrength, Strength); ShakeTime = 0.25f; }

	USceneComponent* WeaponPivot = nullptr;

	// Dashes (dodge, Charged Slash, Flow Strike) drive the movement component at a high speed for a moment.
	void StartDash(const FVector& Dir, float Speed, float Duration, const FArpgHit* Hit = nullptr, float HitRadius = 160.f);
	float DashRemaining = 0.f;
	FVector DashDir = FVector::ForwardVector;
	float DashSpeed = 0.f;
	bool bDashHits = false;
	float DashHitRadius = 160.f;
	FArpgHit DashHit;
	TSet<const AArpgCharacterBase*> DashVictims; // identity only

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Rope;
	float RopeTime = 0.f;
	float GrappleCooldown = 0.f;
	void ShowRope(const FVector& From, const FVector& To);
};
