// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_ChunkProvider.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif
#include "DC_OctreeSettings.h"
#include "DC_ChunkProviderSettings.h"
#include "DC_OctreeCode.h"
#include "DC_OctreeRenderActor.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"


#define USE_NAMED_STATS 1
#define USE_MULTITHREADING 1

void UChunkProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UOctreeSettings::OnChanged().AddUObject(this, &UChunkProvider::ReloadChunks);
	UChunkProviderSettings::OnChanged().AddUObject(this, &UChunkProvider::ReloadReallocChunks);

#if WITH_EDITOR

	if(GetWorld()->WorldType == EWorldType::Editor)
	{
		FEditorDelegates::BeginPIE.AddUObject(this, &UChunkProvider::Cleanup);
		FEditorDelegates::EndPIE.AddUObject(this, &UChunkProvider::Init);
	}
#endif 
	Init(false);
}

void UChunkProvider::Init(bool simulating)
{
	UE_LOG(LogTemp, Display, TEXT(" INIT called on: %i"), GetWorld()->WorldType.GetIntValue());

	//Collection.InitializeDependency(UAOctreeCode::StaticClass());
	chunk_settings = GetDefault<UChunkProviderSettings>();
	octree_manager = GEngine->GetEngineSubsystem<UOctreeCode>();

	FActorSpawnParameters Params;
	Params.Name = TEXT("DC_OctreeRenderActor");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags = RF_Transient;                // don’t save to map
	Params.bNoFail = true;

#if WITH_EDITOR
	//Params.bHideFromSceneOutliner = true;
#endif

	ADC_OctreeRenderActor* created_render_actor = GetWorld()->SpawnActor<ADC_OctreeRenderActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (created_render_actor)
	{
#if WITH_EDITOR
		created_render_actor->bIsEditorOnlyActor = (GetWorld()->WorldType == EWorldType::Editor);
		created_render_actor->SetActorLabel(TEXT("Octree Render (Transient)"));
		created_render_actor->ClearFlags(EObjectFlags::RF_Transactional);

#endif
		if (UActorComponent* root = created_render_actor->GetRootComponent())
		{
			root->SetFlags(RF_Transient);
			root->ClearFlags(RF_Transactional);                             // <- important
		}

		render_actor = created_render_actor;
	}

	ReloadReallocChunks();
}
void UChunkProvider::Cleanup(bool simulating)
{
	UE_LOG(LogTemp, Display, TEXT(" CLEANUP called on: %i"), GetWorld()->WorldType.GetIntValue());

	chunk_grid.chunk_creation_jobs.Empty();
	//for (int32 i = 0; i < chunk_grid.chunk_creation_tasks.Num(); i++)
	//{
	//	auto& future = chunk_grid.chunk_creation_tasks[i];
	//	auto result = future.Consume();
	///*	OctreeNode* possible_root = result.created_root;
	//	if(possible_root)
	//	{
	//		delete possible_root;
	//		possible_root = nullptr;
	//	}*/
	//}
	chunk_grid.chunk_creation_tasks.Empty();

	chunk_grid.chunk_polygonize_jobs.Empty();
	/*for (int32 i = 0; i < chunk_grid.chunk_polygonize_tasks.Num(); i++)
	{
		auto& future = chunk_grid.chunk_polygonize_tasks[i];
		future.Consume();
	}*/
	chunk_grid.chunk_polygonize_tasks.Empty();

	chunk_grid.chunks.Empty();

	render_actor->Destroy();
	render_actor = nullptr;
}

void UChunkProvider::Deinitialize()
{
	Super::Deinitialize();
	
	UOctreeSettings::OnChanged().RemoveAll(this);
	UChunkProviderSettings::OnChanged().RemoveAll(this);

#if WITH_EDITOR
	if(GetWorld()->WorldType == EWorldType::Editor)
	{
		FEditorDelegates::BeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);
	}
#endif
	Cleanup(false);	
}

void UChunkProvider::ReloadChunks()
{
	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));

	last_chunk_coord.X += 100;
}

void UChunkProvider::ReloadReallocChunks()
{
	if (!IsSafeToModifyChunks()) return;

	chunk_grid.Realloc(chunk_settings->chunk_load_distance);

	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));

	last_chunk_coord.X += 100;
}


void UChunkProvider::BuildChunkArea(FIntVector3 current_chunk_coord)
{
	TArray<FIntVector3> local_chunk_indices;
	float size = chunk_settings->chunk_size;
	int32 load_dist = (chunk_grid.dim-1) / 2;

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

		if(chunk_grid.chunks.Contains(world_coord)) continue;

		CreateChunk(world_coord);
		MeshChunk(world_coord, true);
	}
}

