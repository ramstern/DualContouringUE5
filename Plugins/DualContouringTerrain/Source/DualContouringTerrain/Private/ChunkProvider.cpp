// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkProvider.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditorViewport.h"
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

		Chunk chunk;
		chunk.center = FVector3f(world_coord.X * size + size * 0.5f, world_coord.Y * size + size * 0.5f, world_coord.Z * size + size * 0.5f);
		chunk.coordinates = world_coord;
		chunk.root = octree_manager->BuildOctree(chunk.center, size);

		int32 idx = chunk_grid.GetStableChunkIndex(chunk.coordinates);

		chunk_grid.chunks[idx] = MoveTemp(chunk);
	}
}

void UChunkProvider::BuildSlab(FIntVector3 delta, FIntVector3 current_chunk_coord)
{
	int32 load_dist = chunk_grid.dim / 2;
	TSet<FIntVector3> rebuilds;

	uint8 count = !!delta.X + !!delta.Y + !!delta.Z;

	rebuilds.Reserve(count * (chunk_grid.dim*chunk_grid.dim));

	if (delta.X)
	{
		for (int32 y = -load_dist; y < load_dist + 1; y++)
		{
			for (int32 z = -load_dist; z < load_dist + 1; z++)
			{
				FIntVector3 coord = FIntVector3(load_dist * delta.X, y, z) + current_chunk_coord;
				rebuilds.Add(coord);
			}
		}
	}
	if (delta.Y)
	{
		for (int32 x = -load_dist; x < load_dist + 1; x++)
		{
			for (int32 z = -load_dist; z < load_dist + 1; z++)
			{
				FIntVector3 coord = FIntVector3(x, load_dist * delta.Y, z) + current_chunk_coord;
				rebuilds.Add(coord);
			}
		}
	}
	if (delta.Z)
	{
		for (int32 x = -load_dist; x < load_dist + 1; x++)
		{
			for (int32 y = -load_dist; y < load_dist + 1; y++)
			{
				FIntVector3 coord = FIntVector3(x, y, load_dist * delta.Z) + current_chunk_coord;
				rebuilds.Add(coord);
			}
		}
	}

	float size = chunk_settings->chunk_size;
	for (auto& coord : rebuilds)
	{
		int32 flat_idx = chunk_grid.GetStableChunkIndex(coord);

		Chunk chunk;
		chunk.coordinates = coord;
		chunk.center = FVector3f(chunk.coordinates.X * size + size * 0.5f, chunk.coordinates.Y * size + size * 0.5f, chunk.coordinates.Z * size + size * 0.5f);
		chunk.root = octree_manager->BuildOctree(chunk.center, size);

		chunk_grid.chunks[flat_idx] = MoveTemp(chunk);
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
		return APlayerCameraManager * camera_manager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraLocation();
	#endif
}

void UChunkProvider::Tick(float DeltaTime)
{
#if WITH_EDITOR
	if(chunk_settings->draw_debug_chunks)
	{
		for (size_t i = 0; i < chunk_grid.chunks.Num(); i++)
		{
			DrawDebugBox(GetWorld(), FVector(chunk_grid.chunks[i].GetValue().center), FVector(static_cast<float>(chunk_settings->chunk_size)*0.5f), FColor::White);
		}
	}
#endif

	FVector cam_pos = GetActiveCameraLocation();

	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));

	chunk_grid.min_coord = current_chunk_coord - FIntVector3(chunk_grid.dim/2);

	//if somehow we moved multiple chunks in 1 tick OR we just loaded in, just reinit the chunks
	if((current_chunk_coord - last_chunk_coord).GetAbsMax() > 1) 
	{
		InitializeChunks(current_chunk_coord);
	}
	else if(current_chunk_coord != last_chunk_coord)
	{
		FIntVector3 delta = current_chunk_coord - last_chunk_coord;

		BuildSlab(delta, current_chunk_coord);
	}

	last_chunk_coord = current_chunk_coord;
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

int32 UChunkProvider::ChunkGrid::GetStableChunkIndex(FIntVector3 c)
{
	int32 x = WrapCoord(c.X);
	int32 y = WrapCoord(c.Y);
	int32 z = WrapCoord(c.Z);

	return Flatten(x, y, z);
}

void UChunkProvider::ChunkGrid::Realloc(int32 new_load_distance)
{
	dim = (new_load_distance*2)+1;
	chunks.SetNum(dim*dim*dim);
}