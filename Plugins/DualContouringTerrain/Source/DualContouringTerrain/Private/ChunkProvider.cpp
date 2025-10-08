// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkProvider.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif
#include "OctreeSettings.h"
#include "ChunkProviderSettings.h"
#include "OctreeManager.h"


void UChunkProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);


	//maybe for octree settings changes only rebuild the root object inside chunk
#if WITH_EDITOR
	UOctreeSettings::OnChanged().AddUObject(this, &UChunkProvider::ReloadChunks);
	UChunkProviderSettings::OnChanged().AddUObject(this, &UChunkProvider::ReloadChunks);
#endif

	chunk_settings = GetDefault<UChunkProviderSettings>();
	octree_manager = GetWorld()->GetSubsystem<UOctreeManager>();

	ReloadChunks();
}

void UChunkProvider::Deinitialize()
{
	Super::Deinitialize();

	for (size_t i = 0; i < chunk_grid.chunks.Num(); i++)
	{
		if(chunk_grid.chunks[i].IsSet())
		{
			octree_manager->CleanupChunkMesh(chunk_grid.chunks[i].GetValue().mesh_group_key);
		}
	}
}

void UChunkProvider::ReloadChunks()
{
	chunk_grid.Realloc(chunk_settings->chunk_load_distance);

	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));
	InitializeChunks(current_chunk_coord);
}

void UChunkProvider::InitializeChunks(FIntVector3 current_chunk_coord)
{
	TArray<FIntVector3> local_chunk_indices;
	float size = chunk_settings->chunk_size;
	int32 load_dist = chunk_grid.dim / 2;

	local_chunk_indices.Reserve(chunk_grid.dim*chunk_grid.dim*chunk_grid.dim);
	for (int32 x = -load_dist; x < load_dist+1; x++)
	{
		for (int32 y = -load_dist; y < load_dist+1; y++)
		{
			for (int32 z = -load_dist; z < load_dist+1; z++)
			{
				local_chunk_indices.Add(FIntVector3(x, y, z));
			}
		}
	}

	local_chunk_indices.Sort([](const FIntVector3& a, const FIntVector3& b)
	{
		int64 dist_a = static_cast<int64>(a.X) * a.X + static_cast<int64>(a.Y) * a.Y + static_cast<int64>(a.Z) * a.Z;
		int64 dist_b = static_cast<int64>(b.X) * b.X + static_cast<int64>(b.Y) * b.Y + static_cast<int64>(b.Z) * b.Z;

		return dist_a < dist_b;
	});

	for (int32 i = 0; i < local_chunk_indices.Num(); i++)
	{
		FIntVector3 world_coord = current_chunk_coord + local_chunk_indices[i];

		Chunk& chunk = CreateChunk(world_coord);
		MeshChunk(world_coord, true);
	}
}

void UChunkProvider::BuildSlab(FIntVector3 delta, FIntVector3 current_chunk_coord)
{
	int32 load_dist = chunk_grid.dim / 2;

	if (delta.X)
	{
		bool negative = delta.X < 0;
		for (int32 y = -load_dist; y < load_dist + 1; y++)
		{
			for (int32 z = -load_dist; z < load_dist + 1; z++)
			{
				FIntVector3 coord = FIntVector3(load_dist * delta.X, y, z) + current_chunk_coord;
				Chunk& chunk = CreateChunk(coord);
				MeshChunk(coord, negative);
			}
		}
	}
	if (delta.Y)
	{
		bool negative = delta.Y < 0;
		for (int32 x = -load_dist; x < load_dist + 1; x++)
		{
			for (int32 z = -load_dist; z < load_dist + 1; z++)
			{
				FIntVector3 coord = FIntVector3(x, load_dist * delta.Y, z) + current_chunk_coord;
				Chunk& chunk = CreateChunk(coord);
				MeshChunk(coord, negative);
			}
		}
	}
	if (delta.Z)
	{
		bool negative = delta.Z < 0;
		for (int32 x = -load_dist; x < load_dist + 1; x++)
		{
			for (int32 y = -load_dist; y < load_dist + 1; y++)
			{
				FIntVector3 coord = FIntVector3(x, y, load_dist * delta.Z) + current_chunk_coord;
				Chunk& chunk = CreateChunk(coord);

				MeshChunk(coord, negative);
			}
		}
	}
}

