// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_Chunk.h"

Chunk::~Chunk()
{
	delete root;
}

Chunk::Chunk(Chunk&& other) noexcept : root(other.root), coordinates(other.coordinates), center(other.center), mesh_group_key(other.mesh_group_key), mesh(other.mesh)
{
	other.root = nullptr;
	other.mesh = nullptr;
	other.mesh_group_key = FRealtimeMeshSectionGroupKey();
}

Chunk& Chunk::operator=(Chunk&& other) noexcept
{
	if(this != &other)
	{
		coordinates = other.coordinates;
		center = other.center;

		mesh = other.mesh;
		other.mesh = nullptr;

		mesh_group_key = other.mesh_group_key;
		other.mesh_group_key = FRealtimeMeshSectionGroupKey();

		delete root;
		root = other.root;
		other.root = nullptr;
	}
	
	return *this;
}

ChunkCreationResult::ChunkCreationResult(ChunkCreationResult&& other) noexcept : chunk_idx(other.chunk_idx), newly_created(other.newly_created), created_root(other.created_root)
{
	other.created_root = nullptr;
}

ChunkCreationResult& ChunkCreationResult::operator=(ChunkCreationResult&& other) noexcept
{
	if(this != &other)
	{
		chunk_idx = other.chunk_idx;
		newly_created = other.newly_created;

		delete created_root;
		created_root = other.created_root;
		other.created_root = nullptr;
	}

	return *this;
}

ChunkPolygonizeResult::ChunkPolygonizeResult(ChunkPolygonizeResult&& other) noexcept : chunk_idx(other.chunk_idx), created_mesh_key(other.created_mesh_key), stream_set(other.stream_set)
{
	other.created_mesh_key = FRealtimeMeshSectionGroupKey();
}

ChunkPolygonizeResult& ChunkPolygonizeResult::operator=(ChunkPolygonizeResult&& other) noexcept
{
	if (this != &other)
	{
		chunk_idx = other.chunk_idx;

		created_mesh_key = other.created_mesh_key;
		other.created_mesh_key = FRealtimeMeshSectionGroupKey();

		stream_set = MoveTemp(other.stream_set);
	}

	return *this;
}
