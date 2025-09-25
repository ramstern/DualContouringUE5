// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chunk.h"
#include "ChunkProvider.generated.h"

/**
 * 
 */
class UChunkProviderSettings;
class UOctreeManager;

UCLASS()
class UChunkProvider : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	void ReloadChunks();

	TArray<Chunk> chunks;
	int32 chunk_load_distance;

	FORCEINLINE int32 GetChunkIndex(int32 x, int32 y, int32 z) 
	{
		return z + (y * chunk_load_distance) + (x * chunk_load_distance * chunk_load_distance);
	};

	const UChunkProviderSettings* chunk_settings = nullptr;
	UOctreeManager* octree_manager = nullptr;

	// try to get current render camera
	FVector GetActiveCameraLocation();

	FVector camera_pos = FVector();

	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;

	virtual bool DoesSupportWorldType(EWorldType::Type type) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
};
