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
	//mesh_component = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMesh"));
	//mesh_component->SetupAttachment(RootComponent);
	//mesh_component->bCastShadowAsTwoSided = true;
}

ADC_OctreeRenderActor::FetchInfo ADC_OctreeRenderActor::FetchRMComponentInfo(UMaterialInterface* material_interface)
{
	FetchInfo info;

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

		info.mesh = rmc->InitializeRealtimeMesh<URealtimeMeshSimple>();
		FRealtimeMeshCollisionConfiguration config = FRealtimeMeshCollisionConfiguration();
		info.mesh->SetCollisionConfig(config);

		info.mesh->ClearFlags(RF_Transactional);
		info.mesh->SetFlags(RF_Transient);
		info.mesh->SetupMaterialSlot(0, "PrimaryMaterial", material_interface);

		rmc->SetupAttachment(RootComponent);

		rmc->RegisterComponent();

		info.has_section = false;
		info.pooled = false;
		return info;
	}
	
	int32 idx;
	reuse_indices.Dequeue(idx);

	info.has_section = chunks_with_sections.FindAndRemoveChecked(idx);
	info.pooled = true;

	URealtimeMeshComponent* rmc = rmcs[idx];
	info.mesh = rmc->GetRealtimeMeshAs<URealtimeMeshSimple>();
	
	return info;	
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
	chunks_with_sections.Empty();
	reuse_indices.Empty();
}

void ADC_OctreeRenderActor::ReleaseRMC(URealtimeMeshComponent*& component, bool had_section_built)
{
	int32 idx = rmcs.Find(component);
	reuse_indices.Enqueue(idx);

	chunks_with_sections.Add(idx, had_section_built);

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
