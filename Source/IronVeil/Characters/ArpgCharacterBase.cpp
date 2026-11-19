#include "Characters/ArpgCharacterBase.h"

#include "Core/ArpgAssets.h"
#include "Combat/ArpgCombat.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"

AArpgCharacterBase::AArpgCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -96.f));

	GetCapsuleComponent()->InitCapsuleSize(40.f, 96.f);
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = 400.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->JumpZVelocity = 560.f;
	Move->AirControl = 0.35f;
	Move->GravityScale = 1.6f;
}

void AArpgCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	UpdateMoveSpeed();
}

bool AArpgCharacterBase::HasAilment(bool bIgnite, bool bBleed, bool bPoison) const
{
	for (const FArpgDot& D : Dots)
	{
		if (bIgnite && D.Type == EArpgDamageType::Fire && !D.bPoison) { return true; }
		if (bBleed && D.bBleed) { return true; }
		if (bPoison && D.bPoison) { return true; }
	}
	return false;
}

// ---------------------------------------------------------------- body

UStaticMeshComponent* AArpgCharacterBase::AddPart(EArpgShape Shape, const FVector& Loc, const FVector& Scale, const FLinearColor& Color,
	const FRotator& Rot, USceneComponent* Parent, float Emissive)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ArpgAssets::Shape(Shape));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(Parent ? Parent : VisualRoot.Get());
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	UMaterialInstanceDynamic* MID = ArpgAssets::NewColor(this, Color, Emissive);
	C->SetMaterial(0, MID);
	FArpgBodyPart P;
	P.Comp = C; P.MID = MID; P.BaseColor = Color; P.BaseEmissive = Emissive;
	Parts.Add(P);
	return C;
}

UStaticMeshComponent* AArpgCharacterBase::AddMeshPart(UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FRotator& Rot,
	USceneComponent* Parent)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(Parent ? Parent : VisualRoot.Get());
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	FArpgBodyPart P;
	P.Comp = C;
	Parts.Add(P);
	return C;
}

USceneComponent* AArpgCharacterBase::AddPivot(const FVector& Loc, USceneComponent* Parent)
{
	USceneComponent* C = NewObject<USceneComponent>(this);
	C->SetupAttachment(Parent ? Parent : VisualRoot.Get());
	C->SetRelativeLocation(Loc);
	C->RegisterComponent();
	return C;
}

void AArpgCharacterBase::BuildHumanoid(const FLinearColor& Skin, const FLinearColor& Torso, const FLinearColor& Legs, const FLinearColor& Arms,
	float Bulk)
{
	const float W = Bulk;
	Rig.HipL = AddPivot(FVector(0.f, -12.f * W, 92.f));
	Rig.HipR = AddPivot(FVector(0.f, 12.f * W, 92.f));
	AddPart(EArpgShape::Cylinder, FVector(0.f, 0.f, -44.f), FVector(0.17f * W, 0.17f * W, 0.9f), Legs, FRotator::ZeroRotator, Rig.HipL);
	AddPart(EArpgShape::Cylinder, FVector(0.f, 0.f, -44.f), FVector(0.17f * W, 0.17f * W, 0.9f), Legs, FRotator::ZeroRotator, Rig.HipR);
	AddPart(EArpgShape::Cube, FVector(6.f, 0.f, -88.f), FVector(0.3f, 0.14f * W, 0.08f), Legs * 0.6f, FRotator::ZeroRotator, Rig.HipL);
	AddPart(EArpgShape::Cube, FVector(6.f, 0.f, -88.f), FVector(0.3f, 0.14f * W, 0.08f), Legs * 0.6f, FRotator::ZeroRotator, Rig.HipR);

	Rig.Chest = AddPivot(FVector(0.f, 0.f, 92.f));
	AddPart(EArpgShape::Cube, FVector(0.f, 0.f, 8.f), FVector(0.24f * W, 0.36f * W, 0.22f), Legs * 0.8f, FRotator::ZeroRotator, Rig.Chest);
	AddPart(EArpgShape::Cube, FVector(0.f, 0.f, 36.f), FVector(0.27f * W, 0.44f * W, 0.42f), Torso, FRotator::ZeroRotator, Rig.Chest);
	AddPart(EArpgShape::Cylinder, FVector(0.f, 0.f, 62.f), FVector(0.12f, 0.12f, 0.12f), Skin, FRotator::ZeroRotator, Rig.Chest);
	Rig.Head = AddPart(EArpgShape::Sphere, FVector(0.f, 0.f, 78.f), FVector(0.25f, 0.23f, 0.28f), Skin, FRotator::ZeroRotator, Rig.Chest);

	Rig.ShoulderL = AddPivot(FVector(0.f, -27.f * W, 52.f), Rig.Chest);
	Rig.ShoulderR = AddPivot(FVector(0.f, 27.f * W, 52.f), Rig.Chest);
	AddPart(EArpgShape::Sphere, FVector::ZeroVector, FVector(0.18f * W), Torso * 0.9f, FRotator::ZeroRotator, Rig.ShoulderL);
	AddPart(EArpgShape::Sphere, FVector::ZeroVector, FVector(0.18f * W), Torso * 0.9f, FRotator::ZeroRotator, Rig.ShoulderR);
	AddPart(EArpgShape::Cylinder, FVector(0.f, 0.f, -30.f), FVector(0.12f * W, 0.12f * W, 0.6f), Arms, FRotator::ZeroRotator, Rig.ShoulderL);
	AddPart(EArpgShape::Cylinder, FVector(0.f, 0.f, -30.f), FVector(0.12f * W, 0.12f * W, 0.6f), Arms, FRotator::ZeroRotator, Rig.ShoulderR);
	Rig.HandL = AddPivot(FVector(0.f, 0.f, -62.f), Rig.ShoulderL);
	Rig.HandR = AddPivot(FVector(0.f, 0.f, -62.f), Rig.ShoulderR);
	AddPart(EArpgShape::Sphere, FVector::ZeroVector, FVector(0.11f * W), Skin, FRotator::ZeroRotator, Rig.HandL);
	AddPart(EArpgShape::Sphere, FVector::ZeroVector, FVector(0.11f * W), Skin, FRotator::ZeroRotator, Rig.HandR);
}

