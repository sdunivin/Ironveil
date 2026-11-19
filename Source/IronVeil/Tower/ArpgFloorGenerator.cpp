#include "Tower/ArpgFloorGenerator.h"

#include "Core/ArpgAssets.h"
#include "Data/ArpgGameData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FIntPoint GDirs[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };

	int32 DirIndex(const FIntPoint& D)
	{
		for (int32 i = 0; i < 4; ++i) { if (GDirs[i] == D) { return i; } }
		return 0;
	}
}

AArpgFloorGenerator::AArpgFloorGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

// ---------------------------------------------------------------- pools

UInstancedStaticMeshComponent* AArpgFloorGenerator::Pool(UStaticMesh* Mesh, const FLinearColor* Color, float Emissive, bool bCollide)
{
	if (!Mesh) { return nullptr; }
	FString Key = Mesh->GetName();
	if (Color) { Key += FString::Printf(TEXT("_%08x_%d"), Color->ToFColor(false).DWColor(), FMath::RoundToInt(Emissive * 10.f)); }
	Key += bCollide ? TEXT("_C") : TEXT("_N");
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Pools.Find(Key)) { return *Found; }

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetStaticMesh(Mesh);
	ISM->SetupAttachment(Root);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetCanEverAffectNavigation(false);
	if (bCollide)
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ISM->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Color) { ISM->SetMaterial(0, ArpgAssets::SharedColor(*Color, Emissive)); }
	ISM->RegisterComponent();
	Pools.Add(Key, ISM);
	return ISM;
}

void AArpgFloorGenerator::AddShape(EArpgShape Shape, const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide,
	const FRotator& Rot, float Emissive)
{
	// Quantise colours a little so near-identical shades share a pool.
	const FLinearColor C(FMath::GridSnap(Color.R, 0.01f), FMath::GridSnap(Color.G, 0.01f), FMath::GridSnap(Color.B, 0.01f), 1.f);
	if (UInstancedStaticMeshComponent* ISM = Pool(ArpgAssets::Shape(Shape), &C, Emissive, bCollide))
	{
		ISM->AddInstance(FTransform(Rot, Center, Size / 100.f), true);
	}
}

void AArpgFloorGenerator::AddMesh(UStaticMesh* Mesh, const FTransform& Xf, bool bCollide)
{
	if (UInstancedStaticMeshComponent* ISM = Pool(Mesh, nullptr, 0.f, bCollide))
	{
		ISM->AddInstance(Xf, true);
	}
}

void AArpgFloorGenerator::AddRoomLight(const FVector& At, const FLinearColor& Color, float Intensity, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetMobility(EComponentMobility::Movable);
	L->SetWorldLocation(At);
	L->SetLightColor(Color);
	L->SetIntensity(Intensity);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->RegisterComponent();
	Lights.Add(L);
}

void AArpgFloorGenerator::Clear()
{
	for (auto& P : Pools) { if (P.Value) { P.Value->DestroyComponent(); } }
	Pools.Reset();
	for (UPointLightComponent* L : Lights) { if (L) { L->DestroyComponent(); } }
	Lights.Reset();
	Rooms.Reset();
	Corridors.Reset();
	PackSpots.Reset();
	ChestSpots.Reset();
	HuntRoom = INDEX_NONE;
}

// ---------------------------------------------------------------- build

void AArpgFloorGenerator::Build(int32 InFloor, int32 InSeed, EArpgBiome InBiome, EKind InKind)
{
	Clear();
	Floor = InFloor;
	Seed = InSeed;
	Biome = InBiome;
	Kind = InKind;
	Def = &ArpgData::Biome(Biome);
	FRandomStream R(Seed);

	switch (Kind)
	{
	case EKind::Normal:
		for (int32 Attempt = 0; Attempt < 30 && !PlanLayout(R); ++Attempt) {}
		break;
	case EKind::Guardian: PlanArena(R, 2000.f); break;
	case EKind::Final:    PlanArena(R, 2600.f); break;
	case EKind::Hub:      PlanHub(); break;
	}
	BuildGeometry(R);
}

bool AArpgFloorGenerator::PlanLayout(FRandomStream& R)
{
	Rooms.Reset();
	Corridors.Reset();
	TMap<FIntPoint, int32> Grid;

	auto AddRoom = [this, &Grid, &R](const FIntPoint& Cell, EArpgRoomType Type) -> int32
	{
		FArpgRoom Room;
		Room.Cell = Cell;
		Room.Type = Type;
		Room.Center = FVector(Cell.X * CellSize, Cell.Y * CellSize, 0.f);
		switch (Type)
		{
		case EArpgRoomType::Start: Room.Half = FVector2D(R.FRandRange(800.f, 1000.f), R.FRandRange(800.f, 1000.f)); break;
		case EArpgRoomType::Side:  Room.Half = FVector2D(R.FRandRange(700.f, 1000.f), R.FRandRange(700.f, 1000.f)); break;
		case EArpgRoomType::Exit:  Room.Half = FVector2D(R.FRandRange(1100.f, 1400.f), R.FRandRange(1100.f, 1400.f)); break;
		default:                   Room.Half = FVector2D(R.FRandRange(950.f, 1500.f), R.FRandRange(950.f, 1500.f)); break;
		}
		const int32 Index = Rooms.Add(Room);
		Grid.Add(Cell, Index);
		return Index;
	};
	auto Link = [this](int32 A, int32 B)
	{
		Rooms[A].Links.Add(B);
		Rooms[B].Links.Add(A);
		FArpgCorridor C;
		C.A = FMath::Min(A, B);
		C.B = FMath::Max(A, B);
		const FVector Dir = (Rooms[B].Center - Rooms[A].Center).GetSafeNormal();
		const bool bX = FMath::Abs(Dir.X) > 0.5f;
		const float EdgeA = bX ? Rooms[A].Half.X : Rooms[A].Half.Y;
		const float EdgeB = bX ? Rooms[B].Half.X : Rooms[B].Half.Y;
		const FVector PA = Rooms[A].Center + Dir * EdgeA;
		const FVector PB = Rooms[B].Center - Dir * EdgeB;
		C.Center = (PA + PB) * 0.5f;
		const float Len = FVector::Dist(PA, PB) * 0.5f;
		C.Half = bX ? FVector2D(Len, DoorHalf) : FVector2D(DoorHalf, Len);
		Corridors.Add(C);
	};

	// Main path: a random walk that prefers to keep going forward.
	const int32 MainLen = R.RandRange(5, 7) + FMath::Min(3, Floor / 25);
	FIntPoint Cell(0, 0);
	FIntPoint LastDir = GDirs[R.RandRange(0, 3)];
	int32 Prev = AddRoom(Cell, EArpgRoomType::Start);
	for (int32 i = 1; i < MainLen; ++i)
	{
		TArray<FIntPoint> Options;
		TArray<float> Weights;
		for (const FIntPoint& D : GDirs)
		{
			if (D == FIntPoint(-LastDir.X, -LastDir.Y) || Grid.Contains(Cell + D)) { continue; }
			Options.Add(D);
			Weights.Add(D == LastDir ? 3.f : 2.f);
		}
		if (Options.Num() == 0) { return false; }
		float Total = 0.f;
		for (float W : Weights) { Total += W; }
		float Pick = R.FRand() * Total;
		int32 Chosen = Options.Num() - 1;
		for (int32 k = 0; k < Options.Num(); ++k) { Pick -= Weights[k]; if (Pick <= 0.f) { Chosen = k; break; } }
		LastDir = Options[Chosen];
		Cell += LastDir;
		const int32 Room = AddRoom(Cell, i == MainLen - 1 ? EArpgRoomType::Exit : EArpgRoomType::Combat);
		Link(Prev, Room);
		Prev = Room;
	}

	// Side rooms with chests.
	const int32 Sides = R.RandRange(1, 3);
	for (int32 s = 0; s < Sides; ++s)
	{
		const int32 From = R.RandRange(1, MainLen - 2);
		const FIntPoint Base = Rooms[From].Cell;
		TArray<FIntPoint> Free;
		for (const FIntPoint& D : GDirs) { if (!Grid.Contains(Base + D)) { Free.Add(Base + D); } }
		if (Free.Num() == 0) { continue; }
		const int32 Side = AddRoom(Free[R.RandRange(0, Free.Num() - 1)], EArpgRoomType::Side);
		Link(From, Side);
	}

	// Spawn points
	const int32 StartRoom = 0;
	StartLocation = Rooms[StartRoom].Center + FVector(0, 0, 100);
	const FVector FirstDir = (Rooms[Rooms[StartRoom].Links[0]].Center - Rooms[StartRoom].Center).GetSafeNormal();
	StartRotation = FirstDir.Rotation();

	float FarDist = -1.f;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FArpgRoom& Room = Rooms[i];
		if (Room.Type == EArpgRoomType::Combat || Room.Type == EArpgRoomType::Exit)
		{
			const int32 Packs = Room.Type == EArpgRoomType::Exit ? 1 : R.RandRange(1, 2);
			for (int32 p = 0; p < Packs; ++p)
			{
				FArpgSpawnSpot S;
				S.Room = i;
				S.Location = RandomPointInRoom(i, R, 450.f);
				PackSpots.Add(S);
			}
			const float D = FVector::Dist(Room.Center, Rooms[StartRoom].Center);
			if (Room.Type == EArpgRoomType::Combat && D > FarDist) { FarDist = D; HuntRoom = i; }
		}
		else if (Room.Type == EArpgRoomType::Side)
		{
			FArpgSpawnSpot S;
			S.Room = i;
			S.Location = Room.Center + FVector(R.FRandRange(-200.f, 200.f), R.FRandRange(-200.f, 200.f), 0.f);
			ChestSpots.Add(S);
		}
	}

	// Exit portal: far side of the exit room, away from the door we came in by.
	const FArpgRoom& Exit = Rooms[MainLen - 1];
	const FVector InDir = (Exit.Center - Rooms[Exit.Links[0]].Center).GetSafeNormal();
	const float Reach = (FMath::Abs(InDir.X) > 0.5f ? Exit.Half.X : Exit.Half.Y) - 350.f;
	PortalLocation = Exit.Center + InDir * Reach;
	PortalRotation = (-InDir).Rotation();
	RelicLocation = Exit.Center;
	return true;
}

