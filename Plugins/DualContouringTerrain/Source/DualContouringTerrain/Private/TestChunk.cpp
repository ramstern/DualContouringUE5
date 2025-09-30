// Fill out your copyright notice in the Description page of Project Settings.


#include "TestChunk.h"

#include "NoiseDataGenerator.h"
#include "Kismet/KismetMathLibrary.h"

#include "Math/Matrix.h"

#include "probabilistic-quadrics.hh"
#include "DC_Mat3x3.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"

using uemath = pq::math<float, FVector3f, FVector3f, FMatrix3x3>;

//typedef quadrid type
using quadric3 = pq::quadric<uemath>;

// Sets default values
ATestChunk::ATestChunk()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	mesh_component = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMeshComponent"));
	mesh_component->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ATestChunk::BeginPlay()
{
	Super::BeginPlay();
	
	gen = GEngine->GetEngineSubsystem<UNoiseDataGenerator>();

	const unsigned int p_count = (voxel_resolution.X + 1) * (voxel_resolution.Y+1) * (voxel_resolution.Z+1);

	TArray<float> x_positions, y_positions, z_positions;
	x_positions.Reserve(p_count);
	y_positions.Reserve(p_count);
	z_positions.Reserve(p_count);

	float factor_x = static_cast<float>(16) / static_cast<float>(voxel_resolution.X);
	float factor_y = static_cast<float>(16) / static_cast<float>(voxel_resolution.Y);
	float factor_z = static_cast<float>(16) / static_cast<float>(voxel_resolution.Z);

	for (size_t x = 0; x < voxel_resolution.X+1; x++)
	{
		for (size_t y = 0; y < voxel_resolution.Y+1; y++)
		{
			for (size_t z = 0; z < voxel_resolution.Z+1; z++)
			{
				x_positions.Add(static_cast<float>(x) * factor_x);
				y_positions.Add(static_cast<float>(y) * factor_y);
				z_positions.Add(static_cast<float>(z) * factor_z);
			}
		}
	}

	noise_gen_task = gen->GetNoiseFromPositions3D(x_positions.GetData(), y_positions.GetData(), z_positions.GetData(), p_count);
	mesh = mesh_component->InitializeRealtimeMesh<URealtimeMeshSimple>();

	voxel_datas.Reserve(voxel_resolution.X * voxel_resolution.Y * voxel_resolution.Z);
}

void ATestChunk::DrawNoiseDensities(TArray<float>& densities)
{
	for (size_t x = 0; x < voxel_resolution.X+1; x++)
	{
		for (size_t y = 0; y < voxel_resolution.Y+1; y++)
		{
			for (size_t z = 0; z < voxel_resolution.Z+1; z++)
			{
				FVector pos = FVector(static_cast<float>(x)*voxel_size, static_cast<float>(y)*voxel_size, static_cast<float>(z)*voxel_size);

				float density = densities[Get1DIndexFrom3D(x,y,z, FIntVector(voxel_resolution.X+1, voxel_resolution.Y+1, voxel_resolution.Z+1))];
				FColor color;
				color.R = color.G = color.B = FColor::QuantizeUNormFloatTo8(density);
				color.A = 1.f;

				DrawDebugPoint(GetWorld(), pos, voxel_size*0.1f, color);
			}
		}
	}
}

void ATestChunk::DrawSignChangeVoxels()
{
	for (size_t i = 0; i < voxel_datas.Num(); i++)
	{
		if(voxel_datas[i].solid_corners == 255 || voxel_datas[i].solid_corners == 0) continue;
		FIntVector idx_3d = Get3DIndexFrom1D(i, voxel_resolution);

		FVector pos = FVector(static_cast<float>(idx_3d.X) * voxel_size, static_cast<float>(idx_3d.Y) * voxel_size, static_cast<float>(idx_3d.Z) * voxel_size);

		FColor color = FColor::Red;
		DrawDebugPoint(GetWorld(), pos, voxel_size*0.1f, color);
	}
}

