// Fill out your copyright notice in the Description page of Project Settings.


#include "OctreeManager.h"
#include "OctreeSettings.h"

#include "NoiseDataGenerator.h"
#include "OctreeNode.h"
#include <cmath>
#include "probabilistic-quadrics.hh"
#include "DC_Mat3x3.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "DC_OctreeRenderActor.h"

using uemath = pq::math<float, FVector3f, FVector3f, FMatrix3x3>;

//typedef quadrid type
using quadric3 = pq::quadric<uemath>;

void UOctreeManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	octree_settings = GetDefault<UOctreeSettings>();

	noise_gen = GEngine->GetEngineSubsystem<UNoiseDataGenerator>();

	FActorSpawnParameters Params;
	Params.Name = TEXT("DC_OctreeRenderActor");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags = RF_Transient;                // don’t save to map
	Params.bNoFail = true;

#if WITH_EDITOR
	Params.bHideFromSceneOutliner = true;
#endif

	ADC_OctreeRenderActor* created_render_actor = GetWorld()->SpawnActor<ADC_OctreeRenderActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (created_render_actor)
	{
#if WITH_EDITOR
		created_render_actor->bIsEditorOnlyActor = (GetWorld()->WorldType == EWorldType::Editor);
		created_render_actor->SetActorLabel(TEXT("Octree Render (Transient)"));
		created_render_actor->ClearFlags(EObjectFlags::RF_Transactional);

		if (UActorComponent* root = created_render_actor->GetRootComponent())
		{
			root->SetFlags(RF_Transient);
			root->ClearFlags(RF_Transactional);                             // <- important
		}

		if (auto* rmc = created_render_actor->mesh_component)
		{
			rmc->SetFlags(RF_Transient);
			rmc->ClearFlags(RF_Transactional);                              // <- important
		}
#endif
		render_actor = created_render_actor;
	}		


	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UOctreeManager::PostWorldInit);

	octree_mesh = render_actor->mesh_component->InitializeRealtimeMesh<URealtimeMeshSimple>();
	octree_mesh->SetFlags(RF_Transient);
	octree_mesh->ClearFlags(RF_Transactional);

	octree_settings->mesh_material.LoadSynchronous();
}

void UOctreeManager::Deinitialize()
{
	Super::Deinitialize();

#if WITH_EDITOR
	UOctreeSettings::OnChanged().RemoveAll(this);
#endif

	render_actor->Destroy();
	render_actor = nullptr;

	octree_mesh = nullptr;
}

void UOctreeManager::PostWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	//stupid rmc bug where materials do not apply after creation? this fixes it
	render_actor->mesh_component->ReregisterComponent();
}

FVector3f child_offsets[8] =
{
	{-1,-1,-1}, // 0:(0,0,0)
	{1,-1, -1}, // 1:(0,0,1)
	{-1, -1,1}, // 2:(0,1,0)
	{1, -1, 1}, // 3:(0,1,1)
	{ -1,1,-1}, // 4:(1,0,0)
	{ 1, 1, -1}, // 5:(1,0,1)
	{ -1, 1,1}, // 6:(1,1,0)
	{ 1, 1, 1}  // 7:(1,1,1)
};



//for polling noise and QEF calculations, better scaled UE coordinates.
constexpr float scale_factor = 0.01f;
constexpr float inv_scale_factor = 1.f / scale_factor;

constexpr unsigned char edges_corner_map[12][2] =
{
	{0,4},{1,5},{2,6},{3,7},	// x-axis
	{0,2},{1,3},{4,6},{5,7},	// y-axis
	{0,1},{2,3},{4,5},{6,7}		// z-axis
};

