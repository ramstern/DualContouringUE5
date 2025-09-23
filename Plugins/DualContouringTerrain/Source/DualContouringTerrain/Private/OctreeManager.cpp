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
#include "Kismet/GameplayStatics.h"
#include "LevelEditorViewport.h"

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

	ADC_OctreeRenderActor* created_render_actor = GetWorld()->SpawnActor<ADC_OctreeRenderActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (created_render_actor)
	{
#if WITH_EDITOR
		created_render_actor->bIsEditorOnlyActor = (GetWorld()->WorldType == EWorldType::Editor);
		created_render_actor->SetActorLabel(TEXT("Octree Render (Transient)"));
#endif
		render_actor = created_render_actor;
	}

	Params.Name = TEXT("TestCameraProxy");
	
	cam_proxy_actor = GetWorld()->SpawnActor<ADC_OctreeRenderActor>(FVector(50.f, 50.f, 50.f), FRotator::ZeroRotator, Params);
	

#if WITH_EDITOR
	UOctreeSettings::OnChanged().AddUObject(this, &UOctreeManager::RebuildOctree);
#endif

	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UOctreeManager::PostWorldInit);

	octree_mesh = render_actor->mesh_component->InitializeRealtimeMesh<URealtimeMeshSimple>();
	RebuildOctree();
}

void UOctreeManager::Deinitialize()
{
	Super::Deinitialize();

#if WITH_EDITOR
	UOctreeSettings::OnChanged().RemoveAll(this);
#endif

	delete root_node;
	root_node = nullptr;

	render_actor->Destroy();
	render_actor = nullptr;

	octree_mesh = nullptr;
}

void UOctreeManager::PostWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	//stupid rmc bug where materials do not apply after creation? this fixes it
	render_actor->mesh_component->ReregisterComponent();
}


OctreeNode* UOctreeManager::SetupOctree()
{
	OctreeNode* initial_root_node = new OctreeNode();
	initial_root_node->center = FVector3f(octree_settings->initial_size*0.5f, octree_settings->initial_size*0.5f, octree_settings->initial_size*0.5f);
	initial_root_node->depth = 0;

	ConstructChildNodes(initial_root_node);

	return initial_root_node;
}

//FVector3f child_offsets[8] = 
//{
//	{1.f, -1.f, -1.f}, 
//	{1.f, 1.f, -1.f},
//	{-1.f, 1.f, -1.f},
//	{-1.f, -1.f, -1.f},
//	{1.f, -1.f, 1.f},
//	{1.f, 1.f, 1.f},
//	{-1.f, 1.f, 1.f},
//	{-1.f, -1.f, 1.f}
//};

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

OctreeNode* UOctreeManager::ConstructChildNodes(OctreeNode*& node)
{
	float node_size = SizeFromNodeDepth(node->depth);
	return ConstructChildNodes(node, node_size);
}

OctreeNode* UOctreeManager::ConstructChildNodes(OctreeNode*& node, float node_size)
{
	node->size = node_size;

	if(node->depth == octree_settings->max_depth)
	{
		return ConstructLeafNode(node);
	}

	bool has_children = false;

	for (uint8_t i = 0; i < 8; i++)
	{
		OctreeNode* child = new OctreeNode();

		child->depth = node->depth + 1;
		child->center = node->center + child_offsets[i] * node->size * 0.25f;
		node->children[i] = ConstructChildNodes(child);
		node->child_mask |= static_cast<bool>(node->children[i]) << i;

		has_children |= static_cast<bool>(node->children[i]);
	}

	if(!has_children) 
	{
		delete node;
		node = nullptr;
	}

	return node;
}

//constexpr unsigned char edges_corner_map[12][2] =
//{
//	{0, 1}, {1, 2}, {2, 3}, {3, 0},
//	{4, 0}, {1, 5}, {2, 6}, {7, 3},
//	{4, 5}, {5, 6}, {6, 7}, {7, 4}
//};

constexpr unsigned char edges_corner_map[12][2] =
{
	{0,4},{1,5},{2,6},{3,7},	// x-axis 
	{0,2},{1,3},{4,6},{5,7},	// y-axis
	{0,1},{2,3},{4,5},{6,7}		// z-axis
};

