// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DC_ChunkProviderSettings.generated.h"

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

	UPROPERTY(Config, EditAnywhere)
	int32 chunk_size;

	UPROPERTY(Config, EditAnywhere, meta = (NoRebuild = "true"))
	bool stop_chunk_loading = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Drawing", meta = (NoRebuild = "true"))
	bool draw_debug_chunks;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_debug_chunks", EditConditionHides, NoRebuild = "true"), Category = "Debug Drawing")
	int32 chunk_draw_max_dist;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Drawing", meta = (NoRebuild = "true"))
	bool draw_dc_data;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Drawing", meta = (NoRebuild = "true"))
	bool draw_octree;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_octree", EditConditionHides, NoRebuild = "true"), Category = "Debug Drawing")
	bool draw_leaves;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_leaves", EditConditionHides, NoRebuild = "true"), Category = "Debug Drawing")
	bool draw_simplified_leaves;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_octree", EditConditionHides, NoRebuild = "true"), Category = "Debug Drawing")
	int32 debug_draw_how_deep = 1;

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
