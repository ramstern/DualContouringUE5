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
#include "DC_NoiseDataGenerator.h"

#define USE_NAMED_STATS 1

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

	chunk_settings = GetDefault<UChunkProviderSettings>();
	octree_manager = GEngine->GetEngineSubsystem<UOctreeCode>();

	chunk_settings->terrain_material.LoadSynchronous();

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

	Init(false);
}

void UChunkProvider::Init(bool simulating)
{
	UE_LOG(LogTemp, Display, TEXT(" INIT called on: %i"), GetWorld()->WorldType.GetIntValue());
	
	thread_pool = FQueuedThreadPool::Allocate();
	const int32 thread_num = FMath::Max(1, FPlatformMisc::NumberOfWorkerThreadsToSpawn() - 2);
	const int32 stack_size = 256*1024;
	const EThreadPriority thread_prio = EThreadPriority::TPri_Normal;
	thread_pool->Create(thread_num, stack_size, thread_prio, TEXT("DC_ThreadPool"));

	/*Params.Name = TEXT("TerrainFollowTest");
	test_follow_actor = GetWorld()->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	test_follow_actor->SetRootComponent( NewObject<USceneComponent>(test_follow_actor) );*/
	ReloadReallocChunks();
}
void UChunkProvider::Cleanup(bool simulating)
{
	UE_LOG(LogTemp, Display, TEXT(" CLEANUP called on: %i"), GetWorld()->WorldType.GetIntValue());

	chunk_grid.Cleanup();

	render_actor->DestroyAllRMCs();

	thread_pool->Destroy();
	delete thread_pool;
	thread_pool = nullptr;

	FlushRenderingCommands();
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

	render_actor->Destroy();
	render_actor = nullptr;
}

void UChunkProvider::ReloadChunks()
{
	chunk_grid.Cleanup();

	render_actor->DestroyAllRMCs();

	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));
	
	build_initial_area = true;
}

void UChunkProvider::ReloadReallocChunks()
{
	chunk_grid.Cleanup();

	render_actor->DestroyAllRMCs();

	chunk_grid.Realloc(chunk_settings->chunk_load_distance);

	FVector cam_pos = GetActiveCameraLocation();
	FIntVector current_chunk_coord = GetChunkCoordinatesFromPosition(FVector3f(cam_pos));
	
	build_initial_area = true;
}

void UChunkProvider::BuildChunkArea(FIntVector3 current_chunk_coord)
{
	TArray<FIntVector3> chunk_coords = GetChunkArea(current_chunk_coord);

	for (int32 i = 0; i < chunk_coords.Num(); i++)
	{
		FIntVector3 world_coord = chunk_coords[i];

		if(chunk_grid.chunks.Contains(world_coord)) continue;

		CreateChunk(world_coord);
		MeshChunk(world_coord, PolygonizeTaskArg::Area);
	}
}

TArray<FIntVector3> UChunkProvider::GetChunkArea(FIntVector3 around)
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
				//if(x*x + y*y + z*z > load_dist*load_dist) continue;

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

	for (auto& c : local_chunk_indices)
	{
		c += around; 
	}

	return local_chunk_indices;
}

TArray<float> UChunkProvider::BuildNoiseField(const FVector3f& center, float size, int32 max_depth, int32 noise_seed)
{
	int32 dim = UOctreeCode::GetDim(max_depth) + 1;

	TArray<float> x_pos, y_pos, z_pos;
	x_pos.SetNumUninitialized(dim * dim * dim);
	y_pos.SetNumUninitialized(dim * dim * dim);
	z_pos.SetNumUninitialized(dim * dim * dim);

	FVector3f min = center - size * 0.5f;
	float vox_size = size / (dim - 1);

	for (int32 x = 0; x < dim; x++)
	{
		for (int32 y = 0; y < dim; y++)
		{
			for (int32 z = 0; z < dim; z++)
			{
				int32 idx = UOctreeCode::Get1DIndexFrom3D(x, y, z, dim);
				x_pos[idx] = (min.X + vox_size * x) * 0.01f;
				y_pos[idx] = (min.Y + vox_size * y) * 0.01f;
				z_pos[idx] = (min.Z + vox_size * z) * 0.01f;
			}
		}
	}

	TArray<float> noise;
	{
		noise = UNoiseDataGenerator::GetNoiseFromPositions3D_NonThreaded(x_pos.GetData(), y_pos.GetData(), z_pos.GetData(), dim * dim * dim, noise_seed);
	}

	return noise;
}