void AArpgFloorGenerator::PlanArena(FRandomStream& R, float ArenaHalf)
{
	FArpgRoom Start;
	Start.Cell = FIntPoint(0, 0);
	Start.Type = EArpgRoomType::Start;
	Start.Center = FVector::ZeroVector;
	Start.Half = FVector2D(800.f, 800.f);
	Rooms.Add(Start);

	FArpgRoom Arena;
	Arena.Cell = FIntPoint(1, 0);
	Arena.Type = EArpgRoomType::Arena;
	Arena.Center = FVector(800.f + 900.f + ArenaHalf, 0.f, 0.f);
	Arena.Half = FVector2D(ArenaHalf, ArenaHalf);
	Rooms.Add(Arena);
	Rooms[0].Links.Add(1);
	Rooms[1].Links.Add(0);

	FArpgCorridor C;
	C.A = 0; C.B = 1;
	C.Center = FVector(800.f + 450.f, 0.f, 0.f);
	C.Half = FVector2D(450.f, DoorHalf);
	Corridors.Add(C);

	StartLocation = FVector(0, 0, 100);
	StartRotation = FRotator::ZeroRotator;
	ArenaCenter = Arena.Center;
	BossLocation = Arena.Center + FVector(ArenaHalf * 0.45f, 0, 150.f);
	ShrineLocation = Arena.Center;
	PortalLocation = Arena.Center + FVector(ArenaHalf - 400.f, 0, 0);
	PortalRotation = FRotator(0.f, 180.f, 0.f);
	HuntRoom = 1;
}

void AArpgFloorGenerator::PlanHub()
{
	FArpgRoom Plaza;
	Plaza.Type = EArpgRoomType::Hub;
	Plaza.Center = FVector::ZeroVector;
	Plaza.Half = FVector2D(2600.f, 2600.f);
	Rooms.Add(Plaza);
	StartLocation = FVector(-1600.f, 0.f, 100.f);
	StartRotation = FRotator::ZeroRotator;
	PortalLocation = FVector(2200.f, 0.f, 0.f);
	PortalRotation = FRotator(0.f, 180.f, 0.f);
	ShrineLocation = FVector(-600.f, 900.f, 0.f);
}

FVector AArpgFloorGenerator::RandomPointInRoom(int32 Room, FRandomStream& Rng, float Margin) const
{
	if (!Rooms.IsValidIndex(Room)) { return StartLocation; }
	const FArpgRoom& Rm = Rooms[Room];
	const float HX = FMath::Max(50.f, Rm.Half.X - Margin);
	const float HY = FMath::Max(50.f, Rm.Half.Y - Margin);
	return Rm.Center + FVector(Rng.FRandRange(-HX, HX), Rng.FRandRange(-HY, HY), 100.f);
}

FVector AArpgFloorGenerator::RandomPointNear(const FVector& Center, float Radius, FRandomStream& Rng) const
{
	const int32 Room = RoomAt(Center, 50.f);
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FVector P = Center + FVector(Rng.FRandRange(-Radius, Radius), Rng.FRandRange(-Radius, Radius), 0.f);
		if (Room == INDEX_NONE || RoomAt(P, -250.f) == Room) { return FVector(P.X, P.Y, Center.Z + 100.f); }
	}
	return Center + FVector(0, 0, 100.f);
}

// ---------------------------------------------------------------- navigation

int32 AArpgFloorGenerator::RoomAt(const FVector& P, float Margin) const
{
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FArpgRoom& R = Rooms[i];
		if (FMath::Abs(P.X - R.Center.X) <= R.Half.X + Margin && FMath::Abs(P.Y - R.Center.Y) <= R.Half.Y + Margin) { return i; }
	}
	return INDEX_NONE;
}

int32 AArpgFloorGenerator::CorridorAt(const FVector& P) const
{
	for (int32 i = 0; i < Corridors.Num(); ++i)
	{
		const FArpgCorridor& C = Corridors[i];
		if (FMath::Abs(P.X - C.Center.X) <= C.Half.X + 60.f && FMath::Abs(P.Y - C.Center.Y) <= C.Half.Y + 60.f) { return i; }
	}
	return INDEX_NONE;
}

