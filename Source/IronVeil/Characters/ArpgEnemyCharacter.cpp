#include "Characters/ArpgEnemyCharacter.h"

#include "Characters/ArpgPlayerCharacter.h"
#include "Combat/ArpgCombat.h"
#include "Combat/ArpgProjectile.h"
#include "Combat/ArpgAreaEffect.h"
#include "Core/ArpgAssets.h"
#include "Core/ArpgGameMode.h"
#include "Tower/ArpgFloorGenerator.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

using EAtk = EArpgEnemyAttack;

AArpgEnemyCharacter::AArpgEnemyCharacter()
{
	Team = EArpgTeam::Enemy;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
}

void AArpgEnemyCharacter::Setup(const FArpgEnemySpawn& Spawn)
{
	Info = Spawn;
	Arch = ArpgData::Archetype(Spawn.Archetype);
	BiomeDef = &ArpgData::Biome(Spawn.Biome);
	Level = FMath::Max(1, Spawn.Level);
	bIsBoss = Spawn.Archetype == EArpgArchetype::Guardian || Spawn.Archetype == EArpgArchetype::FinalBoss;
	bSummoned = Spawn.bSummoned;
	bAggro = Spawn.bStartAggro || bIsBoss;

	Element = BiomeDef->Element;
	ElementShare = BiomeDef->ElementShare;
	Primary = BiomeDef->EnemyPrimary;
	Secondary = BiomeDef->EnemySecondary;
	Accent = BiomeDef->Accent;
	if (Spawn.bHasGuardian)
	{
		Element = Spawn.Guardian.Element;
		ElementShare = Element == EArpgDamageType::Physical ? 0.f : 0.4f;
		Primary = Spawn.Guardian.Primary;
		Secondary = Spawn.Guardian.Secondary;
		Accent = Spawn.Guardian.Secondary;
	}

	if (!Spawn.NameOverride.IsEmpty()) { DisplayName = Spawn.NameOverride; }
	else if (Spawn.bHasGuardian) { DisplayName = Spawn.Guardian.Name; }
	else
	{
		const int32 Idx = FMath::Clamp((int32)Spawn.Archetype, 0, 4);
		DisplayName = BiomeDef->EnemyNames[Idx];
	}
	StrafeSign = FMath::RandBool() ? 1.f : -1.f;
}

void AArpgEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	SpawnZ = GetActorLocation().Z;
	ApplyStats();
	BuildBody();
	UpdateMoveSpeed();
}

AArpgGameMode* AArpgEnemyCharacter::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AArpgGameMode>() : nullptr;
}

AArpgCharacterBase* AArpgEnemyCharacter::GetTarget() const
{
	AArpgGameMode* GM = GetGM();
	AArpgPlayerCharacter* P = GM ? GM->GetPlayerCharacter() : nullptr;
	return (P && !P->IsDead() && P->IsInGame()) ? P : nullptr;
}

void AArpgEnemyCharacter::ApplyStats()
{
	const float HealthMult = ArpgData::HealthMultiplier(Level) / ArpgData::HealthMultiplier(10);
	float MaxHP = Arch.Health10 * HealthMult * ArpgData::PrototypeMonsterHealthScale;
	// GDD defence values are at level 10; scale them so level-1 hits aren't swallowed by armour.
	const float DefenceScale = FMath::Clamp(Level / 10.f, 0.1f, 6.f);
	Stats.Armour = Arch.Armour * DefenceScale;
	Stats.Evasion = Arch.Evasion * DefenceScale;
	MaxPoise = Arch.Poise;
	PoiseRecoverDelay = Arch.PoiseRecoverDelay;
	bPoiseRecoversGradually = bIsBoss;
	SpeedMult = 1.f;

	switch (Info.Rarity)
	{
	case EArpgMonsterRarity::Magic: MaxHP *= 1.5f; break;
	case EArpgMonsterRarity::Rare:  MaxHP *= 3.f; MaxPoise *= 1.5f; break;
	default: break;
	}
	for (EArpgMonsterMod M : Info.Mods)
	{
		switch (M)
		{
		case EArpgMonsterMod::Hasted:        SpeedMult *= 1.3f; break;
		case EArpgMonsterMod::FireEnchanted: Stats.Resist[(int32)EArpgDamageType::Fire] += 40.f; break;
		case EArpgMonsterMod::Armoured:      Stats.Armour += 300.f; break;
		case EArpgMonsterMod::Regenerating:  Stats.HealthRegen = MaxHP * 0.02f; break;
		default: break;
		}
	}
	// Elemental flavour: monsters resist their own element a little.
	if (Element != EArpgDamageType::Physical) { Stats.Resist[(int32)Element] += 30.f; }
	if (bIsBoss) { for (int32 i = 1; i < ArpgNumDamageTypes; ++i) { Stats.Resist[i] += 15.f; } }

	Stats.MaxHealth = MaxHP;
	Health = MaxHP;
	Poise = MaxPoise;

	const float S = Arch.Scale;
	SetBodySize(38.f * S, 92.f * S);
	VisualRoot->SetRelativeScale3D(FVector(S));
}

float AArpgEnemyCharacter::GetBaseMoveSpeed() const
{
	float Speed = Arch.MoveSpeed * SpeedMult;
	if (bIsBoss && Phase > 1) { Speed *= 1.2f; }
	if (LevitateRemaining > 0.f || bIsGrabbed) { Speed = 0.f; }
	return Speed;
}

float AArpgEnemyCharacter::GetXPReward() const
{
	float XP = Arch.XP * (1.f + 0.12f * (Level - 1));
	if (Info.Rarity == EArpgMonsterRarity::Magic) { XP *= 2.f; }
	if (Info.Rarity == EArpgMonsterRarity::Rare) { XP *= 5.f; }
	if (bSummoned) { XP *= 0.25f; }
	return XP;
}