void UOctreeManager::ConstructLeafNode_V2(OctreeNode* node, const FVector3f& node_p, const float* corner_densities, uint8 corners)
{
	const unsigned int MAX_ZERO_CROSSINGS = 6;
	const int8 max_depth = octree_settings->max_depth;
	const float iso_surface = octree_settings->iso_surface;
	const float stddev_pos = octree_settings->stddev_pos;
	const float stddev_normal = octree_settings->stddev_normal;

	while(node->depth != max_depth)
	{
		uint8 this_idx = GetChildNodeFromPosition(node_p, node->center);

		if (!node->children[this_idx])
		{
			OctreeNode* new_node = new OctreeNode();
			new_node->depth = node->depth + 1;
			new_node->center = node->center + child_offsets[this_idx] * node->size * 0.25f;
			new_node->size = node->size * 0.5f;

			node->children[this_idx] = new_node;
		}

		node = node->children[this_idx];
	}

	uint16 edge_mask = 0;
	for (uint8 i = 0; i < 12; i++)
	{
		auto c1 = edges_corner_map[i][0];
		auto c2 = edges_corner_map[i][1];

		edge_mask |= (((corners >> c1) ^ (corners >> c2)) & 1) << i;
	}

	//node is a leaf
	node->type = NODE_LEAF;
	node->corners = corners;

	//FVector3f mass_point{};
	uint8 edge_count = 0;

	//UE_ASSUME(edge_count != 0);
	FVector3f& vert_normal = node->leaf_data.normal;
	quadric3& vox_pq = node->leaf_data.qef;

	const FVector3f node_center = node->center;
	const float node_size = node->size;

	while(edge_mask /*&& edge_count < MAX_ZERO_CROSSINGS*/)
	{
		int32 idx = FMath::CountTrailingZeros(static_cast<uint32>(edge_mask));

		//unset last 1 bit trick
		edge_mask &= (edge_mask - 1);

		/*unsigned char s1 = (corners >> edges_corner_map[i][0]) & 1;
		unsigned char s2 = (corners >> edges_corner_map[i][1]) & 1;

		if (s1 == s2) continue;*/

		//detected sign change on current edge
		FVector3f corner_1 = (child_offsets[edges_corner_map[idx][0]] * node_size * 0.5f) + node_center;
		FVector3f corner_2 = (child_offsets[edges_corner_map[idx][1]] * node_size * 0.5f) + node_center;

		float d1 = corner_densities[edges_corner_map[idx][0]] - iso_surface;
		float d2 = corner_densities[edges_corner_map[idx][1]] - iso_surface;

		// (1-alpha)*d1 + alpha*d2 = 0
		// d1 - alpha*d1 + alpha*d2 = 0
		// d1 + alpha(-d1+d2) = 0
		// alpha = d1 / (d1-d2)
		float alpha = d1 / (d1 - d2 + FLT_EPSILON);

		FVector3f intersection = FMath::Lerp(corner_1, corner_2, alpha);
		//mass_point += intersection;

		//at 32 vox size, i dont think it's worth doing better zero crossing. below usually gives values in order of 0.001 > x > -0.001
		//float alpha_density = noise_gen->GetNoiseSingle3D(intersection.X * scale_factor, intersection.Y * scale_factor, intersection.Z * scale_factor) - octree_settings->iso_surface;

		FVector3f normal = FDMGetNormal(intersection * scale_factor);
		vert_normal += normal;

		vox_pq += quadric3::probabilistic_plane_quadric(intersection * scale_factor, normal, stddev_pos, stddev_normal);
		edge_count++;
	}

	vert_normal /= edge_count;

	node->leaf_data.minimizer = vox_pq.minimizer();

#if CLAMP_MINIMIZERS

	float half_size = node->size * 0.5f * scale_factor;
	FVector3f scaled_center = node->center * scale_factor;

	if (node->leaf_data->minimizer.X > scaled_center.X + half_size)
	{
		node->leaf_data->minimizer.X = scaled_center.X + half_size;
	}
	if (node->leaf_data->minimizer.Y > scaled_center.Y + half_size)
	{
		node->leaf_data->minimizer.Y = scaled_center.Y + half_size;
	}
	if (node->leaf_data->minimizer.Z > scaled_center.Z + half_size)
	{
		node->leaf_data->minimizer.Z = scaled_center.Z + half_size;
	}


	if (node->leaf_data->minimizer.X < scaled_center.X - half_size)
	{
		node->leaf_data->minimizer.X = scaled_center.X - half_size;
	}
	if (node->leaf_data->minimizer.Y < scaled_center.Y - half_size)
	{
		node->leaf_data->minimizer.Y = scaled_center.Y - half_size;
	}
	if (node->leaf_data->minimizer.Z < scaled_center.Z - half_size)
	{
		node->leaf_data->minimizer.Z = scaled_center.Z - half_size;
	}
#endif

#if UE_BUILD_DEBUG
	if (isnan(node->leaf_data->minimizer.X) || isnan(node->leaf_data->minimizer.Y) || isnan(node->leaf_data->minimizer.Z))
	{
		//check(false);
	}
#endif

	//ConstructLeafNode_V2(node->children[this_idx], node_p, corner_densities, corners);
}

#include "SeamRecursionFunctions.inl"

constexpr uint8 main_node[2] = { 7, 0 };

constexpr uint8 other_nodes[2][7] = 
{
	{0,1,2,3,4,5,6},
	{1,2,3,4,5,6,7}
};

using recfunc_sig = StitchOctreeNode * (*)(OctreeNode*, StitchOctreeNode*, MeshBuilder&);
constexpr recfunc_sig main_seam_operations[2][3] =
{
	{LeftRecurse, BackRecurse, BottomRecurse},
	{FrontRecurse, RightRecurse, TopRecurse}
};

constexpr recfunc_sig other_seam_operations[2][7] =
{
	{CornerMiniRecurse_7, CornerBarRecurseTR, CornerBarRecurseVRF, RightRecurse, CornerBarRecurseTF, TopRecurse, FrontRecurse},
	{BackRecurse, BottomRecurse, CornerBarRecurseBB, LeftRecurse, CornerBarRecurseVLB, CornerBarRecurseBL, CornerMiniRecurse_0}
};

StitchOctreeNode* UOctreeManager::ConstructSeamOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& seam_nodes, bool nd, MeshBuilder& builder)
{
	//this node recursion (no special case needed)
	StitchOctreeNode* stitch_main = main_seam_operations[nd][0](seam_nodes[main_node[nd]], nullptr, builder);
	main_seam_operations[nd][1](seam_nodes[main_node[nd]], stitch_main, builder);
	main_seam_operations[nd][2](seam_nodes[main_node[nd]], stitch_main, builder);

	StitchOctreeNode* stitch_root = new StitchOctreeNode();
	stitch_root->type = NODE_INTERNAL;
	stitch_root->depth = -1;
	stitch_root->corners = 0;
	
	StitchOctreeNode* stitch_left = other_seam_operations[nd][0](seam_nodes[other_nodes[nd][0]], nullptr, builder);
	StitchOctreeNode* stitch_back = other_seam_operations[nd][1](seam_nodes[other_nodes[nd][1]], nullptr, builder);
	StitchOctreeNode* stitch_top = other_seam_operations[nd][2](seam_nodes[other_nodes[nd][2]], nullptr, builder);
	StitchOctreeNode* stitch_bar_x = other_seam_operations[nd][3](seam_nodes[other_nodes[nd][3]], nullptr, builder);
	StitchOctreeNode* stitch_bar_y = other_seam_operations[nd][4](seam_nodes[other_nodes[nd][4]], nullptr, builder);
	StitchOctreeNode* stitch_bar_z = other_seam_operations[nd][5](seam_nodes[other_nodes[nd][5]], nullptr, builder);
	StitchOctreeNode* stitch_corner = other_seam_operations[nd][6](seam_nodes[other_nodes[nd][6]], nullptr, builder);

	stitch_root->children[main_node[nd]] = stitch_main;
	stitch_root->children[other_nodes[nd][0]] = stitch_left;
	stitch_root->children[other_nodes[nd][1]] = stitch_back;
	stitch_root->children[other_nodes[nd][2]] = stitch_top;
	stitch_root->children[other_nodes[nd][3]] = stitch_bar_x;
	stitch_root->children[other_nodes[nd][4]] = stitch_bar_y;
	stitch_root->children[other_nodes[nd][5]] = stitch_bar_z;
	stitch_root->children[other_nodes[nd][6]] = stitch_corner;

	return stitch_root;
}