int32 AArpgFloorGenerator::PathNext(int32 From, int32 To) const
{
	if (From == To || !Rooms.IsValidIndex(From) || !Rooms.IsValidIndex(To)) { return To; }
	TArray<int32> Prev;
	Prev.Init(INDEX_NONE, Rooms.Num());
	TArray<int32> Queue = { From };
	Prev[From] = From;
	for (int32 q = 0; q < Queue.Num(); ++q)
	{
		const int32 Cur = Queue[q];
		if (Cur == To) { break; }
		for (int32 N : Rooms[Cur].Links)
		{
			if (Prev[N] == INDEX_NONE) { Prev[N] = Cur; Queue.Add(N); }
		}
	}
	if (Prev[To] == INDEX_NONE) { return To; }
	int32 Step = To;
	while (Prev[Step] != From) { Step = Prev[Step]; }
	return Step;
}

FVector AArpgFloorGenerator::DoorPoint(int32 Room, int32 Toward, float Inset) const
{
	const FArpgRoom& A = Rooms[Room];
	const FVector Dir = (Rooms[Toward].Center - A.Center).GetSafeNormal2D();
	const float Edge = FMath::Abs(Dir.X) > 0.5f ? A.Half.X : A.Half.Y;
	return A.Center + Dir * (Edge + Inset);
}

FVector AArpgFloorGenerator::GetWaypoint(const FVector& From, const FVector& To) const
{
	int32 Target = RoomAt(To, 100.f);
	if (Target == INDEX_NONE)
	{
		const int32 C = CorridorAt(To);
		if (C != INDEX_NONE)
		{
			const FArpgCorridor& Co = Corridors[C];
			Target = FVector::DistSquared(To, Rooms[Co.A].Center) < FVector::DistSquared(To, Rooms[Co.B].Center) ? Co.A : Co.B;
		}
	}
	if (Target == INDEX_NONE) { return To; }

	const int32 Here = RoomAt(From, -20.f);
	if (Here == INDEX_NONE)
	{
		// In a corridor: head out of whichever end leads toward the target.
		const int32 C = CorridorAt(From);
		if (C == INDEX_NONE) { return To; }
		const FArpgCorridor& Co = Corridors[C];
		const int32 NextA = PathNext(Co.A, Target);
		const bool bGoA = (Co.A == Target) || (Co.B != Target && NextA != Co.B);
		const int32 Into = bGoA ? Co.A : Co.B;
		const int32 Other = bGoA ? Co.B : Co.A;
		return DoorPoint(Into, Other, -250.f);
	}
	if (Here == Target) { return To; }
	const int32 Next = PathNext(Here, Target);
	if (Next == Here) { return To; }
	return DoorPoint(Here, Next, 250.f);
}

// ---------------------------------------------------------------- geometry

void AArpgFloorGenerator::BuildGeometry(FRandomStream& R)
{
	// Ground beneath and around everything.
	FBox Bounds(ForceInit);
	for (const FArpgRoom& Room : Rooms)
	{
		Bounds += Room.Center + FVector(Room.Half.X, Room.Half.Y, 0.f);
		Bounds += Room.Center - FVector(Room.Half.X, Room.Half.Y, 0.f);
	}
	const FVector Mid = Bounds.GetCenter();
	const FVector Ext = Bounds.GetExtent() + FVector(12000.f, 12000.f, 0.f);
	if (Def->bWater)
	{
		AddShape(EArpgShape::Cube, FVector(Mid.X, Mid.Y, -900.f), FVector(Ext.X * 2.f, Ext.Y * 2.f, 100.f), Def->Ground, true);
		AddShape(EArpgShape::Cube, FVector(Mid.X, Mid.Y, -70.f), FVector(Ext.X * 2.f, Ext.Y * 2.f, 4.f), FLinearColor(0.08f, 0.16f, 0.24f), false, FRotator::ZeroRotator, 0.2f);
	}
	else
	{
		AddShape(EArpgShape::Cube, FVector(Mid.X, Mid.Y, -60.f), FVector(Ext.X * 2.f, Ext.Y * 2.f, 100.f), Def->Ground, true);
	}

	for (int32 i = 0; i < Rooms.Num(); ++i) { BuildRoom(i, R); }
	for (const FArpgCorridor& C : Corridors) { BuildCorridor(C, R); }
	for (int32 i = 0; i < Rooms.Num(); ++i) { DressRoom(i, R); }
	if (Kind == EKind::Guardian || Kind == EKind::Final) { BuildArenaFeatures(R); }
	if (Kind == EKind::Hub) { BuildHubTown(R); }
	BuildBackdrop(R);
}

void AArpgFloorGenerator::BuildWallRun(const FVector& From, const FVector& To, float Thickness, FRandomStream& R, float MinH, float MaxH)
{
	const FVector Dir = (To - From).GetSafeNormal();
	const float Len = FVector::Dist(From, To);
	if (Len < 5.f) { return; }
	const FRotator Rot = Dir.Rotation();
	float Pos = 0.f;
	int32 Piece = 0;
	while (Pos < Len - 1.f)
	{
		const float Seg = FMath::Min(Len - Pos, R.FRandRange(300.f, 650.f));
		const float H = R.FRandRange(MinH, MaxH);
		const FVector C = From + Dir * (Pos + Seg * 0.5f);
		const FLinearColor Col = (Piece++ % 3 == 0) ? Def->WallAlt : Def->Wall;
		AddShape(EArpgShape::Cube, C + FVector(0, 0, H * 0.5f), FVector(Seg + 2.f, Thickness, H), Col, true, Rot);
		// Cap stones on taller pieces read as masonry.
		if (H > MinH + 100.f && R.FRand() < 0.5f)
		{
			AddShape(EArpgShape::Cube, C + FVector(0, 0, H + 15.f), FVector(Seg * 0.9f, Thickness + 30.f, 30.f), Col * 0.85f, true, Rot);
		}
		Pos += Seg;
	}
}

