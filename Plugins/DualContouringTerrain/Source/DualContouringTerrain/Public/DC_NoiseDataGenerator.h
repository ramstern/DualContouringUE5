// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "FastNoise/Utility/SmartNode.h"
#include "FastNoise/Generators/Generator.h"
#include "FastNoise/Generators/Modifiers.h"
#include "Tasks/Task.h"
#include "DC_NoiseDataGenerator.generated.h"

/**
 * 
 */
//struct DUALCONTOURINGTERRAIN_API FNoiseSampler
//{
//public:
//	FNoiseSampler() = delete;
//
//	[[nodiscard]] UE::Tasks::TTask<TArray<float>> GetNoiseFromPositions3D(const float* x_pos, const float* y_pos, const float* z_pos, int count) const;
//	[[nodiscard]] TArray<float> GetNoiseFromPositions3D_NonThreaded(const float* x_pos, const float* y_pos, const float* z_pos, int count) const;
//	[[nodiscard]] UE::Tasks::TTask<TArray<float>> GetNoiseUniformGrid2D(int32 x, int32 y) const;
//	[[nodiscard]] float GetNoiseSingle3D(float x, float y, float z) const;
//private:
//	friend class UNoiseDataGenerator;
//
//	FNoiseSampler(FastNoise::SmartNode<FastNoise::Generator> gen,
//				  FastNoise::SmartNode<FastNoise::DomainOffset> finalizer_offset,
//				  FastNoise::SmartNode<FastNoise::DomainScale> finalizer_scale);
//
//	FastNoise::SmartNode<FastNoise::Generator> generator;
//	FastNoise::SmartNode<FastNoise::DomainOffset> finalizer_offset;
//	FastNoise::SmartNode<FastNoise::DomainScale> finalizer_scale;
//
//	int32 seed;
//};

UCLASS()
class DUALCONTOURINGTERRAIN_API UNoiseDataGenerator : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FString GetCPUSIMDFeatureSet();

	void SetSampleOffset(FVector offset);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	void GeneratorFromNoiseToolString(FString string);

	FastSIMD::FeatureSet GetActiveGeneratorFeatureSet();

	//FNoiseSampler MakeNoiseSampler();

	[[nodiscard]] static TArray<float> GetNoiseFromPositions3D_NonThreaded(const float* x_pos, const float* y_pos, const float* z_pos, int count, int32 seed);
	[[nodiscard]] static float GetNoiseSingle3D(float x, float y, float z, int32 seed);

private:
	static FastNoise::SmartNode<FastNoise::Generator> generator;
	static FastNoise::SmartNode<FastNoise::DomainOffset> finalizer_offset;
	static FastNoise::SmartNode<FastNoise::DomainScale> finalizer_scale;
};