void AArpgCharacterBase::PlaySwing(float FromPitch, float ToPitch, float Duration, bool bLeftArm, float Hold)
{
	FSwing& S = bLeftArm ? SwingL : SwingR;
	S.From = FromPitch; S.To = ToPitch; S.Duration = FMath::Max(0.02f, Duration); S.Hold = Hold; S.Time = 0.f;
}

void AArpgCharacterBase::PlayBodyTwist(float Yaw, float Duration)
{
	TwistYaw = Yaw; TwistDuration = FMath::Max(0.05f, Duration); TwistTime = 0.f;
}

void AArpgCharacterBase::AnimateRig(float Dt)
{
	if (!Rig.HipL || bDead) { return; }
	const float Speed = GetVelocity().Size2D();
	const float Amount = FMath::Clamp(Speed / 450.f, 0.f, 1.f);
	WalkPhase += Dt * (2.f + Speed / 55.f);
	const float Leg = FMath::Sin(WalkPhase) * 38.f * Amount;
	Rig.HipL->SetRelativeRotation(FRotator(Leg, 0.f, 0.f));
	Rig.HipR->SetRelativeRotation(FRotator(-Leg, 0.f, 0.f));

	auto ArmPitch = [this, Dt, Leg](FSwing& S, float Natural) -> float
	{
		if (S.Time < 0.f) { return Natural; }
		S.Time += Dt;
		const float Total = S.Duration + S.Hold + 0.18f;
		if (S.Time >= Total) { S.Time = -1.f; return Natural; }
		if (S.Time <= S.Duration)
		{
			const float A = FMath::Clamp(S.Time / S.Duration, 0.f, 1.f);
			return FMath::Lerp(S.From, S.To, 1.f - FMath::Square(1.f - A));
		}
		if (S.Time <= S.Duration + S.Hold) { return S.To; }
		const float B = (S.Time - S.Duration - S.Hold) / 0.18f;
		return FMath::Lerp(S.To, Natural, B);
	};
	const float Breathe = FMath::Sin(GetWorld()->GetTimeSeconds() * 2.f) * 2.f;
	Rig.ShoulderR->SetRelativeRotation(FRotator(ArmPitch(SwingR, RestArmPitch - Leg * 0.6f + Breathe), 0.f, -6.f));
	Rig.ShoulderL->SetRelativeRotation(FRotator(ArmPitch(SwingL, RestArmPitch + Leg * 0.6f + Breathe), 0.f, 6.f));

	float Twist = 0.f;
	if (TwistTime >= 0.f)
	{
		TwistTime += Dt;
		const float A = TwistTime / TwistDuration;
		Twist = A < 1.f ? FMath::Sin(A * PI) * TwistYaw : 0.f;
		if (A >= 1.f) { TwistTime = -1.f; }
	}
	const float Lean = IsStaggered() ? -18.f : Amount * 6.f;
	Rig.Chest->SetRelativeRotation(FRotator(-Lean, Twist, 0.f));
}