FString AArpgEnemyCharacter::GetModText() const
{
	TArray<FString> Names;
	for (EArpgMonsterMod M : Info.Mods) { Names.Add(ArpgData::MonsterModName(M)); }
	return FString::Join(Names, TEXT(", "));
}

FLinearColor AArpgEnemyCharacter::GetNameColor() const
{
	switch (Info.Rarity)
	{
	case EArpgMonsterRarity::Magic: return ArpgColors::Rarity(EArpgRarity::Magic);
	case EArpgMonsterRarity::Rare:  return ArpgColors::Rarity(EArpgRarity::Rare);
	case EArpgMonsterRarity::Boss:  return ArpgColors::Rarity(EArpgRarity::Unique);
	default: return FLinearColor(0.9f, 0.9f, 0.9f);
	}
}

// ---------------------------------------------------------------- body

void AArpgEnemyCharacter::BuildBody()
{
	if (UStaticMesh* Mesh = ArpgAssets::OptionalMesh(Info.bHasGuardian ? Info.Guardian.MeshPath : Arch.MeshPath))
	{
		AddMeshPart(Mesh, FVector::ZeroVector, FVector(1.f), FRotator(0.f, ArpgAssets::ImportedMeshYaw, 0.f));
		return;
	}

	const FLinearColor Skin = Info.Biome == EArpgBiome::Flood ? FLinearColor(0.42f, 0.5f, 0.48f)
		: (Info.Biome == EArpgBiome::Jungle ? FLinearColor(0.42f, 0.28f, 0.18f) : FLinearColor(0.62f, 0.45f, 0.35f));
	const FLinearColor Metal(0.42f, 0.43f, 0.45f);
	const FLinearColor Wood(0.32f, 0.2f, 0.11f);

	switch (Info.Archetype)
	{
	case EArpgArchetype::Swordsman:
		BuildHumanoid(Skin, Primary, Secondary, Primary * 0.9f);
		AddPart(EArpgShape::Cube, FVector(0, 0, -45), FVector(0.06f, 0.1f, 0.9f), Metal, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Cube, FVector(0, 0, -2), FVector(0.06f, 0.28f, 0.04f), Metal * 0.6f, FRotator::ZeroRotator, Rig.HandR);
		break;
	case EArpgArchetype::Archer:
		BuildHumanoid(Skin, Secondary, Primary, Secondary * 0.9f, 0.9f);
		AddPart(EArpgShape::Cube, FVector(0, 0, 0), FVector(0.05f, 0.05f, 1.2f), Wood, FRotator(0, 0, 0), Rig.HandL);
		AddPart(EArpgShape::Cylinder, FVector(-18, 0, 40), FVector(0.14f, 0.14f, 0.55f), Wood * 0.7f, FRotator(15, 0, 0), Rig.Chest);
		break;
	case EArpgArchetype::Duelist:
		BuildHumanoid(Skin, Primary * 1.2f, Secondary, Skin, 0.95f);
		AddPart(EArpgShape::Cube, FVector(0, 0, -38), FVector(0.04f, 0.07f, 0.75f), Metal * 1.3f, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Cube, FVector(0, 0, -38), FVector(0.04f, 0.07f, 0.75f), Metal * 1.3f, FRotator::ZeroRotator, Rig.HandL);
		AddPart(EArpgShape::Cube, FVector(0, 0, 10), FVector(0.29f, 0.46f, 0.08f), Accent * 0.7f, FRotator::ZeroRotator, Rig.Chest);
		break;
	case EArpgArchetype::ShieldBrute:
		BuildHumanoid(Skin, Metal, Secondary, Primary, 1.3f);
		AddPart(EArpgShape::Cube, FVector(25, 0, -20), FVector(0.1f, 0.75f, 1.1f), Primary * 0.8f, FRotator::ZeroRotator, Rig.HandL);
		AddPart(EArpgShape::Cylinder, FVector(0, 0, -35), FVector(0.08f, 0.08f, 0.7f), Wood, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Sphere, FVector(0, 0, -72), FVector(0.22f), Metal, FRotator::ZeroRotator, Rig.HandR);
		break;
	case EArpgArchetype::Ogre:
		BuildHumanoid(Skin * 0.8f, Secondary, Primary, Skin * 0.8f, 1.6f);
		AddPart(EArpgShape::Cylinder, FVector(0, 0, -50), FVector(0.14f, 0.14f, 1.0f), Wood, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Cylinder, FVector(0, 0, -100), FVector(0.3f, 0.3f, 0.45f), Wood * 0.7f, FRotator::ZeroRotator, Rig.HandR);
		break;
	case EArpgArchetype::Guardian:
		BuildHumanoid(Skin, Primary, Primary * 0.7f, Secondary, 1.4f);
		AddPart(EArpgShape::Cube, FVector(0, 0, -75), FVector(0.07f, 0.16f, 1.5f), Metal * 1.2f, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Cube, FVector(0, 0, -5), FVector(0.08f, 0.45f, 0.06f), Secondary, FRotator::ZeroRotator, Rig.HandR);
		AddPart(EArpgShape::Sphere, FVector(16, 0, 40), FVector(0.14f), Accent, FRotator::ZeroRotator, Rig.Chest, 6.f);   // Veil Relic
		AddPart(EArpgShape::Cube, FVector(-18, 0, 20), FVector(0.04f, 0.5f, 0.95f), Secondary * 0.6f, FRotator(-8, 0, 0), Rig.Chest); // cape
		for (int32 i = 0; i < 2; ++i)
		{
			const float Side = i == 0 ? -1.f : 1.f;
			AddPart(EArpgShape::Cone, FVector(0, 9 * Side, 18), FVector(0.07f, 0.07f, 0.25f), Secondary, FRotator(0, 0, 25 * Side), Rig.Head);
		}
		break;
	case EArpgArchetype::FinalBoss:
	{
		BuildHumanoid(FLinearColor(0.75f, 0.72f, 0.65f), Primary, Primary, Primary * 1.3f, 1.3f);
		AddPart(EArpgShape::Cone, FVector(0, 0, 50), FVector(1.1f, 1.1f, 1.2f), Primary * 0.8f);   // robe
		AddPart(EArpgShape::Sphere, FVector(16, 0, 40), FVector(0.18f), Accent * 0.3f + FLinearColor(0.2f, 0.5f, 1.f), FRotator::ZeroRotator, Rig.Chest, 8.f);
		// Clock-hand blade
		AddPart(EArpgShape::Cube, FVector(0, 0, -95), FVector(0.05f, 0.3f, 1.9f), Secondary, FRotator::ZeroRotator, Rig.HandR, 1.5f);
		AddPart(EArpgShape::Cone, FVector(0, 0, -200), FVector(0.3f, 0.06f, 0.4f), Secondary, FRotator(180, 0, 0), Rig.HandR, 1.5f);
		// Crown of hour marks
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2.f * PI * i / 12.f;
			AddPart(EArpgShape::Cube, FVector(FMath::Cos(A) * 22.f, FMath::Sin(A) * 22.f, 22.f), FVector(0.03f, 0.03f, i % 3 == 0 ? 0.3f : 0.16f),
				Secondary, FRotator::ZeroRotator, Rig.Head, 2.f);
		}
		break;
	}
	}

	// Era headgear on the normal archetypes.
	if (Rig.Head && !bIsBoss)
	{
		switch (Info.Biome)
		{
		case EArpgBiome::Forest:    AddPart(EArpgShape::Cone, FVector(-2, 0, 10), FVector(0.3f, 0.3f, 0.32f), Secondary * 0.8f, FRotator(-10, 0, 0), Rig.Head); break;
		case EArpgBiome::Jungle:
			for (int32 i = -2; i <= 2; ++i)
			{
				AddPart(EArpgShape::Cone, FVector(-6, i * 5.f, 16), FVector(0.04f, 0.04f, 0.3f), FMath::Lerp(Accent, FLinearColor::Red, (i + 2) / 4.f),
					FRotator(-15, 0, i * 12.f), Rig.Head);
			}
			break;
		case EArpgBiome::Wasteland: AddPart(EArpgShape::Cube, FVector(4, 0, -2), FVector(0.24f, 0.26f, 0.08f), Accent * 0.5f, FRotator::ZeroRotator, Rig.Head); break;
		case EArpgBiome::Flood:     AddPart(EArpgShape::Cylinder, FVector(0, 0, 11), FVector(0.4f, 0.4f, 0.05f), Primary * 0.6f, FRotator::ZeroRotator, Rig.Head); break;
		case EArpgBiome::IceLands:
			AddPart(EArpgShape::Sphere, FVector(0, 0, 5), FVector(0.27f, 0.25f, 0.22f), FLinearColor(0.5f, 0.52f, 0.55f), FRotator::ZeroRotator, Rig.Head);
			AddPart(EArpgShape::Cone, FVector(0, -12, 14), FVector(0.06f, 0.06f, 0.2f), FLinearColor(0.9f, 0.88f, 0.8f), FRotator(0, 0, -35), Rig.Head);
			AddPart(EArpgShape::Cone, FVector(0, 12, 14), FVector(0.06f, 0.06f, 0.2f), FLinearColor(0.9f, 0.88f, 0.8f), FRotator(0, 0, 35), Rig.Head);
			break;
		default: break;
		}
	}

	// Magic and rare monsters glow so they read at a glance.
	if (Info.Rarity == EArpgMonsterRarity::Magic || Info.Rarity == EArpgMonsterRarity::Rare || bIsBoss)
	{
		const FLinearColor Glow = GetNameColor();
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(VisualRoot);
		L->SetRelativeLocation(FVector(0, 0, 120));
		L->SetLightColor(Glow);
		L->SetIntensity(bIsBoss ? 8000.f : 3000.f);
		L->SetAttenuationRadius(bIsBoss ? 700.f : 350.f);
		L->SetCastShadows(false);
		L->RegisterComponent();
		if (Rig.Chest)
		{
			AddPart(EArpgShape::Cylinder, FVector(0, 0, 70), FVector(0.55f, 0.55f, 0.03f), Glow, FRotator::ZeroRotator, Rig.Chest, 4.f);
		}
	}
}

