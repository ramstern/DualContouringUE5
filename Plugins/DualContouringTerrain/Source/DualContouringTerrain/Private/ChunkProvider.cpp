// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkProvider.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditorViewport.h"
#include "OctreeSettings.h"
#include "ChunkProviderSettings.h"
#include "OctreeManager.h"


void UChunkProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);


#if WITH_EDITOR
	UOctreeSettings::OnChanged().AddUObject(this, &UChunkProvider::ReloadChunks);
	UChunkProviderSettings::OnChanged().AddUObject(this, &UChunkProvider::ReloadChunks);
#endif

	chunk_settings = GetDefault<UChunkProviderSettings>();
	octree_manager = GetWorld()->GetSubsystem<UOctreeManager>();

	chunk_load_distance = chunk_settings->chunk_load_distance;
	chunks.Reserve((chunk_load_distance * chunk_load_distance)*4);
}

void UChunkProvider::Deinitialize()
{
	Super::Deinitialize();
}

void UChunkProvider::ReloadChunks()
{
	chunks.Empty();
	chunk_load_distance = chunk_settings->chunk_load_distance;
	chunks.Reserve((chunk_load_distance * chunk_load_distance) * 4);
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
		return APlayerCameraManager * camera_manager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraLocation();
	#endif
}

void UChunkProvider::Tick(float DeltaTime)
{
	FVector cam_pos = GetActiveCameraLocation();


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