void UChunkProvider::MeshChunk(const FIntVector3& coords, bool negative_delta)
{
	chunk_grid.chunk_polygonize_jobs.Enqueue(MakeTuple(coords, negative_delta));	
}

Chunk& UChunkProvider::CreateChunk(FIntVector3 coord)
{
	float size = chunk_settings->chunk_size;
	int32 flat_idx = chunk_grid.GetStableChunkIndex(coord);

	Chunk chunk;
	chunk.coordinates = coord;
	chunk.center = FVector3f(chunk.coordinates.X * size + size * 0.5f, chunk.coordinates.Y * size + size * 0.5f, chunk.coordinates.Z * size + size * 0.5f);

	FVector3f chunk_center = chunk.center;

	chunk_grid.chunk_creation_jobs.Enqueue(
	[this, chunk_center, size, flat_idx]() -> ChunkCreationResult
	{
		ChunkCreationResult result;
		result.chunk_idx = flat_idx;
		result.created_root = octree_manager->BuildOctree(chunk_center, size);

		return result;
	});

	if (chunk_grid.chunks[flat_idx].IsSet())
	{
		octree_manager->CleanupChunkMesh(chunk_grid.chunks[flat_idx].GetValue().mesh_group_key);
	}
	chunk_grid.chunks[flat_idx] = MoveTemp(chunk);

	return chunk_grid.chunks[flat_idx].GetValue();
}

void UChunkProvider::FillSeamOctreeNodes(TArray<OctreeNode*, TInlineAllocator<8>>& seam_octants, bool negative_delta, const FIntVector3& c, OctreeNode* root)
{
	if (negative_delta)
	{
		//main node 0
		Chunk* chunk_1 = chunk_grid.TryGetChunk(c + FIntVector3(1, 0, 0));
		OctreeNode* octant_1 = chunk_1 ? chunk_1->root : nullptr;

		Chunk* chunk_2 = chunk_grid.TryGetChunk(c + FIntVector3(0, 0, 1));
		OctreeNode* octant_2 = chunk_2 ? chunk_2->root : nullptr;

		Chunk* chunk_3 = chunk_grid.TryGetChunk(c + FIntVector3(1, 0, 1));
		OctreeNode* octant_3 = chunk_3 ? chunk_3->root : nullptr;

		Chunk* chunk_4 = chunk_grid.TryGetChunk(c + FIntVector3(0, 1, 0));
		OctreeNode* octant_4 = chunk_4 ? chunk_4->root : nullptr;

		Chunk* chunk_5 = chunk_grid.TryGetChunk(c + FIntVector3(1, 1, 0));
		OctreeNode* octant_5 = chunk_5 ? chunk_5->root : nullptr;

		Chunk* chunk_6 = chunk_grid.TryGetChunk(c + FIntVector3(0, 1, 1));
		OctreeNode* octant_6 = chunk_6 ? chunk_6->root : nullptr;

		Chunk* chunk_7 = chunk_grid.TryGetChunk(c + FIntVector3(1, 1, 1));
		OctreeNode* octant_7 = chunk_7 ? chunk_7->root : nullptr;

		seam_octants[0] = root;
		seam_octants[1] = octant_1;
		seam_octants[2] = octant_2;
		seam_octants[3] = octant_3;
		seam_octants[4] = octant_4;
		seam_octants[5] = octant_5;
		seam_octants[6] = octant_6;
		seam_octants[7] = octant_7;
	}
	else
	{
		//main node 7

		Chunk* chunk_0 = chunk_grid.TryGetChunk(c + FIntVector3(-1, -1, -1));
		OctreeNode* octant_0 = chunk_0 ? chunk_0->root : nullptr;

		Chunk* chunk_1 = chunk_grid.TryGetChunk(c + FIntVector3(0, -1, -1));
		OctreeNode* octant_1 = chunk_1 ? chunk_1->root : nullptr;

		Chunk* chunk_2 = chunk_grid.TryGetChunk(c + FIntVector3(-1, -1, 0));
		OctreeNode* octant_2 = chunk_2 ? chunk_2->root : nullptr;

		Chunk* chunk_3 = chunk_grid.TryGetChunk(c + FIntVector3(0, -1, 0));
		OctreeNode* octant_3 = chunk_3 ? chunk_3->root : nullptr;

		Chunk* chunk_4 = chunk_grid.TryGetChunk(c + FIntVector3(-1, 0, -1));
		OctreeNode* octant_4 = chunk_4 ? chunk_4->root : nullptr;

		Chunk* chunk_5 = chunk_grid.TryGetChunk(c + FIntVector3(0, 0, -1));
		OctreeNode* octant_5 = chunk_5 ? chunk_5->root : nullptr;

		Chunk* chunk_6 = chunk_grid.TryGetChunk(c + FIntVector3(-1, 0, 0));
		OctreeNode* octant_6 = chunk_6 ? chunk_6->root : nullptr;

		seam_octants[0] = octant_0;
		seam_octants[1] = octant_1;
		seam_octants[2] = octant_2;
		seam_octants[3] = octant_3;
		seam_octants[4] = octant_4;
		seam_octants[5] = octant_5;
		seam_octants[6] = octant_6;
		seam_octants[7] = root;
	}
}