OctreeNode* UOctreeManager::ConstructLeafNode(OctreeNode*& node)
{
	const unsigned int MAX_ZERO_CROSSINGS = 6;
	
	TArray<float> corner_densities = SampleOctreeNodeDensities(node);

	for (uint8_t i = 0; i < 8; i++)
	{
		node->corners |= ((corner_densities[i] - octree_settings->iso_surface) <= 0.f) << i;
	}

	if(node->corners == 255 || node->corners == 0)
	{
		delete node;
		node = nullptr;
		return nullptr;
	}

	//node is a leaf
	node->type = NODE_LEAF;

	FVector3f mass_point{};
	FVector3f vert_normal{};
	uint8 edge_count = 0;
	quadric3 vox_pq;

	//UE_ASSUME(edge_count != 0);

	for (size_t i = 0; i < 12 && edge_count < MAX_ZERO_CROSSINGS; i++)
	{
		unsigned char s1 = (node->corners >> edges_corner_map[i][0]) & 1;
		unsigned char s2 = (node->corners >> edges_corner_map[i][1]) & 1;

		if (s1 == s2) continue;

		//detected sign change on current edge
		FVector3f corner_1 = child_offsets[edges_corner_map[i][0]]*node->size*0.5f + node->center;
		FVector3f corner_2 = child_offsets[edges_corner_map[i][1]]*node->size*0.5f + node->center;

		float d1 = corner_densities[edges_corner_map[i][0]] - octree_settings->iso_surface;
		float d2 = corner_densities[edges_corner_map[i][1]] - octree_settings->iso_surface;

		// (1-alpha)*d1 + alpha*d2 = 0
		// d1 - alpha*d1 + alpha*d2 = 0
		// d1 + alpha(-d1+d2) = 0
		// alpha = d1 / (d1-d2)
		float alpha = d1 / (d1 - d2 + FLT_EPSILON);

		FVector3f intersection = FMath::Lerp(corner_1, corner_2, alpha);
		mass_point += intersection;

		//at 32 vox size, i dont think it's worth doing better zero crossing. below usually gives values in order of 0.001 > x > -0.001
		float alpha_density = noise_gen->GetNoiseSingle3D(intersection.X * scale_factor, intersection.Y * scale_factor, intersection.Z * scale_factor) - octree_settings->iso_surface;

		FVector3f normal = FDMGetNormal(intersection * scale_factor);
		vert_normal += normal;

		vox_pq += quadric3::probabilistic_plane_quadric(intersection * scale_factor, normal, octree_settings->stddev_pos, octree_settings->stddev_normal);
		edge_count++;
	}

	node->leaf_data = new DC_LeafData();
	node->leaf_data->qef = vox_pq;
	node->leaf_data->normal = vert_normal / edge_count;
	node->leaf_data->minimizer = vox_pq.minimizer();

#if CLAMP_MINIMIZERS

	float half_size = node->size * 0.5f * scale_factor;
	FVector3f scaled_center = node->center * scale_factor;

	if(node->leaf_data->minimizer.X > scaled_center.X + half_size)
	{
		node->leaf_data->minimizer.X = scaled_center.X + half_size;
	}
	if(node->leaf_data->minimizer.Y > scaled_center.Y + half_size)
	{
		node->leaf_data->minimizer.Y = scaled_center.Y + half_size;
	}
	if(node->leaf_data->minimizer.Z > scaled_center.Z + half_size)
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
	return node;
}

TArray<float> UOctreeManager::SampleOctreeNodeDensities(OctreeNode* node)
{
	const float scaling_factor = 0.01f;

	float half_node_size = node->size * scaling_factor * 0.5f;

	FVector3f node_center = node->center * scaling_factor;

	////todo repeated operations
	//float x_positions[8] = {node_center.X + half_node_size, node_center.X + half_node_size, node_center.X - half_node_size, node_center.X - half_node_size, node_center.X + half_node_size, node_center.X + half_node_size, node_center.X - half_node_size, node_center.X - half_node_size};
	//float y_positions[8] = {node_center.Y - half_node_size, node_center.Y + half_node_size, node_center.Y + half_node_size, node_center.Y - half_node_size, node_center.Y - half_node_size, node_center.Y + half_node_size, node_center.Y + half_node_size, node_center.Y - half_node_size};
	//float z_positions[8] = {node_center.Z - half_node_size, node_center.Z - half_node_size, node_center.Z - half_node_size, node_center.Z + half_node_size, node_center.Z + half_node_size, node_center.Z + half_node_size, node_center.Z + half_node_size, node_center.Z + half_node_size};

	float x_positions[8], y_positions[8], z_positions[8];
	for (int i = 0; i < 8; ++i)
	{
		x_positions[i] = node_center.X + child_offsets[i].X * half_node_size;
		y_positions[i] = node_center.Y + child_offsets[i].Y * half_node_size;
		z_positions[i] = node_center.Z + child_offsets[i].Z * half_node_size;
	}

	return noise_gen->GetNoiseFromPositions3D_NonThreaded(x_positions, y_positions, z_positions, 8);
}