void UChunkProvider::BuildSlabs(FIntVector3 delta, FIntVector3 current_chunk_coord)
{
	int32 load_dist = (chunk_grid.dim-1) / 2;

	TSet<TTuple<FIntVector3, bool>> build_coords;
	//in most cases, one slab
	build_coords.Reserve(chunk_grid.dim*chunk_grid.dim);

	if (delta.X)
	{
		bool negative = delta.X < 0;
		for (int32 y = -load_dist; y < load_dist + 1; y++)
		{
			for (int32 z = -load_dist; z < load_dist + 1; z++)
			{
				FIntVector3 coord = FIntVector3(load_dist * delta.X, y, z) + current_chunk_coord;
				build_coords.Emplace(MakeTuple(coord, negative));
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
				build_coords.Emplace(MakeTuple(coord, negative));
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
				build_coords.Emplace(MakeTuple(coord, negative));
			}
		}
	}

	for (auto& t : build_coords)
	{
		if(chunk_grid.chunks.Contains(t.Key)) continue;

		CreateChunk(t.Key);
		MeshChunk(t.Key, t.Value);
	}
}

void UChunkProvider::MeshChunk(const FIntVector3& coords, bool negative_delta)
{
	//return;
	chunk_grid.chunk_polygonize_jobs.Enqueue(MakeTuple(coords, negative_delta));	
}

void UChunkProvider::CreateChunk(FIntVector3 coord)
{
	check(!chunk_grid.chunks.Contains(coord));

	float size = chunk_settings->chunk_size;

	/*if(chunk_grid.chunks[coord].mesh)
	{
		auto rmc = static_cast<URealtimeMeshComponent*>(chunk_grid.chunks[flat_idx].mesh->GetOuter());
		render_actor->ReleaseRMC(rmc);
	}*/

	URealtimeMeshSimple* fetched_mesh;
	bool newly_created = render_actor->FetchRMComponentMesh(fetched_mesh);

	Chunk chunk;
	chunk.center = FVector3f(coord.X * size + size * 0.5f, coord.Y * size + size * 0.5f, coord.Z * size + size * 0.5f);
	chunk.mesh = fetched_mesh;

	FVector3f chunk_center = chunk.center;

	OctreeSettingsMultithreadContext settings_context;
	settings_context = *GetDefault<UOctreeSettings>();

	chunk_grid.chunk_creation_jobs.Enqueue(
	[this, coord,chunk_center, size, settings_context, newly_created]() -> ChunkCreationResult
	{
		ChunkCreationResult result;
		result.chunk_coord = coord;
		result.created_root = octree_manager->BuildOctree(chunk_center, size, settings_context);
		result.newly_created = newly_created;

		return result;
	});

	chunk_grid.chunks.Add(coord, MoveTemp(chunk));
}

bool UChunkProvider::IsSafeToModifyChunks()
{
	bool tasks_empty = chunk_grid.chunk_creation_tasks.IsEmpty() && chunk_grid.chunk_polygonize_tasks.IsEmpty();
	bool jobs_empty = chunk_grid.chunk_creation_jobs.IsEmpty() && chunk_grid.chunk_polygonize_jobs.IsEmpty();
	//bool sections_empty = chunk_grid.chunk_section_tasks.IsEmpty();

	return tasks_empty && jobs_empty; //&& sections_empty;
}