// ---------------------------------------------------------------- AI

void AArpgEnemyCharacter::LoseTrack(float Seconds)
{
	if (bIsBoss) { return; }
	UnawareRemaining = FMath::Max(UnawareRemaining, Seconds);
	if (bBusy)
	{
		CancelActions();
		EndAttack(0.2f);
	}
}

void AArpgEnemyCharacter::Alert()
{
	bAggro = true;
}

void AArpgEnemyCharacter::Telegraph(bool bUnblockable, float Seconds)
{
	TelegraphUnblockable = bUnblockable;
	FlashParts(bUnblockable ? FLinearColor(1.f, 0.05f, 0.05f) : FLinearColor(1.f, 0.55f, 0.1f), Seconds, bUnblockable ? 4.f : 1.5f);
}

void AArpgEnemyCharacter::FaceTarget(AArpgCharacterBase* Target, float Dt, float Rate)
{
	if (!Target) { return; }
	const FRotator Want = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Want.Yaw, 0.f), Dt, Rate));
}

void AArpgEnemyCharacter::MoveToward(const FVector& Goal, float Scale)
{
	FVector Dir = Goal - GetActorLocation();
	Dir.Z = 0.f;
	if (Dir.SizeSquared() < 100.f) { return; }
	AddMovementInput(Dir.GetSafeNormal(), Scale);
}

void AArpgEnemyCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bDead) { return; }

	// Fell out of the level (thrown off an edge): falls over 8 m kill normal enemies (GDD).
	if (GetActorLocation().Z < SpawnZ - 1500.f)
	{
		if (bIsBoss) { SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, SpawnZ + 200.f)); }
		else { Kill(Thrower.Get()); return; }
	}

	if (LevitateRemaining > 0.f)
	{
		LevitateRemaining -= Dt;
		if (LevitateRemaining <= 0.f)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			UpdateMoveSpeed();
		}
		return;
	}

	if (bIsGrabbed) { return; }

	if (bThrown)
	{
		ThrownTime += Dt;
		AArpgCharacterBase* By = Thrower.Get();
		for (AArpgCharacterBase* Other : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 140.f, EArpgTeam::Player))
		{
			if (Other == this || ThrowVictims.Contains(Other)) { continue; }
			ThrowVictims.Add(Other);
			FArpgHit Bump = ThrowHit;
			Bump.Knockback = GetVelocity().GetSafeNormal2D() * 600.f + FVector(0, 0, 300);
			if (By) { By->DealHit(Other, Bump); }
		}
		if (ThrownTime > 0.2f && !GetCharacterMovement()->IsFalling())
		{
			bThrown = false;
			ThrowHit.Knockback = FVector::ZeroVector;
			if (By) { By->DealHit(this, ThrowHit); }
			if (bThrowExplodes)
			{
				FArpgHit Boom;
				Boom.Damage[(int32)EArpgDamageType::Fire] = 50.f;
				Boom.PoiseDamage = 20.f;
				Boom.IgniteChance = 0.5f;
				for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 300.f, EArpgTeam::Player))
				{
					if (By) { By->DealHit(T, Boom); }
				}
				AArpgAreaEffect::SpawnFlash(GetWorld(), GetActorLocation(), 300.f, ArpgColors::Damage(EArpgDamageType::Fire));
			}
			ApplyStun(0.8f);
		}
		return;
	}

	// Unblockable charge in progress.
	if (ChargeRemaining > 0.f)
	{
		ChargeRemaining -= Dt;
		AddMovementInput(GetActorForwardVector(), 1.f);
		if (!bChargeHit)
		{
			const FVector Fwd = GetActorForwardVector();
			for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), 120.f * Arch.Scale, Team, &Fwd, 160.f))
			{
				FArpgHit Hit = ChargeHit;
				Hit.Knockback = Fwd * 900.f + FVector(0, 0, 350);
				DealHit(T, Hit);
				bChargeHit = true;
			}
		}
		if (ChargeRemaining <= 0.f)
		{
			GetCharacterMovement()->MaxWalkSpeed = GetBaseMoveSpeed();
			GetCharacterMovement()->MaxAcceleration = 2048.f;
		}
		return;
	}

	if (IsIncapacitated())
	{
		if (bHasToken) { if (AArpgGameMode* GM = GetGM()) { GM->ReleaseAttackToken(this); } bHasToken = false; }
		return;
	}

	UnawareRemaining = FMath::Max(0.f, UnawareRemaining - Dt);
	AttackCooldown -= Dt;
	for (TPair<int32, float>& C : AttackCooldowns) { C.Value -= Dt; }

	AArpgCharacterBase* Target = GetTarget();
	if (!Target || UnawareRemaining > 0.f)
	{
		if (bHasToken) { if (AArpgGameMode* GM = GetGM()) { GM->ReleaseAttackToken(this); } bHasToken = false; }
		return;
	}

	const float Dist = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
	if (!bAggro)
	{
		ThinkTimer -= Dt;
		if (ThinkTimer <= 0.f)
		{
			ThinkTimer = 0.3f;
			if (Dist < Arch.AggroRange && ArpgCombat::HasLineOfSight(GetWorld(), GetActorLocation() + FVector(0, 0, 50), Target->GetActorLocation()))
			{
				bAggro = true;
				// Archers alert the pack (GDD).
				const float AlertRadius = Info.Archetype == EArpgArchetype::Archer ? 2000.f : 900.f;
				for (AArpgCharacterBase* Ally : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), AlertRadius, EArpgTeam::Player))
				{
					if (AArpgEnemyCharacter* E = Cast<AArpgEnemyCharacter>(Ally)) { E->Alert(); }
				}
			}
		}
		return;
	}

	UpdatePhase();

	if (bBusy)
	{
		FaceTarget(Target, Dt, 5.f);
		return;
	}

	if (AttackCooldown <= 0.f)
	{
		if (const FArpgEnemyAttackDef* Atk = ChooseAttack(Dist))
		{
			const bool bRanged = Arch.PreferredRange > 0.f;
			AArpgGameMode* GM = GetGM();
			if (bIsBoss || bHasToken || (GM && GM->RequestAttackToken(this, bRanged)))
			{
				bHasToken = !bIsBoss;
				StartAttack(*Atk);
				return;
			}
		}
	}

	ThinkMove(Dt, Target);
}

const FArpgEnemyAttackDef* AArpgEnemyCharacter::ChooseAttack(float Dist)
{
	float Total = 0.f;
	TArray<int32> Options;
	for (int32 i = 0; i < Arch.Attacks.Num(); ++i)
	{
		const FArpgEnemyAttackDef& A = Arch.Attacks[i];
		if (A.MinPhase > Phase) { continue; }
		if (A.Kind == EAtk::Summon && bSummoned) { continue; }
		if (const float* Cd = AttackCooldowns.Find(i)) { if (*Cd > 0.f) { continue; } }
		const float Reach = A.MaxRange + GetSimpleCollisionRadius();
		if (Dist < A.MinRange || Dist > Reach) { continue; }
		Options.Add(i);
		Total += A.Weight;
	}
	if (Options.Num() == 0) { return nullptr; }
	float R = FMath::FRand() * Total;
	for (int32 i : Options)
	{
		R -= Arch.Attacks[i].Weight;
		if (R <= 0.f)
		{
			AttackCooldowns.Add(i, Arch.Attacks[i].Cooldown);
			return &Arch.Attacks[i];
		}
	}
	AttackCooldowns.Add(Options.Last(), Arch.Attacks[Options.Last()].Cooldown);
	return &Arch.Attacks[Options.Last()];
}

