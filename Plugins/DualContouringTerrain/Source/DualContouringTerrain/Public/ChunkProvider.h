// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chunk.h"
#include "ChunkProviderSettings.h"
#include "Misc/Optional.h"
#include "ChunkProvider.generated.h"
/**
 * 
 */
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
	void ReloadReallocChunks();

	struct ChunkGrid
	{
	public:
		FORCEINLINE int32 WrapCoord(int32 coord) const
		{
			coord %= dim;
			if(coord < 0) coord += dim;

			return coord;
		}

		int32 GetStableChunkIndex(FIntVector3 c) const;

		Chunk* TryGetChunk(FIntVector3 c);

		void Realloc(int32 new_load_distance);

		TArray<Chunk> chunks;
		TQueue<TFunction<ChunkCreationResult()>> chunk_creation_jobs;
		TArray<TFuture<ChunkCreationResult>> chunk_creation_tasks;
		TQueue<TTuple<FIntVector3, bool>> chunk_polygonize_jobs;
		TArray<TFuture<ChunkPolygonizeResult>> chunk_polygonize_tasks;
		TArray<TFuture<ERealtimeMeshProxyUpdateStatus>> chunk_section_tasks;
		int32 dim;
		FIntVector min_coord;

	private:
		FORCEINLINE int32 Flatten(int32 x, int32 y, int32 z) const
		{
			return z + (y * dim) + (x * dim * dim);
		};
	} chunk_grid;


	FORCEINLINE FIntVector3 GetChunkCoordinatesFromPosition(FVector3f position) 
	{
		return FIntVector3(FMath::FloorToInt(position.X / chunk_settings->chunk_size), FMath::FloorToInt(position.Y / chunk_settings->chunk_size), FMath::FloorToInt(position.Z / chunk_settings->chunk_size));
	};

	const UChunkProviderSettings* chunk_settings = nullptr;
	UOctreeManager* octree_manager = nullptr;

	void InitializeChunks(FIntVector3 current_chunk_coord);
	void BuildSlab(FIntVector3 delta, FIntVector3 current_chunk_coord);

	//calls upon octree manager to mesh this chunk.
	void MeshChunk(const FIntVector3& coords, bool negative_delta);

	void CreateChunk(FIntVector3 coord);

	bool IsSafeToModifyChunks();

 	void FillSeamOctreeNodes(TArray<OctreeNode*, TInlineAllocator<8>>& seam_octants, bool negative_delta, const FIntVector3& chunk_coord, OctreeNode* root);

	FRenderCommandFence fence;

	// try to get current render camera
	FVector GetActiveCameraLocation();

	FVector camera_pos = FVector();
	FIntVector3 last_chunk_coord = FIntVector3(-1000, -1000, -1000);

	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;

	virtual bool DoesSupportWorldType(EWorldType::Type type) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
};
