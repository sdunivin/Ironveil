// ArpgInteractables.h
// Things the player presses Interact (E / X) on: loot, chests and locked coffers, the exit stair portal,
// sanctuary shrines, the Veil Relic (Hold objective) and the Spire gate in Greyhollow.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Loot/ArpgItem.h"
#include "ArpgInteractables.generated.h"

class AArpgPlayerCharacter;
class UStaticMeshComponent;
class UPointLightComponent;
class AArpgGameMode;
enum class EArpgShape : uint8;

UCLASS(Abstract)
class IRONVEIL_API AArpgInteractable : public AActor
{
	GENERATED_BODY()

public:
	AArpgInteractable();

	virtual bool CanInteract(const AArpgPlayerCharacter* Player) const { return true; }
	virtual FString GetPrompt(const AArpgPlayerCharacter* Player) const { return TEXT("Interact"); }
	virtual void Interact(AArpgPlayerCharacter* Player) {}

	float InteractRadius = 250.f;

protected:
	AArpgGameMode* GetGM() const;
	UStaticMeshComponent* AddShape(EArpgShape Shape, const FVector& Loc, const FVector& Scale, const FLinearColor& Color,
		float Emissive = 0.f, const FRotator& Rot = FRotator::ZeroRotator, bool bCollide = false);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
};

// An item on the ground with a rarity-coloured beam. Labels are drawn by the HUD.
UCLASS()
class IRONVEIL_API AArpgLootDrop : public AArpgInteractable
{
	GENERATED_BODY()

public:
	static AArpgLootDrop* SpawnDrop(UWorld* World, const FVector& Location, const FArpgItem& Item);

	virtual FString GetPrompt(const AArpgPlayerCharacter* Player) const override;
	virtual void Interact(AArpgPlayerCharacter* Player) override;
	virtual void Tick(float DeltaSeconds) override;

	FArpgItem Item;

private:
	void Build();
	float Age = 0.f;
	FVector Velocity = FVector::ZeroVector;
	bool bLanded = false;
};

UCLASS()
class IRONVEIL_API AArpgChest : public AArpgInteractable
{
	GENERATED_BODY()

public:
	void Init(bool bInLocked, int32 InItemLevel, float InRarity);
	virtual bool CanInteract(const AArpgPlayerCharacter* Player) const override { return !bOpened; }
	virtual FString GetPrompt(const AArpgPlayerCharacter* Player) const override;
	virtual void Interact(AArpgPlayerCharacter* Player) override;

	bool IsLocked() const { return bLocked; }
	bool IsOpened() const { return bOpened; }
	// bPicked: the Thief's Lockpick, which finds the coffer's best contents.
	void Open(AArpgPlayerCharacter* Player, bool bPicked);

private:
	bool bLocked = false;
	bool bOpened = false;
	int32 ItemLevel = 1;
	float Rarity = 0.f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Lid;
};

// The exit stair: dark until the floor's objective is done, then a glowing portal to the next floor.
UCLASS()
class IRONVEIL_API AArpgExitPortal : public AArpgInteractable
{
	GENERATED_BODY()

public:
	void Init(const FLinearColor& Accent, bool bInHub);
	void SetOpen(bool bOpen);
	bool IsOpen() const { return bIsOpen; }
	virtual bool CanInteract(const AArpgPlayerCharacter* Player) const override { return bIsOpen; }
	virtual FString GetPrompt(const AArpgPlayerCharacter* Player) const override;
	virtual void Interact(AArpgPlayerCharacter* Player) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool bIsOpen = false;
	bool bHub = false;
	FLinearColor Accent;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rings;
	float Spin = 0.f;
};

// Sanctuary shrine: heal, refill flasks, save the checkpoint.
UCLASS()
class IRONVEIL_API AArpgShrine : public AArpgInteractable
{
	GENERATED_BODY()

public:
	void Init(const FLinearColor& Accent);
	virtual FString GetPrompt(const AArpgPlayerCharacter* Player) const override { return TEXT("Pray at the shrine (heal, refill flasks, save)"); }
	virtual void Interact(AArpgPlayerCharacter* Player) override;
};

// Hold objective: touch the relic, then survive the waves until the timer runs out.
UCLASS()
class IRONVEIL_API AArpgVeilRelic : public AArpgInteractable
{
	GENERATED_BODY()

public:
	void Init(const FLinearColor& Accent, float InDuration);
	virtual bool CanInteract(const AArpgPlayerCharacter* Player) const override { return !bActive && !bDone; }
	virtual FString GetPrompt(const AArpgPlayerCharacter* Player) const override { return TEXT("Awaken the Veil Relic (survive the onslaught)"); }
	virtual void Interact(AArpgPlayerCharacter* Player) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsActive() const { return bActive; }
	bool IsDone() const { return bDone; }
	float GetRemaining() const { return Remaining; }

private:
	bool bActive = false;
	bool bDone = false;
	float Duration = 45.f;
	float Remaining = 45.f;
	float WaveTimer = 0.f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Crystal;
};