void ATestChunk::DrawVertices()
{
	for (size_t i = 0; i < debug_verts_pos.Num(); i++)
	{
		FVector pos = FVector(debug_verts_pos[i]);
		FVector normal = FVector(debug_verts_normal[i]);

		FColor color = FColor::White;
		FColor normal_color = FLinearColor((normal.X + 1.f) * 0.5f, (normal.Z + 1.f) * 0.5f, (normal.Y + 1.f) * 0.5f).ToFColor(true);

		DrawDebugPoint(GetWorld(), pos, voxel_size * 0.1f, color);
		DrawDebugDirectionalArrow(GetWorld(), pos, pos + normal * 40.f, 10.f, normal_color);
	}

	for (size_t i = 0; i < voxel_datas.Num(); i++)
	{
		if(voxel_datas[i].solid_corners == 255 || voxel_datas[i].solid_corners == 0) continue;

		FIntVector idx = Get3DIndexFrom1D(i, voxel_resolution);

		FVector pos = FVector(idx.X * voxel_size + 0.5f * voxel_size, idx.Y * voxel_size + 0.5f * voxel_size, idx.Z * voxel_size + 0.5f * voxel_size);

		DrawDebugBox(GetWorld(), pos, FVector(voxel_size*0.5f), FColor::White);
	}

	for (size_t i = 0; i < debug_intersections.Num(); i++)
	{
		DrawDebugPoint(GetWorld(), FVector(debug_intersections[i]), voxel_size*0.10f, FColor::Red);
	}
}