void AArpgFloorGenerator::BuildRoom(int32 Index, FRandomStream& R)
{
	const FArpgRoom& Room = Rooms[Index];
	const FVector C = Room.Center;
	const float HX = Room.Half.X, HY = Room.Half.Y;
	const float SlabDepth = Def->bWater ? 900.f : 40.f;

	// Floor slab and a scatter of alternate flagstones.
	AddShape(EArpgShape::Cube, C - FVector(0, 0, SlabDepth * 0.5f), FVector(HX * 2.f, HY * 2.f, SlabDepth), Def->Floor, true);
	const int32 Flags = FMath::RoundToInt(HX * HY / 250000.f);
	for (int32 f = 0; f < Flags; ++f)
	{
		const FVector P = C + FVector(R.FRandRange(-HX + 150.f, HX - 150.f), R.FRandRange(-HY + 150.f, HY - 150.f), 1.f);
		const float S = R.FRandRange(150.f, 380.f);
		AddShape(EArpgShape::Cube, P, FVector(S, S * R.FRandRange(0.6f, 1.2f), 4.f), Def->FloorAlt, false, FRotator(0, R.FRandRange(0.f, 90.f), 0));
	}

	// Walls with doorways on linked sides.
	const bool bArena = Room.Type == EArpgRoomType::Arena;
	const float MinH = bArena ? Def->WallHeightMax * 0.8f : Def->WallHeightMin;
	const float MaxH = bArena ? Def->WallHeightMax * 1.3f : Def->WallHeightMax;
	const float T = WallThick;
	for (int32 d = 0; d < 4; ++d)
	{
		bool bDoor = false;
		for (int32 L : Room.Links)
		{
			const FVector Delta = Rooms[L].Center - C;
			const FIntPoint Dir(FMath::RoundToInt(FMath::Sign(FMath::Abs(Delta.X) > FMath::Abs(Delta.Y) ? Delta.X : 0.f)),
				FMath::RoundToInt(FMath::Sign(FMath::Abs(Delta.Y) >= FMath::Abs(Delta.X) ? Delta.Y : 0.f)));
			if (DirIndex(Dir) == d && (Dir.X != 0 || Dir.Y != 0)) { bDoor = true; }
		}
		if (Room.Type == EArpgRoomType::Hub && d == 0) { bDoor = false; }
		const FIntPoint D = GDirs[d];
		const bool bAlongY = D.X != 0;   // walls on the +/-X sides run along Y
		const float Edge = bAlongY ? HX : HY;
		const float Span = bAlongY ? HY : HX;
		const FVector WallCenter = C + FVector(D.X * (Edge + T * 0.5f), D.Y * (Edge + T * 0.5f), 0.f);
		const FVector Axis = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
		const float Full = Span + T;
		if (bDoor)
		{
			BuildWallRun(WallCenter - Axis * Full, WallCenter - Axis * DoorHalf, T, R, MinH, MaxH);
			BuildWallRun(WallCenter + Axis * DoorHalf, WallCenter + Axis * Full, T, R, MinH, MaxH);
			// Door lintel
			AddShape(EArpgShape::Cube, WallCenter + FVector(0, 0, 360.f), FVector(bAlongY ? T + 20.f : DoorHalf * 2.f + 40.f, bAlongY ? DoorHalf * 2.f + 40.f : T + 20.f, 50.f),
				Def->WallAlt, true);
			AddShape(EArpgShape::Cube, WallCenter - Axis * (DoorHalf + 30.f) + FVector(0, 0, 180.f), FVector(bAlongY ? T + 40.f : 60.f, bAlongY ? 60.f : T + 40.f, 360.f), Def->WallAlt * 0.9f, true);
			AddShape(EArpgShape::Cube, WallCenter + Axis * (DoorHalf + 30.f) + FVector(0, 0, 180.f), FVector(bAlongY ? T + 40.f : 60.f, bAlongY ? 60.f : T + 40.f, 360.f), Def->WallAlt * 0.9f, true);
		}
		else
		{
			BuildWallRun(WallCenter - Axis * Full, WallCenter + Axis * Full, T, R, MinH, MaxH);
		}
	}
}

void AArpgFloorGenerator::BuildCorridor(const FArpgCorridor& C, FRandomStream& R)
{
	const float SlabDepth = Def->bWater ? 900.f : 40.f;
	AddShape(EArpgShape::Cube, C.Center - FVector(0, 0, SlabDepth * 0.5f), FVector(C.Half.X * 2.f, C.Half.Y * 2.f, SlabDepth), Def->FloorAlt, true);
	const bool bAlongX = C.Half.X > C.Half.Y;
	const float Len = bAlongX ? C.Half.X : C.Half.Y;
	const FVector Axis = bAlongX ? FVector(1, 0, 0) : FVector(0, 1, 0);
	const FVector Side = bAlongX ? FVector(0, 1, 0) : FVector(1, 0, 0);
	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector WC = C.Center + Side * s * (DoorHalf + WallThick * 0.5f);
		BuildWallRun(WC - Axis * Len, WC + Axis * Len, WallThick, R, FMath::Max(300.f, Def->WallHeightMin), Def->WallHeightMin + 200.f);
	}
	// A torch halfway along.
	AddRoomLight(C.Center + FVector(0, 0, 250.f), FMath::Lerp(Def->Accent, FLinearColor(1.f, 0.6f, 0.3f), 0.5f), 2500.f, 900.f);
}

TArray<AArpgFloorGenerator::EProp> AArpgFloorGenerator::PropsFor(EArpgBiome B) const
{
	switch (B)
	{
	case EArpgBiome::Forest:        return { EProp::Tree, EProp::Tree, EProp::Rock, EProp::BrokenPillar, EProp::RuinArch, EProp::Crystal, EProp::Fern, EProp::Platform };
	case EArpgBiome::Jungle:        return { EProp::JungleTree, EProp::JungleTree, EProp::Fern, EProp::Fern, EProp::StoneHead, EProp::BrokenPillar, EProp::Rock, EProp::Platform };
	case EArpgBiome::Wasteland:     return { EProp::Car, EProp::Car, EProp::Barrier, EProp::Rubble, EProp::StreetLamp, EProp::NeonSign, EProp::Barrel, EProp::Platform };
	case EArpgBiome::Flood:         return { EProp::Crate, EProp::PierPosts, EProp::Boat, EProp::BrokenPillar, EProp::Rock, EProp::Barrel, EProp::StreetLamp };
	case EArpgBiome::IceLands:      return { EProp::IceSpike, EProp::IceSpike, EProp::SnowRock, EProp::FrozenPillar, EProp::Crystal, EProp::Pine, EProp::Platform };
	case EArpgBiome::FracturedTime: return { EProp::BrokenPillar, EProp::Car, EProp::Crystal, EProp::Rubble, EProp::Tree };
	default:                        return { EProp::Tree, EProp::Rock };
	}
}

const TCHAR* AArpgFloorGenerator::PropName(EProp P)
{
	switch (P)
	{
	case EProp::Tree: return TEXT("Tree"); case EProp::Pine: return TEXT("Pine"); case EProp::JungleTree: return TEXT("JungleTree");
	case EProp::Fern: return TEXT("Fern"); case EProp::Rock: return TEXT("Rock"); case EProp::BrokenPillar: return TEXT("BrokenPillar");
	case EProp::RuinArch: return TEXT("RuinArch"); case EProp::Crystal: return TEXT("Crystal"); case EProp::StoneHead: return TEXT("StoneHead");
	case EProp::Car: return TEXT("Car"); case EProp::Barrier: return TEXT("Barrier"); case EProp::Rubble: return TEXT("Rubble");
	case EProp::StreetLamp: return TEXT("StreetLamp"); case EProp::NeonSign: return TEXT("NeonSign"); case EProp::Barrel: return TEXT("Barrel");
	case EProp::Crate: return TEXT("Crate"); case EProp::PierPosts: return TEXT("PierPosts"); case EProp::Boat: return TEXT("Boat");
	case EProp::IceSpike: return TEXT("IceSpike"); case EProp::SnowRock: return TEXT("SnowRock"); case EProp::FrozenPillar: return TEXT("FrozenPillar");
	case EProp::House: return TEXT("House"); case EProp::Stall: return TEXT("Stall"); case EProp::Well: return TEXT("Well");
	case EProp::Platform: return TEXT("Platform");
	}
	return TEXT("Prop");
}