void UOctreeManager::RebuildOctree()
{
#if UE_BUILD_DEBUG
	debug_edges.Empty();
#endif

	octree_mesh->Reset();

	delete root_node;
	root_node = SetupOctree();
	if(octree_settings->simplify) SimplifyOctree(root_node);

	RealtimeMesh::FRealtimeMeshStreamSet stream_set;
	RealtimeMesh::TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> builder(stream_set);
	builder.EnableTangents();

	BuildMeshData(root_node, builder);
	DC_ProcessCell(root_node, builder);

	const FRealtimeMeshSectionGroupKey group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh", mesh_group_keys.Num()));
	mesh_group_keys.Add(group_key);

	octree_mesh->SetupMaterialSlot(0, "PrimaryMaterial", octree_settings->mesh_material.LoadSynchronous());
	octree_mesh->CreateSectionGroup(group_key, stream_set);
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
	quadric3 node_pq;
	FVector3f average_normal{};
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
				node_pq += node->children[i]->leaf_data->qef;
				average_normal += node->children[i]->leaf_data->normal;
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
	node->leaf_data = new DC_LeafData();
	node->leaf_data->qef = node_pq;
	node->leaf_data->minimizer = minimizer;
	node->leaf_data->normal = average_normal/static_cast<float>(count);

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
		const auto& vertex = builder.AddVertex(node->leaf_data->minimizer * inv_scale_factor).SetNormal(node->leaf_data->normal);
		node->leaf_data->index = vertex.GetIndex();
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
			builder.AddVertex(node->leaf_data->minimizer * inv_scale_factor).SetNormal(node->leaf_data->normal);
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

			indices[node_idx] = edge_nodes[node_idx]->leaf_data->index;

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


void UOctreeManager::DebugDrawOctree(OctreeNode* node, int32 current_depth)
{
	if(!node || current_depth == octree_settings->debug_draw_how_deep) return;

	if(node->type == NODE_LEAF && octree_settings->draw_leaves) 
	{
		DebugDrawNode(node, node->size, FColor::Green);
	}
	else if(node->type == NODE_COLLAPSED_LEAF && octree_settings->draw_simplified_leaves) 
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
			DebugDrawOctree(node->children[i], current_depth+1);
		}
	}
}

void UOctreeManager::DebugDrawDCData()
{
#if UE_BUILD_DEBUG
	for (size_t i = 0; i < debug_edges.Num(); i++)
	{
		DrawDebugLine(GetWorld(), debug_edges[i].start, debug_edges[i].end, FColor::White);
	}
#endif
	DebugDrawNodeMinimizer(root_node);
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
		FVector normal = FVector(node->leaf_data->normal);
		FVector minimizer = FVector(node->leaf_data->minimizer * inv_scale_factor);

		FColor normal_color = FLinearColor((normal.X + 1.f) * 0.5f, (normal.Z + 1.f) * 0.5f, (normal.Y + 1.f) * 0.5f).ToFColor(true);
		DrawDebugDirectionalArrow(GetWorld(), minimizer, minimizer + normal * 40.f, 10.f, normal_color);

		DrawDebugPoint(GetWorld(), minimizer, 10.f, FColor::Green);
	}
	
}

