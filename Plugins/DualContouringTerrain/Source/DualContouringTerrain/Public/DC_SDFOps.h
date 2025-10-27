// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DC_SDFOps.generated.h"

class DUALCONTOURINGTERRAIN_API SDF
{
public:
	static float Box(const FVector3f& p, const FVector3f& extent);
	static float Sphere(const FVector3f& p, float radius);
};

UENUM()
enum ModType : uint8
{
	Union,
	Subtract
};

UENUM()
enum SDFType : uint8
{
	Box,
	Sphere
};

USTRUCT(BlueprintType)
struct DUALCONTOURINGTERRAIN_API FSDFOp
{
	GENERATED_BODY()

	FSDFOp(const FVector3f& pos, const FVector3f& _bounds_size) : position(pos), bounds_size(_bounds_size){}
	FSDFOp() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector3f position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector3f bounds_size;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)

	TEnumAsByte<ModType> mod_type = ModType::Subtract;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<SDFType> sdf_type = SDFType::Box;
};