void AArpgCharacterBase::SetBodySize(float Radius, float HalfHeight)
{
	GetCapsuleComponent()->SetCapsuleSize(Radius, HalfHeight);
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
}

void AArpgCharacterBase::TintParts(const FLinearColor& Color, float Alpha, float Emissive)
{
	for (FArpgBodyPart& P : Parts)
	{
		if (UMaterialInstanceDynamic* MID = P.MID.Get())
		{
			ArpgAssets::SetColor(MID, FMath::Lerp(P.BaseColor, Color, Alpha), FMath::Max(P.BaseEmissive, Emissive));
		}
	}
}

void AArpgCharacterBase::ResetTint()
{
	for (FArpgBodyPart& P : Parts)
	{
		if (UMaterialInstanceDynamic* MID = P.MID.Get())
		{
			ArpgAssets::SetColor(MID, P.BaseColor, P.BaseEmissive);
		}
	}
}

void AArpgCharacterBase::FlashParts(const FLinearColor& Color, float Seconds, float Emissive)
{
	TintParts(Color, 0.75f, Emissive);
	FlashRemaining = Seconds;
}

// ---------------------------------------------------------------- timing

void AArpgCharacterBase::After(float Seconds, TFunction<void()> Fn, bool bInterruptible)
{
	FDelayed D;
	D.Time = Seconds;
	D.Fn = MoveTemp(Fn);
	D.bInterruptible = bInterruptible;
	Delayed.Add(MoveTemp(D));
}

void AArpgCharacterBase::CancelActions()
{
	Delayed.RemoveAll([](const FDelayed& D) { return D.bInterruptible; });
}

void AArpgCharacterBase::ApplyHitstop(float Seconds)
{
	if (Seconds <= 0.f || !GetWorld()) { return; }
	HitstopUntil = FMath::Max(HitstopUntil, (double)GetWorld()->GetRealTimeSeconds() + Seconds);
	UpdateDilation();
}

void AArpgCharacterBase::UpdateDilation()
{
	const bool bHitstop = GetWorld() && GetWorld()->GetRealTimeSeconds() < HitstopUntil;
	CustomTimeDilation = FMath::Max(0.02f, ZoneSlow * (bHitstop ? 0.05f : 1.f));
}