void ATestChunk::DualContour(TArray<float>& densities)
{
	using namespace RealtimeMesh;

	TArray<DC_SignChangeEdge> edges;
	voxel_datas.Empty();
	debug_verts_normal.Empty();
	debug_intersections.Empty();

	RealtimeMesh::FRealtimeMeshStreamSet stream_set;
	RealtimeMesh::TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> builder(stream_set);
	builder.EnableTangents();

	float half_voxel_size = voxel_size*0.5f;

	float scale_fac = 0.01f;

	const FVector3f voxel_corner_offsets[8] = 
	{
		{-half_voxel_size,-half_voxel_size,half_voxel_size}, 
		{half_voxel_size, -half_voxel_size, half_voxel_size},
		{half_voxel_size, -half_voxel_size, -half_voxel_size},
		{-half_voxel_size, -half_voxel_size, -half_voxel_size},
		{-half_voxel_size,half_voxel_size,half_voxel_size},
		{half_voxel_size, half_voxel_size, half_voxel_size},
		{half_voxel_size, half_voxel_size, -half_voxel_size},
		{-half_voxel_size, half_voxel_size, -half_voxel_size}
	};

	uint32 edges_corner_map[12][2] = 
	{
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 0}, {1, 5}, {2, 6}, {7, 3},
		{4, 5}, {5, 6}, {6, 7}, {7, 4}
	};

	const TMap<int32, int32> edge_to_idx = 
	{
		{3, 0}, {2, 1}, {7, 2}
	};
	const TMap<int32, int32> idx_to_edge =
	{
		{0, 3}, {1, 2}, {2, 7}
	};

	//get voxel with its 8 density points and detect sign change
	for (size_t x = 0; x < voxel_resolution.X; x++)
	{
		for (size_t y = 0; y < voxel_resolution.Y; y++)
		{
			for (size_t z = 0; z < voxel_resolution.Z; z++)
			{
				DC_VoxelData data;
				
				int32 densityarr_indices[8];
				FIntVector arr_dim_densities = {voxel_resolution.X+1, voxel_resolution.Y+1, voxel_resolution.Z+1};
				//optimization: we will be checking the same edge multiple times
				densityarr_indices[0] = Get1DIndexFrom3D(x,y,z+1, arr_dim_densities);
				densityarr_indices[1] = Get1DIndexFrom3D(x+1,y,z+1, arr_dim_densities);
				densityarr_indices[2] = Get1DIndexFrom3D(x + 1, y, z, arr_dim_densities);
				densityarr_indices[3] = Get1DIndexFrom3D(x,y,z, arr_dim_densities);
				densityarr_indices[4] = Get1DIndexFrom3D(x, y+1, z+1, arr_dim_densities);
				densityarr_indices[5] = Get1DIndexFrom3D(x+1, y+1, z+1, arr_dim_densities);
				densityarr_indices[6] = Get1DIndexFrom3D(x + 1, y+1, z, arr_dim_densities);
				densityarr_indices[7] = Get1DIndexFrom3D(x, y+1, z, arr_dim_densities);

				for (size_t i = 0; i < 8; i++)
				{
					float density = densities[densityarr_indices[i]] - iso_surface;
					data.solid_corners |= ((density <= 0.f ? 1 : 0) << i);
				}

				voxel_datas.Add(data);
				if (data.solid_corners == 255 || data.solid_corners == 0) continue;
				
				static constexpr uint32 MAX_ZERO_CROSSINGS = 6; 
				uint32 edge_count = 0;
				UE_ASSUME(edge_count != 0);

				FVector3f vox_pos = FVector3f(x*voxel_size+voxel_size*0.5f, y*voxel_size+voxel_size*0.5f, z*voxel_size+voxel_size*0.5f);

				quadric3 vox_pq;

				FVector3f vert_normal{};

				for (size_t i = 0; i < 3; i++)
				{
					unsigned char s1 = (data.solid_corners >> edges_corner_map[idx_to_edge[i]][0]) & 1;
					unsigned char s2 = (data.solid_corners >> edges_corner_map[idx_to_edge[i]][1]) & 1;

					if(s1 == s2) continue;

					edges.Emplace(idx_to_edge[i], !s1, Get1DIndexFrom3D(x, y, z, voxel_resolution));
				}
				
				FVector3f mass_point{};
				for (size_t i = 0; i < 12 && edge_count < MAX_ZERO_CROSSINGS; i++)
				{
					unsigned char s1 = (data.solid_corners >> edges_corner_map[i][0]) & 1;
					unsigned char s2 = (data.solid_corners >> edges_corner_map[i][1]) & 1;

					if(s1 == s2) continue;

					//detected sign change on current edge


					FVector3f corner_1 = voxel_corner_offsets[edges_corner_map[i][0]] + vox_pos;
					FVector3f corner_2 = voxel_corner_offsets[edges_corner_map[i][1]] + vox_pos;

					float d1 = densities[densityarr_indices[edges_corner_map[i][0]]] - iso_surface;
					float d2 = densities[densityarr_indices[edges_corner_map[i][1]]] - iso_surface;

					// (1-alpha)*d1 + alpha*d2 = 0
					// d1 - alpha*d1 + alpha*d2 = 0
					// d1 + alpha(-d1+d2) = 0
					// alpha = d1 / (d1-d2)
					float alpha = d1 / (d1 - d2 + FLT_EPSILON);

					FVector3f intersection = FMath::Lerp(corner_1, corner_2, alpha);
					debug_intersections.Add(intersection);
					mass_point += intersection;

					//at 32 vox size, i dont think it's worth doing better zero crossing
					float alpha_density = gen->GetNoiseSingle3D(intersection.X * scale_fac, intersection.Y * scale_fac, intersection.Z * scale_fac) - iso_surface;

					FVector3f normal = FDMGetNormal(intersection * scale_fac);
					vert_normal += normal;

					vox_pq += quadric3::probabilistic_plane_quadric(intersection * scale_fac, normal, stddev_pos, stddev_normal);
					edge_count++;
				}

				vert_normal /= edge_count;
				mass_point /= edge_count;
				debug_verts_normal.Add(vert_normal);

				FVector3f vertex_pos = vox_pq.minimizer() * 1.f/scale_fac;

				/*if(vertex_pos.X > (vox_pos.X+half_voxel_size) || vertex_pos.X < (vox_pos.X-half_voxel_size) || vertex_pos.Y > (vox_pos.Y+half_voxel_size) || 
					vertex_pos.Y < (vox_pos.Y-half_voxel_size) || vertex_pos.Z > (vox_pos.Z +half_voxel_size) || vertex_pos.Z < (vox_pos.Z-half_voxel_size))
				{
					vertex_pos = mass_point;
				}*/

				voxel_datas.Last().vert_index = builder.AddVertex(vertex_pos).SetNormal(vert_normal).GetIndex();
			}
		}
	}
	
	//strides
	const int s_x = voxel_resolution.Y * voxel_resolution.Z;
	const int s_y = voxel_resolution.Z;
	const int s_z = 1;

	const int32 stride_table[3][3] = 
	{
		{-s_x, -s_x - s_y, -s_y},
		{-s_z, -s_z - s_y, - s_y},
		{-s_x, -s_x - s_z, -s_z}
	};

	for (auto& edge : edges)
	{
		//connect the minimizers of the voxels containing this edge

		unsigned short own_vox_idx = edge.vox_idx; // 0
		FIntVector idx_3D = Get3DIndexFrom1D(own_vox_idx, voxel_resolution);

		//own vox is on lower boundary, accesses will fail
		if(!(idx_3D.X && idx_3D.Y && idx_3D.Z)) continue;

		const int32* offset_table = stride_table[edge_to_idx[edge.edge_idx]];
		unsigned short vox_2_idx = edge.vox_idx + offset_table[0]; // 1
		unsigned short vox_3_idx = edge.vox_idx + offset_table[1]; // 2
		unsigned short vox_4_idx = edge.vox_idx + offset_table[2]; // 3
		
		if(!edge.flip) 
		{
			builder.AddTriangle(voxel_datas[own_vox_idx].vert_index, voxel_datas[vox_4_idx].vert_index, voxel_datas[vox_2_idx].vert_index);
			builder.AddTriangle(voxel_datas[vox_3_idx].vert_index, voxel_datas[vox_2_idx].vert_index, voxel_datas[vox_4_idx].vert_index);
		}
		else 
		{
			builder.AddTriangle(voxel_datas[own_vox_idx].vert_index, voxel_datas[vox_2_idx].vert_index, voxel_datas[vox_4_idx].vert_index);
			builder.AddTriangle(voxel_datas[vox_3_idx].vert_index, voxel_datas[vox_4_idx].vert_index, voxel_datas[vox_2_idx].vert_index);
		}
		
	}
	debug_verts_pos.Empty();
	debug_verts_pos.Reserve(builder.NumVertices());

	for (size_t i = 0; i < builder.NumVertices(); i++)
	{
		 debug_verts_pos.Emplace(builder.EditVertex(i).GetPosition());
	}

	mesh->SetupMaterialSlot(0, "PrimaryMaterial");
	const FRealtimeMeshSectionGroupKey group_key = FRealtimeMeshSectionGroupKey::Create(0, FName("DC_Mesh"));
	mesh->CreateSectionGroup(group_key, stream_set);
}

FVector3f ATestChunk::FDMGetNormal(FVector3f at_point)
{
	//x, y, z axii order
	float x_positions[6] = { at_point.X + fdm_normal_offset,  at_point.X - fdm_normal_offset, at_point.X, at_point.X, at_point.X, at_point.X };
	float y_positions[6] = { at_point.Y,  at_point.Y, at_point.Y + fdm_normal_offset, at_point.Y - fdm_normal_offset, at_point.Y, at_point.Y };
	float z_positions[6] = { at_point.Z,  at_point.Z, at_point.Z, at_point.Z, at_point.Z + fdm_normal_offset, at_point.Z - fdm_normal_offset };

	TArray<float> noise = gen->GetNoiseFromPositions3D_NonThreaded(x_positions, y_positions, z_positions, 6);

	FVector3f normal = FVector3f(noise[0] - noise[1], noise[2] - noise[3], noise[4] - noise[5]);

	return normal.GetUnsafeNormal();
}

// Called every frame
void ATestChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if(noise_gen_task.IsCompleted())
	{
		TArray<float>& result = noise_gen_task.GetResult();
		//DrawNoiseDensities(result);

		DualContour(result);
		if(draw_debug) DrawVertices();
	}
}

