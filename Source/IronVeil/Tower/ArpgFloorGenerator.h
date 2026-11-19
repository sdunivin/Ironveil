// ArpgFloorGenerator.h
// Builds one floor of the Veil Spire from a seed (master plan 7.3, GDD "Floor generation"):
//   floor seed = run seed + floor number -> one FRandomStream for every choice, so a seed always builds the same floor.
//   A main path of rooms on a grid from the start room to the exit, 1-3 side rooms with chests, corridors between them,
//   biome-specific walls, props and backdrop, plus spots for packs, chests, the relic and the exit portal.
// Guardian floors (every 10th) are hand-shaped arenas; floor 100 is the Hourless King's throne; floor 0 is Greyhollow.
//
// Geometry is gray-box: engine basic shapes in instanced mesh pools (one draw call per shape/colour), with
// Blender props (SM_Prop_*) swapped in automatically once imported. Room kits as Level Instances come later (plan 7.2).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ArpgTypes.h"
#include "ArpgFloorGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;
struct FArpgBiomeDef;
enum class EArpgShape : uint8;

enum class EArpgRoomType : uint8
{
	Start,
	Combat,
	Side,
	Exit,
	Arena,
	Hub
};

struct FArpgRoom
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FVector Center = FVector::ZeroVector;
	FVector2D Half = FVector2D(1000.f, 1000.f);
	EArpgRoomType Type = EArpgRoomType::Combat;
	TArray<int32> Links;
};

struct FArpgCorridor
{
	int32 A = INDEX_NONE;
	int32 B = INDEX_NONE;
	FVector Center = FVector::ZeroVector;
	FVector2D Half = FVector2D::ZeroVector;
};

struct FArpgSpawnSpot
{
	FVector Location = FVector::ZeroVector;
	int32 Room = INDEX_NONE;
};

UCLASS()
class IRONVEIL_API AArpgFloorGenerator : public AActor
{
	GENERATED_BODY()

public:
	AArpgFloorGenerator();

	enum class EKind : uint8 { Normal, Guardian, Final, Hub };

	void Build(int32 InFloor, int32 InSeed, EArpgBiome InBiome, EKind InKind);

	// ---------------------------------------------------------------- results
	int32 Floor = 1;
	int32 Seed = 0;
	EArpgBiome Biome = EArpgBiome::Forest;
	EKind Kind = EKind::Normal;
	TArray<FArpgRoom> Rooms;
	TArray<FArpgCorridor> Corridors;
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;
	FVector PortalLocation = FVector::ZeroVector;
	FRotator PortalRotation = FRotator::ZeroRotator;
	FVector RelicLocation = FVector::ZeroVector;
	FVector ArenaCenter = FVector::ZeroVector;
	FVector BossLocation = FVector::ZeroVector;
	FVector ShrineLocation = FVector::ZeroVector;
	TArray<FArpgSpawnSpot> PackSpots;
	TArray<FArpgSpawnSpot> ChestSpots;
	int32 HuntRoom = INDEX_NONE;

	// ---------------------------------------------------------------- navigation for AI (room graph, no NavMesh)
	int32 RoomAt(const FVector& P, float Margin = 0.f) const;
	int32 CorridorAt(const FVector& P) const;
	FVector GetWaypoint(const FVector& From, const FVector& To) const;
	// A random walkable point inside a room (for adds and waves).
	FVector RandomPointInRoom(int32 Room, FRandomStream& Rng, float Margin = 300.f) const;
	FVector RandomPointNear(const FVector& Center, float Radius, FRandomStream& Rng) const;

private:
	void Clear();
	bool PlanLayout(FRandomStream& R);
	void PlanArena(FRandomStream& R, float ArenaHalf);
	void PlanHub();
	void BuildGeometry(FRandomStream& R);
	void BuildRoom(int32 Index, FRandomStream& R);
	void BuildCorridor(const FArpgCorridor& C, FRandomStream& R);
	void BuildWallRun(const FVector& From, const FVector& To, float Thickness, FRandomStream& R, float MinH, float MaxH);
	void DressRoom(int32 Index, FRandomStream& R);
	void BuildBackdrop(FRandomStream& R);
	void BuildSpire(const FVector& Base, float Scale);
	void BuildArenaFeatures(FRandomStream& R);
	void BuildHubTown(FRandomStream& R);
	void AddRoomLight(const FVector& At, const FLinearColor& Color, float Intensity, float Radius);
	int32 PathNext(int32 From, int32 To) const;
	FVector DoorPoint(int32 Room, int32 Toward, float Inset) const;

	// Props
	enum class EProp : uint8
	{
		Tree, Pine, JungleTree, Fern, Rock, BrokenPillar, RuinArch, Crystal, StoneHead,
		Car, Barrier, Rubble, StreetLamp, NeonSign, Barrel, Crate, PierPosts, Boat,
		IceSpike, SnowRock, FrozenPillar, House, Stall, Well, Platform
	};
	void AddProp(EProp Type, const FVector& Loc, float Yaw, float Scale, FRandomStream& R);
	TArray<EProp> PropsFor(EArpgBiome B) const;
	static const TCHAR* PropName(EProp P);

	// Geometry pools
	void AddShape(EArpgShape Shape, const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide = true,
		const FRotator& Rot = FRotator::ZeroRotator, float Emissive = 0.f);
	void AddMesh(UStaticMesh* Mesh, const FTransform& Xf, bool bCollide);
	UInstancedStaticMeshComponent* Pool(UStaticMesh* Mesh, const FLinearColor* Color, float Emissive, bool bCollide);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Pools;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;

	const FArpgBiomeDef* Def = nullptr;
	static constexpr float CellSize = 3600.f;
	static constexpr float DoorHalf = 360.f;
	static constexpr float WallThick = 70.f;
};
