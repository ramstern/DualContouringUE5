// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
struct DUALCONTOURINGTERRAIN_API Chunk
{
public:

	Chunk();
	~Chunk();
	
	struct OctreeNode* root = nullptr;
	FIntVector3 coordinates;
	FVector3f center;
};