//constexpr unsigned char edges_corner_map[12][2] =
//{
//	{0, 1}, {1, 2}, {2, 3}, {3, 0},
//	{4, 0}, {1, 5}, {2, 6}, {7, 3},
//	{4, 5}, {5, 6}, {6, 7}, {7, 4}
//};


OctreeNode* UOctreeManager::BuildOctree(FVector3f center, float size)
{
	QUICK_SCOPE_CYCLE_COUNTER(Stat_BuildOctree)

#if UE_BUILD_DEBUG
	debug_edges.Empty();
#endif


	OctreeNode* root = new OctreeNode();
	root->center = center;
	root->depth = 0;
	root->size = size;

	int32 dim = GetDim(octree_settings->max_depth)+1;

	TArray<float> x_pos, y_pos, z_pos;
	x_pos.SetNumUninitialized(dim * dim * dim);
	y_pos.SetNumUninitialized(dim * dim * dim);
	z_pos.SetNumUninitialized(dim * dim * dim);

	FVector3f min = center - size*0.5f;
	float vox_size = size / (dim-1);

	for (size_t x = 0; x < dim; x++)
	{
		for (size_t y = 0; y < dim; y++)
		{
			for (size_t z = 0; z < dim; z++)
			{
				int32 idx = Get1DIndexFrom3D(x,y,z, dim);
				x_pos[idx] = (min.X + vox_size * x) * 0.01f;
				y_pos[idx] = (min.Y + vox_size * y) * 0.01f;
				z_pos[idx] = (min.Z + vox_size * z) * 0.01f;
			}
		}
	}

	TArray<float> noise;
	{
		QUICK_SCOPE_CYCLE_COUNTER(Stat_BuildOctree_NoisePoll)
		noise = noise_gen->GetNoiseFromPositions3D_NonThreaded(x_pos.GetData(), y_pos.GetData(), z_pos.GetData(), dim*dim*dim);
	}
	int32 vox_dim = dim-1;

	const float iso_surface = octree_settings->iso_surface;

	{
	QUICK_SCOPE_CYCLE_COUNTER(Stat_BuildOctee_voxbuilding)
	for (size_t x = 0; x < vox_dim; x++)
	{
		for (size_t y = 0; y < vox_dim; y++)
		{
			for (size_t z = 0; z < vox_dim; z++)
			{
				QUICK_SCOPE_CYCLE_COUNTER(Stat_BuildOctree_voxiteration)

				//FVector3f local_node_query_p = FVector3f(x * vox_size + vox_size * 0.5f, y * vox_size + vox_size * 0.5f, z * vox_size + vox_size * 0.5f);
				FIntVector3 lc = FIntVector3(x, y, z);

				int32 idx_table[8];
				idx_table[0] = Get1DIndexFrom3D(lc.X, lc.Y, lc.Z, dim);
				idx_table[1] = Get1DIndexFrom3D(lc.X + 1, lc.Y, lc.Z, dim);
				idx_table[2] = Get1DIndexFrom3D(lc.X, lc.Y, lc.Z + 1, dim);
				idx_table[3] = Get1DIndexFrom3D(lc.X + 1, lc.Y, lc.Z + 1, dim);
				idx_table[4] = Get1DIndexFrom3D(lc.X, lc.Y + 1, lc.Z, dim);
				idx_table[5] = Get1DIndexFrom3D(lc.X + 1, lc.Y + 1, lc.Z, dim);
				idx_table[6] = Get1DIndexFrom3D(lc.X, lc.Y + 1, lc.Z + 1, dim);
				idx_table[7] = Get1DIndexFrom3D(lc.X + 1, lc.Y + 1, lc.Z + 1, dim);

				//TArray<float> corner_densities = SampleOctreeNodeDensities(node);

				const float corner_densities[8] = { noise[idx_table[0]], noise[idx_table[1]], noise[idx_table[2]],
											  noise[idx_table[3]], noise[idx_table[4]], noise[idx_table[5]],
											  noise[idx_table[6]], noise[idx_table[7]] };


				int32 vox_idx = Get1DIndexFrom3D(x,y,z, vox_dim);
				uint8 corners = 0;
				for (uint8_t i = 0; i < 8; i++)
				{
					corners |= ((corner_densities[i] - iso_surface) <= 0.f) << i;
				}

				FVector3f world_pos = FVector3f(x*vox_size + vox_size * 0.5f, y*vox_size + vox_size*0.5f, z*vox_size + vox_size * 0.5f) + (root->center-size*0.5f);

				if(corners != 255 && corners != 0)
				{
					ConstructLeafNode_V2(root, world_pos, corner_densities, corners);
				}
			}
		}
	}
	}
	//ConstructChildNodes(root, size, noise, root_min, );

	if(octree_settings->simplify) SimplifyOctree(root);

	return root;
}

FRealtimeMeshSectionGroupKey UOctreeManager::PolygonizeOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& nodes, bool negative_delta)
{
	RealtimeMesh::FRealtimeMeshStreamSet stream_set;
	RealtimeMesh::TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> builder(stream_set);
	builder.EnableTangents();

	BuildMeshData(nodes[main_node[negative_delta]], builder);
	DC_ProcessCell(nodes[main_node[negative_delta]], builder);

	StitchOctreeNode* stitch = ConstructSeamOctree(nodes, negative_delta, builder);

	DC_ProcessCell(stitch, builder);

	delete stitch;

	const FRealtimeMeshSectionGroupKey group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh", *static_cast<int32*>(static_cast<void*>(nodes[main_node[negative_delta]]))));

	octree_mesh->SetupMaterialSlot(0, "PrimaryMaterial", octree_settings->mesh_material.Get());
	octree_mesh->CreateSectionGroup(group_key, stream_set);

	return group_key;
}

