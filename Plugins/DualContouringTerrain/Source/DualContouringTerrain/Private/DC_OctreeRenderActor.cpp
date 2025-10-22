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

bool ADC_OctreeRenderActor::FetchRMComponentMesh(URealtimeMeshSimple*& out_mesh)
{
	//check(IsInGameThread());

	//no available rmc's, create and add new one
	if(reuse_indices.IsEmpty())
	{
		URealtimeMeshComponent* rmc = NewObject<URealtimeMeshComponent>(this, URealtimeMeshComponent::StaticClass(), NAME_None, RF_Transient);

		rmcs.Add(rmc);

		AddInstanceComponent(rmc);

		rmc->ClearFlags(RF_Transactional);
		rmc->SetMobility(EComponentMobility::Stationary);
		rmc->bCastShadowAsTwoSided = true;
		rmc->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
		rmc->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
		rmc->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
		rmc->SetCanEverAffectNavigation(false);

		out_mesh = rmc->InitializeRealtimeMesh<URealtimeMeshSimple>();
		FRealtimeMeshCollisionConfiguration config = FRealtimeMeshCollisionConfiguration();
		out_mesh->SetCollisionConfig(config);

		out_mesh->ClearFlags(RF_Transactional);
		out_mesh->SetFlags(RF_Transient);
		out_mesh->SetupMaterialSlot(0, "PrimaryMaterial", octree_settings->mesh_material.Get());

		rmc->SetupAttachment(RootComponent);

		rmc->RegisterComponent();

		return true;
	}
	
	int32 idx;
	reuse_indices.Dequeue(idx);

	URealtimeMeshComponent* rmc = rmcs[idx];
	out_mesh = rmc->GetRealtimeMeshAs<URealtimeMeshSimple>();

	return false;	
}



void ADC_OctreeRenderActor::DestroyAllRMCs()
{
	for (int32 i = 0; i < rmcs.Num(); i++)
	{
		rmcs[i]->GetRealtimeMeshAs<URealtimeMeshSimple>()->Reset();
		RemoveInstanceComponent(rmcs[i]);
		rmcs[i]->DestroyComponent();
	}
	rmcs.Empty();
	reuse_indices.Empty();
}

void ADC_OctreeRenderActor::ReleaseRMC(URealtimeMeshComponent*& component)
{
	int32 idx = rmcs.Find(component);
	reuse_indices.Enqueue(idx);

	component = nullptr;
}

void ADC_OctreeRenderActor::Destroyed()
{
	DestroyAllRMCs();

	Super::Destroyed();
}



//// Called when the game starts or when spawned
//void ADC_OctreeRenderActor::BeginPlay()
//{
//	Super::BeginPlay();
//}
//
// Called every frame
//void ADC_OctreeRenderActor::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//
//	UE_LOG(LogTemp, Display, TEXT("rcms num: %i"), rmcs.Num());
//	UE_LOG(LogTemp, Display, TEXT("reuse empty: %i"), reuse_indices.IsEmpty());
//}
//