void AArpgEnemyCharacter::ThinkMove(float Dt, AArpgCharacterBase* Target)
{
	const FVector Me = GetActorLocation();
	const FVector Them = Target->GetActorLocation();
	const float Dist = FVector::Dist2D(Me, Them);
	const bool bLOS = ArpgCombat::HasLineOfSight(GetWorld(), Me + FVector(0, 0, 40), Them);

	FVector Goal = Them;
	if (!bLOS)
	{
		if (AArpgGameMode* GM = GetGM())
		{
			if (AArpgFloorGenerator* Floor = GM->GetFloor()) { Goal = Floor->GetWaypoint(Me, Them); }
		}
		MoveToward(Goal);
		return;
	}

	StrafeTimer -= Dt;
	if (StrafeTimer <= 0.f) { StrafeTimer = FMath::FRandRange(1.2f, 2.5f); StrafeSign *= -1.f; }
	const FVector ToThem = (Them - Me).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(ToThem, FVector::UpVector) * StrafeSign;

	if (Arch.PreferredRange > 0.f)
	{
		if (Dist < Arch.PreferredRange * 0.5f) { MoveToward(Me - ToThem * 300.f + Side * 150.f); }
		else if (Dist > Arch.PreferredRange + 250.f) { MoveToward(Them); }
		else { MoveToward(Me + Side * 200.f, 0.6f); }
		return;
	}

	// Melee: close in, or circle at a distance while another enemy holds the attack token.
	const bool bWaitingForToken = !bIsBoss && !bHasToken && AttackCooldown <= 0.f && Dist < 500.f;
	if (bWaitingForToken) { MoveToward(Me + Side * 200.f - ToThem * (Dist < 350.f ? 120.f : 0.f), 0.5f); }
	else if (Dist > 150.f + GetSimpleCollisionRadius()) { MoveToward(Them); }
}

FArpgHit AArpgEnemyCharacter::MakeHit(float DamageMult, float PoiseDmg, bool bUnblockable) const
{
	FArpgHit Hit;
	const float Base = Arch.Hit10 * ArpgData::DamageMultiplier(Level) / ArpgData::DamageMultiplier(10) * DamageMult * FMath::FRandRange(0.85f, 1.15f);
	const float Elem = Base * ElementShare;
	Hit.Damage[(int32)EArpgDamageType::Physical] = Base - Elem;
	Hit.Damage[(int32)Element] += Elem;
	if (Info.Mods.Contains(EArpgMonsterMod::FireEnchanted)) { Hit.Damage[(int32)EArpgDamageType::Fire] += Base * 0.3f; }
	Hit.PoiseDamage = PoiseDmg;
	Hit.bUnblockable = bUnblockable;
	Hit.HitstopSeconds = DamageMult >= 1.3f ? 0.1f : 0.06f;
	Hit.SourceLevel = Level;
	Hit.IgniteChance = Element == EArpgDamageType::Fire ? 0.15f : 0.f;
	Hit.ShockChance = Element == EArpgDamageType::Lightning ? 0.15f : 0.f;
	Hit.PoisonStacks = Element == EArpgDamageType::Chaos ? 1 : 0;
	return Hit;
}

void AArpgEnemyCharacter::MeleeStrike(float Range, float Arc, const FArpgHit& Hit)
{
	if (bDead || IsIncapacitated()) { return; }
	const FVector Fwd = GetActorForwardVector();
	for (AArpgCharacterBase* T : ArpgCombat::FindTargets(GetWorld(), GetActorLocation(), Range + GetSimpleCollisionRadius(), Team, &Fwd, Arc))
	{
		FArpgHit H = Hit;
		H.Knockback = Fwd * (bIsBoss || Arch.Scale > 1.4f ? 500.f : 200.f);
		DealHit(T, H);
	}
}

void AArpgEnemyCharacter::FireShot(AArpgCharacterBase* Target, const FArpgHit& Hit, float Speed, float SpreadDeg)
{
	if (!Target || bDead) { return; }
	const FVector Start = GetActorLocation() + GetActorForwardVector() * 50.f + FVector(0, 0, 40.f * Arch.Scale);
	// Lead the target a little so strafing in a straight line doesn't dodge everything.
	const float T = FVector::Dist(Start, Target->GetActorLocation()) / Speed;
	const FVector Aim = Target->GetActorLocation() + Target->GetVelocity() * T * 0.5f;
	FVector Dir = (Aim - Start).GetSafeNormal();
	Dir = Dir.RotateAngleAxis(SpreadDeg, FVector::UpVector);

	FArpgProjectileParams PP;
	PP.Hit = Hit;
	PP.Velocity = Dir * Speed;
	PP.Radius = 30.f;
	PP.Shape = EArpgShape::Cube;
	PP.VisualScale = FVector(0.5f, 0.05f, 0.05f);
	PP.Color = Element == EArpgDamageType::Physical ? FLinearColor(0.85f, 0.75f, 0.55f) : ArpgColors::Damage(Element);
	PP.Glow = 2.f;
	PP.Lifetime = 2.5f;
	if (bIsBoss)
	{
		PP.Shape = EArpgShape::Sphere;
		PP.VisualScale = FVector(0.4f);
		PP.Color = ArpgColors::Damage(Element == EArpgDamageType::Physical ? EArpgDamageType::Lightning : Element);
		PP.Glow = 4.f;
		PP.Lifetime = 4.f;
	}
	AArpgProjectile::Launch(this, Start, PP);
}

