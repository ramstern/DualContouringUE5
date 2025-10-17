// Fill out your copyright notice in the Description page of Project Settings.

#include "DC_NoiseDataGenerator.h"
#include "FastNoise/FastNoise.h"

FastNoise::SmartNode<FastNoise::Generator> UNoiseDataGenerator::generator = FastNoise::New<FastNoise::Checkerboard>();
FastNoise::SmartNode<FastNoise::DomainOffset> UNoiseDataGenerator::finalizer_offset = FastNoise::New<FastNoise::DomainOffset>();
FastNoise::SmartNode<FastNoise::DomainScale> UNoiseDataGenerator::finalizer_scale = FastNoise::New<FastNoise::DomainScale>();

FString UNoiseDataGenerator::GetCPUSIMDFeatureSet()
{
    return FString(FString("Maximum feature set: ") + FastSIMD::GetFeatureSetString(FastSIMD::DetectCpuMaxFeatureSet()));
}

void UNoiseDataGenerator::SetSampleOffset(FVector offset)
{
    finalizer_offset->SetOffset<FastNoise::Dim::X>(offset.X);
    finalizer_offset->SetOffset<FastNoise::Dim::Y>(offset.Y);
    finalizer_offset->SetOffset<FastNoise::Dim::Z>(offset.Z);
}

void UNoiseDataGenerator::Initialize(FSubsystemCollectionBase& Collection)
{
    GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.f, FColor::Red, GetCPUSIMDFeatureSet());
    GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.f, FColor::Red, FString("Initializing NoiseDataGenerator.."));

   /* generator = FastNoise::New<FastNoise::Checkerboard>();
    finalizer_offset = FastNoise::New<FastNoise::DomainOffset>();
    finalizer_scale = FastNoise::New<FastNoise::DomainScale>();*/
    finalizer_scale->SetScaling(6.f);

    //checkerboard test: Av8=
    GeneratorFromNoiseToolString(FString("Bv8="));

    finalizer_scale->SetSource(generator);
    finalizer_offset->SetSource(finalizer_scale);
}

void UNoiseDataGenerator::Deinitialize()
{
    
}

void UNoiseDataGenerator::GeneratorFromNoiseToolString(FString string)
{
    generator = FastNoise::NewFromEncodedNodeTree(TCHAR_TO_ANSI(string.GetCharArray().GetData()));
}

//UE::Tasks::TTask<TArray<float>> UNoiseDataGenerator::GetNoiseFromPositions3D(const float* x_pos, const float* y_pos, const float* z_pos, int count) const
//{
//    return UE::Tasks::Launch(UE_SOURCE_LOCATION, 
//    [this, count, x_pos, y_pos, z_pos]()
//    {
//        TArray<float> out_noise;
//        out_noise.SetNumUninitialized(count);
//        finalizer_offset->GenPositionArray3D(out_noise.GetData(), count, x_pos, y_pos, z_pos, 0.f, 0.f, 0.f, seed);
//        //rvo or move
//        return out_noise;
//    }, LowLevelTasks::ETaskPriority::BackgroundHigh);
//}

TArray<float> UNoiseDataGenerator::GetNoiseFromPositions3D_NonThreaded(const float* x_pos, const float* y_pos, const float* z_pos, int count, int32 seed)
{
    TArray<float> out_noise;
    out_noise.SetNumUninitialized(count);
    finalizer_offset->GenPositionArray3D(out_noise.GetData(), count, x_pos, y_pos, z_pos, 0.f, 0.f, 0.f, seed);
    return out_noise;
}

//UE::Tasks::TTask<TArray<float>> UNoiseDataGenerator::GetNoiseUniformGrid2D(int32 x, int32 y)
//{
//    return UE::Tasks::Launch(UE_SOURCE_LOCATION, 
//    [this, x, y]()
//    {
//        TArray<float> out_noise;
//        out_noise.SetNumUninitialized(x*y);
//        finalizer_offset->GenUniformGrid2D(out_noise.GetData(), 0, 0, x, y, seed);
//
//        //rvo or move
//        return out_noise;
//    }, LowLevelTasks::ETaskPriority::BackgroundHigh);
//}

float UNoiseDataGenerator::GetNoiseSingle3D(float x, float y, float z, int32 seed)
{
    return finalizer_offset->GenSingle3D(x, y, z, seed);
}

FastSIMD::FeatureSet UNoiseDataGenerator::GetActiveGeneratorFeatureSet()
{
    return generator->GetActiveFeatureSet();
}

//FNoiseSampler::FNoiseSampler(FastNoise::SmartNode<FastNoise::Generator> _gen,
//    FastNoise::SmartNode<FastNoise::DomainOffset> _finalizer_offset,
//    FastNoise::SmartNode<FastNoise::DomainScale> _finalizer_scale)
//{
//    generator = _gen;
//    finalizer_offset = _finalizer_offset;
//    finalizer_scale = _finalizer_scale;
//}
//
//FNoiseSampler UNoiseDataGenerator::MakeNoiseSampler()
//{
//    FNoiseSampler sampler(generator, finalizer_offset, finalizer_scale);
//
//    return sampler;
//}
