#include "Characters/ArpgPlayerCharacter.h"

#include "IronVeil.h"
#include "Characters/ArpgEnemyCharacter.h"
#include "Combat/ArpgCombat.h"
#include "Combat/ArpgProjectile.h"
#include "Combat/ArpgAreaEffect.h"
#include "Core/ArpgAssets.h"
#include "Core/ArpgGameMode.h"
#include "Data/ArpgGameData.h"
#include "Loot/ArpgLootSubsystem.h"
#include "Tower/ArpgInteractables.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	constexpr int32 MaxInventory = 40;
	constexpr int32 ActLight = 0, ActHeavy = 1, ActDodge = 2, ActSkill = 10;
}

AArpgPlayerCharacter::AArpgPlayerCharacter()
{
	Team = EArpgTeam::Player;
	bScatterOnDeath = false;
	StaggerDuration = 0.7f;
	MaxPoise = 50.f;
	Poise = 50.f;
	PoiseRecoverDelay = 2.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 75.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 14.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(BaseFOV);

	Rope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rope"));
	Rope->SetupAttachment(RootComponent);
	Rope->SetUsingAbsoluteLocation(true);
	Rope->SetUsingAbsoluteRotation(true);
	Rope->SetUsingAbsoluteScale(true);
	Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rope->SetCastShadow(false);
	Rope->SetVisibility(false);

	GetCharacterMovement()->MaxAcceleration = 2400.f;
}

void AArpgPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	Rope->SetStaticMesh(ArpgAssets::Shape(EArpgShape::Cylinder));
	Rope->SetMaterial(0, ArpgAssets::SharedColor(FLinearColor(0.75f, 0.65f, 0.45f), 0.5f));
	// Show a preview of the highlighted class while the class select screen is up.
	PlayerClass = EArpgClass::Warrior;
	BuildBody();
	RecalculateStats();
	FullRestore();
}

AArpgGameMode* AArpgPlayerCharacter::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArpgGameMode>() : nullptr;
}

void AArpgPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void AArpgPlayerCharacter::Message(const FString& Text)
{
	LastMessage = Text;
	LastMessageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

// ---------------------------------------------------------------- input setup

UInputAction* AArpgPlayerCharacter::NewAction(const TCHAR* Name, bool bAxis2D)
{
	// Unnamed: re-possession must never collide with an existing object name.
	UInputAction* A = NewObject<UInputAction>(this);
	A->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	Actions.Add(A);
	return A;
}

void AArpgPlayerCharacter::Map(UInputAction* Action, const FKey& Key, bool bNegate, bool bSwizzle)
{
	FEnhancedActionKeyMapping& M = Mapping->MapKey(Action, Key);
	if (bSwizzle)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Mapping);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(Swizzle);
	}
	if (bNegate)
	{
		M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
	}
}

void AArpgPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogIronveil, Error, TEXT("Ironveil needs Enhanced Input. Set Default Input Component Class to EnhancedInputComponent in Project Settings > Input."));
		return;
	}

	Actions.Reset();
	Mapping = NewObject<UInputMappingContext>(this);

	UInputAction* Move = NewAction(TEXT("IA_Move"), true);
	Map(Move, EKeys::W, false, true);
	Map(Move, EKeys::S, true, true);
	Map(Move, EKeys::A, true);
	Map(Move, EKeys::D);
	Map(Move, EKeys::Gamepad_Left2D);
	UInputAction* Look = NewAction(TEXT("IA_Look"), true);
	Map(Look, EKeys::Mouse2D);
	UInputAction* LookPad = NewAction(TEXT("IA_LookPad"), true);
	Map(LookPad, EKeys::Gamepad_Right2D);

	auto Button = [this](const TCHAR* Name, std::initializer_list<FKey> Keys) -> UInputAction*
	{
		UInputAction* A = NewAction(Name);
		for (const FKey& K : Keys) { Map(A, K); }
		return A;
	};
	UInputAction* Light = Button(TEXT("IA_Light"), { EKeys::LeftMouseButton, EKeys::Gamepad_RightShoulder });
	UInputAction* Heavy = Button(TEXT("IA_Heavy"), { EKeys::RightMouseButton, EKeys::Gamepad_RightTrigger });
	UInputAction* Dodge = Button(TEXT("IA_Dodge"), { EKeys::SpaceBar });
	UInputAction* JumpA = Button(TEXT("IA_Jump"), { EKeys::LeftControl });
	UInputAction* Block = Button(TEXT("IA_Block"), { EKeys::LeftShift, EKeys::Gamepad_LeftShoulder });
	UInputAction* Grab = Button(TEXT("IA_Grab"), { EKeys::F });
	UInputAction* Grapple = Button(TEXT("IA_Grapple"), { EKeys::Q });
	UInputAction* LockOn = Button(TEXT("IA_LockOn"), { EKeys::MiddleMouseButton, EKeys::Tab, EKeys::Gamepad_RightThumbstick });
	UInputAction* S1 = Button(TEXT("IA_Skill1"), { EKeys::One });
	UInputAction* S2 = Button(TEXT("IA_Skill2"), { EKeys::Two });
	UInputAction* S3 = Button(TEXT("IA_Skill3"), { EKeys::Three });
	UInputAction* S4 = Button(TEXT("IA_Skill4"), { EKeys::Four });
	UInputAction* Flask = Button(TEXT("IA_Flask"), { EKeys::R, EKeys::Gamepad_DPad_Up });
	UInputAction* Interact = Button(TEXT("IA_Interact"), { EKeys::E });
	UInputAction* Inv = Button(TEXT("IA_Inventory"), { EKeys::I, EKeys::Gamepad_Special_Left });
	UInputAction* Help = Button(TEXT("IA_Help"), { EKeys::F1, EKeys::H, EKeys::Gamepad_Special_Right });
	UInputAction* Up = Button(TEXT("IA_MenuUp"), { EKeys::Up, EKeys::MouseScrollUp, EKeys::Gamepad_DPad_Up });
	UInputAction* Down = Button(TEXT("IA_MenuDown"), { EKeys::Down, EKeys::MouseScrollDown, EKeys::Gamepad_DPad_Down });
	UInputAction* Left = Button(TEXT("IA_MenuLeft"), { EKeys::Left, EKeys::Gamepad_DPad_Left });
	UInputAction* Right = Button(TEXT("IA_MenuRight"), { EKeys::Right, EKeys::Gamepad_DPad_Right });
	UInputAction* Confirm = Button(TEXT("IA_Confirm"), { EKeys::Enter });
	UInputAction* Back = Button(TEXT("IA_Back"), { EKeys::BackSpace });
	UInputAction* Cont = Button(TEXT("IA_Continue"), { EKeys::C });
	UInputAction* Salvage = Button(TEXT("IA_Salvage"), { EKeys::X, EKeys::Delete });
	UInputAction* PadA = Button(TEXT("IA_PadA"), { EKeys::Gamepad_FaceButton_Bottom });
	UInputAction* PadB = Button(TEXT("IA_PadB"), { EKeys::Gamepad_FaceButton_Right });
	UInputAction* PadX = Button(TEXT("IA_PadX"), { EKeys::Gamepad_FaceButton_Left });
	UInputAction* PadY = Button(TEXT("IA_PadY"), { EKeys::Gamepad_FaceButton_Top });
	UInputAction* SkillMod = Button(TEXT("IA_SkillMod"), { EKeys::Gamepad_LeftTrigger });

	using ET = ETriggerEvent;
	EIC->BindAction(Move, ET::Triggered, this, &AArpgPlayerCharacter::OnMove);
	EIC->BindAction(Move, ET::Completed, this, &AArpgPlayerCharacter::OnMove);
	EIC->BindAction(Look, ET::Triggered, this, &AArpgPlayerCharacter::OnLook);
	EIC->BindAction(LookPad, ET::Triggered, this, &AArpgPlayerCharacter::OnLookPad);
	EIC->BindAction(Light, ET::Started, this, &AArpgPlayerCharacter::OnLightPressed);
	EIC->BindAction(Heavy, ET::Started, this, &AArpgPlayerCharacter::OnHeavyPressed);
	EIC->BindAction(Heavy, ET::Completed, this, &AArpgPlayerCharacter::OnHeavyReleased);
	EIC->BindAction(Dodge, ET::Started, this, &AArpgPlayerCharacter::OnDodgePressed);
	EIC->BindAction(Dodge, ET::Completed, this, &AArpgPlayerCharacter::OnDodgeReleased);
	EIC->BindAction(JumpA, ET::Started, this, &AArpgPlayerCharacter::OnJumpPressed);
	EIC->BindAction(Block, ET::Started, this, &AArpgPlayerCharacter::OnBlockPressed);
	EIC->BindAction(Block, ET::Completed, this, &AArpgPlayerCharacter::OnBlockReleased);
	EIC->BindAction(Grab, ET::Started, this, &AArpgPlayerCharacter::OnGrabPressed);
	EIC->BindAction(Grapple, ET::Started, this, &AArpgPlayerCharacter::OnGrapplePressed);
	EIC->BindAction(LockOn, ET::Started, this, &AArpgPlayerCharacter::OnLockOnPressed);
	EIC->BindAction(S1, ET::Started, this, &AArpgPlayerCharacter::OnSkill1);
	EIC->BindAction(S2, ET::Started, this, &AArpgPlayerCharacter::OnSkill2);
	EIC->BindAction(S3, ET::Started, this, &AArpgPlayerCharacter::OnSkill3);
	EIC->BindAction(S4, ET::Started, this, &AArpgPlayerCharacter::OnSkill4);
	EIC->BindAction(Flask, ET::Started, this, &AArpgPlayerCharacter::OnFlask);
	EIC->BindAction(Interact, ET::Started, this, &AArpgPlayerCharacter::OnInteract);
	EIC->BindAction(Inv, ET::Started, this, &AArpgPlayerCharacter::OnInventory);
	EIC->BindAction(Help, ET::Started, this, &AArpgPlayerCharacter::OnHelp);
	EIC->BindAction(Up, ET::Started, this, &AArpgPlayerCharacter::OnMenuUp);
	EIC->BindAction(Down, ET::Started, this, &AArpgPlayerCharacter::OnMenuDown);
	EIC->BindAction(Left, ET::Started, this, &AArpgPlayerCharacter::OnMenuLeft);
	EIC->BindAction(Right, ET::Started, this, &AArpgPlayerCharacter::OnMenuRight);
	EIC->BindAction(Confirm, ET::Started, this, &AArpgPlayerCharacter::OnConfirm);
	EIC->BindAction(Back, ET::Started, this, &AArpgPlayerCharacter::OnBack);
	EIC->BindAction(Cont, ET::Started, this, &AArpgPlayerCharacter::OnContinue);
	EIC->BindAction(Salvage, ET::Started, this, &AArpgPlayerCharacter::OnSalvage);
	EIC->BindAction(PadA, ET::Started, this, &AArpgPlayerCharacter::OnPadA);
	EIC->BindAction(PadB, ET::Started, this, &AArpgPlayerCharacter::OnPadB);
	EIC->BindAction(PadB, ET::Completed, this, &AArpgPlayerCharacter::OnDodgeReleased);
	EIC->BindAction(PadX, ET::Started, this, &AArpgPlayerCharacter::OnPadX);
	EIC->BindAction(PadY, ET::Started, this, &AArpgPlayerCharacter::OnPadY);
	EIC->BindAction(SkillMod, ET::Started, this, &AArpgPlayerCharacter::OnSkillModPressed);
	EIC->BindAction(SkillMod, ET::Completed, this, &AArpgPlayerCharacter::OnSkillModReleased);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Sub->ClearAllMappings();
				Sub->AddMappingContext(Mapping, 0);
			}
		}
	}
}

// ---------------------------------------------------------------- input handlers