void AArpgEnemyCharacter::EndAttack(float Recovery)
{
	After(FMath::Max(0.05f, Recovery), [this]()
	{
		bBusy = false;
		TelegraphUnblockable = false;
		AttackCooldown = bIsBoss ? FMath::FRandRange(0.3f, 0.8f) : FMath::FRandRange(0.6f, 1.4f);
		if (bHasToken)
		{
			if (AArpgGameMode* GM = GetGM()) { GM->ReleaseAttackToken(this); }
			bHasToken = false;
		}
	}, false);
}

void AArpgEnemyCharacter::StartAttack(const FArpgEnemyAttackDef& A)
{
	AArpgCharacterBase* Target = GetTarget();
	if (!Target) { return; }
	bBusy = true;
	const float Speed = SpeedMult * (bIsBoss && Phase > 1 ? 1.25f : 1.f);
	const float Windup = A.Windup / Speed;
	const float Recovery = A.Recovery / Speed;
	const FArpgHit Hit = MakeHit(A.DamageMult, A.Poise, A.Kind == EAtk::Charge);
	TWeakObjectPtr<AArpgCharacterBase> WeakTarget = Target;

	switch (A.Kind)
	{
	case EAtk::Swipe:
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 165.f, Windup, false, 0.f);
		After(Windup, [this, A, Hit]() { PlaySwing(165.f, 30.f, 0.12f); MeleeStrike(A.MaxRange, 120.f, Hit); });
		EndAttack(Windup + Recovery);
		break;

	case EAtk::Combo3:
		for (int32 i = 0; i < 3; ++i)
		{
			const float At = Windup + i * 0.38f / Speed;
			After(At - Windup * 0.6f, [this, Windup]() { Telegraph(false, Windup * 0.6f); PlaySwing(30.f, 160.f, Windup * 0.5f, false, 0.f); });
			After(At, [this, A, Hit, i]()
			{
				PlaySwing(160.f, 25.f, 0.1f);
				PlayBodyTwist(i % 2 == 0 ? 35.f : -35.f, 0.25f);
				FArpgHit H = Hit;
				if (i == 2) { H.Scale(1.3f); H.PoiseDamage *= 1.5f; }
				MeleeStrike(A.MaxRange, 120.f, H);
			});
		}
		EndAttack(Windup + 0.76f / Speed + Recovery);
		break;

	case EAtk::Lunge:
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 150.f, Windup, false, 0.f);
		After(Windup, [this, WeakTarget]()
		{
			if (AArpgCharacterBase* T = WeakTarget.Get())
			{
				const FVector Dir = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				SetActorRotation(Dir.Rotation());
				LaunchCharacter(Dir * 1300.f + FVector(0, 0, 180), true, true);
			}
		});
		After(Windup + 0.28f, [this, A, Hit]() { PlaySwing(150.f, 20.f, 0.1f); MeleeStrike(A.MaxRange * 0.5f, 130.f, Hit); });
		EndAttack(Windup + 0.3f + Recovery);
		break;

	case EAtk::Shot:
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 90.f, Windup * 0.5f, true, Windup);
		After(Windup, [this, WeakTarget, Hit]() { FireShot(WeakTarget.Get(), Hit, 2000.f); });
		EndAttack(Windup + Recovery);
		break;

	case EAtk::Volley:
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 110.f, Windup * 0.5f, true, Windup);
		After(Windup, [this, WeakTarget, Hit]()
		{
			const int32 N = bIsBoss ? 9 : 5;
			for (int32 i = 0; i < N; ++i) { FireShot(WeakTarget.Get(), Hit, 1700.f, (i - (N - 1) * 0.5f) * (bIsBoss ? 9.f : 10.f)); }
		});
		EndAttack(Windup + Recovery);
		break;

	case EAtk::Slam:
	{
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 170.f, Windup * 0.8f, false, Windup * 0.2f);
		PlaySwing(RestArmPitch, 170.f, Windup * 0.8f, true, Windup * 0.2f);
		FArpgAreaParams AP;
		AP.Kind = EArpgAreaKind::Burst;
		AP.Radius = 220.f * Arch.Scale;
		AP.Delay = Windup;
		AP.Hit = Hit;
		AP.Hit.Knockback = FVector(400, 0, 0);
		AP.Color = FLinearColor(1.f, 0.35f, 0.05f);
		AArpgAreaEffect::Spawn(this, GetActorLocation() + GetActorForwardVector() * (160.f * Arch.Scale), AP);
		After(Windup, [this]() { PlaySwing(170.f, 20.f, 0.1f); PlaySwing(170.f, 20.f, 0.1f, true); });
		EndAttack(Windup + Recovery);
		break;
	}

	case EAtk::Stomp:
	{
		Telegraph(false, Windup);
		FArpgAreaParams AP;
		AP.Kind = EArpgAreaKind::Burst;
		AP.Radius = A.MaxRange;
		AP.Delay = Windup;
		AP.Hit = Hit;
		AP.Hit.Knockback = FVector(500, 0, 0);
		AP.Color = FLinearColor(1.f, 0.4f, 0.05f);
		AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
		After(Windup * 0.5f, [this]() { Jump(); });
		EndAttack(Windup + Recovery);
		break;
	}

	case EAtk::Charge:
		Telegraph(true, Windup);
		ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 150), TEXT("!"), FLinearColor::Red, 2.f);
		After(Windup, [this, WeakTarget, Hit]()
		{
			if (AArpgCharacterBase* T = WeakTarget.Get())
			{
				SetActorRotation((T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation());
			}
			ChargeHit = Hit;
			bChargeHit = false;
			ChargeRemaining = 0.75f;
			GetCharacterMovement()->MaxWalkSpeed = 1500.f;
			GetCharacterMovement()->MaxAcceleration = 8000.f;
		});
		EndAttack(Windup + 0.75f + Recovery);
		break;

	case EAtk::Shockwave:
	{
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 170.f, Windup, false, 0.f);
		PlaySwing(RestArmPitch, 170.f, Windup, true, 0.f);
		After(Windup, [this, A, Hit]()
		{
			PlaySwing(170.f, 10.f, 0.1f); PlaySwing(170.f, 10.f, 0.1f, true);
			FArpgAreaParams AP;
			AP.Kind = EArpgAreaKind::Shockwave;
			AP.Radius = A.MaxRange;
			AP.Duration = 1.1f;
			AP.Hit = Hit;
			AP.Color = Element == EArpgDamageType::Physical ? FLinearColor(1.f, 0.6f, 0.2f) : ArpgColors::Damage(Element);
			AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
		});
		EndAttack(Windup + Recovery);
		break;
	}

	case EAtk::RingBurst:
		Telegraph(false, Windup);
		for (int32 i = 0; i < 3; ++i)
		{
			After(Windup + i * 0.7f, [this, A, Hit]()
			{
				FArpgAreaParams AP;
				AP.Kind = EArpgAreaKind::Shockwave;
				AP.Radius = A.MaxRange;
				AP.Duration = 1.4f;
				AP.Hit = Hit;
				AP.Color = FLinearColor(0.3f, 0.6f, 1.f);
				AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
			});
		}
		EndAttack(Windup + 1.4f + Recovery);
		break;

	case EAtk::HourHand:
		Telegraph(false, Windup);
		for (int32 i = 0; i < 24; ++i)
		{
			After(Windup + i * 0.05f, [this, Hit, i]()
			{
				const float Yaw = GetActorRotation().Yaw + i * 30.f;
				FArpgProjectileParams PP;
				PP.Hit = Hit;
				PP.Velocity = FRotator(0.f, Yaw, 0.f).Vector() * 1100.f;
				PP.Radius = 40.f;
				PP.VisualScale = FVector(0.35f);
				PP.Color = FLinearColor(0.85f, 0.7f, 0.3f);
				PP.Glow = 4.f;
				PP.Lifetime = 3.f;
				AArpgProjectile::Launch(this, GetActorLocation() + FVector(0, 0, 40), PP);
			});
		}
		EndAttack(Windup + 1.2f + Recovery);
		break;

	case EAtk::Leap:
	{
		const FVector Dest = AArpgAreaEffect::GroundPoint(GetWorld(), Target->GetActorLocation());
		Telegraph(false, Windup);
		FArpgAreaParams AP;
		AP.Kind = EArpgAreaKind::Burst;
		AP.Radius = 320.f;
		AP.Delay = Windup + 0.65f;
		AP.Hit = Hit;
		AP.Hit.Knockback = FVector(600, 0, 0);
		AP.Color = FLinearColor(1.f, 0.25f, 0.05f);
		AArpgAreaEffect::Spawn(this, Dest, AP);
		After(Windup, [this, Dest]()
		{
			FVector Vel;
			const FVector Start = GetActorLocation();
			if (UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, Vel, Start, Dest + FVector(0, 0, GetSimpleCollisionHalfHeight()),
				GetCharacterMovement()->GetGravityZ(), 0.45f))
			{
				LaunchCharacter(Vel, true, true);
			}
			PlaySwing(RestArmPitch, 170.f, 0.4f, false, 0.25f);
		});
		EndAttack(Windup + 0.65f + Recovery);
		break;
	}

	case EAtk::SlowDome:
	{
		Telegraph(false, Windup);
		After(Windup, [this, A]()
		{
			FArpgAreaParams AP;
			AP.Kind = EArpgAreaKind::SlowField;
			AP.Radius = 600.f;
			AP.Duration = 5.f;
			AP.SlowFactor = 0.4f;
			AP.Color = FLinearColor(0.75f, 0.6f, 0.25f);
			AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
			if (AArpgGameMode* GM = GetGM()) { GM->Announce(TEXT("Time slows around the guardian - get out of the dome!"), FLinearColor(1.f, 0.85f, 0.4f)); }
		});
		EndAttack(Windup + Recovery);
		break;
	}

	case EAtk::Summon:
		Telegraph(false, Windup);
		PlaySwing(RestArmPitch, 175.f, Windup, false, 0.3f);
		After(Windup, [this]()
		{
			if (AArpgGameMode* GM = GetGM()) { GM->SpawnAdds(this, Info.Archetype == EArpgArchetype::FinalBoss ? 3 : 2); }
		});
		EndAttack(Windup + Recovery);
		break;
	}
}