void AArpgFloorGenerator::DressRoom(int32 Index, FRandomStream& R)
{
	const FArpgRoom& Room = Rooms[Index];
	if (Room.Type == EArpgRoomType::Hub) { return; }
	const TArray<EProp> Kit = PropsFor(Biome);
	const float Area = Room.Half.X * Room.Half.Y;
	int32 Count = FMath::Clamp(FMath::RoundToInt(Area / 260000.f), 2, 12);
	if (Room.Type == EArpgRoomType::Arena) { Count = 6; }
	if (Room.Type == EArpgRoomType::Start) { Count = 3; }

	for (int32 i = 0; i < Count; ++i)
	{
		for (int32 Try = 0; Try < 6; ++Try)
		{
			const FVector Local(R.FRandRange(-Room.Half.X + 220.f, Room.Half.X - 220.f), R.FRandRange(-Room.Half.Y + 220.f, Room.Half.Y - 220.f), 0.f);
			// Keep the lanes between doors (a plus shape through the centre) and the arena middle clear.
			if (FMath::Abs(Local.X) < DoorHalf + 150.f || FMath::Abs(Local.Y) < DoorHalf + 150.f) { continue; }
			if (Room.Type == EArpgRoomType::Arena && Local.Size2D() < Room.Half.X * 0.55f) { continue; }
			AddProp(Kit[R.RandRange(0, Kit.Num() - 1)], Room.Center + Local, R.FRandRange(0.f, 360.f), R.FRandRange(0.8f, 1.3f), R);
			break;
		}
	}
	// One mood light per room in the biome accent.
	AddRoomLight(Room.Center + FVector(R.FRandRange(-300.f, 300.f), R.FRandRange(-300.f, 300.f), 450.f), FMath::Lerp(Def->Accent, FLinearColor::White, 0.4f),
		Room.Type == EArpgRoomType::Arena ? 12000.f : 4000.f, FMath::Max(Room.Half.X, Room.Half.Y) * 1.6f);
}