void AArpgPlayerCharacter::OnMove(const FInputActionValue& V)
{
	MoveInput = V.Get<FVector2D>();
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	if (!Controller || MoveInput.IsNearlyZero() || DashRemaining > 0.f) { return; }
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const float Scale = (ActionLock > 0.f || Grabbed.IsValid()) ? 0.15f : 1.f;
	AddMovementInput(Fwd, MoveInput.Y * Scale);
	AddMovementInput(Right, MoveInput.X * Scale);
}

void AArpgPlayerCharacter::OnLook(const FInputActionValue& V)
{
	if (UIState == EArpgPlayerUI::Inventory) { return; }
	const FVector2D L = V.Get<FVector2D>();
	if (LockTarget.IsValid() && FMath::Abs(L.X) > 6.f)
	{
		// Mouse flick switches lock-on target (GDD).
		const FVector Side = Camera->GetRightVector() * FMath::Sign(L.X);
		AArpgEnemyCharacter* Best = nullptr;
		float BestScore = TNumericLimits<float>::Max();
		for (AArpgCharacterBase* C : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 2000.f, Team))
		{
			AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(C);
			if (!E || E == LockTarget.Get()) { continue; }
			const FVector D = E->GetActorLocation() - LockTarget->GetActorLocation();
			if (FVector::DotProduct(D, Side) <= 0.f) { continue; }
			if (D.Size() < BestScore) { BestScore = D.Size(); Best = E; }
		}
		if (Best) { LockTarget = Best; }
		return;
	}
	AddControllerYawInput(L.X);
	AddControllerPitchInput(-L.Y);
}

void AArpgPlayerCharacter::OnLookPad(const FInputActionValue& V)
{
	const FVector2D L = V.Get<FVector2D>();
	const float Dt = GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(L.X * 160.f * Dt);
	AddControllerPitchInput(-L.Y * 110.f * Dt);
}

void AArpgPlayerCharacter::OnLightPressed()
{
	if (UIState != EArpgPlayerUI::Playing) { return; }
	if (Grabbed.IsValid())
	{
		// Slam (GDD): ground area damage.
		AArpgEnemyCharacter* E = Grabbed.Get();
		ReleaseGrab(false);
		FArpgHit Slam;
		Slam.Damage[0] = 35.f * (1.f + 0.1f * (Level - 1)) * (1.f + Stats.IncThrowSlam / 100.f) * CheatDamageMult;
		Slam.PoiseDamage = 30.f;
		Slam.HitstopSeconds = 0.14f;
		Slam.bIsThrow = true;
		for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), E->GetActorLocation(), 280.f, Team)) { DealHit(T, Slam); }
		AArpgAreaEffect::SpawnFlash(GetWorld(), E->GetActorLocation() - FVector(0, 0, 60), 250.f, FLinearColor(0.8f, 0.7f, 0.5f));
		PlaySwing(170.f, 10.f, 0.12f); PlaySwing(170.f, 10.f, 0.12f, true);
		Shake(1.f);
		Lock(0.5f);
		return;
	}
	QueueOrRun(ActLight);
}

void AArpgPlayerCharacter::OnHeavyPressed()
{
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	if (Grabbed.IsValid())
	{
		// Shove: a hard push, for ledges and crowds.
		AArpgEnemyCharacter* E = Grabbed.Get();
		ReleaseGrab(false);
		FArpgHit Shove;
		Shove.PoiseDamage = 20.f;
		Shove.bIsThrow = true;
		E->Throw(GetActorForwardVector() * 1400.f + FVector(0, 0, 120), this, Shove, false);
		Lock(0.4f);
		return;
	}
	bChargingHeavy = true;
	HeavyChargeStart = GetWorld()->GetTimeSeconds();
	if (PlayerClass != EArpgClass::Mage) { PlaySwing(RestArmPitch, 160.f, 0.5f, false, 3.f); }
}

void AArpgPlayerCharacter::OnHeavyReleased()
{
	if (!bChargingHeavy) { return; }
	bChargingHeavy = false;
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	const bool bCharged = GetWorld()->GetTimeSeconds() - HeavyChargeStart >= 0.8f;
	if (CanAct()) { DoHeavyAttack(bCharged); }
	else { BufferedAction = bCharged ? ActHeavy + 100 : ActHeavy; BufferTime = 0.25f; }
}

void AArpgPlayerCharacter::OnDodgePressed()
{
	bDodgeHeld = true;
	DodgeHeldTime = 0.f;
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	if (Grabbed.IsValid()) { ReleaseGrab(false); }
	// Dodge cancels attack recovery (GDD).
	if (ActionLock > 0.f && ActionLock < 0.35f) { ActionLock = 0.f; CancelActions(); }
	QueueOrRun(ActDodge);
}

void AArpgPlayerCharacter::OnDodgeReleased()
{
	bDodgeHeld = false;
	bSprinting = false;
}

void AArpgPlayerCharacter::OnJumpPressed()
{
	if (UIState != EArpgPlayerUI::Playing || !CanAct()) { return; }
	Jump();
}

void AArpgPlayerCharacter::OnBlockPressed()
{
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	bBlocking = true;
	BlockStartTime = GetWorld()->GetTimeSeconds();
	PlaySwing(RestArmPitch, 80.f, 0.08f, true, 60.f);
}

void AArpgPlayerCharacter::OnBlockReleased()
{
	bBlocking = false;
	PlaySwing(80.f, RestArmPitch, 0.1f, true, 0.f);
}

void AArpgPlayerCharacter::OnGrabPressed()
{
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	if (Grabbed.IsValid()) { ReleaseGrab(true); return; }
	if (!CanAct()) { return; }
	const FVector Fwd = GetActorForwardVector();
	for (AArpgCharacterBase* C : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 230.f, Team, &Fwd, 140.f))
	{
		AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(C);
		if (E && E->CanBeGrabbed(this) && SpendStamina(15.f))
		{
			Grabbed = E;
			E->SetGrabbed(true);
			PlaySwing(RestArmPitch, 95.f, 0.1f, false, 30.f);
			PlaySwing(RestArmPitch, 95.f, 0.1f, true, 30.f);
			Message(TEXT("Grabbed! F: throw  -  LMB: slam  -  RMB: shove"));
			Lock(0.25f);
			return;
		}
	}
	Message(TEXT("Nothing to grab: stagger an enemy first, or grab a light one from behind."));
}

void AArpgPlayerCharacter::ReleaseGrab(bool bThrow)
{
	AArpgEnemyCharacter* E = Grabbed.Get();
	Grabbed = nullptr;
	PlaySwing(95.f, RestArmPitch, 0.15f, false, 0.f);
	PlaySwing(95.f, RestArmPitch, 0.15f, true, 0.f);
	if (!E) { return; }
	if (!bThrow)
	{
		E->SetGrabbed(false);
		return;
	}
	FVector Dir = AimDirection();
	if (!MoveInput.IsNearlyZero() && Controller)
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		Dir = (FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveInput.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveInput.X).GetSafeNormal();
	}
	FaceDirection(Dir);
	FArpgHit Impact;
	Impact.Damage[0] = 20.f * (1.f + 0.1f * (Level - 1)) * (1.f + Stats.IncThrowSlam / 100.f) * CheatDamageMult;
	Impact.PoiseDamage = 30.f;
	Impact.HitstopSeconds = 0.14f;
	Impact.bIsThrow = true;
	E->Throw(Dir * 1700.f + FVector(0, 0, 450), this, Impact, HasUnique(TEXT("VarricksGrip")));
	PlaySwing(150.f, 40.f, 0.12f);
	Shake(0.6f);
	Lock(0.35f);
}

void AArpgPlayerCharacter::OnGrapplePressed()
{
	if (UIState != EArpgPlayerUI::Playing || !CanAct()) { return; }
	if (GrappleCooldown > 0.f) { return; }
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * 2800.f;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ArpgGrapple), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q))
	{
		// Visibility doesn't hit capsules; try pawns along the same line.
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Q))
		{
			Message(TEXT("Grapple: nothing in reach."));
			return;
		}
	}
	if (!SpendStamina(10.f)) { return; }
	GrappleCooldown = 1.2f;
	const FVector HandPos = GetActorLocation() + FVector(0, 0, 40);
	ShowRope(HandPos, Hit.ImpactPoint);
	PlaySwing(RestArmPitch, 120.f, 0.08f, false, 0.2f);

	AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(Hit.GetActor());
	if (!E)
	{
		// Pierce pawn-less hit: check if an enemy is right at the impact point.
		E = Cast<AArpgEnemyCharacter>(ArpgCombat::FindNearest(GetWorld(), Hit.ImpactPoint, 120.f, Team));
	}
	if (E && !E->IsDead())
	{
		const bool bLight = (E->MaxPoise < 100.f || E->IsStaggered()) && !E->bIsBoss;
		if (bLight)
		{
			// Pull a light enemy straight to you (GDD).
			const FVector Dir = (GetActorLocation() - E->GetActorLocation()).GetSafeNormal2D();
			E->LaunchCharacter(Dir * 1500.f + FVector(0, 0, 300), true, true);
			E->ApplyStun(0.8f);
			Message(TEXT("Pulled!"));
		}
		else
		{
			const FVector Dir = (E->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			LaunchCharacter(Dir * 1600.f + FVector(0, 0, 400), true, true);
		}
		return;
	}
	const FVector ToPoint = Hit.ImpactPoint - GetActorLocation();
	const FVector Dir = ToPoint.GetSafeNormal();
	const float Up = FMath::Clamp(ToPoint.Z * 1.6f + 450.f, 350.f, 1400.f);
	LaunchCharacter(FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal() * FMath::Min(2000.f, ToPoint.Size2D() * 1.5f + 400.f) + FVector(0, 0, Up), true, true);
}

void AArpgPlayerCharacter::ShowRope(const FVector& From, const FVector& To)
{
	const FVector Mid = (From + To) * 0.5f;
	const float Len = FVector::Dist(From, To);
	Rope->SetWorldLocationAndRotation(Mid, FRotationMatrix::MakeFromZ(To - From).Rotator());
	Rope->SetWorldScale3D(FVector(0.03f, 0.03f, Len / 100.f));
	Rope->SetVisibility(true);
	RopeTime = 0.3f;
}

void AArpgPlayerCharacter::OnLockOnPressed()
{
	if (UIState != EArpgPlayerUI::Playing) { return; }
	if (LockTarget.IsValid()) { LockTarget = nullptr; return; }
	const FVector Fwd = Camera->GetForwardVector();
	AArpgEnemyCharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AArpgCharacterBase* C : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 2500.f, Team, &Fwd, 120.f))
	{
		AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(C);
		if (!E || !ArpgCombat::HasLineOfSight(GetWorld(), Camera->GetComponentLocation(), E->GetActorLocation())) { continue; }
		const FVector D = E->GetActorLocation() - GetActorLocation();
		const float Score = D.Size() * (2.f - FVector::DotProduct(D.GetSafeNormal(), Fwd));
		if (Score < BestScore) { BestScore = Score; Best = E; }
	}
	LockTarget = Best;
}

void AArpgPlayerCharacter::OnSkill(int32 Index)
{
	if (UIState == EArpgPlayerUI::ClassSelect)
	{
		MenuIndex = Index;
		if (AArpgGameMode* GM = GetGM()) { GM->StartNewGame((EArpgClass)Index); }
		return;
	}
	if (UIState != EArpgPlayerUI::Playing) { return; }
	QueueOrRun(ActSkill + Index);
}

void AArpgPlayerCharacter::OnFlask()
{
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	if (Flasks <= 0) { Message(TEXT("No flask charges. Shrines refill them; rare monsters drop one.")); return; }
	if (Health >= Stats.MaxHealth && Dots.Num() == 0) { return; }
	--Flasks;
	FlaskHealRemaining = 1.5f;
	FlaskHealRate = Stats.MaxHealth * 0.4f / 1.5f;
	FlashParts(FLinearColor(0.3f, 1.f, 0.4f), 0.2f, 2.f);
}

