// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DC_OctreeNode.h"
#include "Interface/Core/RealtimeMeshInterfaceFwd.h"

enum class PolygonizeTaskArg : uint8
{
	Area = 0,
	SlabNegative = 1,
	SlabPositive = 2,
	RebuildAllSeams = 3,
};

enum class CreationTaskArg : uint8
{
	None = 0,
	NewlyCreated = 1,
	ModifyOperation = 2
};

struct DUALCONTOURINGTERRAIN_API ChunkCreationResult
{
	FIntVector3 chunk_coord;
	CreationTaskArg task_arg = CreationTaskArg::None;
	OctreeNode* created_root = nullptr;
	TArray<float> noise_field;

	ChunkCreationResult() = default;
};

struct DUALCONTOURINGTERRAIN_API ChunkPolygonizeResult
{
	TFuture<ERealtimeMeshProxyUpdateStatus> mesh_future;
	TFuture<ERealtimeMeshProxyUpdateStatus> collision_future;

	ChunkPolygonizeResult() = default;
};

class URealtimeMeshSimple;

struct DUALCONTOURINGTERRAIN_API Chunk
{
public:
	Chunk() = default;
	~Chunk();

	Chunk(const Chunk&) = delete;
	Chunk& operator=(const Chunk&) = delete;
	
	Chunk(Chunk&& other) noexcept;

	Chunk& operator=(Chunk&& other) noexcept;

	TUniquePtr<struct OctreeNode> root = nullptr;
	FVector3f center;
	bool newly_created = false;
	uint8 ping_counter = 0;
	URealtimeMeshSimple* mesh = nullptr;
	TArray<float> noise_field;
};

