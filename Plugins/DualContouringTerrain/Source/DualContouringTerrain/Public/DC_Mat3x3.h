#pragma once

#include "Math/Vector.h"

struct FMatrix3x3
{
public:

	FMatrix3x3() = default;

	float data[3][3];

	inline float* operator[](int idx)
	{
		return data[idx];
	}

	inline const float* operator[](int idx) const
	{
		return data[idx];
	}

	inline FMatrix3x3 operator*(float scalar)
	{
		FMatrix3x3 mutated;

		mutated[0][0] = data[0][0] * scalar;
		mutated[0][1] = data[0][1] * scalar;
		mutated[0][2] = data[0][2] * scalar;
		mutated[1][0] = data[1][0] * scalar;
		mutated[1][1] = data[1][1] * scalar;
		mutated[1][2] = data[1][2] * scalar;
		mutated[2][0] = data[2][0] * scalar;
		mutated[2][1] = data[2][1] * scalar;
		mutated[2][2] = data[2][2] * scalar;
		
		return mutated;
	}

	inline FVector operator*(const FVector& vec)
	{
		FVector c0 = FVector(data[0][0]*vec.X, data[0][1]*vec.X, data[0][2]*vec.X);
		FVector c1 = FVector(data[1][0]*vec.Y, data[1][1]*vec.Y, data[1][2]*vec.Y);
		FVector c2 = FVector(data[2][0]*vec.Z, data[2][1]*vec.Z, data[2][2]*vec.Z);

		return c0+c1+c2;
	}

	inline FMatrix3x3 operator+(const FMatrix3x3& mat)
	{
		FMatrix3x3 mutated;

		mutated[0][0] = data[0][0] + mat.data[0][0];
		mutated[0][1] = data[0][1] + mat.data[0][1];
		mutated[0][2] = data[0][2] + mat.data[0][2];
		mutated[1][0] = data[1][0] + mat.data[1][0];
		mutated[1][1] = data[1][1] + mat.data[1][1];
		mutated[1][2] = data[1][2] + mat.data[1][2];
		mutated[2][0] = data[2][0] + mat.data[2][0];
		mutated[2][1] = data[2][1] + mat.data[2][1];
		mutated[2][2] = data[2][2] + mat.data[2][2];

		return mutated;
	}

	inline FMatrix3x3 operator-(const FMatrix3x3& mat)
	{
		FMatrix3x3 mutated;

		mutated[0][0] = data[0][0] - mat.data[0][0];
		mutated[0][1] = data[0][1] - mat.data[0][1];
		mutated[0][2] = data[0][2] - mat.data[0][2];
		mutated[1][0] = data[1][0] - mat.data[1][0];
		mutated[1][1] = data[1][1] - mat.data[1][1];
		mutated[1][2] = data[1][2] - mat.data[1][2];
		mutated[2][0] = data[2][0] - mat.data[2][0];
		mutated[2][1] = data[2][1] - mat.data[2][1];
		mutated[2][2] = data[2][2] - mat.data[2][2];
		
		return mutated;
	}
};