void AArpgPlayerCharacter::OnInteract()
{
	if (UIState != EArpgPlayerUI::Playing || bDead) { return; }
	if (AArpgInteractable* I = Cast<AArpgInteractable>(GetInteractTarget()))
	{
		I->Interact(this);
	}
}

void AArpgPlayerCharacter::OnInventory()
{
	if (UIState == EArpgPlayerUI::Playing) { UIState = EArpgPlayerUI::Inventory; InventoryCursor = FMath::Clamp(InventoryCursor, 0, FMath::Max(0, Inventory.Num() - 1)); }
	else if (UIState == EArpgPlayerUI::Inventory) { UIState = EArpgPlayerUI::Playing; }
}

void AArpgPlayerCharacter::OnHelp()
{
	bHelpOpen = !bHelpOpen;
}

void AArpgPlayerCharacter::OnMenuUp()
{
	if (UIState == EArpgPlayerUI::Inventory && Inventory.Num() > 0) { InventoryCursor = (InventoryCursor - 1 + Inventory.Num()) % Inventory.Num(); }
}

void AArpgPlayerCharacter::OnMenuDown()
{
	if (UIState == EArpgPlayerUI::Inventory && Inventory.Num() > 0) { InventoryCursor = (InventoryCursor + 1) % Inventory.Num(); }
}

void AArpgPlayerCharacter::OnMenuLeft()
{
	if (UIState == EArpgPlayerUI::ClassSelect)
	{
		MenuIndex = (MenuIndex + 3) % 4;
		PlayerClass = (EArpgClass)MenuIndex;
		ClearBody(); BuildBody();
	}
}

void AArpgPlayerCharacter::OnMenuRight()
{
	if (UIState == EArpgPlayerUI::ClassSelect)
	{
		MenuIndex = (MenuIndex + 1) % 4;
		PlayerClass = (EArpgClass)MenuIndex;
		ClearBody(); BuildBody();
	}
}

void AArpgPlayerCharacter::OnConfirm()
{
	AArpgGameMode* GM = GetGM();
	switch (UIState)
	{
	case EArpgPlayerUI::ClassSelect: if (GM) { GM->StartNewGame((EArpgClass)MenuIndex); } break;
	case EArpgPlayerUI::Inventory:   EquipFromInventory(InventoryCursor); break;
	case EArpgPlayerUI::Dead:        if (GM) { GM->RespawnPlayer(); } break;
	case EArpgPlayerUI::Victory:     if (GM) { GM->ContinueAfterVictory(); } break;
	default: break;
	}
}

void AArpgPlayerCharacter::OnBack()
{
	if (UIState == EArpgPlayerUI::Inventory) { UIState = EArpgPlayerUI::Playing; }
	bHelpOpen = false;
}

void AArpgPlayerCharacter::OnContinue()
{
	if (UIState == EArpgPlayerUI::ClassSelect)
	{
		if (AArpgGameMode* GM = GetGM()) { GM->ContinueGame(); }
	}
}

void AArpgPlayerCharacter::OnSalvage()
{
	if (UIState != EArpgPlayerUI::Inventory || !Inventory.IsValidIndex(InventoryCursor)) { return; }
	Message(FString::Printf(TEXT("Salvaged %s"), *Inventory[InventoryCursor].DisplayName));
	Inventory.RemoveAt(InventoryCursor);
	InventoryCursor = FMath::Clamp(InventoryCursor, 0, FMath::Max(0, Inventory.Num() - 1));
}

void AArpgPlayerCharacter::OnPadA()
{
	if (UIState != EArpgPlayerUI::Playing) { OnConfirm(); return; }
	if (bSkillModifier) { OnSkill(0); } else { OnJumpPressed(); }
}

void AArpgPlayerCharacter::OnPadB()
{
	if (UIState != EArpgPlayerUI::Playing) { OnBack(); return; }
	if (bSkillModifier) { OnSkill(1); } else { OnDodgePressed(); }
}

void AArpgPlayerCharacter::OnPadX()
{
	if (UIState == EArpgPlayerUI::Inventory) { OnSalvage(); return; }
	if (UIState != EArpgPlayerUI::Playing) { return; }
	if (bSkillModifier) { OnSkill(2); return; }
	// X: pick up / interact when something is close, otherwise grab.
	if (GetInteractTarget()) { OnInteract(); } else { OnGrabPressed(); }
}

void AArpgPlayerCharacter::OnPadY()
{
	if (UIState != EArpgPlayerUI::Playing) { return; }
	if (bSkillModifier) { OnSkill(3); } else { OnGrapplePressed(); }
}

// ---------------------------------------------------------------- classes, body, stats

void AArpgPlayerCharacter::ClearBody()
{
	TArray<USceneComponent*> Children;
	VisualRoot->GetChildrenComponents(true, Children);
	for (USceneComponent* C : Children)
	{
		if (C) { C->DestroyComponent(); }
	}
	Parts.Reset();
	Rig = FArpgRig();
	WeaponPivot = nullptr;
}

void AArpgPlayerCharacter::BuildBody()
{
	const FArpgClassDef& CD = ArpgData::Class(PlayerClass);
	if (UStaticMesh* Mesh = ArpgAssets::OptionalMesh(CD.MeshPath))
	{
		AddMeshPart(Mesh, FVector::ZeroVector, FVector(1.f), FRotator(0.f, ArpgAssets::ImportedMeshYaw, 0.f));
		return;
	}
	using namespace ArpgColors;
	switch (PlayerClass)
	{
	case EArpgClass::Warrior:
	{
		const FLinearColor Steel = Hex(0x3A3D44), Red = Hex(0x8E1E22), Skin = Hex(0xC08A6A);
		BuildHumanoid(Skin, Steel, Steel * 0.8f, Steel);
		AddPart(EArpgShape::Cube, FVector(16, 0, 18), FVector(0.04f, 0.3f, 0.5f), Red, FRotator::ZeroRotator, Rig.Chest);                   // tabard
		AddPart(EArpgShape::Cube, FVector(-17, 0, 18), FVector(0.03f, 0.48f, 0.85f), Red * 0.7f, FRotator(-6, 0, 0), Rig.Chest);           // cape
		AddPart(EArpgShape::Sphere, FVector(0, 0, 4), FVector(0.27f, 0.25f, 0.24f), Hex(0x3A2418), FRotator::ZeroRotator, Rig.Head);        // hair
		WeaponPivot = Rig.HandR;
		AddPart(EArpgShape::Cube, FVector(0, 0, -55), FVector(0.03f, 0.09f, 1.0f), Hex(0x9FB4C8), FRotator::ZeroRotator, Rig.HandR, 0.6f);   // blade
		AddPart(EArpgShape::Cube, FVector(0, 0, -55), FVector(0.035f, 0.02f, 1.0f), Hex(0x3FA0FF), FRotator::ZeroRotator, Rig.HandR, 4.f);   // glowing edge
		AddPart(EArpgShape::Cube, FVector(0, 0, -4), FVector(0.06f, 0.3f, 0.05f), Steel, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Cube, FVector(20, 0, -10), FVector(0.08f, 0.6f, 0.85f), Red * 0.9f, FRotator::ZeroRotator, Rig.HandL);          // shield
		AddPart(EArpgShape::Cube, FVector(25, 0, -10), FVector(0.04f, 0.15f, 0.6f), Hex(0xB08A3A), FRotator::ZeroRotator, Rig.HandL);
		break;
	}
	case EArpgClass::Mage:
	{
		const FLinearColor Robe = Hex(0x3A1E6A), Trim = Hex(0xC8A040), Skin = Hex(0xE8D2C8);
		BuildHumanoid(Skin, Robe, Robe * 0.8f, Robe, 0.9f);
		AddPart(EArpgShape::Cone, FVector(0, 0, 45), FVector(0.65f, 0.6f, 0.9f), Robe * 0.9f);                                               // robe skirt
		AddPart(EArpgShape::Cube, FVector(15, 0, 30), FVector(0.03f, 0.08f, 0.6f), Trim, FRotator::ZeroRotator, Rig.Chest);
		AddPart(EArpgShape::Sphere, FVector(-6, 0, 2), FVector(0.29f, 0.27f, 0.33f), Hex(0xD8D8E8), FRotator::ZeroRotator, Rig.Head);         // silver hair
		AddPart(EArpgShape::Cube, FVector(-14, 0, -30), FVector(0.04f, 0.2f, 0.6f), Hex(0xD8D8E8), FRotator(-10, 0, 0), Rig.Head);
		WeaponPivot = Rig.HandR;
		AddPart(EArpgShape::Cylinder, FVector(0, 0, -20), FVector(0.05f, 0.05f, 1.8f), Hex(0x2A2A35), FRotator::ZeroRotator, Rig.HandR);     // staff
		AddPart(EArpgShape::Sphere, FVector(0, 0, 72), FVector(0.16f), Hex(0xB57BFF), FRotator::ZeroRotator, Rig.HandR, 8.f);               // orb
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2.f * PI * i / 8.f;
			AddPart(EArpgShape::Cube, FVector(FMath::Cos(A) * 14.f, FMath::Sin(A) * 14.f, 72.f), FVector(0.03f, 0.03f, 0.08f), Trim,
				FRotator::ZeroRotator, Rig.HandR, 1.f);
		}
		break;
	}
	case EArpgClass::Thief:
	{
		const FLinearColor Leather = Hex(0x15171C), Cyan = Hex(0x2EC8EA), Skin = Hex(0xB88A70);
		BuildHumanoid(Skin, Leather, Leather, Leather * 1.3f, 0.92f);
		AddPart(EArpgShape::Cone, FVector(-3, 0, 6), FVector(0.33f, 0.31f, 0.4f), Leather * 1.5f, FRotator(-8, 0, 0), Rig.Head);            // hood
		AddPart(EArpgShape::Cube, FVector(10, 0, -6), FVector(0.06f, 0.22f, 0.1f), Leather * 0.6f, FRotator::ZeroRotator, Rig.Head);         // mask
		AddPart(EArpgShape::Cube, FVector(14, 0, 30), FVector(0.02f, 0.02f, 0.45f), Cyan, FRotator::ZeroRotator, Rig.Chest, 4.f);             // glowing seams
		AddPart(EArpgShape::Cube, FVector(0, 0, 10), FVector(0.29f, 0.46f, 0.05f), Cyan, FRotator::ZeroRotator, Rig.Chest, 3.f);
		AddPart(EArpgShape::Cube, FVector(-15, 0, 0), FVector(0.02f, 0.4f, 0.9f), Leather * 1.2f, FRotator(-5, 0, 0), Rig.Chest);
		WeaponPivot = Rig.HandR;
		AddPart(EArpgShape::Cube, FVector(0, 0, -30), FVector(0.025f, 0.06f, 0.55f), Hex(0x9FD8E8), FRotator::ZeroRotator, Rig.HandR, 2.f);
		AddPart(EArpgShape::Cube, FVector(0, 0, -30), FVector(0.025f, 0.06f, 0.55f), Hex(0x9FD8E8), FRotator::ZeroRotator, Rig.HandL, 2.f);
		break;
	}
	case EArpgClass::Monk:
	{
		const FLinearColor Skin = Hex(0xA87050), Gold = Hex(0xE8B030), Wrap = Hex(0xE8E2D2);
		BuildHumanoid(Skin, Wrap, Hex(0x2A2620), Skin, 1.05f);
		AddPart(EArpgShape::Cube, FVector(0, 0, 36), FVector(0.28f, 0.45f, 0.08f), Gold, FRotator(0, 0, 35), Rig.Chest, 1.5f);              // sash
		AddPart(EArpgShape::Cube, FVector(0, 0, 10), FVector(0.29f, 0.46f, 0.1f), Gold * 0.8f, FRotator::ZeroRotator, Rig.Chest);
		for (int32 i = 0; i < 10; ++i)
		{
			const float A = 2.f * PI * i / 10.f;
			AddPart(EArpgShape::Sphere, FVector(FMath::Cos(A) * 10.f + 6.f, FMath::Sin(A) * 14.f, 56.f + FMath::Sin(A) * 3.f), FVector(0.06f),
				Hex(0x3A2418), FRotator::ZeroRotator, Rig.Chest);                                                                             // prayer beads
		}
		AddPart(EArpgShape::Cylinder, FVector(0, 0, 4), FVector(0.3f, 0.3f, 0.02f), Gold, FRotator(90, 0, 0), Rig.HandR, 6.f);           // chakram rings
		AddPart(EArpgShape::Cylinder, FVector(0, 0, 4), FVector(0.3f, 0.3f, 0.02f), Gold, FRotator(90, 0, 0), Rig.HandL, 6.f);
		AddPart(EArpgShape::Cube, FVector(0, 0, 10), FVector(0.02f, 0.25f, 0.02f), Gold, FRotator::ZeroRotator, Rig.Head, 3.f);            // tattoo line
		WeaponPivot = Rig.HandR;
		break;
	}
	}
}

