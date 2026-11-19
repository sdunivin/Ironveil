#include "Core/ArpgAssets.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace
{
	const TCHAR* GBaseMaterialPath = TEXT("/Game/_Game/Materials/M_IronveilBase.M_IronveilBase");
	const TCHAR* GFallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	// Rooted so the cache survives garbage collection for the whole session.
	TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>>& ColorCache()
	{
		static TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Cache;
		return Cache;
	}

	template <typename T>
	T* LoadRooted(const TCHAR* Path, bool bQuiet)
	{
		T* Obj = LoadObject<T>(nullptr, Path, nullptr, bQuiet ? (LOAD_NoWarn | LOAD_Quiet) : LOAD_None);
		if (Obj)
		{
			Obj->AddToRoot();
		}
		return Obj;
	}
}

UStaticMesh* ArpgAssets::Shape(EArpgShape S)
{
	static UStaticMesh* Meshes[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	static const TCHAR* Paths[5] = {
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		TEXT("/Engine/BasicShapes/Sphere.Sphere"),
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/BasicShapes/Cone.Cone"),
		TEXT("/Engine/BasicShapes/Plane.Plane") };
	const int32 I = (int32)S;
	if (!Meshes[I])
	{
		Meshes[I] = LoadRooted<UStaticMesh>(Paths[I], false);
	}
	return Meshes[I];
}

UMaterialInterface* ArpgAssets::BaseMaterial()
{
	static UMaterialInterface* Mat = nullptr;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Mat = LoadRooted<UMaterialInterface>(GBaseMaterialPath, true);
		if (!Mat)
		{
			Mat = LoadRooted<UMaterialInterface>(GFallbackMaterialPath, false);
		}
	}
	return Mat;
}

bool ArpgAssets::HasEmissiveMaterial()
{
	UMaterialInterface* Mat = BaseMaterial();
	return Mat && Mat->GetPathName().StartsWith(TEXT("/Game/"));
}

void ArpgAssets::SetColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive)
{
	if (!MID) { return; }
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	if (HasEmissiveMaterial())
	{
		MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
	}
}

UMaterialInstanceDynamic* ArpgAssets::NewColor(UObject* Outer, const FLinearColor& Color, float Emissive)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial(), Outer ? Outer : GetTransientPackage());
	SetColor(MID, Color, Emissive);
	return MID;
}

UMaterialInstanceDynamic* ArpgAssets::SharedColor(const FLinearColor& Color, float Emissive)
{
	const FColor C = Color.ToFColor(false);
	const uint32 Key = HashCombine(C.DWColor(), GetTypeHash(FMath::RoundToInt(Emissive * 10.f)));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = ColorCache().Find(Key))
	{
		if (IsValid(*Found)) { return *Found; }
	}
	UMaterialInstanceDynamic* MID = NewColor(GetTransientPackage(), Color, Emissive);
	MID->AddToRoot();
	ColorCache().Add(Key, MID);
	return MID;
}

UStaticMesh* ArpgAssets::OptionalMesh(const FString& Path)
{
	if (Path.IsEmpty()) { return nullptr; }
	static TMap<FString, UStaticMesh*> Cache;
	if (UStaticMesh** Found = Cache.Find(Path))
	{
		return *Found;
	}
	UStaticMesh* Mesh = LoadRooted<UStaticMesh>(*Path, true);
	Cache.Add(Path, Mesh);
	return Mesh;
}
