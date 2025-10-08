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

	UPROPERTY(Config, EditAnywhere, meta = (UIMin = -1, UIMax = 1))
	float iso_surface = 0.5f;

	UPROPERTY(EditAnywhere, Config, Category = "Rendering", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
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
