// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class DUALCONTOURINGTERRAIN_API SDF
{
public:
	enum class Type : uint8
	{
		Box = 1
	};

	static float Box(FVector3f p, FVector3f extent);
};

struct DUALCONTOURINGTERRAIN_API SDFOp
{
	SDF::Type type;
	FVector3f position;
	FVector3f size;
};
