// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "FastNoise/Utility/SmartNode.h"
#include "FastNoise/Generators/Generator.h"
#include "FastNoise/Generators/Modifiers.h"
#include "Tasks/Task.h"
#include "NoiseDataGenerator.generated.h"

/**
 * 
 */
UCLASS()
class DUALCONTOURINGTERRAIN_API UNoiseDataGenerator : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FString GetCPUSIMDFeatureSet();
	
	UFUNCTION(BlueprintCallable)
	FORCEINLINE void SetSeed(int32 _seed) { seed = _seed; }

	UFUNCTION(BlueprintCallable)
	FORCEINLINE int32 GetSeed() const { return seed; }

	void SetSampleOffset(FVector offset);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	void GeneratorFromNoiseToolString(FString string);

	[[nodiscard]] UE::Tasks::TTask<TArray<float>> GetNoiseFromPositions3D(const float* x_pos, const float* y_pos, const float* z_pos, int count);
	[[nodiscard]] TArray<float> GetNoiseFromPositions3D_NonThreaded(const float* x_pos, const float* y_pos, const float* z_pos, int count);
	[[nodiscard]] UE::Tasks::TTask<TArray<float>> GetNoiseUniformGrid2D(int32 x, int32 y);
	[[nodiscard]] float GetNoiseSingle3D(float x, float y, float z);

	FastSIMD::FeatureSet GetActiveGeneratorFeatureSet();

private:
	FastNoise::SmartNode<FastNoise::Generator> generator;
	FastNoise::SmartNode<FastNoise::DomainOffset> finalizer_offset;
	FastNoise::SmartNode<FastNoise::DomainScale> finalizer_scale;

	int32 seed;
};