void AArpgFloorGenerator::AddProp(EProp Type, const FVector& Loc, float Yaw, float S, FRandomStream& R)
{
	// Imported Blender props win over gray-box shapes.
	if (UStaticMesh* Mesh = ArpgAssets::OptionalMesh(FString::Printf(TEXT("/Game/_Game/Art/Props/SM_Prop_%s.SM_Prop_%s"), PropName(Type), PropName(Type))))
	{
		AddMesh(Mesh, FTransform(FRotator(0.f, Yaw + ArpgAssets::ImportedMeshYaw, 0.f), Loc, FVector(S)), Type != EProp::Fern);
		return;
	}

	const FRotator Rot(0.f, Yaw, 0.f);
	auto P = [&](float X, float Y, float Z) { return Loc + Rot.RotateVector(FVector(X, Y, Z) * S); };
	auto Box = [&](EArpgShape Shape, float X, float Y, float Z, float SX, float SY, float SZ, const FLinearColor& Col, bool bCollide = true,
		const FRotator& Extra = FRotator::ZeroRotator, float Emissive = 0.f)
	{
		AddShape(Shape, P(X, Y, Z), FVector(SX, SY, SZ) * S, Col, bCollide, FRotator(Rot.Quaternion() * Extra.Quaternion()), Emissive);
	};
	const FLinearColor Bark(0.25f, 0.17f, 0.1f), Stone = Def->Wall, Rust(0.45f, 0.22f, 0.1f), Ice(0.62f, 0.82f, 0.95f), Snow(0.92f, 0.94f, 0.97f);

	switch (Type)
	{
	case EProp::Tree:
		Box(EArpgShape::Cylinder, 0, 0, 250, 50, 50, 500, Bark);
		Box(EArpgShape::Sphere, 0, 0, 560, 380, 380, 300, Def->Foliage, false);
		Box(EArpgShape::Sphere, 90, 60, 680, 260, 260, 220, Def->Foliage * 1.15f, false);
		break;
	case EProp::Pine:
		Box(EArpgShape::Cylinder, 0, 0, 120, 40, 40, 240, Bark);
		Box(EArpgShape::Cone, 0, 0, 380, 320, 320, 420, Def->Foliage, false);
		Box(EArpgShape::Cone, 0, 0, 600, 220, 220, 300, Snow, false);
		break;
	case EProp::JungleTree:
		Box(EArpgShape::Cylinder, 0, 0, 450, 60, 60, 900, Bark * 1.2f);
		Box(EArpgShape::Sphere, 0, 0, 920, 600, 600, 160, Def->Foliage, false);
		for (int32 v = 0; v < 4; ++v)
		{
			Box(EArpgShape::Cylinder, R.FRandRange(-250.f, 250.f), R.FRandRange(-250.f, 250.f), 650, 8, 8, R.FRandRange(300.f, 500.f), Def->Foliage * 0.7f, false);
		}
		break;
	case EProp::Fern:
		for (int32 f = 0; f < 4; ++f)
		{
			Box(EArpgShape::Cone, R.FRandRange(-60.f, 60.f), R.FRandRange(-60.f, 60.f), 40, 90, 90, 90, Def->Foliage * R.FRandRange(0.9f, 1.3f), false,
				FRotator(R.FRandRange(-20.f, 20.f), 0, R.FRandRange(-20.f, 20.f)));
		}
		break;
	case EProp::Rock:
		Box(EArpgShape::Sphere, 0, 0, 40, 260, 200, 160, Stone * 0.8f, true, FRotator(R.FRandRange(-15.f, 15.f), 0, R.FRandRange(-15.f, 15.f)));
		Box(EArpgShape::Sphere, 110, 60, 30, 140, 120, 100, Stone * 0.7f);
		break;
	case EProp::BrokenPillar:
		Box(EArpgShape::Cube, 0, 0, 15, 160, 160, 30, Stone * 0.9f);
		Box(EArpgShape::Cylinder, 0, 0, 30 + 180 * R.FRandRange(0.6f, 1.6f) * 0.5f, 100, 100, 180 * R.FRandRange(0.6f, 1.6f), Stone);
		Box(EArpgShape::Cylinder, 130, 40, 30, 90, 90, 120, Stone * 0.85f, true, FRotator(0, 0, 90));
		break;
	case EProp::RuinArch:
		Box(EArpgShape::Cube, 0, -220, 250, 90, 90, 500, Stone);
		Box(EArpgShape::Cube, 0, 220, 200, 90, 90, 400, Stone);
		Box(EArpgShape::Cube, 0, -60, 520, 100, 360, 70, Stone * 0.9f, true, FRotator(0, 0, -8));
		break;
	case EProp::Crystal:
		for (int32 c = 0; c < 4; ++c)
		{
			const float H = R.FRandRange(120.f, 320.f);
			Box(EArpgShape::Cone, R.FRandRange(-60.f, 60.f), R.FRandRange(-60.f, 60.f), H * 0.5f, 60, 60, H, Def->Accent, c == 0,
				FRotator(R.FRandRange(-20.f, 20.f), 0, R.FRandRange(-20.f, 20.f)), 3.f);
		}
		AddRoomLight(P(0, 0, 150), Def->Accent, 1500.f, 500.f);
		break;
	case EProp::StoneHead:
		Box(EArpgShape::Cube, 0, 0, 160, 260, 220, 320, Stone * 0.8f);
		Box(EArpgShape::Cube, 112, -50, 210, 40, 50, 30, Def->Accent, false, FRotator::ZeroRotator, 2.f);
		Box(EArpgShape::Cube, 112, 50, 210, 40, 50, 30, Def->Accent, false, FRotator::ZeroRotator, 2.f);
		Box(EArpgShape::Cube, 0, 0, 340, 300, 260, 50, Def->Foliage, false);
		break;
	case EProp::Car:
		Box(EArpgShape::Cube, 0, 0, 70, 420, 180, 90, Rust, true, FRotator(0, 0, R.FRandRange(-6.f, 6.f)));
		Box(EArpgShape::Cube, -20, 0, 145, 220, 165, 70, Rust * 0.8f);
		for (int32 w = 0; w < 4; ++w)
		{
			Box(EArpgShape::Cylinder, (w < 2 ? 140.f : -140.f), (w % 2 ? 90.f : -90.f), 35, 70, 70, 25, FLinearColor(0.06f, 0.06f, 0.06f), false, FRotator(0, 0, 90));
		}
		break;
	case EProp::Barrier:
		Box(EArpgShape::Cube, 0, 0, 55, 380, 70, 110, FLinearColor(0.6f, 0.58f, 0.55f));
		Box(EArpgShape::Cube, 0, 0, 115, 380, 72, 12, FLinearColor(0.85f, 0.65f, 0.15f), false);
		break;
	case EProp::Rubble:
		for (int32 b = 0; b < 6; ++b)
		{
			const float SZ = R.FRandRange(60.f, 160.f);
			Box(EArpgShape::Cube, R.FRandRange(-150.f, 150.f), R.FRandRange(-150.f, 150.f), SZ * 0.4f, SZ * 1.4f, SZ, SZ, Stone * R.FRandRange(0.7f, 1.f), b < 3,
				FRotator(R.FRandRange(-25.f, 25.f), R.FRandRange(0.f, 90.f), R.FRandRange(-25.f, 25.f)));
		}
		break;
	case EProp::StreetLamp:
		Box(EArpgShape::Cylinder, 0, 0, 260, 18, 18, 520, FLinearColor(0.15f, 0.15f, 0.16f));
		Box(EArpgShape::Cube, 50, 0, 520, 120, 30, 20, FLinearColor(0.15f, 0.15f, 0.16f), false);
		Box(EArpgShape::Cube, 100, 0, 505, 40, 40, 15, FLinearColor(1.f, 0.8f, 0.5f), false, FRotator::ZeroRotator, 6.f);
		AddRoomLight(P(100, 0, 480), FLinearColor(1.f, 0.75f, 0.45f), 3000.f, 700.f);
		break;
	case EProp::NeonSign:
		Box(EArpgShape::Cube, 0, 0, 200, 40, 40, 400, FLinearColor(0.2f, 0.2f, 0.22f));
		Box(EArpgShape::Cube, 0, 0, 420, 30, 260, 140, Def->Accent, false, FRotator::ZeroRotator, 6.f);
		AddRoomLight(P(60, 0, 420), Def->Accent, 4000.f, 800.f);
		break;
	case EProp::Barrel:
		Box(EArpgShape::Cylinder, 0, 0, 55, 70, 70, 110, Rust * 0.9f);
		Box(EArpgShape::Cylinder, 90, 20, 55, 70, 70, 110, FLinearColor(0.2f, 0.3f, 0.35f));
		break;
	case EProp::Crate:
		Box(EArpgShape::Cube, 0, 0, 60, 120, 120, 120, Bark * 1.6f);
		Box(EArpgShape::Cube, 20, 10, 170, 100, 100, 100, Bark * 1.4f, true, FRotator(0, 20, 0));
		break;
	case EProp::PierPosts:
		for (int32 p = 0; p < 4; ++p)
		{
			Box(EArpgShape::Cylinder, (p % 2) * 220.f, (p / 2) * 220.f, 100, 40, 40, 300, Bark);
		}
		Box(EArpgShape::Cube, 110, 110, 200, 300, 300, 20, Bark * 1.3f);
		break;
	case EProp::Boat:
		Box(EArpgShape::Cube, 0, 0, 40, 420, 150, 70, Bark * 1.3f, true, FRotator(0, 0, 8));
		Box(EArpgShape::Cone, 260, 0, 40, 120, 150, 70, Bark * 1.3f, false, FRotator(-90, 0, 0));
		break;
	case EProp::IceSpike:
		for (int32 c = 0; c < 3; ++c)
		{
			const float H = R.FRandRange(250.f, 600.f);
			Box(EArpgShape::Cone, R.FRandRange(-80.f, 80.f), R.FRandRange(-80.f, 80.f), H * 0.5f, 120, 120, H, Ice, c == 0,
				FRotator(R.FRandRange(-12.f, 12.f), 0, R.FRandRange(-12.f, 12.f)), 1.f);
		}
		break;
	case EProp::SnowRock:
		Box(EArpgShape::Sphere, 0, 0, 50, 280, 220, 180, FLinearColor(0.4f, 0.42f, 0.46f));
		Box(EArpgShape::Sphere, 0, 0, 110, 250, 200, 90, Snow, false);
		break;
	case EProp::FrozenPillar:
		Box(EArpgShape::Cylinder, 0, 0, 220, 110, 110, 440, Stone);
		Box(EArpgShape::Cylinder, 0, 0, 200, 150, 150, 300, Ice, false, FRotator::ZeroRotator, 0.8f);
		break;
	case EProp::House:
		Box(EArpgShape::Cube, 0, 0, 220, 700, 600, 440, FLinearColor(0.55f, 0.5f, 0.42f));
		Box(EArpgShape::Cone, 0, 0, 600, 1000, 850, 320, FLinearColor(0.32f, 0.18f, 0.12f), true, FRotator(0, 45, 0));
		Box(EArpgShape::Cube, 352, 0, 110, 10, 140, 220, Bark, false);
		Box(EArpgShape::Cube, 352, 180, 280, 10, 90, 90, FLinearColor(1.f, 0.75f, 0.4f), false, FRotator::ZeroRotator, 3.f);
		break;
	case EProp::Stall:
		Box(EArpgShape::Cube, 0, 0, 50, 300, 160, 100, Bark * 1.4f);
		Box(EArpgShape::Cube, 0, 0, 260, 340, 200, 15, FLinearColor(0.6f, 0.15f, 0.12f), false, FRotator(0, 0, 8));
		Box(EArpgShape::Cylinder, 150, 90, 130, 12, 12, 260, Bark, false);
		Box(EArpgShape::Cylinder, -150, 90, 130, 12, 12, 260, Bark, false);
		break;
	case EProp::Well:
		Box(EArpgShape::Cylinder, 0, 0, 50, 220, 220, 100, Stone);
		Box(EArpgShape::Cylinder, 0, 0, 101, 170, 170, 4, FLinearColor(0.05f, 0.1f, 0.15f), false);
		Box(EArpgShape::Cube, 0, 0, 280, 260, 30, 20, Bark, false);
		break;
	case EProp::Platform:
		// A raised ledge to fight on or jump over (90 high: jumpable).
		Box(EArpgShape::Cube, 0, 0, 45, 500, 400, 90, Def->FloorAlt * 0.9f);
		Box(EArpgShape::Cube, 0, 0, 92, 520, 420, 6, Def->WallAlt, false);
		break;
	}
}