void UOctreeManager::CleanupChunkMesh(FRealtimeMeshSectionGroupKey key)
{
	if(octree_mesh && key.IsValid())
	{
		octree_mesh->RemoveSectionGroup(key);
	}
}

bool UOctreeManager::SimplifyOctree(OctreeNode* node)
{
	if(!node) return false;

	//cant simplify if this node is a leaf / collapsed leaf
	if(node->type) return true;
	
	bool simplify = true;
	unsigned char corners = 0;
	unsigned char unset_corners = 0;
	unsigned char mid_sign = 0;
	quadric3& node_pq = node->leaf_data.qef;
	FVector3f avg_normal = node->leaf_data.normal;
	uint8 count = 0;

	for (uint8 i = 0; i < 8; i++)
	{
		if(SimplifyOctree(node->children[i])) // returns true if node exists
		{
			mid_sign = (node->children[i]->corners >> (7 - i)) & 1;
			if (!simplify) continue;

			if (node->children[i]->type == NODE_INTERNAL)
			{
				//one of this child node's children stays a non-leaf, cant simplify this one.
				simplify = false;
			}
			else
			{
				node_pq += node->children[i]->leaf_data.qef;
				avg_normal += node->children[i]->leaf_data.normal;
				corners |= (((node->children[i]->corners >> i) & 1) << i);
				++count;
			}
		}
		else
		{
			unset_corners |= 1 << i;
		}
	}

	if(!simplify || !count) return true;

	for (uint8 i = 0; i < 8; i++)
	{
		if((unset_corners >> i) & 1) corners |= mid_sign << i;
	}

	FVector3f minimizer = node_pq.minimizer();
	float error = node_pq(minimizer);

	//possible simplification doesn't approximate the surface well->return
	if(error > octree_settings->simplify_threshold) 
	{
		return true;
	}

	node->type = NODE_COLLAPSED_LEAF;
	node->corners = corners;
	node->leaf_data.minimizer = minimizer;
	avg_normal /= static_cast<float>(count);

#if UE_BUILD_DEBUG
	if(isnan(node->leaf_data->minimizer.X) || isnan(node->leaf_data->minimizer.Y) || isnan(node->leaf_data->minimizer.Z))
	{
		//check(false);
	}
#endif

	for (size_t i = 0; i < 8; i++)
	{
		delete node->children[i];
		node->children[i] = nullptr;
	}

	return true;
}

void UOctreeManager::BuildMeshData(OctreeNode* node, MeshBuilder& builder)
{
	if(!node) return;
	
	if(node->type == NODE_INTERNAL)
	{
		for (uint8 i = 0; i < 8; i++)
		{
			BuildMeshData(node->children[i], builder);
		}
	}
	else 
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		node->leaf_data.index = vertex.GetIndex();
	}
}



void UOctreeManager::BuildStitchMeshData(OctreeNode* node, OctreeNode* parent, MeshBuilder& builder)
{
	// left x side
	// AI gave me this self idea... interesting way to avoid declaring the lambda function
	auto left_x_recurse = [&](auto&& self, OctreeNode* node, MeshBuilder& builder)
	{
		if(!node) return;

		if(node->type == NODE_INTERNAL)
		{
			for (uint8 i = 0; i < 4; i++)
			{
				self(self, node->children[i], builder);
			}
		}
		else 
		{
			builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		}
	};
	left_x_recurse(left_x_recurse, node, builder);	
}

constexpr unsigned char DIRECTION_X = 1;
constexpr unsigned char DIRECTION_Y = 0;
constexpr unsigned char DIRECTION_Z = 2;

//constexpr unsigned char process_cell_face_nodes[12][3] = 
//{
//	{0,1, DIRECTION_Y}, {4,5, DIRECTION_Y}, {3, 2, DIRECTION_Y}, {7, 6, DIRECTION_Y}, // dir y
//	{0, 3, DIRECTION_X}, {1, 2, DIRECTION_X}, {4,7, DIRECTION_X}, {5, 6, DIRECTION_X}, // dir x
//	{0,4, DIRECTION_Z}, {1, 5, DIRECTION_Z}, {2, 6, DIRECTION_Z}, {3, 7, DIRECTION_Z} // dir z
//};

constexpr unsigned char process_cell_face_nodes[12][3] = 
{ 
	{0,4,0},{1,5,0},{2,6,0},{3,7,0},{0,2,1},{4,6,1},{1,3,1},{5,7,1},{0,1,2},{2,3,2},{4,5,2},{6,7,2} 
};

//constexpr unsigned char process_edge_nodes[6][5] =
//{
//	{4, 5, 6, 7, DIRECTION_Z},
//	{0, 1, 2, 3, DIRECTION_Z}, 
//	{4, 5, 1, 0, DIRECTION_X}, 
//	{5, 6, 2, 1, DIRECTION_Y}, 
//	{6, 7, 3, 2, DIRECTION_X}, 
//	{7, 4, 0, 3, DIRECTION_Y}
//};

constexpr unsigned char process_edge_nodes[6][5] = 
{ {0,1,2,3,0},{4,5,6,7,0},{0,4,1,5,1},{2,6,3,7,1},{0,2,4,6,2},{1,3,5,7,2} };

void UOctreeManager::DC_ProcessCell(OctreeNode* node, MeshBuilder& builder)
{
	if(!node) return;

	if(node->type == NODE_INTERNAL) // if node is internal
	{
		// recurse to each child 
		for (size_t i = 0; i < 8; i++)
		{
			DC_ProcessCell(node->children[i], builder);
		}

		//handles every interior face of the node
		for (size_t i = 0; i < 12; i++)
		{
			OctreeNode* child_1 = node->children[process_cell_face_nodes[i][0]];
			OctreeNode* child_2 = node->children[process_cell_face_nodes[i][1]];
			DC_ProcessFace(child_1, child_2, process_cell_face_nodes[i][2], builder);
		}

		//interior 6 edges of this node
		for (size_t i = 0; i < 6; i++)
		{
			OctreeNode* child_1 = node->children[process_edge_nodes[i][0]];
			OctreeNode* child_2 = node->children[process_edge_nodes[i][1]];
			OctreeNode* child_3 = node->children[process_edge_nodes[i][2]];
			OctreeNode* child_4 = node->children[process_edge_nodes[i][3]];
			DC_ProcessEdge(child_1, child_2, child_3, child_4, process_edge_nodes[i][4], builder);
		}
	}
}