void AArpgPlayerCharacter::ChooseClass(EArpgClass NewClass, bool bGiveStartingGear)
{
	PlayerClass = NewClass;
	MenuIndex = (int32)NewClass;
	bClassChosen = true;
	ClearBody();
	BuildBody();
	if (bGiveStartingGear)
	{
		Level = 1;
		XP = 0.f;
		Inventory.Reset();
		for (FArpgItem& E : Equipped) { E = FArpgItem(); }
		if (UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this))
		{
			for (const FName& Base : ArpgData::Class(NewClass).StartingItems)
			{
				FArpgItem Item = Loot->MakeBaseItem(Base, 1);
				if (Item.IsValid()) { Equipped[(int32)SlotFor(Item)] = Item; }
			}
		}
	}
	for (float& Cd : SkillCooldowns) { Cd = 0.f; }
	RecalculateStats();
	FullRestore();
	UIState = EArpgPlayerUI::Playing;
}

void AArpgPlayerCharacter::FullRestore()
{
	Health = Stats.MaxHealth;
	Mana = Stats.MaxMana;
	Stamina = Stats.MaxStamina;
	EnergyShield = Stats.MaxEnergyShield;
	Poise = MaxPoise;
	Flasks = MaxFlasks;
}

void AArpgPlayerCharacter::ResetForRespawn()
{
	bDead = false;
	Dots.Reset();
	ClearAilments();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
	Barrier = 0.f;
	BarrierRemaining = FuryRemaining = ZenRemaining = GuaranteedCritRemaining = 0.f;
	bUnstaggerable = false;
	DamageTakenMultiplier = 1.f;
	StaggerRemaining = StunRemaining = 0.f;
	ActionLock = 0.f;
	Grabbed = nullptr;
	LockTarget = nullptr;
	for (float& Cd : SkillCooldowns) { Cd = 0.f; }
	RecalculateStats();
	FullRestore();
	UIState = EArpgPlayerUI::Playing;
}

EArpgGearSlot AArpgPlayerCharacter::SlotFor(const FArpgItem& Item) const
{
	const UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
	const FArpgItemBaseRow* Base = Loot ? Loot->FindBase(Item.BaseName) : nullptr;
	if (!Base) { return EArpgGearSlot::MainHand; }
	switch (Base->Slot)
	{
	case EArpgEquipSlot::MainHand: return EArpgGearSlot::MainHand;
	case EArpgEquipSlot::OffHand:  return EArpgGearSlot::OffHand;
	case EArpgEquipSlot::Helmet:   return EArpgGearSlot::Helmet;
	case EArpgEquipSlot::Body:     return EArpgGearSlot::Body;
	case EArpgEquipSlot::Gloves:   return EArpgGearSlot::Gloves;
	case EArpgEquipSlot::Boots:    return EArpgGearSlot::Boots;
	case EArpgEquipSlot::Belt:     return EArpgGearSlot::Belt;
	case EArpgEquipSlot::Amulet:   return EArpgGearSlot::Amulet;
	case EArpgEquipSlot::Ring:
		return (Equipped[(int32)EArpgGearSlot::Ring1].IsValid() && !Equipped[(int32)EArpgGearSlot::Ring2].IsValid())
			? EArpgGearSlot::Ring2 : EArpgGearSlot::Ring1;
	}
	return EArpgGearSlot::MainHand;
}

bool AArpgPlayerCharacter::CanEquip(const FArpgItem& Item, FString* OutReason) const
{
	const UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
	const FArpgItemBaseRow* Base = Loot ? Loot->FindBase(Item.BaseName) : nullptr;
	if (!Base) { return false; }
	FString Reason;
	if (Level < Base->RequiredLevel) { Reason = FString::Printf(TEXT("Requires level %d"), Base->RequiredLevel); }
	else if (Stats.Strength < Base->ReqStrength) { Reason = FString::Printf(TEXT("Requires %d Strength"), Base->ReqStrength); }
	else if (Stats.Dexterity < Base->ReqDexterity) { Reason = FString::Printf(TEXT("Requires %d Dexterity"), Base->ReqDexterity); }
	else if (Stats.Intelligence < Base->ReqIntelligence) { Reason = FString::Printf(TEXT("Requires %d Intelligence"), Base->ReqIntelligence); }
	if (OutReason) { *OutReason = Reason; }
	return Reason.IsEmpty();
}

bool AArpgPlayerCharacter::EquipFromInventory(int32 Index)
{
	if (!Inventory.IsValidIndex(Index)) { return false; }
	const FArpgItem Item = Inventory[Index];
	FString Reason;
	if (!CanEquip(Item, &Reason)) { Message(Reason); return false; }
	const UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
	const FArpgItemBaseRow* Base = Loot ? Loot->FindBase(Item.BaseName) : nullptr;
	if (!Base) { return false; }

	const EArpgGearSlot Slot = SlotFor(Item);
	Inventory.RemoveAt(Index);
	if (Equipped[(int32)Slot].IsValid()) { Inventory.Insert(Equipped[(int32)Slot], FMath::Min(Index, Inventory.Num())); }
	Equipped[(int32)Slot] = Item;

	// Two-handers and off-hands exclude each other.
	auto IsTwoHanded = [Loot](const FArpgItem& I)
	{
		const FArpgItemBaseRow* B = Loot->FindBase(I.BaseName);
		return B && B->Handedness == EArpgHandedness::TwoHand;
	};
	FArpgItem& Main = Equipped[(int32)EArpgGearSlot::MainHand];
	FArpgItem& Off = Equipped[(int32)EArpgGearSlot::OffHand];
	if (Slot == EArpgGearSlot::MainHand && IsTwoHanded(Item) && Off.IsValid()) { Inventory.Add(Off); Off = FArpgItem(); }
	if (Slot == EArpgGearSlot::OffHand && Main.IsValid() && IsTwoHanded(Main)) { Inventory.Add(Main); Main = FArpgItem(); }

	Message(FString::Printf(TEXT("Equipped %s"), *Item.DisplayName));
	RecalculateStats();
	InventoryCursor = FMath::Clamp(InventoryCursor, 0, FMath::Max(0, Inventory.Num() - 1));
	return true;
}

void AArpgPlayerCharacter::PickUp(const FArpgItem& Item)
{
	if (!Item.IsValid()) { return; }
	// Fill empty slots straight away so early drops feel useful.
	const EArpgGearSlot Slot = SlotFor(Item);
	if (!Equipped[(int32)Slot].IsValid() && CanEquip(Item))
	{
		const UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);
		const FArpgItemBaseRow* Base = Loot ? Loot->FindBase(Item.BaseName) : nullptr;
		const FArpgItemBaseRow* MainBase = Loot ? Loot->FindBase(Equipped[0].BaseName) : nullptr;
		const bool bBlockedByTwoHander = Slot == EArpgGearSlot::OffHand && MainBase && MainBase->Handedness == EArpgHandedness::TwoHand;
		if (Base && !bBlockedByTwoHander)
		{
			Equipped[(int32)Slot] = Item;
			RecalculateStats();
			Message(FString::Printf(TEXT("Equipped %s"), *Item.DisplayName));
			return;
		}
	}
	if (Inventory.Num() >= MaxInventory)
	{
		Message(TEXT("Inventory full (I to open, X to salvage)."));
		return;
	}
	Inventory.Add(Item);
	Message(FString::Printf(TEXT("Picked up %s"), *Item.DisplayName));
}

bool AArpgPlayerCharacter::HasUnique(FName Effect) const
{
	for (const FArpgItem& I : Equipped)
	{
		if (I.IsValid() && I.UniqueEffect == Effect) { return true; }
	}
	return false;
}