void UChunkProvider::FillSeamOctreeNodes(TArray<OctreeNode*, TInlineAllocator<8>>& seam_octants, bool negative_delta, const FIntVector3& c, OctreeNode* root)
{
	if (negative_delta)
	{
		//main node 0
		Chunk* chunk_1 = chunk_grid.TryGet(c + FIntVector3(1, 0, 0));
		OctreeNode* octant_1 = chunk_1 ? chunk_1->root.Get() : nullptr;

		Chunk* chunk_2 = chunk_grid.TryGet(c + FIntVector3(0, 0, 1));
		OctreeNode* octant_2 = chunk_2 ? chunk_2->root.Get() : nullptr;

		Chunk* chunk_3 = chunk_grid.TryGet(c + FIntVector3(1, 0, 1));
		OctreeNode* octant_3 = chunk_3 ? chunk_3->root.Get() : nullptr;

		Chunk* chunk_4 = chunk_grid.TryGet(c + FIntVector3(0, 1, 0));
		OctreeNode* octant_4 = chunk_4 ? chunk_4->root.Get() : nullptr;

		Chunk* chunk_5 = chunk_grid.TryGet(c + FIntVector3(1, 1, 0));
		OctreeNode* octant_5 = chunk_5 ? chunk_5->root.Get() : nullptr;

		Chunk* chunk_6 = chunk_grid.TryGet(c + FIntVector3(0, 1, 1));
		OctreeNode* octant_6 = chunk_6 ? chunk_6->root.Get() : nullptr;

		Chunk* chunk_7 = chunk_grid.TryGet(c + FIntVector3(1, 1, 1));
		OctreeNode* octant_7 = chunk_7 ? chunk_7->root.Get() : nullptr;

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

		Chunk* chunk_0 = chunk_grid.TryGet(c + FIntVector3(-1, -1, -1));
		OctreeNode* octant_0 = chunk_0 ? chunk_0->root.Get() : nullptr;

		Chunk* chunk_1 = chunk_grid.TryGet(c + FIntVector3(0, -1, -1));
		OctreeNode* octant_1 = chunk_1 ? chunk_1->root.Get() : nullptr;

		Chunk* chunk_2 = chunk_grid.TryGet(c + FIntVector3(-1, -1, 0));
		OctreeNode* octant_2 = chunk_2 ? chunk_2->root.Get() : nullptr;

		Chunk* chunk_3 = chunk_grid.TryGet(c + FIntVector3(0, -1, 0));
		OctreeNode* octant_3 = chunk_3 ? chunk_3->root.Get() : nullptr;

		Chunk* chunk_4 = chunk_grid.TryGet(c + FIntVector3(-1, 0, -1));
		OctreeNode* octant_4 = chunk_4 ? chunk_4->root.Get() : nullptr;

		Chunk* chunk_5 = chunk_grid.TryGet(c + FIntVector3(0, 0, -1));
		OctreeNode* octant_5 = chunk_5 ? chunk_5->root.Get() : nullptr;

		Chunk* chunk_6 = chunk_grid.TryGet(c + FIntVector3(-1, 0, 0));
		OctreeNode* octant_6 = chunk_6 ? chunk_6->root.Get() : nullptr;

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
	//we need this, as otherwise it will tick twice when PIE' ing
#if WITH_EDITOR
	bool isPIE = GEditor->PlayWorld != nullptr;
	if(isPIE && (GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::EditorPreview)) return; 
#endif

	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));
	chunk_grid.min_coord = current_chunk_coord - FIntVector3(chunk_grid.dim / 2);

#if WITH_EDITOR
	if(chunk_settings->draw_debug_chunks)
	{
		for (const auto& pair : chunk_grid.chunks)
		{
			FVector3f chunk_center = pair.Value.center;

			FIntVector3 c = pair.Key;

			int32 dist = FMath::Sqrt(static_cast<float>((c.X - current_chunk_coord.X)*(c.X-current_chunk_coord.X) + (c.Y - current_chunk_coord.Y) * (c.Y - current_chunk_coord.Y) + (c.Z - current_chunk_coord.Z) * (c.Z - current_chunk_coord.Z)));

			if(dist > chunk_settings->chunk_draw_max_dist) continue; 

			DrawDebugBox(GetWorld(), FVector(chunk_center), FVector(static_cast<float>(chunk_settings->chunk_size)*0.5f), FColor::White);
		}
		GEditor->AddOnScreenDebugMessage(144, 0.1f, FColor::White, current_chunk_coord.ToString());
	}

	if(chunk_settings->draw_octree) 
	{
		OctreeNode* node = chunk_grid.Get(current_chunk_coord).root.Get();

		octree_manager->DebugDrawOctree(node, 0, chunk_settings->draw_leaves, chunk_settings->draw_simplified_leaves, chunk_settings->debug_draw_how_deep);
	}

