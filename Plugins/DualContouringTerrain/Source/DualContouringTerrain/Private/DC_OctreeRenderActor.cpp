// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_OctreeRenderActor.h"

#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "DC_OctreeSettings.h"

// Sets default values
ADC_OctreeRenderActor::ADC_OctreeRenderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	octree_settings = GetDefault<UOctreeSettings>();
	octree_settings->mesh_material.LoadSynchronous();

	//mesh_component = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMesh"));
	//mesh_component->SetupAttachment(RootComponent);
	//mesh_component->bCastShadowAsTwoSided = true;
}

URealtimeMeshSimple* ADC_OctreeRenderActor::CreateRMComponentMesh()
{
	check(IsInGameThread());

	URealtimeMeshComponent*	rmc = NewObject<URealtimeMeshComponent>(this, URealtimeMeshComponent::StaticClass(), NAME_None, RF_Transient);

	rmcs.Add(rmc);

	AddInstanceComponent(rmc);

	rmc->ClearFlags(RF_Transactional);
	rmc->SetMobility(EComponentMobility::Stationary);
	rmc->bCastShadowAsTwoSided = true;

	URealtimeMeshSimple* mesh = rmc->InitializeRealtimeMesh<URealtimeMeshSimple>();

	mesh->ClearFlags(RF_Transactional);
	mesh->SetFlags(RF_Transient);
	mesh->SetupMaterialSlot(0, "PrimaryMaterial", octree_settings->mesh_material.Get());

	rmc->SetupAttachment(RootComponent);

	rmc->RegisterComponent();

	return mesh;
}



void ADC_OctreeRenderActor::DestroyAllRMCs()
{
	for (int32 i = 0; i < rmcs.Num(); i++)
	{
		rmcs[i]->DestroyComponent();
	}
	rmcs.Empty();
}

void ADC_OctreeRenderActor::DestroyRMC(URealtimeMeshComponent*& component)
{
	RemoveInstanceComponent(component);

	component->DestroyComponent();
	rmcs.RemoveSingle(component);

	component = nullptr;
}



//// Called when the game starts or when spawned
//void ADC_OctreeRenderActor::BeginPlay()
//{
//	Super::BeginPlay();
//}
//
//// Called every frame
//void ADC_OctreeRenderActor::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//
//}
//