void UChunkProvider::EditNoiseField(TArray<float>& noise, const FVector3f& center, float size, int32 max_depth, const FSDFOp& sdf_op)
{
	int32 dim = UOctreeCode::GetDim(max_depth) + 1;

	TArray<float> x_pos, y_pos, z_pos;
	x_pos.SetNumUninitialized(dim * dim * dim);
	y_pos.SetNumUninitialized(dim * dim * dim);
	z_pos.SetNumUninitialized(dim * dim * dim);

	FVector3f min = center - size * 0.5f;
	float vox_size = size / (dim - 1);

	for (int32 x = 0; x < dim; x++)
	{
		for (int32 y = 0; y < dim; y++)
		{
			for (int32 z = 0; z < dim; z++)
			{
				int32 idx = UOctreeCode::Get1DIndexFrom3D(x, y, z, dim);
				x_pos[idx] = (min.X + vox_size * x) * 0.01f;
				y_pos[idx] = (min.Y + vox_size * y) * 0.01f;
				z_pos[idx] = (min.Z + vox_size * z) * 0.01f;
			}
		}
	}

	for (int32 i = 0; i < dim * dim * dim; i++)
	{
		FVector3f point_pos = FVector3f(x_pos[i], y_pos[i], z_pos[i]);

		FVector3f local_pos = point_pos - (sdf_op.position * 0.01f);

		float sdf_val = 0.f;
		switch (sdf_op.sdf_type)
		{
		case SDFType::Box:
			sdf_val = SDF::Box(local_pos, ((sdf_op.bounds_size * 0.5f) * 0.01f));
			break;
		case SDFType::Sphere:
			sdf_val = SDF::Sphere(local_pos, (sdf_op.bounds_size.X*0.5f) * 0.01f);
			break;
		}

		switch (sdf_op.mod_type)
		{
		case ModType::Subtract:
			noise[i] = FMath::Max(noise[i], -sdf_val);
			break;
		case ModType::Union:
			noise[i] = FMath::Min(noise[i], sdf_val);
			break;
		}
	}
}