#endif

	if(chunk_settings->stop_chunk_loading) return;

	//latent creation and polygonization
	while(!chunk_grid.chunk_creation_jobs.IsEmpty())
	{
		TFunction<ChunkCreationResult()> job;
		chunk_grid.chunk_creation_jobs.Dequeue(job);

#if USE_MULTITHREADING
		chunk_grid.chunk_creation_tasks.Add(Async(EAsyncExecution::LargeThreadPool, MoveTemp(job)));
#else 
		const ChunkCreationResult result = job();
		chunk_grid.chunks[result.chunk_idx].root = result.created_root;
#endif
	}
	for (int32 i = 0; i < chunk_grid.chunk_creation_tasks.Num(); i++)
	{
		auto& result = chunk_grid.chunk_creation_tasks[i];
		if(result.IsReady())
		{
			const ChunkCreationResult creation_result = result.Consume();

			Chunk& chunk = chunk_grid.GetMutable(creation_result.chunk_coord);
			chunk.root = TUniquePtr<OctreeNode>(creation_result.created_root);
			chunk.newly_created = creation_result.newly_created;

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

			const Chunk& chunk = chunk_grid.Get(tuple.Key);
			bool negative_delta = tuple.Value;

			OctreeNode* root = chunk.root.Get();
			bool newly_created = chunk.newly_created;

			TArray<OctreeNode*, TInlineAllocator<8>> seam_octants;
			seam_octants.SetNumUninitialized(8);
			FillSeamOctreeNodes(seam_octants, negative_delta, tuple.Key, root);

			URealtimeMeshSimple* chunk_mesh = chunk.mesh;

#if USE_MULTITHREADING
			chunk_grid.chunk_polygonize_tasks.Add(Async(EAsyncExecution::LargeThreadPool,
			[this, coord = tuple.Key, negative_delta, seam_octants, chunk_mesh, newly_created]() -> ChunkPolygonizeResult
			{
				ChunkPolygonizeResult result;

				int32 hash_1 = HashCombineFast(coord.X, coord.Y);
				int32 hash_2 = HashCombineFast(hash_1, coord.Z);

				FRealtimeMeshSectionGroupKey mesh_group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh", hash_2));

				RealtimeMesh::FRealtimeMeshStreamSet stream_set = octree_manager->PolygonizeOctree(seam_octants, negative_delta);

				//create / update mesh section of chunk
				
				if(newly_created)
				{
					result.mesh_future = chunk_mesh->CreateSectionGroup(mesh_group_key, MoveTemp(stream_set));
				}
				else 
				{
					result.mesh_future = chunk_mesh->UpdateSectionGroup(mesh_group_key, MoveTemp(stream_set));
				}

				FRealtimeMeshSectionKey section_key = FRealtimeMeshSectionKey::Create(mesh_group_key, FName("Section_PolyGroup"));
				result.collision_future = chunk_mesh->UpdateSectionConfig(section_key, FRealtimeMeshSectionConfig(), true);

				return result;
			}));
#else 
			chunk_grid.chunks[flat_idx].mesh_group_key = octree_manager->PolygonizeOctree(seam_octants, negative_delta, flat_idx, has_section_group);
#endif
		}
	}
	for (int32 i = 0; i < chunk_grid.chunk_polygonize_tasks.Num(); i++)
	{
		auto& result = chunk_grid.chunk_polygonize_tasks[i];
		if (result.IsReady() && result.Get().collision_future.IsReady() && result.Get().mesh_future.IsReady())
		{
#if USE_NAMED_STATS
			QUICK_SCOPE_CYCLE_COUNTER(Stat_CreateOrUpdateMesh)
#endif
			chunk_grid.chunk_polygonize_tasks.RemoveAt(i);
			i--;
		}
	}

	if(IsSafeToModifyChunks())
	{
		//if somehow we moved multiple chunks in 1 tick OR we just loaded in, just reinit the chunks
		if ((current_chunk_coord - last_chunk_coord).GetAbsMax() > 1)
		{
#if WITH_EDITOR
			if(!isPIE)
			{
				//CollectGarbage(EObjectFlags::RF_NoFlags);
			}
#endif
			BuildChunkArea(current_chunk_coord);
		}
		else if (current_chunk_coord != last_chunk_coord)
		{
#if WITH_EDITOR
			if(!isPIE)
			{
				//CollectGarbage(EObjectFlags::RF_NoFlags);
			}
#endif
			FIntVector3 delta = current_chunk_coord - last_chunk_coord;

			BuildSlabs(delta, current_chunk_coord);
		}

		last_chunk_coord = current_chunk_coord;
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

Chunk* UChunkProvider::ChunkGrid::TryGet(FIntVector3 c)
{
	return chunks.Find(c);
}

const Chunk& UChunkProvider::ChunkGrid::Get(FIntVector3 c)
{
	return chunks.FindChecked(c);
}

Chunk& UChunkProvider::ChunkGrid::GetMutable(FIntVector3 c)
{
	return chunks.FindChecked(c);
}

void UChunkProvider::ChunkGrid::Realloc(int32 new_load_distance)
{
	dim = (new_load_distance*2)+1;

	chunk_creation_tasks.Reserve(dim * dim * dim);
	chunk_polygonize_tasks.Reserve(dim * dim * dim);
}