void AArpgCharacterBase::UpdateMoveSpeed()
{
	float Speed = GetBaseMoveSpeed() * (1.f + Stats.IncMoveSpeed / 100.f) * (1.f - ChillAmount);
	if (IsIncapacitated()) { Speed = 0.f; }
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void AArpgCharacterBase::Tick(float Dt)
{
	Super::Tick(Dt);

	// Delayed actions. A callback may add more; RemoveAtSwap keeps this loop safe.
	for (int32 i = 0; i < Delayed.Num();)
	{
		Delayed[i].Time -= Dt;
		if (Delayed[i].Time <= 0.f)
		{
			TFunction<void()> Fn = MoveTemp(Delayed[i].Fn);
			Delayed.RemoveAtSwap(i);
			if (Fn) { Fn(); }
		}
		else
		{
			++i;
		}
	}

	const float WorldDt = GetWorld()->GetDeltaSeconds();
	ZoneSlowTimer -= WorldDt;
	if (ZoneSlowTimer <= 0.f) { ZoneSlow = 1.f; }
	UpdateDilation();

	if (bDead) { return; }

	AnimateRig(Dt);

	const bool bWasIncapacitated = IsIncapacitated();
	StaggerRemaining = FMath::Max(0.f, StaggerRemaining - Dt);
	StunRemaining = FMath::Max(0.f, StunRemaining - Dt);
	const bool bWasFrozen = FreezeRemaining > 0.f;
	FreezeRemaining = FMath::Max(0.f, FreezeRemaining - Dt);
	ChillRemaining = FMath::Max(0.f, ChillRemaining - Dt);
	if (ChillRemaining <= 0.f) { ChillAmount = 0.f; }
	ShockRemaining = FMath::Max(0.f, ShockRemaining - Dt);
	ExposedRemaining = FMath::Max(0.f, ExposedRemaining - Dt);
	TimeSinceHit += Dt;
	TimeSinceDamaged += Dt;
	if (bWasFrozen && FreezeRemaining <= 0.f) { ResetTint(); }
	if (bWasIncapacitated != IsIncapacitated()) { UpdateMoveSpeed(); }

	if (FlashRemaining > 0.f)
	{
		FlashRemaining -= WorldDt;
		if (FlashRemaining <= 0.f)
		{
			if (FreezeRemaining > 0.f) { TintParts(FLinearColor(0.6f, 0.85f, 1.f), 0.7f, 0.5f); }
			else { ResetTint(); }
		}
	}

	// Poise
	if (TimeSinceHit > PoiseRecoverDelay && Poise < MaxPoise)
	{
		Poise = bPoiseRecoversGradually ? FMath::Min(MaxPoise, Poise + MaxPoise * 0.1f * Dt) : MaxPoise;
	}

	// Regeneration and energy shield recharge (20% per second after 3 s without damage)
	if (Stats.HealthRegen > 0.f) { Health = FMath::Min(Stats.MaxHealth, Health + Stats.HealthRegen * Dt); }
	if (Stats.MaxEnergyShield > 0.f && TimeSinceDamaged > 3.f)
	{
		EnergyShield = FMath::Min(Stats.MaxEnergyShield, EnergyShield + Stats.MaxEnergyShield * 0.2f * Dt);
	}

	// Damage over time
	if (Dots.Num() > 0)
	{
		float Total = 0.f;
		for (FArpgDot& D : Dots)
		{
			const float Step = FMath::Min(Dt, D.Remaining);
			float Amt = D.DPS * Step;
			if (D.bBleed && GetVelocity().Size2D() > 50.f) { Amt *= 2.f; }
			Total += Amt;
			D.Remaining -= Dt;
		}
		Dots.RemoveAll([](const FArpgDot& D) { return D.Remaining <= 0.f; });
		Health -= Total * DamageTakenMultiplier;
		DotNumberAccum += Total;
		if (DotNumberAccum >= FMath::Max(3.f, Stats.MaxHealth * 0.04f))
		{
			ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 40), FString::Printf(TEXT("%d"), FMath::RoundToInt(DotNumberAccum)),
				FLinearColor(0.9f, 0.6f, 0.3f), 0.7f);
			DotNumberAccum = 0.f;
		}
		if (Health <= 0.f)
		{
			Kill(LastAttacker.Get());
		}
	}
}

// ---------------------------------------------------------------- hits

float AArpgCharacterBase::DealHit(AArpgCharacterBase* Target, FArpgHit Hit)
{
	if (!Target || Target->IsDead() || Target == this) { return 0.f; }
	Hit.SourceLevel = Level;
	const float Dealt = Target->ReceiveHit(Hit, this);
	if (Dealt > 0.f)
	{
		if (!Hit.bSpell && Stats.HealthOnHit > 0.f) { Heal(Stats.HealthOnHit, false); }
		ApplyHitstop(Hit.HitstopSeconds);
		OnHitLanded(Target, Hit, Dealt);
	}
	return Dealt;
}