void UChunkProvider::BuildSlabs(FIntVector3 delta, FIntVector3 current_chunk_coord)
{
	int32 load_dist = (chunk_grid.dim-1) / 2;

	TSet<TTuple<FIntVector3, PolygonizeTaskArg>> build_coords;
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
				build_coords.Emplace(MakeTuple(coord, negative ? PolygonizeTaskArg::SlabNegative : PolygonizeTaskArg::SlabPositive));
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
				build_coords.Emplace(MakeTuple(coord, negative ? PolygonizeTaskArg::SlabNegative : PolygonizeTaskArg::SlabPositive));
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
				build_coords.Emplace(MakeTuple(coord, negative ? PolygonizeTaskArg::SlabNegative : PolygonizeTaskArg::SlabPositive));
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

void UChunkProvider::MeshChunk(const FIntVector3& coords, PolygonizeTaskArg task_arg)
{
	chunk_grid.chunk_polygonize_jobs.Enqueue(MakeTuple(coords, task_arg));	
}

void UChunkProvider::CreateChunk(FIntVector3 coord)
{
	checkSlow(!chunk_grid.chunks.Contains(coord));

	auto info  = render_actor->FetchRMComponentInfo(chunk_settings->terrain_material.Get());

	float size = chunk_settings->chunk_size;

	Chunk chunk;
	chunk.center = FVector3f(coord.X * size + size * 0.5f, coord.Y * size + size * 0.5f, coord.Z * size + size * 0.5f);
	chunk.mesh = info.mesh;
	chunk.rmc_newly_created = !info.pooled;
	chunk.has_section_built = info.has_section;

	chunk_grid.chunks.Add(coord, MoveTemp(chunk));

	CreationTaskArg task_arg = info.pooled ? CreationTaskArg::NewlyCreated : CreationTaskArg::Update;

	//building octree job
	chunk_grid.chunk_creation_jobs.Enqueue(MakeTuple(coord, task_arg));
}

void UChunkProvider::RebuildChunk(FIntVector3 coord)
{
	checkSlow(chunk_grid.chunks.Contains(coord));

	Chunk& chunk = chunk_grid.GetMutable(coord);
	chunk.root.Reset();

	chunk_grid.chunk_creation_jobs.Enqueue(MakeTuple(coord, CreationTaskArg::ModifyOperation));
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

void UChunkProvider::DrainChunkBuildQueues()
{
	const uint32 per_frame_polygonize_dispatch_count = 10;
	uint32 dispatch_counter = 0;

	//latent creation and polygonization
	while (!chunk_grid.chunk_creation_jobs.IsEmpty())
	{
		TTuple<FIntVector, CreationTaskArg> tuple;
		chunk_grid.chunk_creation_jobs.Dequeue(tuple);

		Chunk& chunk = chunk_grid.GetMutable(tuple.Key);

		FVector3f chunk_center = chunk.center;

		OctreeSettingsMultithreadContext settings_context;
		settings_context = *GetDefault<UOctreeSettings>();

		float size = chunk_settings->chunk_size;

		CreationTaskArg task_arg = tuple.Value;

		if (task_arg == CreationTaskArg::ModifyOperation)
		{
			FSDFOp op;
			chunk_grid.modify_operations.Dequeue(op);

			chunk_grid.GetMutable(tuple.Key).sdf_ops.Add(op);

			chunk_grid.chunk_creation_tasks.Add(AsyncPool(*thread_pool, [this, coord = tuple.Key, chunk_center, size, settings_context, op, &noise_field = chunk.noise_field, &sdf_ops = chunk.sdf_ops]() -> ChunkCreationResult
				{
					ChunkCreationResult result;
					result.chunk_coord = coord;

					EditNoiseField(noise_field, chunk_center, size, settings_context.max_depth, op);

					result.created_root = UOctreeCode::RebuildOctree(chunk_center, size, settings_context, noise_field, sdf_ops);
					result.chunk_update = true;

					return result;
				}));
		}
		else
		{
			chunk_grid.chunk_creation_tasks.Add(AsyncPool(*thread_pool, [this, coord = tuple.Key, chunk_center, size, settings_context]() -> ChunkCreationResult
				{
					ChunkCreationResult result;
					result.chunk_coord = coord;
					result.noise_field = BuildNoiseField(chunk_center, size, settings_context.max_depth, settings_context.seed);
					result.created_root = UOctreeCode::BuildOctree(chunk_center, size, settings_context, result.noise_field);
					result.chunk_update = false;

					return result;
				}));
		}
	}
	for (int32 i = 0; i < chunk_grid.chunk_creation_tasks.Num(); i++)
	{
		auto& result = chunk_grid.chunk_creation_tasks[i];
		if (result.IsReady())
		{
			ChunkCreationResult creation_result = result.Consume();

			Chunk& chunk = chunk_grid.GetMutable(creation_result.chunk_coord);
			chunk.root = MoveTemp(creation_result.created_root);
			//chunk.rmc_newly_created = creation_result.task_arg == CreationTaskArg::NewlyCreated;

			if (!creation_result.chunk_update)
			{
				chunk.noise_field = MoveTemp(creation_result.noise_field);
			}

			temp_created_chunks.Add(creation_result.chunk_coord);

			chunk_grid.chunk_creation_tasks.RemoveAt(i);
			i--;
		}
	}
	if (chunk_grid.chunk_creation_tasks.IsEmpty())
	{
		while (!chunk_grid.chunk_polygonize_jobs.IsEmpty() && dispatch_counter < per_frame_polygonize_dispatch_count)
		{
			TTuple<FIntVector3, PolygonizeTaskArg> tuple;
			chunk_grid.chunk_polygonize_jobs.Dequeue(tuple);

			Chunk& chunk = chunk_grid.GetMutable(tuple.Key);
			FIntVector3 coord = tuple.Key;
			PolygonizeTaskArg task_arg = tuple.Value;

			if(chunk.root.IsValid())
			{
				bool edge_case = false;
				if (task_arg != PolygonizeTaskArg::Area)
				{
					if (task_arg == PolygonizeTaskArg::RebuildAllSeams) edge_case = true;
					else if (task_arg == PolygonizeTaskArg::SlabNegative)
					{
						Chunk* back;
						Chunk* down;
						Chunk* left;

						if (temp_created_chunks.Contains(coord + FIntVector3(-1, 0, 0)))
						{
							back = nullptr;
						}
						else
						{
							back = chunk_grid.TryGet(coord + FIntVector3(-1, 0, 0));
						}

						if (temp_created_chunks.Contains(coord + FIntVector3(0, 0, -1)))
						{
							down = nullptr;
						}
						else
						{
							down = chunk_grid.TryGet(coord + FIntVector3(0, 0, -1));
						}

						if (temp_created_chunks.Contains(coord + FIntVector3(0, -1, 0)))
						{
							left = nullptr;
						}
						else
						{
							left = chunk_grid.TryGet(coord + FIntVector3(0, -1, 0));
						}

						edge_case = back || down || left;
					}
					else
					{
						Chunk* front;
						Chunk* up;
						Chunk* right;

						if (temp_created_chunks.Contains(coord + FIntVector3(1, 0, 0)))
						{
							front = nullptr;
						}
						else
						{
							front = chunk_grid.TryGet(coord + FIntVector3(1, 0, 0));
						}

						if (temp_created_chunks.Contains(coord + FIntVector3(0, 0, 1)))
						{
							up = nullptr;
						}
						else
						{
							up = chunk_grid.TryGet(coord + FIntVector3(0, 0, 1));
						}

						if (temp_created_chunks.Contains(coord + FIntVector3(0, 1, 0)))
						{
							right = nullptr;
						}
						else
						{
							right = chunk_grid.TryGet(coord + FIntVector3(0, 1, 0));
						}

						edge_case = front || up || right;
					}
				}

				bool negative_delta = (task_arg == PolygonizeTaskArg::SlabNegative);

				OctreeNode* root = chunk.root.Get();
				bool rmc_newly_created = chunk.rmc_newly_created;
				bool has_section_built = chunk.has_section_built;

				TArray<OctreeNode*, TInlineAllocator<8>> seam_octants;
				seam_octants.SetNumUninitialized(8);
				FillSeamOctreeNodes(seam_octants, negative_delta, coord, root);

				URealtimeMeshSimple* chunk_mesh = chunk.mesh;

				if (edge_case)
				{
					TArray<OctreeNode*, TInlineAllocator<8>> ec_seam_octants;
					ec_seam_octants.SetNumUninitialized(8);
					FillSeamOctreeNodes(ec_seam_octants, !negative_delta, coord, root);

					chunk_grid.chunk_polygonize_tasks.Add(AsyncPool(*thread_pool,
						[this, coord, negative_delta, seam_octants, ec_seam_octants, chunk_mesh, rmc_newly_created, has_section_built]() -> ChunkPolygonizeResult
						{
							ChunkPolygonizeResult result;
							result.chunk_coord = coord;

							FRealtimeMeshSectionGroupKey mesh_group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh"));
							RealtimeMesh::FRealtimeMeshStreamSet stream_set;

							stream_set = UOctreeCode::PolygonizeOctree(seam_octants, ec_seam_octants, negative_delta);
							FRealtimeMeshStreamKey key = stream_set.GetStreamKeys().Get(FSetElementId::FromInteger(0));
							//create / update mesh section of chunk
							int32 idx_num = stream_set.Find(key)->Num();
							if (idx_num < 3)
							{
								result.rm_aborted = true;
							}
							else
							{
								if (rmc_newly_created || !has_section_built)
								{
									result.mesh_future = chunk_mesh->CreateSectionGroup(mesh_group_key, MoveTemp(stream_set));
								}
								else
								{
									result.mesh_future = chunk_mesh->UpdateSectionGroup(mesh_group_key, MoveTemp(stream_set));
								}

								FRealtimeMeshSectionKey section_key = FRealtimeMeshSectionKey::Create(mesh_group_key, FName("Section_PolyGroup"));
								result.collision_future = chunk_mesh->UpdateSectionConfig(section_key, FRealtimeMeshSectionConfig(), true);
							}

							return result;
						}));

				}
				else
				{
					chunk_grid.chunk_polygonize_tasks.Add(AsyncPool(*thread_pool,
						[this, coord, negative_delta, seam_octants, chunk_mesh, rmc_newly_created, has_section_built]() -> ChunkPolygonizeResult
						{
							ChunkPolygonizeResult result;
							result.chunk_coord = coord;

							FRealtimeMeshSectionGroupKey mesh_group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh"));
							RealtimeMesh::FRealtimeMeshStreamSet stream_set;

							stream_set = UOctreeCode::PolygonizeOctree(seam_octants, negative_delta);
							FRealtimeMeshStreamKey key = stream_set.GetStreamKeys().Get(FSetElementId::FromInteger(1));
							
							int32 idx_num = stream_set.Find(key)->Num();
							if(idx_num < 3) 
							{
								result.rm_aborted = true;
							}
							else 
							{
								//create / update mesh section of chunk
								if (rmc_newly_created || !has_section_built)
								{
									FRealtimeMeshSectionGroupConfig config;
									config.DrawType = ERealtimeMeshSectionDrawType::Dynamic;
									result.mesh_future = chunk_mesh->CreateSectionGroup(mesh_group_key, MoveTemp(stream_set), config);
								}
								else
								{
									result.mesh_future = chunk_mesh->UpdateSectionGroup(mesh_group_key, MoveTemp(stream_set));
								}


								FRealtimeMeshSectionKey section_key = FRealtimeMeshSectionKey::Create(mesh_group_key, FName("Section_PolyGroup"));
								result.collision_future = chunk_mesh->UpdateSectionConfig(section_key, FRealtimeMeshSectionConfig(), true);

							}
							
							
							
							return result;
						}));
				}

				dispatch_counter++;
			}
			else 
			{
				chunk.has_section_built = false;
				ReleaseChunkMesh(chunk);
			}
		}
	}
	for (int32 i = 0; i < chunk_grid.chunk_polygonize_tasks.Num(); i++)
	{
		auto& result = chunk_grid.chunk_polygonize_tasks[i];
		if (result.IsReady())
		{
			const ChunkPolygonizeResult& polygonize_result = result.Get();
			if(polygonize_result.rm_aborted)
			{
				Chunk& chunk = chunk_grid.GetMutable(polygonize_result.chunk_coord);
				chunk.has_section_built = false;

				ReleaseChunkMesh(chunk);

				chunk_grid.chunk_polygonize_tasks.RemoveAt(i);
				i--;
			}
			else if(polygonize_result.collision_future.IsReady() && polygonize_result.mesh_future.IsReady())
			{
				result.GetMutable().collision_future.Consume();
				ERealtimeMeshProxyUpdateStatus status = result.GetMutable().mesh_future.Consume();

				result.Consume();

				chunk_grid.chunk_polygonize_tasks.RemoveAt(i);
				i--;
			}
		}
	}

}

void UChunkProvider::ReleaseChunkMesh(Chunk& chunk)
{
	//if chunk mesh was already released
	if(!chunk.mesh) return;

	auto rmc = static_cast<URealtimeMeshComponent*>(chunk.mesh->GetOuter());
	render_actor->ReleaseRMC(rmc, chunk.has_section_built);
	chunk.mesh = nullptr;
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

void UChunkProvider::ModifyOperation(const FSDFOp& sdf_operation)
{
	if(!IsSafeToModifyChunks()) return;

	const float inflate_factor = 2.f;

	FIntVector3 coord = GetChunkCoordinatesFromPosition(sdf_operation.position);

	if(!chunk_grid.chunks.Contains(coord)) return;
	
	chunk_grid.modify_operations.Enqueue(sdf_operation);

	float chunk_size = chunk_settings->chunk_size;

	FVector3f op_min = sdf_operation.position - ((sdf_operation.bounds_size * 0.5f) * inflate_factor);
	FVector3f op_max = sdf_operation.position + ((sdf_operation.bounds_size * 0.5f) * inflate_factor);

	UE::Math::TBox<float> op_bb(op_min, op_max);

	bool on_seam = false;
	for (int32 x = -1; x < 2; x++)
	{
		for (int32 y = -1; y < 2; y++)
		{
			for (int32 z = -1; z < 2; z++)
			{
				FIntVector3 neigbor_coord = coord + FIntVector3(x, y, z);

				if(coord == neigbor_coord) continue;

				if(Chunk* neighbor = chunk_grid.TryGet(neigbor_coord))
				{
					FVector3f chunk_min = neighbor->center - chunk_size * 0.5f;
					FVector3f chunk_max = neighbor->center + chunk_size * 0.5f;

					UE::Math::TBox<float> chunk_bb(chunk_min, chunk_max);

					if(chunk_bb.Intersect(op_bb)) 
					{
						on_seam = true;
						chunk_grid.modify_operations.Enqueue(sdf_operation);
						RebuildChunk(neigbor_coord);
						MeshChunk(neigbor_coord, PolygonizeTaskArg::RebuildAllSeams);
					}
				}
			}
		}
	}

	RebuildChunk(coord);
	MeshChunk(coord, PolygonizeTaskArg::RebuildAllSeams);
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
		if(chunk_grid.chunks.Contains(current_chunk_coord))
		{
			OctreeNode* node = chunk_grid.Get(current_chunk_coord).root.Get();

			octree_manager->DebugDrawOctree(GetWorld(), node, 0, chunk_settings->draw_leaves, chunk_settings->draw_simplified_leaves, chunk_settings->debug_draw_how_deep);
		}
	}

#endif

	if(chunk_settings->stop_chunk_loading) return;

	DrainChunkBuildQueues();

	if(IsSafeToModifyChunks())
	{
		temp_created_chunks.Empty();

		bool poll_lifetime = false;
		if (build_initial_area)
		{
			BuildChunkArea(current_chunk_coord);
			chunk_grid.current_generator_pos = current_chunk_coord;

			build_initial_area = false;
		}
		else if(current_chunk_coord != chunk_grid.current_generator_pos)
		{
			FIntVector3 gen_delta = current_chunk_coord - chunk_grid.current_generator_pos;
			FIntVector3 clamp = FIntVector3(FMath::Clamp(gen_delta.X, -1, 1), FMath::Clamp(gen_delta.Y, -1, 1), FMath::Clamp(gen_delta.Z, -1,1));
			chunk_grid.current_generator_pos += clamp;

			BuildSlabs(clamp, chunk_grid.current_generator_pos);

			poll_lifetime = true;
		}

		if(poll_lifetime)
		{
			//TSparseArray for coordinates that exist with big big worlds..?

			TArray<FIntVector3> poll_chunks = GetChunkArea(chunk_grid.current_generator_pos);
			for (int32 i = 0; i < poll_chunks.Num(); i++)
			{
				if (chunk_grid.chunks.Contains(poll_chunks[i]))
				{
					Chunk& chunk = chunk_grid.GetMutable(poll_chunks[i]);
					chunk.ping_counter = 0;
				}
			}
			
			//ping lifetime of existing chunks
			TArray<FIntVector3> cleanup_chunks;
			for (auto& pair : chunk_grid.chunks)
			{
				Chunk& chunk = pair.Value;

				if (chunk.ping_counter >= chunk_settings->chunk_ping_deletion_at)
				{
					ReleaseChunkMesh(chunk);

					cleanup_chunks.Add(pair.Key);
				}
				else chunk.ping_counter++;
			}
			for (int32 i = 0; i < cleanup_chunks.Num(); i++)
			{
				chunk_grid.chunks.FindAndRemoveChecked(cleanup_chunks[i]);
			}

		}
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

void UChunkProvider::ChunkGrid::Cleanup()
{
	chunk_creation_jobs.Empty();
	for (int32 i = 0; i < chunk_creation_tasks.Num(); i++)
	{
		auto& future = chunk_creation_tasks[i];
		auto result = future.Consume();
	}
	chunk_creation_tasks.Empty();

	chunk_polygonize_jobs.Empty();
	for (int32 i = 0; i < chunk_polygonize_tasks.Num(); i++)
	{
		auto& future = chunk_polygonize_tasks[i];
		auto result = future.Consume();
		result.mesh_future.Wait();
		result.collision_future.Wait();
	}
	chunk_polygonize_tasks.Empty();

	chunks.Empty();
}