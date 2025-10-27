// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DC_OctreeRenderActor.generated.h"

class URealtimeMeshComponent;
class URealtimeMeshSimple;
class UOctreeSettings;

UCLASS()
class DUALCONTOURINGTERRAIN_API ADC_OctreeRenderActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADC_OctreeRenderActor();

	//returns whether the mesh and its rmc were newly created, instead of pooled
	bool FetchRMComponentMesh(URealtimeMeshSimple*& out_mesh, UMaterialInterface* material_interface);

	void DestroyAllRMCs();

	void ReleaseRMC(URealtimeMeshComponent*& component, bool had_section_built = true);

protected:
	// Called when the game starts or when spawned
	//virtual void BeginPlay() override;

	TArray<URealtimeMeshComponent*> rmcs;
	TQueue<int32> reuse_indices;
	TMap<int32, bool> chunks_with_sections;
public:	
	virtual void Destroyed() override;

	// Called every frame
	//virtual void Tick(float DeltaTime) override;
};