FVector UChunkProvider::GetActiveCameraLocation()
{
	#if WITH_EDITOR
		if (GEditor->IsPlaySessionInProgress())
		{
			if (APlayerCameraManager* camera_manager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
			{
				return camera_manager->GetCameraLocation();
			}
			else 
			{
				//possible fallback
				//UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)->
				return FVector();
			}
			
		}
		else if(GCurrentLevelEditingViewportClient)
		{
			return GCurrentLevelEditingViewportClient->GetViewLocation();
		}
		else 
		{
			return FVector();
		}
	#else 
		if(APlayerCameraManager * camera_manager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			return camera_manager->GetCameraLocation();
		}
		else return FVector();
	#endif
}

void UChunkProvider::Tick(float DeltaTime)
{
	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));

#if WITH_EDITOR
	if(chunk_settings->draw_debug_chunks)
	{
		for (size_t i = 0; i < chunk_grid.chunks.Num(); i++)
		{
			DrawDebugBox(GetWorld(), FVector(chunk_grid.chunks[i].GetValue().center), FVector(static_cast<float>(chunk_settings->chunk_size)*0.5f), FColor::White);
		}
	}

	if(chunk_settings->draw_octree) 
	{
		int32 idx = chunk_grid.GetStableChunkIndex(current_chunk_coord);
		OctreeNode* node = chunk_grid.chunks[idx].GetValue().root;

		octree_manager->DebugDrawOctree(node, 0, chunk_settings->draw_leaves, chunk_settings->draw_simplified_leaves, chunk_settings->debug_draw_how_deep);
	}