void AArpgEnemyCharacter::UpdatePhase()
{
	if (!bIsBoss) { return; }
	const float F = GetHealthFraction();
	int32 NewPhase = 1;
	if (Info.Archetype == EArpgArchetype::FinalBoss) { NewPhase = F < 0.33f ? 3 : (F < 0.66f ? 2 : 1); }
	else { NewPhase = F < 0.5f ? 2 : 1; }
	if (NewPhase <= Phase) { return; }
	Phase = NewPhase;
	UpdateMoveSpeed();
	if (AArpgGameMode* GM = GetGM())
	{
		const FString Msg = Info.Archetype == EArpgArchetype::FinalBoss
			? (Phase == 2 ? TEXT("The Hourless King unwinds the hours...") : TEXT("The King stops the clock. Every second is his."))
			: FString::Printf(TEXT("%s draws on the Veil Relic!"), *DisplayName);
		GM->Announce(Msg, FLinearColor(1.f, 0.5f, 0.2f));
	}
	// A roar that pushes the player back and opens the new phase.
	FArpgAreaParams AP;
	AP.Kind = EArpgAreaKind::Shockwave;
	AP.Radius = 700.f;
	AP.Duration = 0.8f;
	AP.Hit = MakeHit(0.5f, 20.f);
	AP.Color = Accent;
	AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
	Poise = MaxPoise;
	if (Info.Archetype == EArpgArchetype::FinalBoss && Phase == 3) { bUnstaggerable = true; }
}

