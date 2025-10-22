// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DC_OctreeNode.h"

#include "Interface/Core/RealtimeMeshKeys.h"
#include "Interface/Core/RealtimeMeshDataStream.h"

struct DUALCONTOURINGTERRAIN_API ChunkCreationResult
{
	FIntVector3 chunk_coord;
	bool newly_created = false;
	OctreeNode* created_root = nullptr;

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
	FRealtimeMeshSectionGroupKey mesh_group_key;
	URealtimeMeshSimple* mesh = nullptr;
};

