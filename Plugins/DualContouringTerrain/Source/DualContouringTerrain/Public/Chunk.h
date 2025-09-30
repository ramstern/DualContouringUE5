// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OctreeNode.h"
/**
 * 
 */
struct DUALCONTOURINGTERRAIN_API Chunk
{
public:

	Chunk() = default;
	~Chunk();

	Chunk(const Chunk&) = delete;
	Chunk& operator=(const Chunk&) = delete;
	
	Chunk(Chunk&& other) noexcept;

	Chunk& operator=(Chunk&& other) noexcept;

	struct OctreeNode* root = nullptr;
	FIntVector3 coordinates;
	FVector3f center;
};
