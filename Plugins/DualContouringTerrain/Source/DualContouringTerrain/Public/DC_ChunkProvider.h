// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DC_Chunk.h"
#include "DC_ChunkProviderSettings.h"
#include "Misc/Optional.h"
#include "DC_SDFOps.h"
#include "DC_ChunkProvider.generated.h"
/**
 * 
 */
class UOctreeCode;
struct OctreeNode;
class ADC_OctreeRenderActor;
class URealtimeMeshSimple;

UCLASS()
class DUALCONTOURINGTERRAIN_API UChunkProvider : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	void ModifyOperation(const SDFOp& sdf_operation);

private:
	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void ReloadChunks();
	void ReloadReallocChunks();

	//alloc funcs
	void Init(bool simulating);
	void Cleanup(bool simulating);

	struct ChunkGrid
	{
	public:
		Chunk* TryGet(FIntVector3 c);
		const Chunk& Get(FIntVector3 c);
		Chunk& GetMutable(FIntVector3 c);

		//waits until everything is finished and cleans up chunk resources
		void Cleanup();

		void Realloc(int32 new_load_distance);

		TMap<FIntVector3, Chunk> chunks;

		TQueue<TTuple<FIntVector3, CreationTaskArg>> chunk_creation_jobs;
		TArray<TFuture<ChunkCreationResult>> chunk_creation_tasks;
		TQueue<TTuple<FIntVector3, PolygonizeTaskArg>> chunk_polygonize_jobs;
		TArray<TFuture<ChunkPolygonizeResult>> chunk_polygonize_tasks;

		TQueue<SDFOp> modify_operations;

		int32 dim;
		FIntVector min_coord;
		FIntVector3 current_generator_pos;
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
	UOctreeCode* octree_manager = nullptr;

	void BuildChunkArea(FIntVector3 current_chunk_coord);
	void BuildSlabs(FIntVector3 delta, FIntVector3 current_chunk_coord);
	TArray<FIntVector3> GetChunkArea(FIntVector3 around);

	TArray<float> BuildNoiseField(const FVector3f& center, float size, int32 max_depth, int32 noise_seed);
	void EditNoiseField(TArray<float>& noise_field, const FVector3f& center, float size, int32 max_depth, const SDFOp& sdf_op);

	//calls upon octree manager to mesh this chunk.
	void MeshChunk(const FIntVector3& coords, PolygonizeTaskArg task_arg);
	void CreateChunk(FIntVector3 coord);
	void RebuildChunk(FIntVector3 coord);

	bool IsSafeToModifyChunks();

 	void FillSeamOctreeNodes(TArray<OctreeNode*, TInlineAllocator<8>>& seam_octants, bool negative_delta, const FIntVector3& chunk_coord, OctreeNode* root);

	// try to get current render camera
	FVector GetActiveCameraLocation();

	FVector camera_pos = FVector();

	//TArray<UE::Math::TBox<float>> ops;

	// actor for rendering the octree mesh
	ADC_OctreeRenderActor* render_actor = nullptr;
	//AActor* test_follow_actor = nullptr;
	bool build_initial_area = false;
	TSet<FIntVector3> temp_created_chunks;

	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;

	virtual bool DoesSupportWorldType(EWorldType::Type type) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
};
