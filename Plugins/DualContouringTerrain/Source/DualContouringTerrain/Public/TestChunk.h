// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestChunk.generated.h"

class UNoiseDataGenerator;
class URealtimeMeshComponent;
class URealtimeMeshSimple;

struct DC_VoxelData 
{
	unsigned char solid_corners = 0;
	int32 vert_index;
};

struct DC_SignChangeEdge
{
	unsigned char edge_idx;
	unsigned char flip;
	unsigned short vox_idx;
};

UCLASS()
class DUALCONTOURINGTERRAIN_API ATestChunk : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestChunk();

	UPROPERTY(EditInstanceOnly)
	FIntVector voxel_resolution;

	UPROPERTY(EditInstanceOnly)
	float voxel_size;

	UPROPERTY(EditInstanceOnly)
	float iso_surface = 0.5f;

	UPROPERTY(EditInstanceOnly)
	float stddev_pos = 0.05f;

	UPROPERTY(EditInstanceOnly)
	float stddev_normal = 0.10f;

	UPROPERTY(EditInstanceOnly)
	float fdm_normal_offset = 0.02f;

	UPROPERTY(EditInstanceOnly)
	bool draw_debug;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void DrawNoiseDensities(TArray<float>& densities);
	void DrawSignChangeVoxels();
	void DrawVertices();

	void DualContour(TArray<float>& densities);
	bool has_contoured = false;

	FORCEINLINE int32 Get1DIndexFrom3D(int32 x, int32 y, int32 z, FIntVector arr_dim) const 
	{
		return z + (y * arr_dim.Z) + (x * arr_dim.Y * arr_dim.Z);
	}
	FORCEINLINE FIntVector Get3DIndexFrom1D(int32 idx, FIntVector arr_dim) const 
	{
		int32 XY = arr_dim.Y * arr_dim.Z;

		int32 x = idx / XY;
		int32 rem = idx % XY;
		int32 y = rem / arr_dim.Z;
		int32 z = rem % arr_dim.Z;

		return FIntVector(x, y, z);
	}

	FVector3f FDMGetNormal(FVector3f at_point);

	UGameInstance* game_instance = nullptr;
	UNoiseDataGenerator* gen = nullptr;
	TArray<float> noise_data;
	URealtimeMeshComponent* mesh_component = nullptr;
	URealtimeMeshSimple* mesh = nullptr;

	TArray<DC_VoxelData> voxel_datas;
	TArray<FVector3f> debug_verts_pos;
	TArray<FVector3f> debug_verts_normal;
	TArray<FVector3f> debug_intersections;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