void AArpgPlayerCharacter::RecalculateStats()
{
	const FArpgClassDef& CD = ArpgData::Class(PlayerClass);
	FArpgStats S;
	const float L = (float)(Level - 1);
	S.Strength = CD.Strength + CD.StrPerLevel * L;
	S.Dexterity = CD.Dexterity + CD.DexPerLevel * L;
	S.Intelligence = CD.Intelligence + CD.IntPerLevel * L;

	// Unarmed: the Monk fights with fists by design.
	if (PlayerClass == EArpgClass::Monk) { S.WeaponMin[0] = 4.f; S.WeaponMax[0] = 9.f; S.WeaponAPS = 1.6f; }
	else { S.WeaponMin[0] = 2.f; S.WeaponMax[0] = 5.f; S.WeaponAPS = 1.2f; }
	S.WeaponCrit = 5.f;

	float FlatHealth = 0.f, FlatMana = 0.f, FlatStamina = 0.f, ES = 0.f, IncES = 0.f;
	const UArpgLootSubsystem* Loot = UArpgLootSubsystem::Get(this);

	auto ApplyGlobal = [&S, &FlatHealth, &FlatMana, &FlatStamina](FName Stat, EArpgModType Type, float V, float V2)
	{
		const FString N = Stat.ToString();
		if (Type == EArpgModType::Added)
		{
			if (N == TEXT("AttackPhysicalDamage")) { S.AttackAddedPhysMin += V; S.AttackAddedPhysMax += V2; }
			else if (N == TEXT("SpellFireDamage")) { S.SpellAddedMin[1] += V; S.SpellAddedMax[1] += V2; }
			else if (N == TEXT("SpellColdDamage")) { S.SpellAddedMin[2] += V; S.SpellAddedMax[2] += V2; }
			else if (N == TEXT("SpellLightningDamage")) { S.SpellAddedMin[3] += V; S.SpellAddedMax[3] += V2; }
			return;
		}
		if (Type == EArpgModType::Increased)
		{
			if (N == TEXT("AttackSpeed")) { S.IncAttackSpeed += V; }
			else if (N == TEXT("CastSpeed")) { S.IncCastSpeed += V; }
			else if (N == TEXT("SpellDamage")) { S.IncSpell += V; }
			else if (N == TEXT("ElementalDamage")) { S.IncElemental += V; }
			else if (N == TEXT("ItemRarity")) { S.IncItemRarity += V; }
			else if (N == TEXT("ManaRegen")) { S.IncManaRegen += V; }
			else if (N == TEXT("MovementSpeed")) { S.IncMoveSpeed += V; }
			else if (N == TEXT("PoiseDamage")) { S.IncPoise += V; }
			else if (N == TEXT("SkillDuration")) { S.IncSkillDuration += V; }
			else if (N == TEXT("CooldownRecovery")) { S.IncCooldownRecovery += V; }
			else if (N == TEXT("StaminaRegen")) { S.IncStaminaRegen += V; }
			else if (N == TEXT("ThrowSlamDamage")) { S.IncThrowSlam += V; }
			return;
		}
		// Flat
		if (N == TEXT("Strength")) { S.Strength += V; }
		else if (N == TEXT("Dexterity")) { S.Dexterity += V; }
		else if (N == TEXT("Intelligence")) { S.Intelligence += V; }
		else if (N == TEXT("AllAttributes")) { S.Strength += V; S.Dexterity += V; S.Intelligence += V; }
		else if (N == TEXT("MaxHealth")) { FlatHealth += V; }
		else if (N == TEXT("MaxMana")) { FlatMana += V; }
		else if (N == TEXT("MaxStamina")) { FlatStamina += V; }
		else if (N == TEXT("FireResist")) { S.Resist[1] += V; }
		else if (N == TEXT("ColdResist")) { S.Resist[2] += V; }
		else if (N == TEXT("LightningResist")) { S.Resist[3] += V; }
		else if (N == TEXT("ChaosResist")) { S.Resist[4] += V; }
		else if (N == TEXT("AllElementalResist")) { S.Resist[1] += V; S.Resist[2] += V; S.Resist[3] += V; }
		else if (N == TEXT("BlockChance")) { S.BlockChance += V; }
		else if (N == TEXT("CritMultiplier")) { S.CritMultiplier += V; }
		else if (N == TEXT("HealthRegen")) { S.HealthRegen += V; }
		else if (N == TEXT("HealthOnHit")) { S.HealthOnHit += V; }
		else if (N == TEXT("SpellDamage")) { S.IncSpell += V; }
		else if (N == TEXT("SkillDuration")) { S.IncSkillDuration += V; }
		else if (N == TEXT("AttackPhysicalDamageMax")) { S.AttackAddedPhysMin += 1.f; S.AttackAddedPhysMax += V; }
	};

	for (int32 SlotIdx = 0; SlotIdx < ArpgNumGearSlots; ++SlotIdx)
	{
		const FArpgItem& Item = Equipped[SlotIdx];
		const FArpgItemBaseRow* Base = (Loot && Item.IsValid()) ? Loot->FindBase(Item.BaseName) : nullptr;
		if (!Base) { continue; }

		float PhysMin = Base->PhysicalMin, PhysMax = Base->PhysicalMax, IncPhys = 0.f;
		float APS = Base->AttacksPerSecond, IncAS = 0.f, Crit = Base->CritChance, IncCrit = 0.f;
		float ElemMin[ArpgNumDamageTypes] = { 0.f, 0.f, 0.f, 0.f, 0.f };
		float ElemMax[ArpgNumDamageTypes] = { 0.f, 0.f, 0.f, 0.f, 0.f };
		float Arm = Base->Armour, IncArm = 0.f, Eva = Base->Evasion, IncEva = 0.f, LES = Base->EnergyShield, IncLES = 0.f;
		S.BlockChance += Base->BlockChance;

		// Implicit
		const FString Imp = Base->ImplicitStat.ToString();
		if (Imp == TEXT("CritChanceLocal")) { IncCrit += Item.ImplicitValue; }
		else if (Imp == TEXT("AttackSpeedLocal")) { IncAS += Item.ImplicitValue; }
		else if (!Base->ImplicitStat.IsNone()) { ApplyGlobal(Base->ImplicitStat, EArpgModType::Flat, Item.ImplicitValue, 0.f); }

		for (const FArpgRolledAffix& A : Item.Affixes)
		{
			const FString N = A.Stat.ToString();
			if (A.ModType == EArpgModType::LocalAdded)
			{
				if (N == TEXT("PhysicalDamage")) { PhysMin += A.Value; PhysMax += A.Value2; }
				else if (N == TEXT("FireDamage")) { ElemMin[1] += A.Value; ElemMax[1] += A.Value2; }
				else if (N == TEXT("ColdDamage")) { ElemMin[2] += A.Value; ElemMax[2] += A.Value2; }
				else if (N == TEXT("LightningDamage")) { ElemMin[3] += A.Value; ElemMax[3] += A.Value2; }
				else if (N == TEXT("Armour")) { Arm += A.Value; }
				else if (N == TEXT("Evasion")) { Eva += A.Value; }
				else if (N == TEXT("EnergyShield")) { LES += A.Value; }
			}
			else if (A.ModType == EArpgModType::LocalIncreased)
			{
				if (N == TEXT("PhysicalDamage")) { IncPhys += A.Value; }
				else if (N == TEXT("AttackSpeed")) { IncAS += A.Value; }
				else if (N == TEXT("CritChance")) { IncCrit += A.Value; }
				else if (N == TEXT("Armour")) { IncArm += A.Value; }
				else if (N == TEXT("Evasion")) { IncEva += A.Value; }
				else if (N == TEXT("EnergyShield")) { IncLES += A.Value; }
			}
			else
			{
				ApplyGlobal(A.Stat, A.ModType, A.Value, A.Value2);
			}
		}

		if (SlotIdx == (int32)EArpgGearSlot::MainHand && PhysMax > 0.f)
		{
			S.WeaponMin[0] = PhysMin * (1.f + IncPhys / 100.f);
			S.WeaponMax[0] = PhysMax * (1.f + IncPhys / 100.f);
			S.WeaponAPS = APS * (1.f + IncAS / 100.f);
			S.WeaponCrit = Crit * (1.f + IncCrit / 100.f);
			for (int32 i = 1; i < ArpgNumDamageTypes; ++i) { S.WeaponMin[i] = ElemMin[i]; S.WeaponMax[i] = ElemMax[i]; }
		}
		else if (Base->Slot == EArpgEquipSlot::MainHand && PlayerClass == EArpgClass::Monk)
		{
			// Monk wraps without physical damage still add their elemental damage to fists.
			for (int32 i = 1; i < ArpgNumDamageTypes; ++i) { S.WeaponMin[i] += ElemMin[i]; S.WeaponMax[i] += ElemMax[i]; }
		}
		S.Armour += Arm * (1.f + IncArm / 100.f);
		S.Evasion += Eva * (1.f + IncEva / 100.f);
		ES += LES * (1.f + IncLES / 100.f);
		IncES += 0.f;
	}

	// Attributes (GDD)
	S.MaxHealth = 50.f + 12.f * L + S.Strength / 2.f + FlatHealth;
	S.MaxMana = 40.f + 6.f * L + S.Intelligence / 2.f + FlatMana;
	S.MaxStamina = 100.f + FlatStamina;
	S.IncMeleePhysical += S.Strength * 0.2f;
	S.Evasion += S.Dexterity * 2.f;
	S.IncAttackSpeed += S.Dexterity * 0.2f;
	S.MaxEnergyShield = ES * (1.f + (IncES + S.Intelligence * 0.2f) / 100.f);
	if (ZenRemaining > 0.f) { for (int32 i = 1; i < ArpgNumDamageTypes; ++i) { S.Resist[i] += 25.f; } }

	Stats = S;
	Health = FMath::Min(Health, Stats.MaxHealth);
	Mana = FMath::Min(Mana, Stats.MaxMana);
	Stamina = FMath::Min(Stamina, Stats.MaxStamina);
	EnergyShield = FMath::Min(EnergyShield, Stats.MaxEnergyShield);
	UpdateMoveSpeed();
}

void AArpgPlayerCharacter::AddXP(float Amount)
{
	if (Level >= 100) { return; }
	XP += Amount;
	bool bLevelled = false;
	while (Level < 100 && XP >= ArpgData::XPToNext(Level))
	{
		XP -= ArpgData::XPToNext(Level);
		++Level;
		bLevelled = true;
	}
	if (bLevelled)
	{
		RecalculateStats();
		Health = Stats.MaxHealth;
		Mana = Stats.MaxMana;
		ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 140), FString::Printf(TEXT("LEVEL %d"), Level), FLinearColor(1.f, 0.85f, 0.3f), 1.6f);
		AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 200.f, FLinearColor(1.f, 0.85f, 0.3f));
	}
}

void AArpgPlayerCharacter::SetLevelDirect(int32 NewLevel)
{
	Level = FMath::Clamp(NewLevel, 1, 100);
	XP = 0.f;
	RecalculateStats();
	FullRestore();
}

// ---------------------------------------------------------------- combat core

float AArpgPlayerCharacter::GetBaseMoveSpeed() const
{
	float Speed = ArpgData::Class(PlayerClass).MoveSpeed;
	if (bSprinting) { Speed *= 1.55f; }
	if (bBlocking) { Speed *= 0.5f; }
	if (bChargingHeavy) { Speed *= 0.45f; }
	return Speed;
}

bool AArpgPlayerCharacter::CanAct() const
{
	return !bDead && !IsIncapacitated() && ActionLock <= 0.f && DashRemaining <= 0.f && UIState == EArpgPlayerUI::Playing;
}

bool AArpgPlayerCharacter::SpendStamina(float Amount)
{
	if (bGodMode) { return true; }
	if (bExhausted || Stamina <= 0.f)
	{
		Message(TEXT("Out of stamina!"));
		return false;
	}
	Stamina = FMath::Max(0.f, Stamina - Amount);
	StaminaDelay = 0.8f;
	if (Stamina <= 0.f) { bExhausted = true; }
	return true;
}

bool AArpgPlayerCharacter::SpendMana(float Amount)
{
	if (bGodMode) { return true; }
	if (Mana < Amount)
	{
		Message(TEXT("Not enough mana."));
		return false;
	}
	Mana -= Amount;
	return true;
}

void AArpgPlayerCharacter::QueueOrRun(int32 Action)
{
	if (!CanAct())
	{
		// 0.2 s input buffer (GDD), a little longer for skills.
		BufferedAction = Action;
		BufferTime = Action >= ActSkill ? 0.3f : 0.22f;
		return;
	}
	BufferedAction = -1;
	if (Action == ActLight) { DoLightAttack(); }
	else if (Action == ActHeavy) { DoHeavyAttack(false); }
	else if (Action == ActHeavy + 100) { DoHeavyAttack(true); }
	else if (Action == ActDodge) { DoDodge(); }
	else if (Action >= ActSkill) { UseSkill(Action - ActSkill); }
}

float AArpgPlayerCharacter::AttackSpeedMult() const
{
	return (1.f + Stats.IncAttackSpeed / 100.f) * (FuryRemaining > 0.f ? 1.3f : 1.f);
}

FVector AArpgPlayerCharacter::AimDirection() const
{
	if (AArpgEnemyCharacter* T = LockTarget.Get())
	{
		return (T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}
	if (AArpgCharacterBase* Soft = SoftTarget(900.f, 70.f))
	{
		return (Soft->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}
	const FVector CamFwd = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
	return FVector(CamFwd.X, CamFwd.Y, 0.f).GetSafeNormal();
}

AArpgCharacterBase* AArpgPlayerCharacter::SoftTarget(float Range, float Arc) const
{
	const FVector CamFwd = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
	const FVector Fwd(CamFwd.X, CamFwd.Y, 0.f);
	return ArpgCombat::FindNearest(GetWorld(), GetActorLocation(), Range, Team, &Fwd, Arc);
}

void AArpgPlayerCharacter::FaceDirection(const FVector& Dir)
{
	if (!Dir.IsNearlyZero()) { SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f)); }
}

FArpgHit AArpgPlayerCharacter::MakeAttackHit(float WeaponPct, float MoreMult, float Poise, bool bMelee) const
{
	FArpgHit H;
	float Phys = FMath::FRandRange(Stats.WeaponMin[0] + Stats.AttackAddedPhysMin, Stats.WeaponMax[0] + Stats.AttackAddedPhysMax);
	Phys *= 1.f + (Stats.IncPhysical + (bMelee ? Stats.IncMeleePhysical : 0.f)) / 100.f;
	H.Damage[0] = Phys;
	for (int32 i = 1; i < ArpgNumDamageTypes; ++i)
	{
		if (Stats.WeaponMax[i] > 0.f) { H.Damage[i] = FMath::FRandRange(Stats.WeaponMin[i], Stats.WeaponMax[i]) * (1.f + Stats.IncElemental / 100.f); }
	}
	// Each "more" multiplies separately (GDD).
	H.Scale(WeaponPct * MoreMult * ((FuryRemaining > 0.f && bMelee) ? 1.2f : 1.f) * CheatDamageMult);
	H.PoiseDamage = Poise * (1.f + Stats.IncPoise / 100.f);
	H.bCrit = GuaranteedCritRemaining > 0.f || FMath::FRand() * 100.f < Stats.WeaponCrit;
	if (H.bCrit) { H.Scale(Stats.CritMultiplier / 100.f); H.PoiseDamage *= 1.5f; }
	H.HitstopSeconds = 0.06f;
	H.SourceLevel = Level;
	H.IgniteChance = H.Damage[1] > 0.f ? 0.1f : 0.f;
	H.ShockChance = H.Damage[3] > 0.f ? 0.1f : 0.f;
	return H;
}