//similarities to process_cell_face_nodes, maybe redundant?
//constexpr unsigned char process_face_direction_cells[3][4][2]  = 
//{
//	{{1, 0}, {2, 3}, {5, 4}, {6, 7}}, // dir y 
//	{{3, 0}, {2, 1}, {7, 4}, {6, 5}}, // dir x
//	{{4, 0}, {5, 1}, {6, 2}, {7, 3}} // dir z
//}; 

constexpr unsigned char process_face_direction_cells[3][4][3] = {
	{{4,0,0},{5,1,0},{6,2,0},{7,3,0}},
	{{2,0,1},{6,4,1},{3,1,1},{7,5,1}},
	{{1,0,2},{3,2,2},{5,4,2},{7,6,2}}
};

// stored as [directions][edge_idx][order (0) nodes (1-4)]
// cw edge idxing
//constexpr unsigned char process_face_edge_nodes[3][4][6] = 
//{
//	{{0, 6, 5, 7, 4, DIRECTION_Z}, {0, 1, 5, 0, 4, DIRECTION_X}, {0, 1, 2, 0, 3, DIRECTION_Z}, {0, 2, 6, 3, 7, DIRECTION_X}}, //dir y
//	{{0, 4, 5, 6, 7, DIRECTION_Z}, {0, 1, 5, 2, 6, DIRECTION_Y}, {0, 0, 1, 2, 3, DIRECTION_Z}, {0, 0, 4, 3, 7, DIRECTION_Y}}, // dir x
//	{{0, 4, 5, 0, 1, DIRECTION_X}, {0, 5, 6, 1, 2, DIRECTION_Y}, {0, 6, 7, 2, 3, DIRECTION_X}, {0, 4, 7, 0, 3, DIRECTION_Y}} // dir z
//};

constexpr unsigned char process_face_edge_nodes[3][4][6] = {
	{{1,4,0,5,1,1},{1,6,2,7,3,1},{0,4,6,0,2,2},{0,5,7,1,3,2}},
	{{0,2,3,0,1,0},{0,6,7,4,5,0},{1,2,0,6,4,2},{1,3,1,7,5,2}},
	{{1,1,0,3,2,0},{1,5,4,7,6,0},{0,1,5,0,4,1},{0,3,7,2,6,1}}
};

void UOctreeManager::DC_ProcessFace(OctreeNode* node_1, OctreeNode* node_2, unsigned char direction, MeshBuilder& builder)
{
	if(!(node_1 && node_2)) return;

	//either one of the nodes has children nodes
	if(node_1->type == NODE_INTERNAL || node_2->type == NODE_INTERNAL)
	{
		// 4 face calls
		for (size_t face_idx = 0; face_idx < 4; face_idx++)
		{
			OctreeNode* face_node_1 = nullptr;
			if(node_1->type == NODE_INTERNAL)
			{
				face_node_1 = node_1->children[process_face_direction_cells[direction][face_idx][0]];
			}
			else //node is a leaf / collapsed leaf 
			{
				face_node_1 = node_1;
			}

			OctreeNode* face_node_2 = nullptr;
			if(node_2->type == NODE_INTERNAL)
			{
				face_node_2 = node_2->children[process_face_direction_cells[direction][face_idx][1]];
			}
			else face_node_2 = node_2;

			DC_ProcessFace(face_node_1, face_node_2, /*process_face_direction_cells[direction][face_idx][2]*/ direction, builder);
		}

		const unsigned char orders[2][4] =
		{
			{ 0, 0, 1, 1 },
			{ 0, 1, 0, 1 },
		};

		OctreeNode* face_nodes[2] = {node_1, node_2};

		// 4 edge calls, on the boundary between nodes
		for (size_t edge_idx = 0; edge_idx < 4; edge_idx++)
		{
			OctreeNode* edge_nodes[4];

			/*bool node_1_is_leaf = node_1->type != NODE_INTERNAL;
			bool node_2_is_leaf = node_2->type != NODE_INTERNAL;*/

			unsigned char indices[4] = 
			{
				process_face_edge_nodes[direction][edge_idx][1],
				process_face_edge_nodes[direction][edge_idx][2],
				process_face_edge_nodes[direction][edge_idx][3],
				process_face_edge_nodes[direction][edge_idx][4]
			};

			const unsigned char* order = orders[process_face_edge_nodes[direction][edge_idx][0]];
			for (size_t node_idx = 0; node_idx < 4; node_idx++)
			{
				if(face_nodes[order[node_idx]]->type != NODE_INTERNAL)
				{
					edge_nodes[node_idx] = face_nodes[order[node_idx]];
				}
				else 
				{
					edge_nodes[node_idx] = face_nodes[order[node_idx]]->children[indices[node_idx]];
				}
			}

			DC_ProcessEdge(edge_nodes[0], edge_nodes[1], edge_nodes[2], edge_nodes[3], process_face_edge_nodes[direction][edge_idx][5], builder);

			/*if(node_1_is_leaf)
			{
				edge_node_1 = node_1;
				edge_node_2 = node_1;
			}
			else 
			{
				edge_node_1 = node_1->children[process_face_edge_nodes[direction][edge_idx][1]];
				edge_node_2 = node_1->children[process_face_edge_nodes[direction][edge_idx][2]];
			}

			if(node_2_is_leaf)
			{
				edge_node_3 = node_2;
				edge_node_4 = node_2;
			}
			else 
			{
				edge_node_3 = node_2->children[process_face_edge_nodes[direction][edge_idx][3]];
				edge_node_4 = node_2->children[process_face_edge_nodes[direction][edge_idx][4]];
			}*/

			//DC_ProcessEdge(edge_node_1, edge_node_2, edge_node_3, edge_node_4, process_face_edge_nodes[direction][edge_idx][5], builder);
		}
	}
}