float AArpgCharacterBase::ReceiveHit(const FArpgHit& InHit, AArpgCharacterBase* Source)
{
	if (bDead || bInvulnerable) { return 0.f; }
	FArpgHit Hit = InHit;
	if (!PreReceiveHit(Hit, Source)) { return 0.f; }

	UWorld* World = GetWorld();
	const FVector NumberLoc = GetActorLocation() + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), GetSimpleCollisionHalfHeight());

	if (!Hit.bSpell && !Hit.bIsThrow && FMath::FRand() < ArpgCombat::EvadeChance(Stats.Evasion, Hit.SourceLevel))
	{
		ArpgCombat::ShowNumber(World, NumberLoc, TEXT("Evade"), FLinearColor(0.7f, 0.9f, 0.7f), 0.8f);
		return 0.f;
	}

	float Mult = DamageTakenMultiplier;
	if (IsStaggered()) { Mult *= 1.25f; }
	if (ShockRemaining > 0.f) { Mult *= 1.f + ShockAmount; }
	if (ExposedRemaining > 0.f) { Mult *= 1.2f; }

	float Dealt[ArpgNumDamageTypes];
	float Total = 0.f;
	for (int32 i = 0; i < ArpgNumDamageTypes; ++i)
	{
		float D = Hit.Damage[i] * Mult;
		if (D <= 0.f) { Dealt[i] = 0.f; continue; }
		if (i == (int32)EArpgDamageType::Physical)
		{
			const float Armour = ExposedRemaining > 0.f ? 0.f : Stats.Armour;
			D *= 1.f - ArpgCombat::ArmourReduction(Armour, D);
		}
		else
		{
			D *= 1.f - FMath::Min(Stats.Resist[i], 75.f) / 100.f;
		}
		Dealt[i] = D;
		Total += D;
	}

	float Remaining = Total;
	if (Barrier > 0.f)
	{
		const float Absorb = FMath::Min(Barrier, Remaining);
		Barrier -= Absorb;
		Remaining -= Absorb;
	}
	if (EnergyShield > 0.f)
	{
		const float Absorb = FMath::Min(EnergyShield, Remaining);
		EnergyShield -= Absorb;
		Remaining -= Absorb;
	}
	Health -= Remaining;
	TimeSinceHit = 0.f;
	TimeSinceDamaged = 0.f;
	if (Source) { LastAttacker = Source; }

	// Poise and stagger
	if (!bUnstaggerable && Hit.PoiseDamage > 0.f && !IsStaggered())
	{
		Poise -= Hit.PoiseDamage;
		if (Poise <= 0.f)
		{
			Poise = 0.f;
			ApplyStagger(StaggerDuration);
		}
	}

	// Ailments (GDD)
	const float MaxHP = FMath::Max(1.f, Stats.MaxHealth);
	const float Fire = Dealt[(int32)EArpgDamageType::Fire];
	const float Cold = Dealt[(int32)EArpgDamageType::Cold];
	const float Light = Dealt[(int32)EArpgDamageType::Lightning];
	const float Phys = Dealt[(int32)EArpgDamageType::Physical];
	const float Chaos = Dealt[(int32)EArpgDamageType::Chaos];
	if (Fire > 0.f && FMath::FRand() < Hit.IgniteChance)
	{
		const float DPS = Fire * 0.9f / 4.f;
		FArpgDot* Existing = Dots.FindByPredicate([](const FArpgDot& D) { return D.Type == EArpgDamageType::Fire && !D.bPoison; });
		if (!Existing) { FArpgDot D; D.Type = EArpgDamageType::Fire; D.DPS = DPS; D.Remaining = 4.f; Dots.Add(D); }
		else if (DPS >= Existing->DPS) { Existing->DPS = DPS; Existing->Remaining = 4.f; }
	}
	if (Phys > 0.f && FMath::FRand() < Hit.BleedChance)
	{
		FArpgDot D; D.Type = EArpgDamageType::Physical; D.DPS = Phys * 0.7f / 5.f; D.Remaining = 5.f; D.bBleed = true;
		Dots.RemoveAll([](const FArpgDot& X) { return X.bBleed; });
		Dots.Add(D);
	}
	for (int32 s = 0; s < Hit.PoisonStacks; ++s)
	{
		FArpgDot D; D.Type = EArpgDamageType::Chaos; D.DPS = FMath::Max(1.f, (Phys + Chaos) * 0.3f / 2.f); D.Remaining = 2.f; D.bPoison = true;
		Dots.Add(D);
	}
	if (Cold > 0.f)
	{
		const bool bWasChilled = ChillRemaining > 0.f;
		const float Chill = FMath::Clamp(Cold / (MaxHP * 0.1f) * 0.15f, 0.08f, 0.3f);
		ChillAmount = FMath::Max(ChillAmount, Chill);
		ChillRemaining = 2.f;
		// Big cold hits freeze; already-chilled targets freeze from smaller spell hits (Frost Nova in the GDD).
		const bool bBigHit = Cold > MaxHP * 0.15f || (bWasChilled && Hit.bSpell && Cold > MaxHP * 0.06f);
		if (bBigHit && !bIsBoss && FreezeRemaining <= 0.f)
		{
			FreezeRemaining = FMath::Clamp(Cold / MaxHP * 4.f, 0.3f, 1.f);
			CancelActions();
			TintParts(FLinearColor(0.6f, 0.85f, 1.f), 0.7f, 0.5f);
		}
	}
	if (Light > 0.f && FMath::FRand() < Hit.ShockChance)
	{
		ShockAmount = FMath::Max(ShockAmount * (ShockRemaining > 0.f ? 1.f : 0.f), FMath::Clamp(Light / MaxHP, 0.05f, 0.3f));
		ShockRemaining = 2.f;
	}
	UpdateMoveSpeed();

	// Feedback
	const FLinearColor NumColor = Hit.bCrit ? FLinearColor(1.f, 0.85f, 0.2f) : ArpgColors::Damage(Hit.Dominant());
	ArpgCombat::ShowNumber(World, NumberLoc, FString::Printf(Hit.bCrit ? TEXT("%d!") : TEXT("%d"), FMath::Max(1, FMath::RoundToInt(Total))),
		Team == EArpgTeam::Player ? FLinearColor(1.f, 0.3f, 0.3f) : NumColor, Hit.bCrit ? 1.35f : 1.f);
	ApplyHitstop(Hit.HitstopSeconds);
	if (FreezeRemaining <= 0.f) { FlashParts(FLinearColor::White, 0.08f, 1.5f); }
	if (!Hit.Knockback.IsNearlyZero() && !bIsBoss)
	{
		LaunchCharacter(Hit.Knockback, true, false);
	}

	if (Health <= 0.f)
	{
		Kill(Source);
	}
	return Total;
}