FVector3f UOctreeManager::FDMGetNormal(FVector3f at_point)
{
	float h = octree_settings->normal_fdm_offset;

	//x, y, z axii order
	float x_positions[6] = { at_point.X + h,  at_point.X - h, at_point.X, at_point.X, at_point.X, at_point.X };
	float y_positions[6] = { at_point.Y,  at_point.Y, at_point.Y + h, at_point.Y - h, at_point.Y, at_point.Y };
	float z_positions[6] = { at_point.Z,  at_point.Z, at_point.Z, at_point.Z, at_point.Z + h, at_point.Z - h };

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



FVector UOctreeManager::GetActiveCameraLocation()
{
	return cam_proxy_actor->GetActorLocation();

//#if WITH_EDITOR
//	if (GEditor->IsPlaySessionInProgress())
//	{
//		if (APlayerCameraManager* camera_manager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
//		{
//			return camera_manager->GetCameraLocation();
//		}
//		else 
//		{
//			//possible fallback
//			//UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)->
//			return FVector();
//		}
//		
//	}
//	else if(GCurrentLevelEditingViewportClient)
//	{
//		return GCurrentLevelEditingViewportClient->GetViewLocation();
//	}
//	else 
//	{
//		return FVector();
//	}
//#else 
//	return APlayerCameraManager * camera_manager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraLocation();
//#endif
}

uint8 UOctreeManager::GetChildNodeFromPosition(FVector3f p, FVector3f node_center)
{
	bool x = p.X > node_center.X;
	bool y = p.Y > node_center.Y;
	bool z = p.Z > node_center.Z;

	//x is 0b100, y 0b010, z 0b001

	return (static_cast<uint8>(y) << 2) | (static_cast<uint8>(z) << 1) | static_cast<uint8>(x);
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

void UOctreeManager::Tick(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	if(octree_settings->draw_octree) DebugDrawOctree(root_node, 0);
	if(octree_settings->draw_dc_data) DebugDrawDCData();
#endif


	// the dynamic octree construction loop

	camera_pos = GetActiveCameraLocation();

	uint8 extending_node_idx = GetChildNodeFromPosition(FVector3f(camera_pos), root_node->center);

	UE_LOG(LogTemp,Display, TEXT("%i"), extending_node_idx);

	if(octree_settings->stop_dynamic_octree) return;
	//detect camera node change
	if(last_visited_child_idx != extending_node_idx)
	{
		uint8 new_extending_node_idx;
		uint8 flip;
		do
		{
			float quarter_extending_node_size = root_node->size * 0.5f;
			FVector3f new_center = root_node->center + child_offsets[extending_node_idx] * quarter_extending_node_size;

			uint8 existing_node_idx = GetChildNodeFromPosition(root_node->center, new_center);
			uint8 child_compute_mask = ~(1 << existing_node_idx);

			OctreeNode* extending_node = new OctreeNode();
			extending_node->center = new_center;
			extending_node->depth = root_node->depth - 1;
			extending_node->size = root_node->size * 2.f;

			// assing the old root node to the new extending root child at existing_node_idx
			extending_node->children[existing_node_idx] = root_node;

			for (uint8 i = 0; i < 8; i++)
			{
				// selectively make the remaining 7 nodes
				if ((child_compute_mask >> i) & 1)
				{
					OctreeNode* extending_child = new OctreeNode();
					extending_child->center = new_center + child_offsets[i] * quarter_extending_node_size;
					extending_child->size = root_node->size;
					extending_child->depth = root_node->depth;

					ConstructChildNodes(extending_child, extending_child->size);

					extending_node->children[i] = extending_child;
				}
			}

			PolygonizeExtendingNode(extending_node, existing_node_idx, 0);

			root_node = extending_node;

			//ConstructSeamOctree(extending_node->children[existing_node_idx], existing_node_idx, extending_node, );


			//RealtimeMesh::FRealtimeMeshStreamSet stitch_stream_set;
			//
			//

			//MeshBuilder stitch_builder(stitch_stream_set);
			//stitch_builder.EnableTangents();

			////the least we can do is reserve the amount up front
			///*stitch_builder.ReserveNumTriangles(total_children_tris);
			//stitch_builder.ReserveNumVertices(total_children_verts);*/

			////BuildMeshData(extending_node, stitch_builder);


			////handles every interior face of the extending node
			//for (size_t i = 0; i < 12; i++)
			//{
			//	OctreeNode* child_1 = extending_node->children[process_cell_face_nodes[i][0]];
			//	OctreeNode* child_2 = extending_node->children[process_cell_face_nodes[i][1]];
			//	DC_ProcessFace(child_1, child_2, process_cell_face_nodes[i][2], stitch_builder);
			//}

			////interior 6 edges of the extending node
			//for (size_t i = 0; i < 6; i++)
			//{
			//	OctreeNode* child_1 = extending_node->children[process_edge_nodes[i][0]];
			//	OctreeNode* child_2 = extending_node->children[process_edge_nodes[i][1]];
			//	OctreeNode* child_3 = extending_node->children[process_edge_nodes[i][2]];
			//	OctreeNode* child_4 = extending_node->children[process_edge_nodes[i][3]];
			//	DC_ProcessEdge(child_1, child_2, child_3, child_4, process_edge_nodes[i][4], stitch_builder);
			//}

			//const FRealtimeMeshSectionGroupKey group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh_Stitch", mesh_group_keys.Num()));
			//mesh_group_keys.Add(group_key);
			//octree_mesh->CreateSectionGroup(group_key, stitch_stream_set);


			//the node the camera is supposed to be in, when extended
			new_extending_node_idx = GetChildNodeFromPosition(FVector3f(camera_pos), extending_node->center);

			flip = ~new_extending_node_idx & 0b00000111;
		}
		while(extending_node_idx != flip);

		//make sure the extending_node_idx is flipped this time, when we set last_visited to extending_node_idx
		extending_node_idx = ~extending_node_idx & 0b00000111;
	}

	last_visited_child_idx = extending_node_idx;
}

#include "SeamRecursionFunctions.inl"

//gets two mirrored nodes along center that are not the input node idx
constexpr unsigned char seam_node_pair[8][2] =
{
	{2, 5}, {3, 4}, {0, 7}, {1, 6}, {6, 1}, {7, 0}, {4, 3}, {5, 2}
};

using recfunc_sig = StitchOctreeNode* (*)(OctreeNode*, StitchOctreeNode*, MeshBuilder&);

constexpr recfunc_sig own_seam_operations[8][3] = 
{
	{&FrontRecurse, &RightRecurse, &TopRecurse},
	{&BackRecurse, &RightRecurse, &TopRecurse},
	{&FrontRecurse, &RightRecurse, &BottomRecurse},
	{&BackRecurse, &RightRecurse, &BottomRecurse},
	{&FrontRecurse, &LeftRecurse, &TopRecurse},
	{&BackRecurse, &LeftRecurse, &TopRecurse},
	{&FrontRecurse, &LeftRecurse, &BottomRecurse},
	{&BackRecurse, &LeftRecurse, &BottomRecurse}
};

constexpr recfunc_sig other_seam_operations[8][8] = 
{
	{nullptr, &BackRecurse, &BottomRecurse, &CornerBarRecurseBB, &RightRecurse, &CornerBarRecurseVLB, &CornerBarRecurseBL, &CornerMiniRecurse_0},
	{&FrontRecurse, nullptr, &CornerBarRecurseBF, &BottomRecurse, &CornerBarRecurseVLF, &LeftRecurse, &CornerMiniRecurse_1, &CornerBarRecurseBL},
	{&TopRecurse, &CornerBarRecurseTB, nullptr, &BackRecurse,	&CornerBarRecurseTL, &CornerMiniRecurse_2, &LeftRecurse, &CornerBarRecurseVLB},
	{&CornerBarRecurseTF, &TopRecurse, &FrontRecurse, nullptr, &CornerMiniRecurse_3, &CornerBarRecurseTL, &CornerBarRecurseVLB, &LeftRecurse},
	{&RightRecurse, &CornerBarRecurseVRB, &CornerBarRecurseBR, &CornerMiniRecurse_4, nullptr, &BackRecurse, &BottomRecurse, &CornerBarRecurseBB},
	{&CornerBarRecurseVRF, &RightRecurse, &CornerMiniRecurse_5, &CornerBarRecurseBR, &FrontRecurse, nullptr, &CornerBarRecurseBF, &BottomRecurse},
	{&CornerBarRecurseTR, &CornerMiniRecurse_6, &RightRecurse, &CornerBarRecurseVRB, &TopRecurse, &CornerBarRecurseTB, nullptr, &BackRecurse},
	{&CornerMiniRecurse_7, &CornerBarRecurseTR, &CornerBarRecurseVRB, &RightRecurse, &CornerBarRecurseTF, &TopRecurse, &FrontRecurse, nullptr}
};

void UOctreeManager::PolygonizeExtendingNode(OctreeNode* extending_node, uint8 existing_node_idx, int8 mesh_depth)
{
	//if(!extending_node) return;


	//code below can only be run if the extending node is only 1 level bigger,
	//or if we found a valid bigger quadrant and subdivided until mesh_depth - 1
	if(extending_node->depth == mesh_depth - 1)
	{
		// twice for each node in a seam_node_pair, we need to gather the operations needed on the 8 nodes to stitch the seams,
		// based on existing_node_idx.

		// requisites:

		// we need a table for each node idx that describes the 3 operations for this node
		// we need a table for each node idx that describes the operation for every other node
		
		const unsigned char* pair = seam_node_pair[existing_node_idx];

		RealtimeMesh::FRealtimeMeshStreamSet stream_sets[8];
		MeshBuilder builders[8] = {MeshBuilder(stream_sets[0]), MeshBuilder(stream_sets[1]), MeshBuilder(stream_sets[2]), MeshBuilder(stream_sets[3]),
								   MeshBuilder(stream_sets[4]), MeshBuilder(stream_sets[5]), MeshBuilder(stream_sets[6]), MeshBuilder(stream_sets[7])};
		
		//isolate polygonize
		for (uint8 i = 0; i < 8; i++)
		{
			if (i == existing_node_idx) continue;

			builders[i].EnableTangents();

			IsolatedPolygonizeNode(extending_node->children[i], builders[i]);
		}

		StitchOctreeNode* stitch_root[2];
		for (uint8 i = 0; i < 2; i++)
		{
			stitch_root[i] = new StitchOctreeNode();
			stitch_root[i]->type = NODE_INTERNAL;
			stitch_root[i]->depth = extending_node->depth;
			stitch_root[i]->corners = 0;

			StitchOctreeNode* stitch_main = own_seam_operations[pair[i]][0](extending_node->children[pair[i]], nullptr, builders[pair[i]]);
											own_seam_operations[pair[i]][1](extending_node->children[pair[i]], stitch_main, builders[pair[i]]);
											own_seam_operations[pair[i]][2](extending_node->children[pair[i]], stitch_main, builders[pair[i]]);

			stitch_root[i]->children[pair[0]] = stitch_main;

			for (uint8 j = 0; j < 8; j++)
			{
				if (j == pair[i]) continue;

				stitch_root[i]->children[j] = other_seam_operations[pair[i]][j](extending_node->children[j], nullptr, builders[pair[i]]);
			}

			DC_ProcessCell(stitch_root[i], builders[pair[i]]);
			delete stitch_root[i];
		}

		for (uint8 i = 0; i < 8; i++)
		{
			const FRealtimeMeshSectionGroupKey group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh", mesh_group_keys.Num()));
			mesh_group_keys.Add(group_key);
			octree_mesh->CreateSectionGroup(group_key, stream_sets[i]);
		}
	}

	//if(node->depth == mesh_depth)
	//{
	//	//polygonize octant mesh
	//	RealtimeMesh::FRealtimeMeshStreamSet stream_set;
	//	MeshBuilder builder(stream_set);
	//	builder.EnableTangents();

	//	////call method to walk down octant and construct seam octree 
	//	StitchOctreeNode* stitched = ConstructSeamOctree(node, node_idx, parent, builder);

	//	DC_ProcessCell(stitched, builder);

	//	const FRealtimeMeshSectionGroupKey group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh", mesh_group_keys.Num()));
	//	mesh_group_keys.Add(group_key);
	//	octree_mesh->CreateSectionGroup(group_key, stream_set);
	//}
	//else
	//{
	//	//subdivide
	//	for (uint8 i = 0; i < 8; i++)
	//	{
	//		PolygonizeExtendingNode(node->children[i], i, node, mesh_depth);
	//	}
	//}
	
}

void UOctreeManager::IsolatedPolygonizeNode(OctreeNode* node, MeshBuilder& builder)
{
	BuildMeshData(node, builder);

	DC_ProcessCell(node, builder);
}

//StitchOctreeNode* UOctreeManager::ConstructSeamOctree(OctreeNode* from_node, uint8 node_idx, OctreeNode* parent, MeshBuilder& builder)
//{
//	// we need to walk down this with the normal octree nodes, 
//	// then construct the stitch nodes along the way and assign index to the stitch node and vertex to the builder
//	
//	//this node recursion (no special case needed)
//	StitchOctreeNode* stitch_main = left_x_recurse(left_x_recurse, from_node, nullptr, builder);
//	back_z_recurse(back_z_recurse, from_node, stitch_main, builder);
//	top_y_recurse(top_y_recurse, from_node, stitch_main, builder);
//
//	FVector3f left_query_p = from_node->center - FVector3f(0.f, from_node->size, 0.f);
//	FVector3f back_query_p = from_node->center - FVector3f(from_node->size, 0.f, 0.f);
//	FVector3f top_query_p = from_node->center + FVector3f(0.f, 0.f, from_node->size);
//	FVector3f corner_y_query_p = from_node->center - FVector3f(from_node->size, from_node->size, 0.f);
//	FVector3f corner_z_query_p = from_node->center + FVector3f(0.f, -from_node->size, from_node->size);
//	FVector3f corner_x_query_p = from_node->center + FVector3f(-from_node->size, 0.f, from_node->size);
//	FVector3f corner_mini_query_p = from_node->center - FVector3f(from_node->size, from_node->size, from_node->size);
//
//	StitchOctreeNode* stitch_root = new StitchOctreeNode();
//	stitch_root->type = NODE_INTERNAL;
//	stitch_root->depth = from_node->depth-1;
//	stitch_root->corners = 0;
//
//	OctreeNode* left_neighbor = nullptr;
//	OctreeNode* back_neighbor = nullptr;
//	OctreeNode* top_neighbor = nullptr;
//	OctreeNode* corner_y_neighbor = nullptr;
//	OctreeNode* corner_z_neighbor = nullptr;
//	OctreeNode* corner_x_neighbor = nullptr;
//	OctreeNode* corner_mini_neighbor = nullptr;
//
//	//now, we need to get neighbor side nodes that lay next to from_node...
//	switch(node_idx)
//	{
//	case 0:
//
//		left_neighbor = GetNodeFromPositionDepth(root_node, left_query_p, from_node->depth);
//		back_neighbor = GetNodeFromPositionDepth(root_node, back_query_p, from_node->depth);
//		top_neighbor = parent->children[2];
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = GetNodeFromPositionDepth(root_node, corner_z_query_p, from_node->depth);
//		corner_x_neighbor = GetNodeFromPositionDepth(root_node, corner_x_query_p, from_node->depth);
//		corner_mini_neighbor = GetNodeFromPositionDepth(root_node,corner_mini_query_p, from_node->depth);
//
//		break;
//	case 1:
//
//		left_neighbor = GetNodeFromPositionDepth(root_node, left_query_p, from_node->depth);
//		back_neighbor = parent->children[0];
//		top_neighbor = parent->children[3];
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = GetNodeFromPositionDepth(root_node, corner_z_query_p, from_node->depth);
//		corner_x_neighbor = parent->children[2];
//		corner_mini_neighbor = GetNodeFromPositionDepth(root_node, corner_mini_query_p, from_node->depth);
//
//		break;
//	case 2:
//
//		left_neighbor = GetNodeFromPositionDepth(root_node, left_query_p, from_node->depth);
//		back_neighbor = GetNodeFromPositionDepth(root_node, back_query_p, from_node->depth);
//		top_neighbor = GetNodeFromPositionDepth(root_node, top_query_p, from_node->depth);
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = GetNodeFromPositionDepth(root_node, corner_z_query_p, from_node->depth);
//		corner_x_neighbor = GetNodeFromPositionDepth(root_node, corner_x_query_p, from_node->depth);
//		corner_mini_neighbor= GetNodeFromPositionDepth(root_node, corner_mini_query_p, from_node->depth);
//
//		break;
//	case 3:
//
//		left_neighbor = GetNodeFromPositionDepth(root_node, left_query_p, from_node->depth);
//		back_neighbor = parent->children[2];
//		top_neighbor = GetNodeFromPositionDepth(root_node, top_query_p, from_node->depth);
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = GetNodeFromPositionDepth(root_node, corner_z_query_p, from_node->depth);
//		corner_x_neighbor = GetNodeFromPositionDepth(root_node, corner_x_query_p, from_node->depth);
//		corner_mini_neighbor = GetNodeFromPositionDepth(root_node, corner_mini_query_p, from_node->depth);
//
//		break;
//	case 4:
//
//		left_neighbor = parent->children[0];
//		back_neighbor = GetNodeFromPositionDepth(root_node, back_query_p, from_node->depth);
//		top_neighbor = parent->children[6];
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = parent->children[2];
//		corner_x_neighbor = GetNodeFromPositionDepth(root_node, corner_x_query_p, from_node->depth);
//		corner_mini_neighbor = GetNodeFromPositionDepth(root_node, corner_mini_query_p, from_node->depth);
//
//		break;
//	case 5:
//
//		left_neighbor = parent->children[1];
//		back_neighbor = parent->children[4];
//		top_neighbor = parent->children[7];
//		corner_y_neighbor = parent->children[0];
//		corner_z_neighbor = parent->children[3];
//		corner_x_neighbor = parent->children[6];
//		corner_mini_neighbor = parent->children[2];
//
//		break;
//	case 6:
//
//		left_neighbor = parent->children[2];
//		back_neighbor = GetNodeFromPositionDepth(root_node, back_query_p, from_node->depth);
//		top_neighbor = GetNodeFromPositionDepth(root_node, top_query_p, from_node->depth);
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = GetNodeFromPositionDepth(root_node, corner_z_query_p, from_node->depth);
//		corner_x_neighbor = GetNodeFromPositionDepth(root_node, corner_x_query_p, from_node->depth);
//		corner_mini_neighbor = GetNodeFromPositionDepth(root_node, corner_mini_query_p, from_node->depth);
//
//		break;
//	case 7:
//
//		left_neighbor = parent->children[3];
//		back_neighbor = parent->children[6];
//		top_neighbor = GetNodeFromPositionDepth(root_node, top_query_p, from_node->depth);
//		corner_y_neighbor = GetNodeFromPositionDepth(root_node, corner_y_query_p, from_node->depth);
//		corner_z_neighbor = GetNodeFromPositionDepth(root_node, corner_z_query_p, from_node->depth);
//		corner_x_neighbor = GetNodeFromPositionDepth(root_node, corner_x_query_p, from_node->depth);
//		corner_mini_neighbor = GetNodeFromPositionDepth(root_node, corner_mini_query_p, from_node->depth);
//
//		break;
//	}
//
//	//border cases
//	if(left_neighbor == from_node) left_neighbor = nullptr;
//	if(back_neighbor == from_node) back_neighbor = nullptr;
//	if(top_neighbor == from_node) top_neighbor = nullptr;
//	if(corner_y_neighbor == from_node) corner_y_neighbor = nullptr;
//	if(corner_x_neighbor == from_node) corner_x_neighbor = nullptr;
//	if(corner_z_neighbor == from_node) corner_z_neighbor = nullptr;
//	if(corner_mini_neighbor == from_node) corner_mini_neighbor = nullptr;
//
//	StitchOctreeNode* stitch_left = right_x_recurse(right_x_recurse, left_neighbor, nullptr, builder);
//	StitchOctreeNode* stitch_back = front_z_recurse(front_z_recurse, back_neighbor, nullptr, builder);
//	StitchOctreeNode* stitch_top = bottom_y_recurse(bottom_y_recurse, top_neighbor, nullptr, builder);
//	StitchOctreeNode* stitch_y_corner = corner_y_recurse(corner_y_recurse, corner_y_neighbor, nullptr, builder);
//	StitchOctreeNode* stitch_z_corner = corner_z_recurse(corner_z_recurse, corner_z_neighbor, nullptr, builder);
//	StitchOctreeNode* stitch_x_corner = corner_x_recurse(corner_x_recurse, corner_x_neighbor, nullptr, builder);
//	StitchOctreeNode* stitch_mini_corner = corner_mini_recurse(corner_mini_recurse, corner_mini_neighbor, nullptr, builder);
//
//	stitch_root->children[5] = stitch_main;
//	stitch_root->children[1] = stitch_left;
//	stitch_root->children[0] = stitch_y_corner;
//	stitch_root->children[4] = stitch_back;
//	stitch_root->children[7] = stitch_top;
//	stitch_root->children[6] = stitch_x_corner;
//	stitch_root->children[3] = stitch_z_corner;
//	stitch_root->children[2] = stitch_mini_corner;
//
//	return stitch_root;
//}

TStatId UOctreeManager::GetStatId() const
{
	return TStatId();
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

bool UOctreeManager::IsTickable() const
{
	return true;
}

bool UOctreeManager::IsTickableInEditor() const
{
	return true;
}