//constexpr unsigned char process_sub_edge_nodes[3][2][4] = 
//{
//	{{3, 0, 4, 7}, {2, 1, 5, 6}}, // dir y // 3 0 4 7 | 2 1 5 6 
//	{{2, 3, 7, 6}, {1, 0, 4, 5}}, // dir x
//	{{2, 3, 0, 1}, {6, 7, 4, 5}} // dir z, // 2 3 0 1 | 6 7 4 5
//};

constexpr unsigned char process_sub_edge_nodes[3][2][5] = {
	{{3,2,1,0,0},{7,6,5,4,0}},
	{{5,1,4,0,1},{7,3,6,2,1}},
	{{6,4,2,0,2},{7,5,3,1,2}},
};


//constexpr unsigned char node_edge[3][4] = 
//{
//	{0, 2, 8, 10}, //dir y
//	{1, 3, 9, 11}, // dir x
//	{4, 5, 6, 7} // dir z
//};

constexpr unsigned char node_edge[3][4] = 
{
	{3,2,1,0},{7,5,6,4},{11,10,9,8} 
};
//constexpr unsigned char edge_corners[12][2] = 
//{
//	{0, 1}, {1, 2}, {2, 3}, {3, 0},
//	{0, 4}, {1, 5}, {2, 6}, {3, 7},
//	{4, 5}, {5, 6}, {6, 7}, {7, 4}
//};

constexpr unsigned char edge_corners[12][2] = 
{
	{0,4},{1,5},{2,6},{3,7},
	{0,2},{1,3},{4,6},{5,7},
	{0,1},{2,3},{4,5},{6,7}
};

void UOctreeManager::DC_ProcessEdge(OctreeNode* node_1, OctreeNode* node_2, OctreeNode* node_3, OctreeNode* node_4, unsigned char direction, MeshBuilder& builder)
{
	if(!(node_1 && node_2 && node_3 && node_4)) return;

	unsigned char types[4] = {node_1->type, node_2->type, node_3->type, node_4->type};
	OctreeNode* edge_nodes[4] = { node_1, node_2, node_3, node_4 };

	if(types[0] && types[1] && types[2] && types[3])
	{
		//all nodes are leaves / collapsed, we can add to the polygon buffer

		uint32 indices[4];
		unsigned char sign_changes[4] = {0, 0, 0, 0};
		
		// we only want to add quads for a smallest node that owns the edge
		unsigned char lowest_depth = 0;
		// used to idx into sign_changes
		unsigned char minimal_node_idx = 0;

		bool flip = false;

 		for (size_t node_idx = 0; node_idx < 4; node_idx++)
		{
			unsigned char edge_idx = node_edge[direction][node_idx];

			unsigned char corner_1 = edge_corners[edge_idx][0];
			unsigned char corner_2 = edge_corners[edge_idx][1];

#if UE_BUILD_DEBUG
			float node_half_size = edge_nodes[node_idx]->size * 0.5f;
			FVector3f corner_1_p = edge_nodes[node_idx]->center + child_offsets[corner_1] * node_half_size;
			FVector3f corner_2_p = edge_nodes[node_idx]->center + child_offsets[corner_2] * node_half_size;

			dbg_edge edge;
			edge.start = FVector(corner_1_p);
			edge.end = FVector(corner_2_p);

			debug_edges.Add(edge);
#endif
			unsigned char inside_1 = (edge_nodes[node_idx]->corners >> corner_1) & 1;
			unsigned char inside_2 = (edge_nodes[node_idx]->corners >> corner_2) & 1;

			indices[node_idx] = edge_nodes[node_idx]->leaf_data.index;

			sign_changes[node_idx] = inside_1 != inside_2;

			if(edge_nodes[node_idx]->depth > lowest_depth)
			{
				lowest_depth = edge_nodes[node_idx]->depth;
				minimal_node_idx = node_idx;
				flip = static_cast<bool>(inside_1);
			}
		}

		if(sign_changes[minimal_node_idx])
		{
			if(flip)
			{
				builder.AddTriangle(indices[0], indices[1], indices[3]);
				builder.AddTriangle(indices[0], indices[3], indices[2]);
			}
			else 
			{
				builder.AddTriangle(indices[0], indices[3], indices[1]);
				builder.AddTriangle(indices[0], indices[2], indices[3]);
			}	
		}
	}
	else 
	{
		for (size_t i = 0; i < 2; i++)
		{
			OctreeNode* next_edge_nodes[4];
			for (size_t node_idx = 0; node_idx < 4; node_idx++)
			{
				if(types[node_idx])
				{
					//leaf
					next_edge_nodes[node_idx] = edge_nodes[node_idx];
				}
				else 
				{
					next_edge_nodes[node_idx] = edge_nodes[node_idx]->children[process_sub_edge_nodes[direction][i][node_idx]];
				}
			}

			DC_ProcessEdge(next_edge_nodes[0], next_edge_nodes[1], next_edge_nodes[2], next_edge_nodes[3], process_sub_edge_nodes[direction][i][4], builder);
		}
	}
}