void AArpgCharacterBase::Heal(float Amount, bool bShowNumber)
{
	if (bDead || Amount <= 0.f) { return; }
	const float Before = Health;
	Health = FMath::Min(Stats.MaxHealth, Health + Amount);
	if (bShowNumber && Health - Before >= 1.f)
	{
		ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 80), FString::Printf(TEXT("+%d"), FMath::RoundToInt(Health - Before)),
			FLinearColor(0.4f, 1.f, 0.4f), 0.85f);
	}
}

void AArpgCharacterBase::ApplyStagger(float Seconds)
{
	if (bUnstaggerable || bDead) { return; }
	StaggerRemaining = FMath::Max(StaggerRemaining, Seconds);
	CancelActions();
	UpdateMoveSpeed();
	ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 120), TEXT("Staggered"), FLinearColor(1.f, 0.8f, 0.3f), 0.9f);
	OnStaggered();
}

void AArpgCharacterBase::ApplyStun(float Seconds)
{
	if (bDead) { return; }
	StunRemaining = FMath::Max(StunRemaining, Seconds);
	CancelActions();
	UpdateMoveSpeed();
}

void AArpgCharacterBase::ClearAilments()
{
	Dots.Reset();
	ChillAmount = 0.f; ChillRemaining = 0.f;
	ShockAmount = 0.f; ShockRemaining = 0.f;
	FreezeRemaining = 0.f;
	ResetTint();
	UpdateMoveSpeed();
}

void AArpgCharacterBase::Kill(AArpgCharacterBase* Killer)
{
	if (bDead) { return; }
	bDead = true;
	Health = 0.f;
	Dots.Reset();
	CancelActions();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnDeath(Killer);
}

void AArpgCharacterBase::OnDeath(AArpgCharacterBase* Killer)
{
	if (!bScatterOnDeath) { return; }
	// Throw the gray-box pieces apart in the direction of the killing blow.
	FVector Dir = Killer ? (GetActorLocation() - Killer->GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector() * -1.f;
	for (FArpgBodyPart& P : Parts)
	{
		UStaticMeshComponent* C = P.Comp.Get();
		if (!C) { continue; }
		UBodySetup* Body = C->GetBodySetup();
		if (!Body || Body->AggGeom.GetElementCount() == 0) { continue; }
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->SetSimulatePhysics(true);
		C->AddImpulse((Dir * FMath::FRandRange(250.f, 550.f) + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f),
			FMath::FRandRange(150.f, 400.f))), NAME_None, true);
		C->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-360.f, 360.f), 0.f), NAME_None, true);
	}
	TintParts(FLinearColor(0.08f, 0.08f, 0.08f), 0.5f);
	SetLifeSpan(4.f);
}
