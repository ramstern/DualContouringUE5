// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_OctreeRenderActor.h"

#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"

// Sets default values
ADC_OctreeRenderActor::ADC_OctreeRenderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	mesh_component = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMesh"));
	mesh_component->SetupAttachment(RootComponent);
	mesh_component->bCastShadowAsTwoSided = true;
}

// Called when the game starts or when spawned
void ADC_OctreeRenderActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ADC_OctreeRenderActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

