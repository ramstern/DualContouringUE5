// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class DUALCONTOURINGTERRAIN_API SDF
{
public:
	static float Box(const FVector3f& p, const FVector3f& extent);
};
struct DUALCONTOURINGTERRAIN_API SDFOp
{
	SDFOp(const FVector3f& pos, const FVector3f& _bounds_size) : position(pos), bounds_size(_bounds_size){}
	SDFOp() = default;

	FVector3f position;
	FVector3f bounds_size;

	enum ModType : uint8
	{
		Union,
		Subtract
	} mod_type = ModType::Subtract;
	enum SDFType : uint8
	{
		Box,
		Sphere
	} sdf_type = SDFType::Box;
};

