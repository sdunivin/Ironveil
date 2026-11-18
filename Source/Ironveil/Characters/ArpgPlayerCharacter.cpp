#include "Characters/ArpgPlayerCharacter.h"

#include "AbilitySystem/ArpgAbilitySystemComponent.h"
#include "AbilitySystem/ArpgGameplayTags.h"
#include "CableComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Characters/ArpgCharacterMovementComponent.h"
#include "Components/ArpgGrappleComponent.h"
#include "Components/ArpgLockOnComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/ArpgPlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AArpgPlayerCharacter::AArpgPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Over-the-shoulder camera (plan 1.5): right-shoulder offset, smoothed with camera lag.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 320.f;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->ProbeSize = 14.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(DefaultFieldOfView);

	LockOn = CreateDefaultSubobject<UArpgLockOnComponent>(TEXT("LockOn"));
	Grapple = CreateDefaultSubobject<UArpgGrappleComponent>(TEXT("Grapple"));

	// Grapple rope. Starts at the right hand, hidden until a grapple fires.
	GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrappleCable"));
	GrappleCable->SetupAttachment(GetMesh(), TEXT("hand_r"));
	GrappleCable->CableLength = 50.f;
	GrappleCable->NumSegments = 12;
	GrappleCable->CableWidth = 2.f;
	GrappleCable->bAttachEnd = true;
	GrappleCable->SetVisibility(false);
	GrappleCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ---------------------------------------------------------------- GAS setup

void AArpgPlayerCharacter::InitAbilityActorInfo()
{
	AArpgPlayerState* PS = GetPlayerState<AArpgPlayerState>();
	if (!PS)
	{
		return;
	}

	AbilitySystemComponent = PS->GetArpgAbilitySystemComponent();
	AttributeSet = PS->GetAttributeSet();
	AbilitySystemComponent->InitAbilityActorInfo(PS, this);
}

void AArpgPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: the PlayerState is ready here.
	InitAbilityActorInfo();
	InitializeAttributes();
	GrantStartupAbilities();
}

void AArpgPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Clients: the PlayerState arrives a little later through replication.
	InitAbilityActorInfo();
}

// ---------------------------------------------------------------- Input

void AArpgPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AArpgPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
	}
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (DodgeSprintAction)
	{
		Input->BindAction(DodgeSprintAction, ETriggerEvent::Started, this, &ThisClass::OnDodgeSprintStarted);
		Input->BindAction(DodgeSprintAction, ETriggerEvent::Triggered, this, &ThisClass::OnDodgeSprintHeld);
		Input->BindAction(DodgeSprintAction, ETriggerEvent::Completed, this, &ThisClass::OnDodgeSprintReleased);
	}
	if (LockOnAction)
	{
		Input->BindAction(LockOnAction, ETriggerEvent::Started, this, &ThisClass::OnLockOnPressed);
	}
	if (SwitchTargetAction)
	{
		Input->BindAction(SwitchTargetAction, ETriggerEvent::Triggered, this, &ThisClass::OnSwitchTarget);
	}
	if (GrappleAction)
	{
		Input->BindAction(GrappleAction, ETriggerEvent::Started, this, &ThisClass::OnGrapplePressed);
	}

	for (const FArpgAbilityInputBinding& Binding : AbilityInputBindings)
	{
		if (Binding.InputAction && Binding.InputTag.IsValid())
		{
			Input->BindAction(Binding.InputAction, ETriggerEvent::Started, this, &ThisClass::AbilityInputPressed, Binding.InputTag);
			Input->BindAction(Binding.InputAction, ETriggerEvent::Completed, this, &ThisClass::AbilityInputReleased, Binding.InputTag);
		}
	}
}

void AArpgPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	// Move relative to where the camera faces, ignoring camera pitch.
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AArpgPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	// While locked on, the lock-on component steers the camera.
	if (LockOn && LockOn->IsLockedOn())
	{
		return;
	}

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AArpgPlayerCharacter::OnDodgeSprintStarted()
{
	DodgeSprintPressTime = GetWorld()->GetTimeSeconds();
	bDodgeSprintIsSprinting = false;
}

void AArpgPlayerCharacter::OnDodgeSprintHeld()
{
	if (!bDodgeSprintIsSprinting && GetWorld()->GetTimeSeconds() - DodgeSprintPressTime >= SprintHoldThreshold)
	{
		bDodgeSprintIsSprinting = true;
		if (UArpgCharacterMovementComponent* Move = GetArpgMovement())
		{
			Move->SetSprinting(true);
		}
	}
}

void AArpgPlayerCharacter::OnDodgeSprintReleased()
{
	if (bDodgeSprintIsSprinting)
	{
		if (UArpgCharacterMovementComponent* Move = GetArpgMovement())
		{
			Move->SetSprinting(false);
		}
	}
	else
	{
		// A tap: dodge on release, like most action RPGs. The ability with InputTag.Dodge runs.
		AbilityInputPressed(ArpgTags::InputTag_Dodge);
		AbilityInputReleased(ArpgTags::InputTag_Dodge);
	}
	bDodgeSprintIsSprinting = false;
}

void AArpgPlayerCharacter::OnLockOnPressed()
{
	if (LockOn)
	{
		LockOn->ToggleLockOn();
	}
}

void AArpgPlayerCharacter::OnSwitchTarget(const FInputActionValue& Value)
{
	if (LockOn && LockOn->IsLockedOn())
	{
		LockOn->SwitchTarget(Value.Get<float>());
	}
}

void AArpgPlayerCharacter::OnGrapplePressed()
{
	if (Grapple)
	{
		Grapple->TryGrapple();
	}
}

void AArpgPlayerCharacter::AbilityInputPressed(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AArpgPlayerCharacter::AbilityInputReleased(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

// ---------------------------------------------------------------- Tick

void AArpgPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Runs on the server and on the owning client so both rotate the character the same way.
	if (HasAuthority() || IsLocallyControlled())
	{
		UpdateRotationMode();
	}

	if (IsLocallyControlled())
	{
		UpdateCameraFieldOfView(DeltaSeconds);
	}
}

void AArpgPlayerCharacter::UpdateRotationMode()
{
	UArpgCharacterMovementComponent* Move = GetArpgMovement();
	if (!Move)
	{
		return;
	}

	// Locked on (and not sprinting): face the target and strafe around it.
	// Otherwise: turn toward the direction of movement.
	const bool bStrafe = LockOn && LockOn->IsLockedOn() && !Move->IsSprinting();
	Move->bOrientRotationToMovement = !bStrafe;
	Move->bUseControllerDesiredRotation = bStrafe;
}

void AArpgPlayerCharacter::UpdateCameraFieldOfView(float DeltaSeconds)
{
	const UArpgCharacterMovementComponent* Move = GetArpgMovement();
	const bool bSprintingAndMoving = Move && Move->IsSprinting() && GetVelocity().SizeSquared2D() > FMath::Square(100.f);
	const float TargetFOV = bSprintingAndMoving ? SprintFieldOfView : DefaultFieldOfView;
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, FieldOfViewInterpSpeed));
}

// ---------------------------------------------------------------- Landing

void AArpgPlayerCharacter::Landed(const FHitResult& Hit)
{
	// Velocity still holds the falling speed at this point.
	const float FallSpeed = -GetCharacterMovement()->Velocity.Z;

	Super::Landed(Hit);

	if (FallSpeed < HeavyLandingSpeed)
	{
		return;
	}

	if (HeavyLandingMontage)
	{
		PlayAnimMontage(HeavyLandingMontage);
	}

	if (IsLocallyControlled() && HeavyLandingCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ClientStartCameraShake(HeavyLandingCameraShake);
		}
	}
}