#endif

	if(chunk_settings->stop_chunk_loading) return;

	//latent creation and polygonization
	while(!chunk_grid.chunk_creation_jobs.IsEmpty())
	{
		TFunction<ChunkCreationResult()> job;
		chunk_grid.chunk_creation_jobs.Dequeue(job);

		chunk_grid.chunk_creation_tasks.Add(Async(EAsyncExecution::ThreadPool, MoveTemp(job)));
	}
	for (int32 i = 0; i < chunk_grid.chunk_creation_tasks.Num(); i++)
	{
		auto& result = chunk_grid.chunk_creation_tasks[i];
		if(result.IsReady())
		{
			const ChunkCreationResult& creation_result = result.Get();

			chunk_grid.chunks[creation_result.chunk_idx].GetValue().root = creation_result.created_root;

			chunk_grid.chunk_creation_tasks.RemoveAt(i);
			i--; 
		}
	}
	if(chunk_grid.chunk_creation_tasks.IsEmpty())
	{
		while(!chunk_grid.chunk_polygonize_jobs.IsEmpty())
		{
			TTuple<FIntVector3, bool> tuple;
			chunk_grid.chunk_polygonize_jobs.Dequeue(tuple);

			int32 flat_idx = chunk_grid.GetStableChunkIndex(tuple.Key);
			bool negative_delta = tuple.Value;

			OctreeNode* root = chunk_grid.chunks[flat_idx].GetValue().root;

			TArray<OctreeNode*, TInlineAllocator<8>> seam_octants;
			seam_octants.SetNumUninitialized(8);
			FillSeamOctreeNodes(seam_octants, negative_delta, tuple.Key, root);

			chunk_grid.chunk_polygonize_tasks.Add(Async(EAsyncExecution::ThreadPool,
			[this, flat_idx, negative_delta, seam_octants]() -> ChunkPolygonizeResult
			{
				ChunkPolygonizeResult result;
				result.chunk_idx = flat_idx;

				result.created_mesh_key = octree_manager->PolygonizeOctree(seam_octants, negative_delta);

				return result;
			}));
		}
	}
	for (int32 i = 0; i < chunk_grid.chunk_polygonize_tasks.Num(); i++)
	{
		auto& result = chunk_grid.chunk_polygonize_tasks[i];
		if (result.IsReady())
		{
			const ChunkPolygonizeResult& polygonize_result = result.Get();

			chunk_grid.chunks[polygonize_result.chunk_idx].GetValue().mesh_group_key = polygonize_result.created_mesh_key;

			chunk_grid.chunk_polygonize_tasks.RemoveAt(i);
			i--;
		}
	}


	if(chunk_grid.chunk_creation_tasks.IsEmpty() && chunk_grid.chunk_polygonize_tasks.IsEmpty())
	{
		//if somehow we moved multiple chunks in 1 tick OR we just loaded in, just reinit the chunks
		if ((current_chunk_coord - last_chunk_coord).GetAbsMax() > 1)
		{
			InitializeChunks(current_chunk_coord);
		}
		else if (current_chunk_coord != last_chunk_coord)
		{
			FIntVector3 delta = current_chunk_coord - last_chunk_coord;

			BuildSlab(delta, current_chunk_coord);
		}

		last_chunk_coord = current_chunk_coord;
		chunk_grid.min_coord = current_chunk_coord - FIntVector3(chunk_grid.dim / 2);
	}	

}

TStatId UChunkProvider::GetStatId() const
{
	return TStatId();
}

bool UChunkProvider::DoesSupportWorldType(EWorldType::Type type) const
{
	return type == EWorldType::Game || type == EWorldType::Editor || type == EWorldType::PIE;
}

bool UChunkProvider::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* W = Cast<UWorld>(Outer))
	{
		const auto WT = W->WorldType;
		return WT == EWorldType::Game || WT == EWorldType::PIE || WT == EWorldType::Editor;
	}
	return false;
}

bool UChunkProvider::IsTickable() const
{
	return true;
}

bool UChunkProvider::IsTickableInEditor() const
{
	return true;
}

int32 UChunkProvider::ChunkGrid::GetStableChunkIndex(FIntVector3 c) const
{
	int32 x = WrapCoord(c.X);
	int32 y = WrapCoord(c.Y);
	int32 z = WrapCoord(c.Z);

	return Flatten(x, y, z);
}

Chunk* UChunkProvider::ChunkGrid::TryGetChunk(FIntVector3 c)
{
	int32 half = (dim-1) / 2;
	FIntVector3 local_c = c - min_coord - FIntVector3(half);

	if(local_c.X > half || local_c.X < -half || local_c.Y > half || local_c.Y < -half || local_c.Z > half || local_c.Z < -half)
	{
		return nullptr;
	}

	int32 idx = GetStableChunkIndex(c);

	return &chunks[idx].GetValue();
}

void UChunkProvider::ChunkGrid::Realloc(int32 new_load_distance)
{
	dim = (new_load_distance*2)+1;
	chunks.SetNum(dim*dim*dim);

	chunk_creation_tasks.Reserve((dim * dim) + (dim * dim) + (dim * dim));
	chunk_polygonize_tasks.Reserve((dim*dim) + (dim*dim) + (dim*dim));
}