void UOctreeManager::DC_ProcessCell(StitchOctreeNode* node, MeshBuilder& builder)
{
	if (!node) return;

	if (node->type == NODE_INTERNAL) // if node is internal
	{
		// recurse to each child 
		for (size_t i = 0; i < 8; i++)
		{
			DC_ProcessCell(node->children[i], builder);
		}

		//handles every interior face of the node
		for (size_t i = 0; i < 12; i++)
		{
			StitchOctreeNode* child_1 = node->children[process_cell_face_nodes[i][0]];
			StitchOctreeNode* child_2 = node->children[process_cell_face_nodes[i][1]];
			DC_ProcessFace(child_1, child_2, process_cell_face_nodes[i][2], builder);
		}

		//interior 6 edges of this node
		for (size_t i = 0; i < 6; i++)
		{
			StitchOctreeNode* child_1 = node->children[process_edge_nodes[i][0]];
			StitchOctreeNode* child_2 = node->children[process_edge_nodes[i][1]];
			StitchOctreeNode* child_3 = node->children[process_edge_nodes[i][2]];
			StitchOctreeNode* child_4 = node->children[process_edge_nodes[i][3]];
			DC_ProcessEdge(child_1, child_2, child_3, child_4, process_edge_nodes[i][4], builder);
		}
	}
}

void UOctreeManager::DC_ProcessFace(StitchOctreeNode* node_1, StitchOctreeNode* node_2, unsigned char direction, MeshBuilder& builder)
{
	if (!(node_1 && node_2)) return;

	//either one of the nodes has children nodes
	if (node_1->type == NODE_INTERNAL || node_2->type == NODE_INTERNAL)
	{
		// 4 face calls
		for (size_t face_idx = 0; face_idx < 4; face_idx++)
		{
			StitchOctreeNode* face_node_1 = nullptr;
			if (node_1->type == NODE_INTERNAL)
			{
				face_node_1 = node_1->children[process_face_direction_cells[direction][face_idx][0]];
			}
			else //node is a leaf / collapsed leaf 
			{
				face_node_1 = node_1;
			}

			StitchOctreeNode* face_node_2 = nullptr;
			if (node_2->type == NODE_INTERNAL)
			{
				face_node_2 = node_2->children[process_face_direction_cells[direction][face_idx][1]];
			}
			else face_node_2 = node_2;

			DC_ProcessFace(face_node_1, face_node_2, /*process_face_direction_cells[direction][face_idx][2]*/ direction, builder);
		}

		const unsigned char orders[2][4] =
		{
			{ 0, 0, 1, 1 },
			{ 0, 1, 0, 1 },
		};

		StitchOctreeNode* face_nodes[2] = { node_1, node_2 };

		// 4 edge calls, on the boundary between nodes
		for (size_t edge_idx = 0; edge_idx < 4; edge_idx++)
		{
			StitchOctreeNode* edge_nodes[4];

			/*bool node_1_is_leaf = node_1->type != NODE_INTERNAL;
			bool node_2_is_leaf = node_2->type != NODE_INTERNAL;*/

			unsigned char indices[4] =
			{
				process_face_edge_nodes[direction][edge_idx][1],
				process_face_edge_nodes[direction][edge_idx][2],
				process_face_edge_nodes[direction][edge_idx][3],
				process_face_edge_nodes[direction][edge_idx][4]
			};

			const unsigned char* order = orders[process_face_edge_nodes[direction][edge_idx][0]];
			for (size_t node_idx = 0; node_idx < 4; node_idx++)
			{
				if (face_nodes[order[node_idx]]->type != NODE_INTERNAL)
				{
					edge_nodes[node_idx] = face_nodes[order[node_idx]];
				}
				else
				{
					edge_nodes[node_idx] = face_nodes[order[node_idx]]->children[indices[node_idx]];
				}
			}

			DC_ProcessEdge(edge_nodes[0], edge_nodes[1], edge_nodes[2], edge_nodes[3], process_face_edge_nodes[direction][edge_idx][5], builder);

			/*if(node_1_is_leaf)
			{
				edge_node_1 = node_1;
				edge_node_2 = node_1;
			}
			else
			{
				edge_node_1 = node_1->children[process_face_edge_nodes[direction][edge_idx][1]];
				edge_node_2 = node_1->children[process_face_edge_nodes[direction][edge_idx][2]];
			}

			if(node_2_is_leaf)
			{
				edge_node_3 = node_2;
				edge_node_4 = node_2;
			}
			else
			{
				edge_node_3 = node_2->children[process_face_edge_nodes[direction][edge_idx][3]];
				edge_node_4 = node_2->children[process_face_edge_nodes[direction][edge_idx][4]];
			}*/

			//DC_ProcessEdge(edge_node_1, edge_node_2, edge_node_3, edge_node_4, process_face_edge_nodes[direction][edge_idx][5], builder);
		}
	}
}