void AArpgFloorGenerator::BuildArenaFeatures(FRandomStream& R)
{
	const FArpgRoom& Arena = Rooms.Last();
	const float H = Arena.Half.X;
	if (Kind == EKind::Final)
	{
		// The Hourless King's clock-face floor: twelve hour pillars and a brass ring.
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2.f * PI * i / 12.f;
			const FVector P = Arena.Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * H * 0.82f;
			AddShape(EArpgShape::Cylinder, P + FVector(0, 0, 450), FVector(140, 140, 900), Def->Wall, true);
			AddShape(EArpgShape::Sphere, P + FVector(0, 0, 960), FVector(120), FLinearColor(0.3f, 0.6f, 1.f), false, FRotator::ZeroRotator, 6.f);
			AddShape(EArpgShape::Cube, Arena.Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * H * 0.6f + FVector(0, 0, 3), FVector(220, 30, 6),
				FLinearColor(0.75f, 0.58f, 0.25f), false, FRotator(0, FMath::RadiansToDegrees(A), 0), 1.f);
		}
		AddShape(EArpgShape::Cylinder, Arena.Center + FVector(0, 0, 2), FVector(H * 1.25f, H * 1.25f, 4), FLinearColor(0.12f, 0.12f, 0.16f), false);
		AddShape(EArpgShape::Cylinder, Arena.Center + FVector(0, 0, 3), FVector(300, 300, 6), FLinearColor(0.75f, 0.58f, 0.25f), false, FRotator::ZeroRotator, 2.f);
		// Clock hands
		AddShape(EArpgShape::Cube, Arena.Center + FVector(500, 0, 4), FVector(1000, 60, 6), FLinearColor(0.75f, 0.58f, 0.25f), false, FRotator::ZeroRotator, 2.f);
		AddShape(EArpgShape::Cube, Arena.Center + FVector(0, 350, 4), FVector(50, 700, 6), FLinearColor(0.75f, 0.58f, 0.25f), false, FRotator::ZeroRotator, 2.f);
		// Throne behind the boss
		const FVector T = Arena.Center + FVector(H - 300.f, 0, 0);
		AddShape(EArpgShape::Cube, T + FVector(0, 0, 100), FVector(500, 700, 200), Def->Wall * 0.8f, true);
		AddShape(EArpgShape::Cube, T + FVector(150, 0, 600), FVector(120, 500, 900), FLinearColor(0.15f, 0.18f, 0.3f), true);
		AddRoomLight(Arena.Center + FVector(0, 0, 900), FLinearColor(0.5f, 0.65f, 1.f), 40000.f, H * 2.5f);
		return;
	}
	// Guardian arena: 4 pillars (GDD: pillars the Warlord can break by charging; breakable later) and a Veil seam.
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = PI * 0.25f + PI * 0.5f * i;
		const FVector P = Arena.Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * H * 0.5f;
		AddShape(EArpgShape::Cylinder, P + FVector(0, 0, 400), FVector(220, 220, 800), Def->Wall, true);
	}
	// The seam where two eras meet (GDD Fractured Time).
	AddShape(EArpgShape::Cube, Arena.Center + FVector(0, 0, 4), FVector(60, H * 2.f, 6), FLinearColor(0.3f, 0.65f, 1.f), false, FRotator(0, 20, 0), 6.f);
	AddShape(EArpgShape::Cube, Arena.Center + FVector(-H * 0.5f, 0, 2), FVector(H, H * 2.f, 3), ArpgData::Biome(EArpgBiome::Forest).Floor, false);
	AddShape(EArpgShape::Cube, Arena.Center + FVector(H * 0.5f, 0, 2), FVector(H, H * 2.f, 3), ArpgData::Biome(EArpgBiome::Wasteland).Floor, false);
	AddRoomLight(Arena.Center + FVector(0, 0, 900), Def->Accent, 30000.f, H * 2.5f);
}

void AArpgFloorGenerator::BuildHubTown(FRandomStream& R)
{
	const FArpgRoom& Plaza = Rooms[0];
	const float H = Plaza.Half.X;
	// Cobbled plaza ring and a well
	AddShape(EArpgShape::Cylinder, FVector(0, 0, 2), FVector(1600, 1600, 4), Def->FloorAlt, false);
	AddProp(EProp::Well, FVector(0, 0, 0), 0.f, 1.f, R);
	// Houses along the south, west and north edges
	for (int32 i = 0; i < 4; ++i)
	{
		AddProp(EProp::House, FVector(-H + 600.f + i * 1150.f, -H + 500.f, 0.f), 90.f, 1.f, R);
		AddProp(EProp::House, FVector(-H + 600.f + i * 1150.f, H - 500.f, 0.f), -90.f, 1.f, R);
	}
	AddProp(EProp::House, FVector(-H + 500.f, 0.f, 0.f), 0.f, 1.2f, R);
	AddProp(EProp::Stall, FVector(-400.f, -900.f, 0.f), 90.f, 1.f, R);
	AddProp(EProp::Stall, FVector(400.f, -900.f, 0.f), 90.f, 1.f, R);
	AddProp(EProp::StreetLamp, FVector(1400.f, 600.f, 0.f), 180.f, 1.f, R);
	AddProp(EProp::StreetLamp, FVector(1400.f, -600.f, 0.f), 180.f, 1.f, R);
	AddProp(EProp::Tree, FVector(-1200.f, 1200.f, 0.f), 0.f, 1.2f, R);
	AddProp(EProp::Tree, FVector(-1200.f, -1200.f, 0.f), 0.f, 1.1f, R);
	// Road from the gate to the Spire
	AddShape(EArpgShape::Cube, FVector(H + 4000.f, 0.f, -5.f), FVector(8000.f, 700.f, 20.f), Def->FloorAlt, true);
	BuildSpire(FVector(H + 9000.f, 0.f, 0.f), 1.f);
	AddRoomLight(FVector(0, 0, 900), FLinearColor(1.f, 0.85f, 0.6f), 20000.f, 5000.f);
}

void AArpgFloorGenerator::BuildSpire(const FVector& Base, float Scale)
{
	if (UStaticMesh* Mesh = ArpgAssets::OptionalMesh(TEXT("/Game/_Game/Art/Tower/SM_VeilSpire.SM_VeilSpire")))
	{
		AddMesh(Mesh, FTransform(FRotator::ZeroRotator, Base, FVector(Scale)), true);
		return;
	}
	// The Veil Spire from the concept art: a dark tapering tower wrapped in glowing blue rings.
	const FLinearColor Dark(0.12f, 0.13f, 0.17f), Glow(0.25f, 0.6f, 1.f);
	float Z = 0.f;
	float Radius = 1400.f * Scale;
	for (int32 Tier = 0; Tier < 9; ++Tier)
	{
		const float TierH = 1600.f * Scale;
		AddShape(EArpgShape::Cylinder, Base + FVector(0, 0, Z + TierH * 0.5f), FVector(Radius * 2.f, Radius * 2.f, TierH), Dark * (1.f + 0.05f * Tier), Tier < 2);
		// Buttresses
		for (int32 b = 0; b < 6; ++b)
		{
			const float A = 2.f * PI * b / 6.f + Tier * 0.3f;
			AddShape(EArpgShape::Cube, Base + FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, Z + TierH * 0.5f),
				FVector(260.f * Scale, 260.f * Scale, TierH * 0.9f), Dark * 0.8f, false, FRotator(0, FMath::RadiansToDegrees(A), 0));
		}
		// A glowing ring around every other tier
		if (Tier % 2 == 1 || Tier == 8)
		{
			AddShape(EArpgShape::Cylinder, Base + FVector(0, 0, Z + TierH * 0.6f), FVector(Radius * 2.9f, Radius * 2.9f, 40.f * Scale), Glow, false,
				FRotator(FMath::Sin(Tier * 1.7f) * 6.f, 0, FMath::Cos(Tier * 1.3f) * 6.f), 8.f);
		}
		Z += TierH;
		Radius *= 0.84f;
	}
	AddShape(EArpgShape::Cone, Base + FVector(0, 0, Z + 1200.f * Scale), FVector(Radius * 2.f, Radius * 2.f, 2400.f * Scale), Dark, false);
	AddShape(EArpgShape::Cylinder, Base + FVector(0, 0, Z * 0.5f), FVector(120.f * Scale, 120.f * Scale, Z), Glow, false, FRotator::ZeroRotator, 10.f); // the light seam
	// Floating rocks
	FRandomStream R(77);
	for (int32 i = 0; i < 14; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = R.FRandRange(2500.f, 5000.f) * Scale;
		AddShape(EArpgShape::Sphere, Base + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, R.FRandRange(1500.f, 9000.f) * Scale),
			FVector(R.FRandRange(150.f, 500.f) * Scale), Dark * 1.3f, false);
	}
	AddRoomLight(Base + FVector(-Radius * 3.f, 0, 3000.f * Scale), Glow, 200000.f, 9000.f * Scale);
}