// ---------------------------------------------------------------- reactions

bool AArpgEnemyCharacter::PreReceiveHit(FArpgHit& Hit, AArpgCharacterBase* Source)
{
	if (Source && Source->Team != Team && !bAggro) { bAggro = true; }

	const bool bCanReact = !IsIncapacitated() && !bIsGrabbed && !bThrown && UnawareRemaining <= 0.f;
	const bool bFromFront = Source && FVector::DotProduct(GetActorForwardVector(), (Source->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) > 0.3f;

	// Duelist: parries every third hit and ripostes (GDD). Heavy attacks and spells get through.
	if (Arch.bParries && bCanReact && Source && !Hit.bSpell && Hit.PoiseDamage < 15.f)
	{
		if (++HitsTaken % 3 == 0)
		{
			ArpgCombat::ShowNumber(GetWorld(), GetActorLocation() + FVector(0, 0, 110), TEXT("Parried"), FLinearColor(0.8f, 0.9f, 1.f), 1.f);
			FlashParts(FLinearColor(0.7f, 0.85f, 1.f), 0.15f, 3.f);
			CancelActions();
			bBusy = false;
			FArpgEnemyAttackDef Riposte;
			Riposte.Kind = EAtk::Swipe; Riposte.MaxRange = 220.f; Riposte.Windup = 0.18f; Riposte.Recovery = 0.4f; Riposte.DamageMult = 1.5f; Riposte.Poise = 15.f;
			StartAttack(Riposte);
			return false;
		}
	}

	// Shield Brute: 50% less damage from the front.
	if (Arch.bFrontBlock && bCanReact && bFromFront)
	{
		Hit.Scale(0.5f);
		Hit.PoiseDamage *= 0.5f;
	}
	return true;
}

void AArpgEnemyCharacter::OnHitLanded(AArpgCharacterBase* Target, const FArpgHit& Hit, float Dealt)
{
	if (Info.Mods.Contains(EArpgMonsterMod::Vampiric)) { Heal(Dealt * 0.1f, false); }
}

void AArpgEnemyCharacter::OnStaggered()
{
	bBusy = false;
	ChargeRemaining = 0.f;
	GetCharacterMovement()->MaxAcceleration = 2048.f;
	if (bHasToken) { if (AArpgGameMode* GM = GetGM()) { GM->ReleaseAttackToken(this); } bHasToken = false; }
	TintParts(FLinearColor(1.f, 0.9f, 0.4f), 0.35f, 0.5f);
	After(2.f, [this]() { if (!bDead && FreezeRemaining <= 0.f) { ResetTint(); } }, false);
}

void AArpgEnemyCharacter::OnDeath(AArpgCharacterBase* Killer)
{
	if (bHasToken) { if (AArpgGameMode* GM = GetGM()) { GM->ReleaseAttackToken(this); } bHasToken = false; }
	if (Info.Mods.Contains(EArpgMonsterMod::Volatile))
	{
		FArpgAreaParams AP;
		AP.Kind = EArpgAreaKind::Burst;
		AP.Radius = 300.f;
		AP.Delay = 2.f;
		AP.Hit = MakeHit(2.f, 20.f);
		AP.Hit.Damage[(int32)EArpgDamageType::Fire] += AP.Hit.Damage[(int32)EArpgDamageType::Physical];
		AP.Hit.Damage[(int32)EArpgDamageType::Physical] = 0.f;
		AP.Color = FLinearColor(1.f, 0.3f, 0.05f);
		AArpgAreaEffect::Spawn(this, GetActorLocation(), AP);
	}
	if (AArpgGameMode* GM = GetGM()) { GM->OnEnemyKilled(this, Killer); }
	Super::OnDeath(Killer);
}

// ---------------------------------------------------------------- grabs and throws

bool AArpgEnemyCharacter::CanBeGrabbed(const AActor* By) const
{
	if (bDead || bIsGrabbed || bThrown || Info.Archetype == EArpgArchetype::FinalBoss) { return false; }
	if (IsStaggered() || IsFrozen()) { return true; }
	if (bIsBoss || Info.Archetype == EArpgArchetype::Ogre) { return false; }
	const bool bBehind = By && FVector::DotProduct(GetActorForwardVector(), (By->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) < -0.3f;
	const bool bIronGrip = false; // passive tree keystone (later)
	return (bBehind && MaxPoise < 100.f) || bIronGrip || IsUnaware();
}

void AArpgEnemyCharacter::SetGrabbed(bool bGrabbed)
{
	bIsGrabbed = bGrabbed;
	CancelActions();
	bBusy = false;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bGrabbed ? ECR_Ignore : ECR_Block);
	GetCharacterMovement()->SetMovementMode(bGrabbed ? MOVE_Flying : MOVE_Falling);
	if (bGrabbed) { GetCharacterMovement()->StopMovementImmediately(); }
	UpdateMoveSpeed();
}

void AArpgEnemyCharacter::Throw(const FVector& Velocity, AArpgCharacterBase* InThrower, const FArpgHit& ImpactHit, bool bExplodes)
{
	SetGrabbed(false);
	bThrown = true;
	ThrownTime = 0.f;
	Thrower = InThrower;
	ThrowHit = ImpactHit;
	bThrowExplodes = bExplodes;
	ThrowVictims.Reset();
	ThrowVictims.Add(this);
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	LaunchCharacter(Velocity, true, true);
}

void AArpgEnemyCharacter::Levitate(float Seconds)
{
	if (bDead || bIsBoss) { return; }
	LevitateRemaining = Seconds;
	CancelActions();
	bBusy = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocation(GetActorLocation() + FVector(0, 0, 180.f), false);
	UpdateMoveSpeed();
}
