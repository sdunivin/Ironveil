// ArpgAssets.h
// Finds the meshes and materials the gray-box game is built from.
//
// Everything falls back to the engine's basic shapes and BasicShapeMaterial, so the game runs in an
// empty project. Once Tools/unreal/setup_ironveil_content.py has run, M_IronveilBase (with an Emissive
// parameter) and the Blender meshes under /Game/_Game/Art are picked up automatically.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UObject;

enum class EArpgShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Plane
};

namespace ArpgAssets
{
	UStaticMesh* Shape(EArpgShape S);

	// The base material: /Game/_Game/Materials/M_IronveilBase if it exists, otherwise BasicShapeMaterial.
	UMaterialInterface* BaseMaterial();
	bool HasEmissiveMaterial();

	// A material instance with Color (and Emissive when supported). Shared instances are cached per colour.
	UMaterialInstanceDynamic* SharedColor(const FLinearColor& Color, float Emissive = 0.f);
	UMaterialInstanceDynamic* NewColor(UObject* Outer, const FLinearColor& Color, float Emissive = 0.f);
	void SetColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive = 0.f);

	// Loads an optional imported mesh; returns nullptr quietly if it isn't there.
	UStaticMesh* OptionalMesh(const FString& Path);

	// Yaw added to imported Blender meshes so they face +X. Blender characters face -Y.
	constexpr float ImportedMeshYaw = -90.f;
}