void AArpgFloorGenerator::BuildBackdrop(FRandomStream& R)
{
	FBox Bounds(ForceInit);
	for (const FArpgRoom& Room : Rooms)
	{
		Bounds += Room.Center + FVector(Room.Half.X, Room.Half.Y, 0.f);
		Bounds += Room.Center - FVector(Room.Half.X, Room.Half.Y, 0.f);
	}
	const FVector Mid = Bounds.GetCenter();
	const float Inner = Bounds.GetExtent().Size2D() + 2500.f;
	const int32 Count = Kind == EKind::Hub ? 26 : 40;

	for (int32 i = 0; i < Count; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI);
		const float D = Inner + R.FRandRange(0.f, 7000.f);
		const FVector P = Mid + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
		const float S = R.FRandRange(0.8f, 1.6f);
		EArpgBiome Look = Biome;
		if (Biome == EArpgBiome::FracturedTime) { Look = FMath::Cos(A) > 0.f ? EArpgBiome::Wasteland : EArpgBiome::Forest; }
		if (Biome == EArpgBiome::Greyhollow) { Look = EArpgBiome::Forest; }
		const FArpgBiomeDef& LD = ArpgData::Biome(Look);
		switch (Look)
		{
		case EArpgBiome::Forest:
			AddShape(EArpgShape::Cylinder, P + FVector(0, 0, 900 * S), FVector(220 * S, 220 * S, 1800 * S), FLinearColor(0.2f, 0.14f, 0.09f), false);
			AddShape(EArpgShape::Sphere, P + FVector(0, 0, 2000 * S), FVector(1600 * S, 1600 * S, 1100 * S), LD.Foliage * R.FRandRange(0.8f, 1.1f), false);
			break;
		case EArpgBiome::Jungle:
			AddShape(EArpgShape::Cylinder, P + FVector(0, 0, 1300 * S), FVector(260 * S, 260 * S, 2600 * S), FLinearColor(0.25f, 0.17f, 0.1f), false);
			AddShape(EArpgShape::Sphere, P + FVector(0, 0, 2700 * S), FVector(2200 * S, 2200 * S, 600 * S), LD.Foliage, false);
			if (i % 6 == 0)
			{
				// Cliff with a waterfall
				AddShape(EArpgShape::Cube, P + FVector(0, 0, 1500 * S), FVector(2000 * S, 1200 * S, 3000 * S), LD.Wall * 0.6f, false, FRotator(0, FMath::RadiansToDegrees(A), 0));
				AddShape(EArpgShape::Cube, P + FVector(0, 0, 1400 * S) - FVector(FMath::Cos(A), FMath::Sin(A), 0) * 620.f * S, FVector(60 * S, 400 * S, 2800 * S),
					FLinearColor(0.7f, 0.85f, 0.95f), false, FRotator(0, FMath::RadiansToDegrees(A), 0), 1.f);
			}
			break;
		case EArpgBiome::Wasteland:
		{
			const float Hgt = R.FRandRange(2500.f, 7000.f) * S;
			const FRotator Tilt(R.FRandRange(-4.f, 4.f), R.FRandRange(0.f, 90.f), R.FRandRange(-4.f, 4.f));
			AddShape(EArpgShape::Cube, P + FVector(0, 0, Hgt * 0.5f), FVector(1200 * S, 1200 * S, Hgt), FLinearColor(0.22f, 0.21f, 0.22f) * R.FRandRange(0.8f, 1.2f), false, Tilt);
			for (int32 w = 0; w < 4; ++w)
			{
				if (R.FRand() < 0.5f)
				{
					AddShape(EArpgShape::Cube, P + FVector(0, 0, R.FRandRange(0.2f, 0.9f) * Hgt), FVector(1220 * S, 1220 * S, 60 * S),
						R.FRand() < 0.3f ? LD.Accent : FLinearColor(1.f, 0.75f, 0.4f), false, Tilt, 3.f);
				}
			}
			break;
		}
		case EArpgBiome::Flood:
			if (i % 2 == 0)
			{
				AddShape(EArpgShape::Cube, P + FVector(0, 0, 900 * S), FVector(900 * S, 900 * S, 2600 * S), LD.Wall * 0.7f, false, FRotator(R.FRandRange(-8.f, 8.f), R.FRandRange(0.f, 90.f), R.FRandRange(-8.f, 8.f)));
			}
			else
			{
				AddShape(EArpgShape::Cube, P + FVector(0, 0, 500 * S), FVector(300 * S, 300 * S, 1200 * S), LD.Wall * 0.6f, false);
				AddShape(EArpgShape::Cube, P + FVector(0, 900 * S, 500 * S), FVector(300 * S, 300 * S, 1200 * S), LD.Wall * 0.6f, false);
				AddShape(EArpgShape::Cube, P + FVector(0, 450 * S, 1150 * S), FVector(320 * S, 1300 * S, 200 * S), LD.Wall * 0.6f, false);
			}
			break;
		case EArpgBiome::IceLands:
			AddShape(EArpgShape::Cone, P + FVector(0, 0, 2500 * S), FVector(6000 * S, 6000 * S, 5000 * S), FLinearColor(0.75f, 0.8f, 0.88f), false);
			AddShape(EArpgShape::Cone, P + FVector(0, 0, 3900 * S), FVector(2600 * S, 2600 * S, 2200 * S), FLinearColor(0.95f, 0.97f, 1.f), false);
			if (i % 5 == 0)
			{
				AddShape(EArpgShape::Cone, P + FVector(0, 0, 1800 * S), FVector(500 * S, 500 * S, 3600 * S), FLinearColor(0.45f, 0.75f, 1.f), false, FRotator::ZeroRotator, 3.f);
			}
			break;
		default:
			break;
		}
	}
	// Floors above the hub glimpse the Spire's light seam above them.
	if (Kind != EKind::Hub)
	{
		AddShape(EArpgShape::Cylinder, Mid + FVector(0, 0, 30000.f), FVector(400.f, 400.f, 20000.f), FLinearColor(0.25f, 0.6f, 1.f), false, FRotator::ZeroRotator, 6.f);
	}
}