FArpgHit AArpgPlayerCharacter::MakeSpellHit(EArpgDamageType Type, float Min, float Max, float MoreMult, float Poise) const
{
	FArpgHit H;
	H.bSpell = true;
	const int32 T = (int32)Type;
	float Base = FMath::FRandRange(Min, Max) * (1.f + 0.1f * (Level - 1));      // gem level ~ character level for now
	Base += FMath::FRandRange(Stats.SpellAddedMin[T], Stats.SpellAddedMax[T]);
	const bool bElemental = Type == EArpgDamageType::Fire || Type == EArpgDamageType::Cold || Type == EArpgDamageType::Lightning;
	const float Inc = Stats.IncSpell + (bElemental ? Stats.IncElemental : 0.f) + Stats.Intelligence * 0.1f;
	H.Damage[T] = Base * (1.f + Inc / 100.f) * MoreMult * CheatDamageMult;
	H.PoiseDamage = Poise * (1.f + Stats.IncPoise / 100.f);
	H.bCrit = FMath::FRand() < 0.07f;
	if (H.bCrit) { H.Scale(Stats.CritMultiplier / 100.f); }
	H.HitstopSeconds = 0.05f;
	H.SourceLevel = Level;
	return H;
}

bool AArpgPlayerCharacter::IsBackstab(AArpgCharacterBase* Target) const
{
	if (PlayerClass != EArpgClass::Thief) { return false; }
	AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(Target);
	if (!E || E->bIsBoss) { return false; }
	const FVector ToMe = (GetActorLocation() - E->GetActorLocation()).GetSafeNormal2D();
	return E->IsUnaware() || FVector::DotProduct(E->GetActorForwardVector(), ToMe) < -0.35f;
}

void AArpgPlayerCharacter::StrikeArc(float Range, float Arc, const FArpgHit& Hit, float Knockback)
{
	const FVector Fwd = GetActorForwardVector();
	bool bAny = false;
	for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), Range, Team, &Fwd, Arc))
	{
		FArpgHit H = Hit;
		if (!H.bCrit && IsBackstab(T))
		{
			H.bCrit = true;
			H.Scale(Stats.CritMultiplier / 100.f);
			ArpgCombat::ShowNumber(GetWorld(), T->GetActorLocation() + FVector(0, 0, 130), TEXT("Backstab"), FLinearColor(0.4f, 0.95f, 1.f), 0.9f);
			if (HasUnique(TEXT("UnseenHand"))) { SkillCooldowns[2] = 0.f; }
		}
		H.Knockback = Fwd * Knockback;
		DealHit(T, H);
		bAny = true;
	}
	if (bAny)
	{
		GuaranteedCritRemaining = 0.f;
		Shake(Hit.HitstopSeconds > 0.09f ? 0.8f : 0.3f);
	}
}

void AArpgPlayerCharacter::OnHitLanded(AArpgCharacterBase* Target, const FArpgHit& Hit, float Dealt)
{
	if (Target && !Hit.bSpell)
	{
		AArpgAreaEffect::SpawnFlash(GetWorld(), Target->GetActorLocation() + FVector(0, 0, 30), 30.f,
			Hit.bCrit ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(1.f, 0.6f, 0.4f));
	}
}

void AArpgPlayerCharacter::StartDash(const FVector& Dir, float Speed, float Duration, const FArpgHit* Hit, float HitRadius)
{
	DashDir = Dir.GetSafeNormal2D();
	if (DashDir.IsNearlyZero()) { DashDir = GetActorForwardVector(); }
	DashSpeed = Speed;
	DashRemaining = Duration;
	bDashHits = Hit != nullptr;
	if (Hit) { DashHit = *Hit; }
	DashHitRadius = HitRadius;
	DashVictims.Reset();
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	GetCharacterMovement()->MaxAcceleration = 40000.f;
}

void AArpgPlayerCharacter::DoDodge()
{
	if (!SpendStamina(20.f)) { return; }
	FVector Dir = -GetActorForwardVector();
	if (!MoveInput.IsNearlyZero() && Controller)
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveInput.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveInput.X;
	}
	const FArpgClassDef& CD = ArpgData::Class(PlayerClass);
	InvulnRemaining = CD.DodgeIFrames;
	StartDash(Dir, PlayerClass == EArpgClass::Thief ? 1750.f : 1500.f, 0.32f);
	TintParts(CD.Color, 0.3f, 1.f);
	FlashRemaining = 0.3f;
	if (HasUnique(TEXT("StolenHour"))) { DodgeCooldownBoost = 3.f; }
	Lock(0.1f);
}

void AArpgPlayerCharacter::DoLightAttack()
{
	if (PlayerClass == EArpgClass::Mage) { FireBolt(false); return; }
	if (!SpendStamina(8.f)) { return; }
	const float Dur = 1.f / FMath::Max(0.3f, Stats.WeaponAPS * AttackSpeedMult());
	const int32 Step = ComboIndex;
	ComboIndex = (ComboIndex + 1) % 3;
	ComboTimer = Dur + 0.7f;
	FaceDirection(AimDirection());
	Lock(Dur * 0.8f);

	const bool bThief = PlayerClass == EArpgClass::Thief;
	const bool bMonk = PlayerClass == EArpgClass::Monk;
	const float Range = PlayerClass == EArpgClass::Warrior ? 270.f : (bThief ? 230.f : 220.f);
	const float Arc = Step == 2 ? 220.f : 140.f;
	const bool bLeft = (bThief || bMonk) && Step == 1;
	PlaySwing(Step == 1 ? 40.f : 165.f, Step == 1 ? 120.f : 25.f, Dur * 0.4f, bLeft, 0.05f);
	PlayBodyTwist(Step == 0 ? 30.f : (Step == 1 ? -30.f : 60.f), Dur * 0.6f);
	StartDash(GetActorForwardVector(), 600.f, 0.08f);
	After(Dur * 0.35f, [this, Step, Range, Arc, bThief]()
	{
		FArpgHit H = MakeAttackHit(Step == 2 ? 1.3f : 1.f, 1.f, Step == 2 ? 10.f : 6.f);
		if (bThief) { H.BleedChance = 0.1f; }
		if (Step == 2) { H.HitstopSeconds = 0.09f; }
		StrikeArc(Range, Arc, H, Step == 2 ? 350.f : 120.f);
	});
}

void AArpgPlayerCharacter::DoHeavyAttack(bool bCharged)
{
	if (PlayerClass == EArpgClass::Mage) { FireBolt(true); return; }
	if (!SpendStamina(bCharged ? 28.f : 18.f)) { return; }
	const float Dur = 1.5f / FMath::Max(0.3f, Stats.WeaponAPS * AttackSpeedMult());
	FaceDirection(AimDirection());
	Lock(Dur * 0.85f);
	ComboIndex = 0;
	PlaySwing(170.f, 15.f, Dur * 0.45f, false, 0.1f);
	if (PlayerClass == EArpgClass::Monk) { PlaySwing(170.f, 15.f, Dur * 0.45f, true, 0.1f); }
	StartDash(GetActorForwardVector(), bCharged ? 900.f : 650.f, 0.12f);
	After(Dur * 0.45f, [this, bCharged]()
	{
		FArpgHit H = MakeAttackHit(bCharged ? 2.4f : 1.6f, 1.f, bCharged ? 28.f : 18.f);
		H.HitstopSeconds = bCharged ? 0.14f : 0.1f;
		StrikeArc(bCharged ? 320.f : 290.f, 160.f, H, bCharged ? 700.f : 400.f);
		if (bCharged) { AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation() + GetActorForwardVector() * 180.f, 160.f, ArpgData::Class(PlayerClass).Color); }
	});
}

void AArpgPlayerCharacter::FireBolt(bool bHeavy)
{
	if (!SpendStamina(bHeavy ? 14.f : 5.f)) { return; }
	const float Dur = (bHeavy ? 1.3f : 1.f) / FMath::Max(0.3f, Stats.WeaponAPS * AttackSpeedMult());
	const FVector Dir = AimDirection();
	FaceDirection(Dir);
	Lock(Dur * 0.7f);
	PlaySwing(RestArmPitch, 100.f, Dur * 0.3f, false, 0.1f);
	After(Dur * 0.3f, [this, bHeavy]()
	{
		FArpgHit H = MakeAttackHit(bHeavy ? 1.9f : 1.f, 1.f, bHeavy ? 18.f : 4.f, false);
		// Staff bolts are arcane: weapon damage converted to lightning.
		H.Damage[3] += H.Damage[0];
		H.Damage[0] = 0.f;
		H.ShockChance = 0.1f;
		FArpgProjectileParams PP;
		PP.Hit = H;
		FVector Aim = AimDirection();
		if (AArpgEnemyCharacter* T = LockTarget.Get())
		{
			Aim = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			PP.Target = T;
			PP.HomingStrength = 3.f;
		}
		PP.Velocity = Aim * (bHeavy ? 2000.f : 2800.f);
		PP.Radius = bHeavy ? 60.f : 35.f;
		PP.VisualScale = FVector(bHeavy ? 0.45f : 0.22f);
		PP.Color = ArpgData::Class(EArpgClass::Mage).Color;
		PP.Glow = 5.f;
		PP.Pierce = bHeavy ? 2 : 0;
		PP.ExplodeRadius = bHeavy ? 200.f : 0.f;
		PP.Lifetime = 1.6f;
		AArpgProjectile::Launch(this, GetActorLocation() + GetActorForwardVector() * 60.f + FVector(0, 0, 50), PP);
	});
}

// ---------------------------------------------------------------- skills

float AArpgPlayerCharacter::GetCooldownMultiplier() const
{
	return 1.f / (1.f + Stats.IncCooldownRecovery / 100.f);
}

float AArpgPlayerCharacter::GetCooldownMax(int32 Skill) const
{
	return ArpgData::Class(PlayerClass).Skills[FMath::Clamp(Skill, 0, 3)].Cooldown * GetCooldownMultiplier();
}

void AArpgPlayerCharacter::UseSkill(int32 Index)
{
	const FArpgSkillDef& Def = ArpgData::Class(PlayerClass).Skills[FMath::Clamp(Index, 0, 3)];
	if (SkillCooldowns[Index] > 0.f) { Message(FString::Printf(TEXT("%s is recovering (%.1fs)"), *Def.Name, SkillCooldowns[Index])); return; }
	if (!SpendMana(Def.ManaCost)) { return; }
	SkillCooldowns[Index] = GetCooldownMax(Index);
	switch (PlayerClass)
	{
	case EArpgClass::Warrior: WarriorSkill(Index); break;
	case EArpgClass::Mage:    MageSkill(Index); break;
	case EArpgClass::Thief:   ThiefSkill(Index); break;
	case EArpgClass::Monk:    MonkSkill(Index); break;
	}
}

