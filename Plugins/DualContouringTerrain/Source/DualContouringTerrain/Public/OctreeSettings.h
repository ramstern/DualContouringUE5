// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OctreeSettings.generated.h"

UENUM()
enum class EOctreeDebugDrawMode
{
	Box,
	Point
};

UCLASS(Config=Game, DefaultConfig)
class DUALCONTOURINGTERRAIN_API UOctreeSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UOctreeSettings();

	

	UPROPERTY(Config, EditAnywhere, meta = (ClampMin = 2, ClampMax = 10))
	uint32 max_depth = 2;

	UPROPERTY(Config, EditAnywhere, meta = (ClampMax = -1, ClampMin = -5))
	int32 shrink_depth = -4;

	UPROPERTY(Config, EditAnywhere, meta=(Multiple=2))
	float initial_size = 3200.f;

	UPROPERTY(Config, EditAnywhere, meta = (UIMin = -1, UIMax = 1))
	float iso_surface = 0.5f;

	UPROPERTY(EditAnywhere, Config, Category = "Rendering", meta = (AllowedClasses = "MaterialInterface"))
	TSoftObjectPtr<UMaterialInterface> mesh_material;

	UPROPERTY(Config, EditAnywhere)
	bool simplify = false;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "simplify", EditConditionHides, UIMin = 0))
	float simplify_threshold = 0.014f;

	UPROPERTY(Config, EditAnywhere, AdvancedDisplay)
	float normal_fdm_offset = 0.01f;

	UPROPERTY(Config, EditAnywhere, AdvancedDisplay)
	float stddev_pos = 0.01f;

	UPROPERTY(Config, EditAnywhere, AdvancedDisplay)
	float stddev_normal = 0.01f;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Drawing", meta=(NoRebuild="true"))
	bool draw_dc_data;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Drawing", meta=(NoRebuild="true"))
	bool draw_octree;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_octree", EditConditionHides, NoRebuild="true"), Category = "Debug Drawing")
	bool draw_leaves;

	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_leaves", EditConditionHides, NoRebuild="true"), Category = "Debug Drawing")
	bool draw_simplified_leaves;
	
	UPROPERTY(Config, EditAnywhere, meta = (EditCondition = "draw_octree", EditConditionHides, NoRebuild="true"), Category = "Debug Drawing")
	int32 debug_draw_how_deep = 1;

	UPROPERTY(Config, EditAnywhere, meta=(NoRebuild="true"))
	bool stop_dynamic_octree = false;

	DECLARE_MULTICAST_DELEGATE(FOnOctreeSettingsChanged);
	static FOnOctreeSettingsChanged& OnChanged()
	{
		return GetMutableDefault<UOctreeSettings>()->settings_changed;
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override
	{
		Super::PostEditChangeProperty(Event);

		if(Event.Property->HasMetaData(TEXT("NoRebuild"))) return;

		SaveConfig(); // persist to .ini
		settings_changed.Broadcast();
	}
#endif

private:
	FOnOctreeSettingsChanged settings_changed;
};
