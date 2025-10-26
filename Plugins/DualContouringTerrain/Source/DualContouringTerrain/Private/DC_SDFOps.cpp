// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_SDFOps.h"


float SDF::Box(const FVector3f& p, const FVector3f& extent)
{
	FVector3f q = p.GetAbs() - extent;
	return q.ComponentMax(FVector3f(0.f)).Length() + FMath::Min(FMath::Max(q.X, FMath::Max(q.Y, q.Z)), 0.f);
}

float SDF::Sphere(const FVector3f& p, float radius)
{
	return p.Length() - radius;
}