void AArpgPlayerCharacter::WarriorSkill(int32 Index)
{
	const float Dur = 1.f + Stats.IncSkillDuration / 100.f;
	switch (Index)
	{
	case 0: // Power Strike
	{
		FaceDirection(AimDirection());
		const float T = 0.4f / AttackSpeedMult();
		Lock(T + 0.35f);
		PlaySwing(RestArmPitch, 175.f, T * 0.8f, false, T * 0.2f);
		After(T, [this]()
		{
			PlaySwing(175.f, 5.f, 0.08f);
			const FVector At = GetActorLocation() + GetActorForwardVector() * 170.f;
			FArpgHit H = MakeAttackHit(2.5f, 1.f, 40.f);
			H.HitstopSeconds = 0.14f;
			for (AArpgCharacterBase* Tg : ArpgCombat::FindTargets(GetWorld(), At, 260.f, Team))
			{
				FArpgHit HH = H;
				HH.Knockback = GetActorForwardVector() * 500.f;
				DealHit(Tg, HH);
			}
			AArpgAreaEffect::SpawnFlash(GetWorld(), At, 220.f, FLinearColor(1.f, 0.4f, 0.2f));
			Shake(1.2f);
		});
		break;
	}
	case 1: // Battle Fury
		FuryRemaining = 8.f * Dur;
		FlashParts(FLinearColor(1.f, 0.1f, 0.1f), 0.4f, 4.f);
		AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 180.f, FLinearColor(1.f, 0.15f, 0.1f));
		Message(TEXT("Battle Fury!"));
		break;
	case 2: // Shield Barrier
		Barrier = Stats.MaxHealth * 0.3f;
		BarrierRemaining = 6.f * Dur;
		FlashParts(FLinearColor(0.5f, 0.7f, 1.f), 0.4f, 4.f);
		Message(TEXT("Shield Barrier"));
		break;
	case 3: // Charged Slash
	{
		const FVector Dir = AimDirection();
		FaceDirection(Dir);
		FArpgHit H = MakeAttackHit(1.6f, 1.f, 25.f);
		H.HitstopSeconds = 0.08f;
		PlaySwing(160.f, 60.f, 0.15f, false, 0.25f);
		StartDash(Dir, 2400.f, 0.38f, &H, 170.f);
		InvulnRemaining = 0.2f;
		Lock(0.45f);
		break;
	}
	}
}

void AArpgPlayerCharacter::MageSkill(int32 Index)
{
	const float CastMult = 1.f / (1.f + Stats.IncCastSpeed / 100.f);
	const float Dur = 1.f + Stats.IncSkillDuration / 100.f;
	switch (Index)
	{
	case 0: // Arcane Blast
	{
		const FVector Dir = AimDirection();
		FaceDirection(Dir);
		Lock(0.35f * CastMult);
		PlaySwing(RestArmPitch, 110.f, 0.15f * CastMult, false, 0.1f);
		After(0.15f * CastMult, [this]()
		{
			FArpgProjectileParams PP;
			PP.Hit = MakeSpellHit(EArpgDamageType::Lightning, 22.f, 32.f, 1.f, 12.f);
			PP.Hit.ShockChance = 0.2f;
			FVector Aim = AimDirection();
			if (AArpgEnemyCharacter* T = LockTarget.Get())
			{
				Aim = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				PP.Target = T;
				PP.HomingStrength = 2.f;
			}
			PP.Velocity = Aim * 2100.f;
			PP.Radius = 55.f;
			PP.VisualScale = FVector(0.5f);
			PP.Color = FLinearColor(0.75f, 0.45f, 1.f);
			PP.Glow = 8.f;
			PP.ExplodeRadius = 250.f;
			PP.Lifetime = 1.8f;
			AArpgProjectile::Launch(this, GetActorLocation() + GetActorForwardVector() * 70.f + FVector(0, 0, 60), PP);
		});
		break;
	}
	case 1: // Mana Surge
	{
		Mana = FMath::Min(Stats.MaxMana, Mana + Stats.MaxMana * 0.35f);
		FArpgHit H = MakeSpellHit(EArpgDamageType::Lightning, 12.f, 20.f, 1.f, 15.f);
		for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 450.f, Team))
		{
			FArpgHit HH = H;
			HH.Knockback = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 900.f + FVector(0, 0, 250);
			DealHit(T, HH);
		}
		AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 420.f, FLinearColor(0.45f, 0.55f, 1.f));
		Lock(0.3f);
		Shake(0.5f);
		break;
	}
	case 2: // Telekinesis
	{
		const FVector Fwd = AimDirection();
		FaceDirection(Fwd);
		PlaySwing(RestArmPitch, 150.f, 0.25f, false, 0.7f);
		PlaySwing(RestArmPitch, 150.f, 0.25f, true, 0.7f);
		TArray<TWeakObjectPtr<AArpgCharacterBase>> Lifted;
		for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 800.f, Team, &Fwd, 110.f))
		{
			if (AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(T)) { E->Levitate(0.9f * Dur); }
			Lifted.Add(T);
		}
		Lock(0.5f);
		After(0.9f * Dur, [this, Lifted]()
		{
			PlaySwing(150.f, 10.f, 0.1f); PlaySwing(150.f, 10.f, 0.1f, true);
			for (const TWeakObjectPtr<AArpgCharacterBase>& W : Lifted)
			{
				if (AArpgCharacterBase* T = W.Get())
				{
					FArpgHit H = MakeSpellHit(EArpgDamageType::Physical, 20.f, 32.f, 1.f, 40.f);
					H.Knockback = FVector(0, 0, -1200);
					DealHit(T, H);
					AArpgAreaEffect::SpawnFlash(GetWorld(), T->GetActorLocation(), 120.f, FLinearColor(0.8f, 0.55f, 1.f));
				}
			}
			Shake(0.8f);
		}, false);
		break;
	}
	case 3: // Spell Barrier
		Barrier = 25.f + Stats.MaxMana * 0.6f;
		BarrierRemaining = 8.f * Dur;
		FlashParts(FLinearColor(0.6f, 0.5f, 1.f), 0.4f, 5.f);
		Message(TEXT("Spell Barrier"));
		break;
	}
}

void AArpgPlayerCharacter::ThiefSkill(int32 Index)
{
	const float Dur = 1.f + Stats.IncSkillDuration / 100.f;
	switch (Index)
	{
	case 0: // Shadow Step
	{
		AArpgCharacterBase* T = LockTarget.Get();
		if (!T) { T = SoftTarget(1500.f, 180.f); }
		if (!T) { T = ArpgCombat::FindNearest(GetWorld(), GetActorLocation(), 1500.f, Team); }
		if (!T)
		{
			Message(TEXT("Shadow Step needs a target."));
			SkillCooldowns[0] = 0.f;
			Mana += ArpgData::Class(PlayerClass).Skills[0].ManaCost;
			return;
		}
		AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 120.f, FLinearColor(0.2f, 0.85f, 1.f));
		const FVector Behind = T->GetActorLocation() - T->GetActorForwardVector() * (T->GetSimpleCollisionRadius() + 90.f);
		SetActorLocation(FVector(Behind.X, Behind.Y, GetActorLocation().Z + 20.f), false, nullptr, ETeleportType::TeleportPhysics);
		FaceDirection((T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D());
		if (AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(T)) { E->LoseTrack(0.6f); }
		GuaranteedCritRemaining = 2.f;
		InvulnRemaining = 0.2f;
		AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 120.f, FLinearColor(0.2f, 0.85f, 1.f));
		break;
	}
	case 1: // Twin Strike
	{
		FaceDirection(AimDirection());
		Lock(0.45f / AttackSpeedMult());
		for (int32 i = 0; i < 2; ++i)
		{
			After((0.08f + i * 0.14f) / AttackSpeedMult(), [this, i]()
			{
				PlaySwing(150.f, 30.f, 0.08f, i == 1);
				PlayBodyTwist(i == 0 ? 35.f : -35.f, 0.15f);
				FArpgHit H = MakeAttackHit(0.9f, 1.f, 8.f);
				H.BleedChance = 0.3f;
				H.PoisonStacks = 1;
				StrikeArc(240.f, 120.f, H, 100.f);
			});
		}
		break;
	}
	case 2: // Smoke Bomb
	{
		FArpgAreaParams AP;
		AP.Kind = EArpgAreaKind::Smoke;
		AP.Radius = 400.f;
		AP.Duration = 5.f * Dur;
		AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
		Message(TEXT("Smoke Bomb: hits on enemies in the cloud are backstabs."));
		break;
	}
	case 3: // Lockpick
	{
		for (TActorIterator<AArpgChest> It(GetWorld()); It; ++It)
		{
			if (It->IsLocked() && !It->IsOpened() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 350.f)
			{
				It->Open(this, true);
				Message(TEXT("Lock picked: the coffer's best loot is yours."));
				return;
			}
		}
		AArpgCharacterBase* T = LockTarget.Get();
		if (!T) { T = ArpgCombat::FindNearest(GetWorld(), GetActorLocation(), 600.f, Team); }
		if (T)
		{
			T->ApplyExpose(6.f * Dur);
			ArpgCombat::ShowNumber(GetWorld(), T->GetActorLocation() + FVector(0, 0, 130), TEXT("Exposed"), FLinearColor(0.6f, 1.f, 1.f), 1.f);
		}
		else
		{
			Message(TEXT("Nothing to pick or expose nearby."));
		}
		break;
	}
	}
}

void AArpgPlayerCharacter::FlowStrikeChain(int32 Remaining, TSet<AArpgCharacterBase*> AlreadyStruck)
{
	if (Remaining <= 0 || bDead) { return; }
	AArpgCharacterBase* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	const FVector Fwd = AimDirection();
	const bool bFirst = AlreadyStruck.Num() == 0;
	for (AArpgCharacterBase* C : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), bFirst ? 750.f : 600.f, Team, bFirst ? &Fwd : nullptr, bFirst ? 160.f : 360.f))
	{
		if (AlreadyStruck.Contains(C)) { continue; }
		const float D = FVector::DistSquared(C->GetActorLocation(), GetActorLocation());
		if (D < BestD) { BestD = D; Best = C; }
	}
	if (!Best) { return; }
	AlreadyStruck.Add(Best);
	const FVector ToT = (Best->GetActorLocation() - GetActorLocation());
	const FVector Dest = Best->GetActorLocation() - ToT.GetSafeNormal2D() * (Best->GetSimpleCollisionRadius() + 70.f);
	SetActorLocation(FVector(Dest.X, Dest.Y, GetActorLocation().Z), true);
	FaceDirection(ToT.GetSafeNormal2D());
	PlaySwing(40.f, 100.f, 0.06f, AlreadyStruck.Num() % 2 == 0);
	FArpgHit H = MakeAttackHit(1.4f, 1.f, 12.f);
	H.HitstopSeconds = 0.07f;
	H.Knockback = ToT.GetSafeNormal2D() * 250.f;
	DealHit(Best, H);
	AArpgAreaEffect::SpawnFlash(GetWorld(), Best->GetActorLocation(), 90.f, ArpgData::Class(EArpgClass::Monk).Color);
	InvulnRemaining = FMath::Max(InvulnRemaining, 0.2f);
	Lock(0.2f);
	After(0.16f, [this, Remaining, AlreadyStruck]() { FlowStrikeChain(Remaining - 1, AlreadyStruck); });
}

