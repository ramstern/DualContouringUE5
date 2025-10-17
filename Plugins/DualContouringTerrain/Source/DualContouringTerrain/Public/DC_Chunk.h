// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DC_OctreeNode.h"

#include "Interface/Core/RealtimeMeshKeys.h"
#include "Interface/Core/RealtimeMeshDataStream.h"

struct DUALCONTOURINGTERRAIN_API ChunkCreationResult
{
	int32 chunk_idx = -1;
	OctreeNode* created_root = nullptr;

	ChunkCreationResult() = default;

	ChunkCreationResult(const ChunkCreationResult&) = delete;
	ChunkCreationResult& operator=(const ChunkCreationResult&) = delete;

	ChunkCreationResult(ChunkCreationResult&& other) noexcept;
	ChunkCreationResult& operator=(ChunkCreationResult&& other) noexcept;
};

struct DUALCONTOURINGTERRAIN_API ChunkPolygonizeResult
{
	int32 chunk_idx = -1;
	FRealtimeMeshSectionGroupKey created_mesh_key;
	RealtimeMesh::FRealtimeMeshStreamSet stream_set;

	ChunkPolygonizeResult() = default;

	ChunkPolygonizeResult(const ChunkPolygonizeResult&) = delete;
	ChunkPolygonizeResult& operator=(const ChunkPolygonizeResult&) = delete;

	ChunkPolygonizeResult(ChunkPolygonizeResult&& other) noexcept;
	ChunkPolygonizeResult& operator=(ChunkPolygonizeResult&& other) noexcept;
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

	struct OctreeNode* root = nullptr;
	FIntVector3 coordinates;
	FVector3f center;
	FRealtimeMeshSectionGroupKey mesh_group_key;
	URealtimeMeshSimple* mesh = nullptr;
	bool has_group_key = false;
};