void UOctreeManager::DC_ProcessEdge(StitchOctreeNode* node_1, StitchOctreeNode* node_2, StitchOctreeNode* node_3, StitchOctreeNode* node_4, unsigned char direction, MeshBuilder& builder)
{
	if (!(node_1 && node_2 && node_3 && node_4)) return;

	unsigned char types[4] = { node_1->type, node_2->type, node_3->type, node_4->type };
	StitchOctreeNode* edge_nodes[4] = { node_1, node_2, node_3, node_4 };

	if (types[0] && types[1] && types[2] && types[3])
	{
		//all nodes are leaves / collapsed, we can add to the polygon buffer

		uint32 indices[4];
		unsigned char sign_changes[4] = { 0, 0, 0, 0 };

		// we only want to add quads for a smallest node that owns the edge
		unsigned char lowest_depth = 0;
		// used to idx into sign_changes
		unsigned char minimal_node_idx = 0;

		bool flip = false;

		for (size_t node_idx = 0; node_idx < 4; node_idx++)
		{
			unsigned char edge_idx = node_edge[direction][node_idx];

			unsigned char corner_1 = edge_corners[edge_idx][0];
			unsigned char corner_2 = edge_corners[edge_idx][1];

#if UE_BUILD_DEBUG
			float node_half_size = edge_nodes[node_idx]->size * 0.5f;
			FVector3f corner_1_p = edge_nodes[node_idx]->center + child_offsets[corner_1] * node_half_size;
			FVector3f corner_2_p = edge_nodes[node_idx]->center + child_offsets[corner_2] * node_half_size;

			dbg_edge edge;
			edge.start = FVector(corner_1_p);
			edge.end = FVector(corner_2_p);

			debug_edges.Add(edge);
#endif
			unsigned char inside_1 = (edge_nodes[node_idx]->corners >> corner_1) & 1;
			unsigned char inside_2 = (edge_nodes[node_idx]->corners >> corner_2) & 1;

			indices[node_idx] = edge_nodes[node_idx]->tri_index;

			sign_changes[node_idx] = inside_1 != inside_2;

			if (edge_nodes[node_idx]->depth > lowest_depth)
			{
				lowest_depth = edge_nodes[node_idx]->depth;
				minimal_node_idx = node_idx;
				flip = static_cast<bool>(inside_1);
			}
		}

		if (sign_changes[minimal_node_idx])
		{
			if (flip)
			{
				builder.AddTriangle(indices[0], indices[1], indices[3]);
				builder.AddTriangle(indices[0], indices[3], indices[2]);
			}
			else
			{
				builder.AddTriangle(indices[0], indices[3], indices[1]);
				builder.AddTriangle(indices[0], indices[2], indices[3]);
			}
		}
	}
	else
	{
		for (size_t i = 0; i < 2; i++)
		{
			StitchOctreeNode* next_edge_nodes[4];
			for (size_t node_idx = 0; node_idx < 4; node_idx++)
			{
				if (types[node_idx])
				{
					//leaf
					next_edge_nodes[node_idx] = edge_nodes[node_idx];
				}
				else
				{
					next_edge_nodes[node_idx] = edge_nodes[node_idx]->children[process_sub_edge_nodes[direction][i][node_idx]];
				}
			}

			DC_ProcessEdge(next_edge_nodes[0], next_edge_nodes[1], next_edge_nodes[2], next_edge_nodes[3], process_sub_edge_nodes[direction][i][4], builder);
		}
	}
}


void UOctreeManager::DebugDrawOctree(OctreeNode* node, int32 current_depth, bool draw_leaves, bool draw_simple_leaves, int32 how_deep)
{
	if(!node || current_depth == how_deep) return;

	if(node->type == NODE_LEAF && draw_leaves) 
	{
		DebugDrawNode(node, node->size, FColor::Green);
	}
	else if(node->type == NODE_COLLAPSED_LEAF && draw_simple_leaves) 
	{
		DebugDrawNode(node, node->size, FColor::Red);
	}
	else if(node->type == NODE_INTERNAL)
	{
		DebugDrawNode(node, node->size, FColor::White);
	}

	for (size_t i = 0; i < 8; i++)
	{
		if(node->children[i])
		{
			DebugDrawOctree(node->children[i], current_depth+1, draw_leaves, draw_simple_leaves, how_deep);
		}
	}
}

void UOctreeManager::DebugDrawDCData(OctreeNode* node)
{
#if UE_BUILD_DEBUG
	for (size_t i = 0; i < debug_edges.Num(); i++)
	{
		DrawDebugLine(GetWorld(), debug_edges[i].start, debug_edges[i].end, FColor::White);
	}
#endif
	DebugDrawNodeMinimizer(node);
}

void UOctreeManager::DebugDrawNode(OctreeNode* node, float size, FColor color)
{
	DrawDebugBox(GetWorld(), FVector(node->center), FVector(size * 0.5f), color);
}

void UOctreeManager::DebugDrawNodeMinimizer(OctreeNode* node)
{
	if(!node) return;

	for (size_t i = 0; i < 8; i++)
	{
		DebugDrawNodeMinimizer(node->children[i]);	
	}

	if(node->type)
	{
		FVector normal = FVector(node->leaf_data.normal);
		FVector minimizer = FVector(node->leaf_data.minimizer * inv_scale_factor);

		FColor normal_color = FLinearColor((normal.X + 1.f) * 0.5f, (normal.Z + 1.f) * 0.5f, (normal.Y + 1.f) * 0.5f).ToFColor(true);
		DrawDebugDirectionalArrow(GetWorld(), minimizer, minimizer + normal * 40.f, 10.f, normal_color);

		DrawDebugPoint(GetWorld(), minimizer, 10.f, FColor::Green);
	}
	
}

FVector3f UOctreeManager::FDMGetNormal(const FVector3f& at_point)
{
	const float h = octree_settings->normal_fdm_offset;

	//x, y, z axii order
	const float x_positions[6] = { at_point.X + h,  at_point.X - h, at_point.X, at_point.X, at_point.X, at_point.X };
	const float y_positions[6] = { at_point.Y,  at_point.Y, at_point.Y + h, at_point.Y - h, at_point.Y, at_point.Y };
	const float z_positions[6] = { at_point.Z,  at_point.Z, at_point.Z, at_point.Z, at_point.Z + h, at_point.Z - h };

	TArray<float> noise = noise_gen->GetNoiseFromPositions3D_NonThreaded(x_positions, y_positions, z_positions, 6);

	FVector3f normal = FVector3f(noise[0] - noise[1], noise[2] - noise[3], noise[4] - noise[5]);

	normal /= (2.f * h);

#if UE_BUILD_DEBUG
	if(normal.X == 0.f && normal.Y == 0.f && normal.Z == 0.f) 
	{
		//check(false);
	}
#endif

	return normal.GetUnsafeNormal();
}

OctreeNode* UOctreeManager::GetNodeFromPositionDepth(OctreeNode* start, FVector3f p, int8 depth)
{
	OctreeNode* current = start;
	while(current->depth != depth)
	{
		uint8 idx = GetChildNodeFromPosition(p, current->center);
		current = current->children[idx];
	}

	return current;
}

bool UOctreeManager::DoesSupportWorldType(EWorldType::Type type) const
{
	return type == EWorldType::Game || type == EWorldType::Editor || type == EWorldType::PIE;
}

bool UOctreeManager::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* W = Cast<UWorld>(Outer))
	{
		const auto WT = W->WorldType;
		return WT == EWorldType::Game || WT == EWorldType::PIE  || WT == EWorldType::Editor;
	}
	return false;
}
