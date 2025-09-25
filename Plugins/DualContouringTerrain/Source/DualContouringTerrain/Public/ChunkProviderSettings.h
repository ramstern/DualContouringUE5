// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ChunkProviderSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig)
class DUALCONTOURINGTERRAIN_API UChunkProviderSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere)
	int32 chunk_load_distance;

	DECLARE_MULTICAST_DELEGATE(FOnChunkProviderSettingsChanged);
	static FOnChunkProviderSettingsChanged& OnChanged()
	{
		return GetMutableDefault<UChunkProviderSettings>()->settings_changed;
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override
	{
		Super::PostEditChangeProperty(Event);

		if (Event.Property->HasMetaData(TEXT("NoRebuild"))) return;

		SaveConfig(); // persist to .ini
		settings_changed.Broadcast();
	}
#endif

private:
	FOnChunkProviderSettingsChanged settings_changed;
};