void AArpgPlayerCharacter::MonkSkill(int32 Index)
{
	const float Dur = 1.f + Stats.IncSkillDuration / 100.f;
	switch (Index)
	{
	case 0: // Flow Strike
	{
		TSet<AArpgCharacterBase*> None;
		if (!ArpgCombat::FindNearest(GetWorld(), GetActorLocation(), 750.f, Team))
		{
			Message(TEXT("Flow Strike needs an enemy within 7 m."));
			SkillCooldowns[0] = 0.f;
			Mana += ArpgData::Class(PlayerClass).Skills[0].ManaCost;
			return;
		}
		FlowStrikeChain(3, None);
		break;
	}
	case 1: // Chi Heal
		ChiHealRemaining = 2.f;
		ChiHealRate = Stats.MaxHealth * 0.3f / 2.f;
		ClearAilments();
		FlashParts(FLinearColor(0.5f, 1.f, 0.6f), 0.5f, 4.f);
		AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 150.f, FLinearColor(0.5f, 1.f, 0.6f));
		break;
	case 2: // Zen Resolve
		ZenRemaining = 6.f * Dur;
		bUnstaggerable = true;
		DamageTakenMultiplier = 0.85f;
		RecalculateStats();
		FlashParts(FLinearColor(1.f, 0.85f, 0.4f), 0.5f, 4.f);
		Message(TEXT("Zen Resolve"));
		break;
	case 3: // Divine Palm
	{
		const FVector Dir = AimDirection();
		FaceDirection(Dir);
		Lock(0.4f);
		PlaySwing(RestArmPitch, 95.f, 0.12f, false, 0.25f);
		After(0.12f, [this, Dir]()
		{
			FArpgHit H = MakeAttackHit(1.8f, 1.f, 30.f);
			const float Half = H.Damage[0] * 0.5f;
			H.Damage[0] -= Half;
			H.Damage[1] += Half;
			H.IgniteChance = 0.2f;
			H.Knockback = FVector(900, 0, 0);
			H.HitstopSeconds = 0.09f;
			FArpgProjectileParams PP;
			PP.Hit = H;
			PP.Shape = EArpgShape::Cube;
			PP.Velocity = Dir * 1900.f;
			PP.Radius = 160.f;
			PP.VisualScale = FVector(0.25f, 2.8f, 1.6f);
			PP.Color = FLinearColor(1.f, 0.72f, 0.15f);
			PP.Glow = 6.f;
			PP.Pierce = -1;
			PP.Lifetime = 0.75f;
			PP.bIgnoreWalls = false;
			PP.HealOwnerFraction = HasUnique(TEXT("StillLake")) ? 0.1f : 0.f;
			AArpgProjectile::Launch(this, GetActorLocation() + Dir * 80.f + FVector(0, 0, 40), PP);
			Shake(0.4f);
		});
		break;
	}
	}
}

// ---------------------------------------------------------------- defence

bool AArpgPlayerCharacter::PreReceiveHit(FArpgHit& Hit, AArpgCharacterBase* Source)
{
	if (bGodMode) { return false; }
	if (InvulnRemaining > 0.f)
	{
		return false;
	}
	if (bBlocking && Source && !Hit.bUnblockable && !Hit.bSpell)
	{
		const FVector ToSource = (Source->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(GetActorForwardVector(), ToSource) > 0.2f)
		{
			if (GetWorld()->GetTimeSeconds() - BlockStartTime <= 0.15f)
			{
				// Perfect parry: no damage, the attacker is staggered.
				ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 120), TEXT("PERFECT PARRY"), FLinearColor(1.f, 0.95f, 0.5f), 1.3f);
				Source->Poise = 0.f;
				Source->ApplyStagger(Source->bIsBoss ? 1.f : 2.f);
				Stamina = FMath::Min(Stats.MaxStamina, Stamina + 20.f);
				ApplyHitstop(0.12f);
				Shake(0.8f);
				return false;
			}
			const float Cost = BarrierRemaining > 0.f && PlayerClass == EArpgClass::Warrior ? 0.f : Hit.Total() * 0.5f;
			if (Stamina >= Cost)
			{
				Stamina -= Cost;
				StaminaDelay = 0.8f;
				ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 110), TEXT("Blocked"), FLinearColor(0.8f, 0.85f, 0.9f), 0.8f);
				FlashParts(FLinearColor(0.8f, 0.9f, 1.f), 0.06f, 1.f);
				ApplyHitstop(0.06f);
				return false;
			}
			// Guard break (GDD): stunned for 1.2 s.
			Stamina = 0.f;
			bExhausted = true;
			bBlocking = false;
			ApplyStun(1.2f);
			ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 120), TEXT("GUARD BREAK"), FLinearColor(1.f, 0.3f, 0.2f), 1.2f);
		}
	}
	if (Stats.BlockChance > 0.f && !Hit.bUnblockable && !Hit.bSpell && FMath::FRand() * 100.f < Stats.BlockChance)
	{
		ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 110), TEXT("Block"), FLinearColor(0.8f, 0.85f, 0.9f), 0.8f);
		return false;
	}
	Shake(Hit.Total() > Stats.MaxHealth * 0.15f ? 0.9f : 0.4f);
	return true;
}

void AArpgPlayerCharacter::OnDeath(AArpgCharacterBase* Killer)
{
	Super::OnDeath(Killer);
	if (Grabbed.IsValid()) { ReleaseGrab(false); }
	LockTarget = nullptr;
	bBlocking = false;
	UIState = EArpgPlayerUI::Dead;
	VisualRoot->SetRelativeRotation(FRotator(0.f, 0.f, 80.f));
	TintParts(FLinearColor(0.1f, 0.1f, 0.1f), 0.5f);
	if (AArpgGameMode* GM = GetGM()) { GM->OnPlayerDied(); }
}

AArpgEnemyCharacter* AArpgPlayerCharacter::GetLockTarget() const
{
	return LockTarget.Get();
}

AActor* AArpgPlayerCharacter::GetInteractTarget() const
{
	AArpgInteractable* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<AArpgInteractable> It(GetWorld()); It; ++It)
	{
		if (!It->CanInteract(this)) { continue; }
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < It->InteractRadius && D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

FString AArpgPlayerCharacter::GetInteractText() const
{
	if (AArpgInteractable* I = Cast<AArpgInteractable>(GetInteractTarget())) { return I->GetPrompt(this); }
	return FString();
}

// ---------------------------------------------------------------- tick

void AArpgPlayerCharacter::UpdateCamera(float Dt)
{
	if (!Controller) { return; }
	if (AArpgEnemyCharacter* T = LockTarget.Get())
	{
		if (T->IsDead() || FVector::Dist(T->GetActorLocation(), GetActorLocation()) > 3200.f) { LockTarget = nullptr; }
		else
		{
			const FVector Mid = (T->GetActorLocation() + GetActorLocation()) * 0.5f;
			FRotator Want = (Mid - Camera->GetComponentLocation()).Rotation();
			Want.Pitch = FMath::Clamp(Want.Pitch - 8.f, -35.f, 10.f);
			Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Want, Dt, 6.f));
		}
	}
	// FOV: sprint widens it (GDD camera).
	const float WantFOV = BaseFOV + (bSprinting ? 8.f : 0.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, WantFOV, Dt, 6.f));
	// Simple shake: jitter the camera locally (cosmetic only).
	if (ShakeTime > 0.f)
	{
		ShakeTime -= Dt;
		const float A = ShakeStrength * FMath::Max(0.f, ShakeTime / 0.25f) * 6.f;
		Camera->SetRelativeLocation(FVector(0.f, FMath::FRandRange(-A, A), FMath::FRandRange(-A, A)));
		if (ShakeTime <= 0.f) { ShakeStrength = 0.f; Camera->SetRelativeLocation(FVector::ZeroVector); }
	}
}

void AArpgPlayerCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	UpdateCamera(GetWorld()->GetDeltaSeconds());

	if (RopeTime > 0.f)
	{
		RopeTime -= Dt;
		if (RopeTime <= 0.f) { Rope->SetVisibility(false); }
	}

	if (bDead || UIState == EArpgPlayerUI::ClassSelect) { return; }

	// Timers
	ActionLock = FMath::Max(0.f, ActionLock - Dt);
	InvulnRemaining = FMath::Max(0.f, InvulnRemaining - Dt);
	GrappleCooldown = FMath::Max(0.f, GrappleCooldown - Dt);
	DodgeCooldownBoost = FMath::Max(0.f, DodgeCooldownBoost - Dt);
	GuaranteedCritRemaining = FMath::Max(0.f, GuaranteedCritRemaining - Dt);
	const float CdRate = DodgeCooldownBoost > 0.f ? 2.f : 1.f;
	for (float& Cd : SkillCooldowns) { Cd = FMath::Max(0.f, Cd - Dt * CdRate); }
	if (ComboTimer > 0.f) { ComboTimer -= Dt; if (ComboTimer <= 0.f) { ComboIndex = 0; } }
	if (FuryRemaining > 0.f) { FuryRemaining -= Dt; }
	if (BarrierRemaining > 0.f) { BarrierRemaining -= Dt; if (BarrierRemaining <= 0.f) { Barrier = 0.f; } }
	if (ZenRemaining > 0.f)
	{
		ZenRemaining -= Dt;
		if (ZenRemaining <= 0.f) { bUnstaggerable = false; DamageTakenMultiplier = 1.f; RecalculateStats(); }
	}
	if (FlaskHealRemaining > 0.f) { Heal(FlaskHealRate * FMath::Min(Dt, FlaskHealRemaining), false); FlaskHealRemaining -= Dt; }
	if (ChiHealRemaining > 0.f) { Heal(ChiHealRate * FMath::Min(Dt, ChiHealRemaining), false); ChiHealRemaining -= Dt; }

	// Dash
	if (DashRemaining > 0.f)
	{
		DashRemaining -= Dt;
		AddMovementInput(DashDir, 1.f);
		if (bDashHits)
		{
			for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), DashHitRadius, Team))
			{
				if (DashVictims.Contains(T)) { continue; }
				DashVictims.Add(T);
				FArpgHit H = DashHit;
				H.Knockback = FVector::CrossProduct(DashDir, FVector::UpVector) * 300.f;
				DealHit(T, H);
			}
		}
		if (DashRemaining <= 0.f)
		{
			GetCharacterMovement()->MaxAcceleration = 2400.f;
			GetCharacterMovement()->Velocity *= 0.3f;
			UpdateMoveSpeed();
		}
	}
	else
	{
		UpdateMoveSpeed();
	}

	// Stamina, mana
	if (bDodgeHeld) { DodgeHeldTime += Dt; }
	bSprinting = bDodgeHeld && DodgeHeldTime > 0.35f && !MoveInput.IsNearlyZero() && !bExhausted && DashRemaining <= 0.f;
	if (bSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - 10.f * Dt);
		StaminaDelay = 0.8f;
		if (Stamina <= 0.f) { bExhausted = true; bSprinting = false; }
	}
	StaminaDelay -= Dt;
	if (StaminaDelay <= 0.f && !bBlocking)
	{
		Stamina = FMath::Min(Stats.MaxStamina, Stamina + 30.f * (1.f + Stats.IncStaminaRegen / 100.f) * (ZenRemaining > 0.f ? 1.5f : 1.f) * Dt);
	}
	if (bExhausted && Stamina >= 25.f) { bExhausted = false; }
	Mana = FMath::Min(Stats.MaxMana, Mana + Stats.MaxMana * 0.02f * (1.f + Stats.IncManaRegen / 100.f) * Dt);

	// Hold a grabbed enemy in front.
	if (AArpgEnemyCharacter* G = Grabbed.Get())
	{
		if (G->IsDead()) { Grabbed = nullptr; }
		else { G->SetActorLocation(GetActorLocation() + GetActorForwardVector() * 110.f + FVector(0, 0, 40), false); }
	}

	// Face the lock-on target while not sprinting.
	if (AArpgEnemyCharacter* T = LockTarget.Get())
	{
		const bool bStrafe = !bSprinting && DashRemaining <= 0.f;
		GetCharacterMovement()->bOrientRotationToMovement = !bStrafe;
		if (bStrafe)
		{
			const FRotator Want((T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation());
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Want.Yaw, 0.f), Dt, 12.f));
		}
	}
	else
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	// Buffered input
	if (BufferedAction >= 0)
	{
		BufferTime -= Dt;
		if (BufferTime <= 0.f) { BufferedAction = -1; }
		else if (CanAct()) { QueueOrRun(BufferedAction); }
	}

	// Fell out of the world: put the player back at the floor start.
	if (GetActorLocation().Z < -4000.f)
	{
		if (AArpgGameMode* GM = GetGM()) { GM->RecoverPlayer(); }
	}
}
